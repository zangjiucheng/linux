// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Generic System Framebuffers
 * Copyright (c) 2012-2013 David Herrmann <dh.herrmann@gmail.com>
 */

/*
 * simple-framebuffer probing
 * Try to convert "screen_info" into a "simple-framebuffer" compatible mode.
 * If the mode is incompatible, we return "false" and let the caller create
 * legacy nodes instead.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/platform_data/simplefb.h>
#include <linux/platform_device.h>
#include <linux/screen_info.h>
#include <linux/sysfb.h>

static const char simplefb_resname[] = "BOOTFB";
static const struct simplefb_format formats[] = SIMPLEFB_FORMATS;

/* try parsing screen_info into a simple-framebuffer mode struct */
__init bool sysfb_parse_mode(const struct screen_info *si,
			     struct simplefb_platform_data *mode)
{
	__u8 type;
	u32 bits_per_pixel;
	unsigned int i;

	type = si->orig_video_isVGA;
	if (type != VIDEO_TYPE_VLFB && type != VIDEO_TYPE_EFI)
		return false;

	bits_per_pixel = __screen_info_lfb_bits_per_pixel(si);

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		const struct simplefb_format *f = &formats[i];

		if (f->transp.length)
			continue; /* transparent formats are unsupported by VESA/EFI */

		if (bits_per_pixel == f->bits_per_pixel &&
		    si->red_size == f->red.length &&
		    si->red_pos == f->red.offset &&
		    si->green_size == f->green.length &&
		    si->green_pos == f->green.offset &&
		    si->blue_size == f->blue.length &&
		    si->blue_pos == f->blue.offset) {
			mode->format = f->name;
			mode->width = si->lfb_width;
			mode->height = si->lfb_height;
			mode->stride = si->lfb_linelength;
			return true;
		}
	}

	return false;
}

__init struct platform_device *sysfb_create_simplefb(const struct screen_info *si,
						     const struct simplefb_platform_data *mode,
						     struct device *parent)
{
	struct platform_device *pd;
	struct resource res;
	u64 base, size;
	u32 length;
	int ret;

	/*
	 * If the 64BIT_BASE capability is set, ext_lfb_base will contain the
	 * upper half of the base address. Assemble the address, then make sure
	 * it is valid and we can actually access it.
	 */
	base = si->lfb_base;
	if (si->capabilities & VIDEO_CAPABILITY_64BIT_BASE)
		base |= (u64)si->ext_lfb_base << 32;
	if (!base || (u64)(resource_size_t)base != base) {
		printk(KERN_DEBUG "sysfb: inaccessible VRAM base\n");
		return ERR_PTR(-EINVAL);
	}

	/*
	 * Don't use lfb_size as IORESOURCE size, since it may contain the
	 * entire VMEM, and thus require huge mappings. Use just the part we
	 * need, that is, the part where the framebuffer is located. But verify
	 * that it does not exceed the advertised VMEM.
	 * Note that in case of VBE, the lfb_size is shifted by 16 bits for
	 * historical reasons.
	 */
	size = si->lfb_size;
	if (si->orig_video_isVGA == VIDEO_TYPE_VLFB)
		size <<= 16;
	length = mode->height * mode->stride;
	if (length > size) {
		printk(KERN_WARNING "sysfb: VRAM smaller than advertised\n");
		return ERR_PTR(-EINVAL);
	}
	length = PAGE_ALIGN(length);

	/* setup IORESOURCE_MEM as framebuffer memory */
	memset(&res, 0, sizeof(res));
	res.flags = IORESOURCE_MEM;
	res.name = simplefb_resname;
	res.start = base;
	res.end = res.start + length - 1;
	if (res.end <= res.start)
		return ERR_PTR(-EINVAL);

	pd = platform_device_alloc("simple-framebuffer", 0);
	if (!pd)
		return ERR_PTR(-ENOMEM);

	pd->dev.parent = parent;

	sysfb_set_efifb_fwnode(pd);

	ret = platform_device_add_resources(pd, &res, 1);
	if (ret)
		goto err_put_device;

	ret = platform_device_add_data(pd, mode, sizeof(*mode));
	if (ret)
		goto err_put_device;

	ret = platform_device_add(pd);
	if (ret)
		goto err_put_device;

	return pd;

err_put_device:
	platform_device_put(pd);

	return ERR_PTR(ret);
}
