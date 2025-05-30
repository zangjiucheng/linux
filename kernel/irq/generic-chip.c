// SPDX-License-Identifier: GPL-2.0
/*
 * Library implementing the most common irq chip callback functions
 *
 * Copyright (C) 2011, Thomas Gleixner
 */
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/syscore_ops.h>

#include "internals.h"

static LIST_HEAD(gc_list);
static DEFINE_RAW_SPINLOCK(gc_lock);

/**
 * irq_gc_noop - NOOP function
 * @d: irq_data
 */
void irq_gc_noop(struct irq_data *d)
{
}
EXPORT_SYMBOL_GPL(irq_gc_noop);

/**
 * irq_gc_mask_disable_reg - Mask chip via disable register
 * @d: irq_data
 *
 * Chip has separate enable/disable registers instead of a single mask
 * register.
 */
void irq_gc_mask_disable_reg(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = irq_data_get_chip_type(d);
	u32 mask = d->mask;

	guard(raw_spinlock)(&gc->lock);
	irq_reg_writel(gc, mask, ct->regs.disable);
	*ct->mask_cache &= ~mask;
}
EXPORT_SYMBOL_GPL(irq_gc_mask_disable_reg);

/**
 * irq_gc_mask_set_bit - Mask chip via setting bit in mask register
 * @d: irq_data
 *
 * Chip has a single mask register. Values of this register are cached
 * and protected by gc->lock
 */
void irq_gc_mask_set_bit(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = irq_data_get_chip_type(d);
	u32 mask = d->mask;

	guard(raw_spinlock)(&gc->lock);
	*ct->mask_cache |= mask;
	irq_reg_writel(gc, *ct->mask_cache, ct->regs.mask);
}
EXPORT_SYMBOL_GPL(irq_gc_mask_set_bit);

/**
 * irq_gc_mask_clr_bit - Mask chip via clearing bit in mask register
 * @d: irq_data
 *
 * Chip has a single mask register. Values of this register are cached
 * and protected by gc->lock
 */
void irq_gc_mask_clr_bit(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = irq_data_get_chip_type(d);
	u32 mask = d->mask;

	guard(raw_spinlock)(&gc->lock);
	*ct->mask_cache &= ~mask;
	irq_reg_writel(gc, *ct->mask_cache, ct->regs.mask);
}
EXPORT_SYMBOL_GPL(irq_gc_mask_clr_bit);

/**
 * irq_gc_unmask_enable_reg - Unmask chip via enable register
 * @d: irq_data
 *
 * Chip has separate enable/disable registers instead of a single mask
 * register.
 */
void irq_gc_unmask_enable_reg(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = irq_data_get_chip_type(d);
	u32 mask = d->mask;

	guard(raw_spinlock)(&gc->lock);
	irq_reg_writel(gc, mask, ct->regs.enable);
	*ct->mask_cache |= mask;
}
EXPORT_SYMBOL_GPL(irq_gc_unmask_enable_reg);

/**
 * irq_gc_ack_set_bit - Ack pending interrupt via setting bit
 * @d: irq_data
 */
void irq_gc_ack_set_bit(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = irq_data_get_chip_type(d);
	u32 mask = d->mask;

	guard(raw_spinlock)(&gc->lock);
	irq_reg_writel(gc, mask, ct->regs.ack);
}
EXPORT_SYMBOL_GPL(irq_gc_ack_set_bit);

/**
 * irq_gc_ack_clr_bit - Ack pending interrupt via clearing bit
 * @d: irq_data
 */
void irq_gc_ack_clr_bit(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = irq_data_get_chip_type(d);
	u32 mask = ~d->mask;

	guard(raw_spinlock)(&gc->lock);
	irq_reg_writel(gc, mask, ct->regs.ack);
}

/**
 * irq_gc_mask_disable_and_ack_set - Mask and ack pending interrupt
 * @d: irq_data
 *
 * This generic implementation of the irq_mask_ack method is for chips
 * with separate enable/disable registers instead of a single mask
 * register and where a pending interrupt is acknowledged by setting a
 * bit.
 *
 * Note: This is the only permutation currently used.  Similar generic
 * functions should be added here if other permutations are required.
 */
void irq_gc_mask_disable_and_ack_set(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = irq_data_get_chip_type(d);
	u32 mask = d->mask;

	guard(raw_spinlock)(&gc->lock);
	irq_reg_writel(gc, mask, ct->regs.disable);
	*ct->mask_cache &= ~mask;
	irq_reg_writel(gc, mask, ct->regs.ack);
}
EXPORT_SYMBOL_GPL(irq_gc_mask_disable_and_ack_set);

/**
 * irq_gc_eoi - EOI interrupt
 * @d: irq_data
 */
void irq_gc_eoi(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = irq_data_get_chip_type(d);
	u32 mask = d->mask;

	guard(raw_spinlock)(&gc->lock);
	irq_reg_writel(gc, mask, ct->regs.eoi);
}

/**
 * irq_gc_set_wake - Set/clr wake bit for an interrupt
 * @d:  irq_data
 * @on: Indicates whether the wake bit should be set or cleared
 *
 * For chips where the wake from suspend functionality is not
 * configured in a separate register and the wakeup active state is
 * just stored in a bitmask.
 */
int irq_gc_set_wake(struct irq_data *d, unsigned int on)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	u32 mask = d->mask;

	if (!(mask & gc->wake_enabled))
		return -EINVAL;

	guard(raw_spinlock)(&gc->lock);
	if (on)
		gc->wake_active |= mask;
	else
		gc->wake_active &= ~mask;
	return 0;
}
EXPORT_SYMBOL_GPL(irq_gc_set_wake);

static u32 irq_readl_be(void __iomem *addr)
{
	return ioread32be(addr);
}

static void irq_writel_be(u32 val, void __iomem *addr)
{
	iowrite32be(val, addr);
}

void irq_init_generic_chip(struct irq_chip_generic *gc, const char *name,
			   int num_ct, unsigned int irq_base,
			   void __iomem *reg_base, irq_flow_handler_t handler)
{
	struct irq_chip_type *ct = gc->chip_types;
	int i;

	raw_spin_lock_init(&gc->lock);
	gc->num_ct = num_ct;
	gc->irq_base = irq_base;
	gc->reg_base = reg_base;
	for (i = 0; i < num_ct; i++)
		ct[i].chip.name = name;
	gc->chip_types->handler = handler;
}

/**
 * irq_alloc_generic_chip - Allocate a generic chip and initialize it
 * @name:	Name of the irq chip
 * @num_ct:	Number of irq_chip_type instances associated with this
 * @irq_base:	Interrupt base nr for this chip
 * @reg_base:	Register base address (virtual)
 * @handler:	Default flow handler associated with this chip
 *
 * Returns an initialized irq_chip_generic structure. The chip defaults
 * to the primary (index 0) irq_chip_type and @handler
 */
struct irq_chip_generic *
irq_alloc_generic_chip(const char *name, int num_ct, unsigned int irq_base,
		       void __iomem *reg_base, irq_flow_handler_t handler)
{
	struct irq_chip_generic *gc;

	gc = kzalloc(struct_size(gc, chip_types, num_ct), GFP_KERNEL);
	if (gc) {
		irq_init_generic_chip(gc, name, num_ct, irq_base, reg_base,
				      handler);
	}
	return gc;
}
EXPORT_SYMBOL_GPL(irq_alloc_generic_chip);

static void
irq_gc_init_mask_cache(struct irq_chip_generic *gc, enum irq_gc_flags flags)
{
	struct irq_chip_type *ct = gc->chip_types;
	u32 *mskptr = &gc->mask_cache, mskreg = ct->regs.mask;
	int i;

	for (i = 0; i < gc->num_ct; i++) {
		if (flags & IRQ_GC_MASK_CACHE_PER_TYPE) {
			mskptr = &ct[i].mask_cache_priv;
			mskreg = ct[i].regs.mask;
		}
		ct[i].mask_cache = mskptr;
		if (flags & IRQ_GC_INIT_MASK_CACHE)
			*mskptr = irq_reg_readl(gc, mskreg);
	}
}

/**
 * irq_domain_alloc_generic_chips - Allocate generic chips for an irq domain
 * @d:		irq domain for which to allocate chips
 * @info:	Generic chip information
 *
 * Return: 0 on success, negative error code on failure
 */
int irq_domain_alloc_generic_chips(struct irq_domain *d,
				   const struct irq_domain_chip_generic_info *info)
{
	struct irq_domain_chip_generic *dgc;
	struct irq_chip_generic *gc;
	int numchips, i;
	size_t dgc_sz;
	size_t gc_sz;
	size_t sz;
	void *tmp;
	int ret;

	if (d->gc)
		return -EBUSY;

	numchips = DIV_ROUND_UP(d->revmap_size, info->irqs_per_chip);
	if (!numchips)
		return -EINVAL;

	/* Allocate a pointer, generic chip and chiptypes for each chip */
	gc_sz = struct_size(gc, chip_types, info->num_ct);
	dgc_sz = struct_size(dgc, gc, numchips);
	sz = dgc_sz + numchips * gc_sz;

	tmp = dgc = kzalloc(sz, GFP_KERNEL);
	if (!dgc)
		return -ENOMEM;
	dgc->irqs_per_chip = info->irqs_per_chip;
	dgc->num_chips = numchips;
	dgc->irq_flags_to_set = info->irq_flags_to_set;
	dgc->irq_flags_to_clear = info->irq_flags_to_clear;
	dgc->gc_flags = info->gc_flags;
	dgc->exit = info->exit;
	d->gc = dgc;

	/* Calc pointer to the first generic chip */
	tmp += dgc_sz;
	for (i = 0; i < numchips; i++) {
		/* Store the pointer to the generic chip */
		dgc->gc[i] = gc = tmp;
		irq_init_generic_chip(gc, info->name, info->num_ct,
				      i * dgc->irqs_per_chip, NULL,
				      info->handler);

		gc->domain = d;
		if (dgc->gc_flags & IRQ_GC_BE_IO) {
			gc->reg_readl = &irq_readl_be;
			gc->reg_writel = &irq_writel_be;
		}

		if (info->init) {
			ret = info->init(gc);
			if (ret)
				goto err;
		}

		scoped_guard (raw_spinlock_irqsave, &gc_lock)
			list_add_tail(&gc->list, &gc_list);
		/* Calc pointer to the next generic chip */
		tmp += gc_sz;
	}
	return 0;

err:
	while (i--) {
		if (dgc->exit)
			dgc->exit(dgc->gc[i]);
		irq_remove_generic_chip(dgc->gc[i], ~0U, 0, 0);
	}
	d->gc = NULL;
	kfree(dgc);
	return ret;
}
EXPORT_SYMBOL_GPL(irq_domain_alloc_generic_chips);

/**
 * irq_domain_remove_generic_chips - Remove generic chips from an irq domain
 * @d: irq domain for which generic chips are to be removed
 */
void irq_domain_remove_generic_chips(struct irq_domain *d)
{
	struct irq_domain_chip_generic *dgc = d->gc;
	unsigned int i;

	if (!dgc)
		return;

	for (i = 0; i < dgc->num_chips; i++) {
		if (dgc->exit)
			dgc->exit(dgc->gc[i]);
		irq_remove_generic_chip(dgc->gc[i], ~0U, 0, 0);
	}
	d->gc = NULL;
	kfree(dgc);
}
EXPORT_SYMBOL_GPL(irq_domain_remove_generic_chips);

/**
 * __irq_alloc_domain_generic_chips - Allocate generic chips for an irq domain
 * @d:			irq domain for which to allocate chips
 * @irqs_per_chip:	Number of interrupts each chip handles (max 32)
 * @num_ct:		Number of irq_chip_type instances associated with this
 * @name:		Name of the irq chip
 * @handler:		Default flow handler associated with these chips
 * @clr:		IRQ_* bits to clear in the mapping function
 * @set:		IRQ_* bits to set in the mapping function
 * @gcflags:		Generic chip specific setup flags
 */
int __irq_alloc_domain_generic_chips(struct irq_domain *d, int irqs_per_chip,
				     int num_ct, const char *name,
				     irq_flow_handler_t handler,
				     unsigned int clr, unsigned int set,
				     enum irq_gc_flags gcflags)
{
	struct irq_domain_chip_generic_info info = {
		.irqs_per_chip		= irqs_per_chip,
		.num_ct			= num_ct,
		.name			= name,
		.handler		= handler,
		.irq_flags_to_clear	= clr,
		.irq_flags_to_set	= set,
		.gc_flags		= gcflags,
	};

	return irq_domain_alloc_generic_chips(d, &info);
}
EXPORT_SYMBOL_GPL(__irq_alloc_domain_generic_chips);

static struct irq_chip_generic *
__irq_get_domain_generic_chip(struct irq_domain *d, unsigned int hw_irq)
{
	struct irq_domain_chip_generic *dgc = d->gc;
	int idx;

	if (!dgc)
		return ERR_PTR(-ENODEV);
	idx = hw_irq / dgc->irqs_per_chip;
	if (idx >= dgc->num_chips)
		return ERR_PTR(-EINVAL);
	return dgc->gc[idx];
}

/**
 * irq_get_domain_generic_chip - Get a pointer to the generic chip of a hw_irq
 * @d:			irq domain pointer
 * @hw_irq:		Hardware interrupt number
 */
struct irq_chip_generic *
irq_get_domain_generic_chip(struct irq_domain *d, unsigned int hw_irq)
{
	struct irq_chip_generic *gc = __irq_get_domain_generic_chip(d, hw_irq);

	return !IS_ERR(gc) ? gc : NULL;
}
EXPORT_SYMBOL_GPL(irq_get_domain_generic_chip);

/*
 * Separate lockdep classes for interrupt chip which can nest irq_desc
 * lock and request mutex.
 */
static struct lock_class_key irq_nested_lock_class;
static struct lock_class_key irq_nested_request_class;

/*
 * irq_map_generic_chip - Map a generic chip for an irq domain
 */
int irq_map_generic_chip(struct irq_domain *d, unsigned int virq,
			 irq_hw_number_t hw_irq)
{
	struct irq_data *data = irq_domain_get_irq_data(d, virq);
	struct irq_domain_chip_generic *dgc = d->gc;
	struct irq_chip_generic *gc;
	struct irq_chip_type *ct;
	struct irq_chip *chip;
	int idx;

	gc = __irq_get_domain_generic_chip(d, hw_irq);
	if (IS_ERR(gc))
		return PTR_ERR(gc);

	idx = hw_irq % dgc->irqs_per_chip;

	if (test_bit(idx, &gc->unused))
		return -ENOTSUPP;

	if (test_bit(idx, &gc->installed))
		return -EBUSY;

	ct = gc->chip_types;
	chip = &ct->chip;

	/* We only init the cache for the first mapping of a generic chip */
	if (!gc->installed) {
		guard(raw_spinlock_irqsave)(&gc->lock);
		irq_gc_init_mask_cache(gc, dgc->gc_flags);
	}

	/* Mark the interrupt as installed */
	set_bit(idx, &gc->installed);

	if (dgc->gc_flags & IRQ_GC_INIT_NESTED_LOCK)
		irq_set_lockdep_class(virq, &irq_nested_lock_class,
				      &irq_nested_request_class);

	if (chip->irq_calc_mask)
		chip->irq_calc_mask(data);
	else
		data->mask = 1 << idx;

	irq_domain_set_info(d, virq, hw_irq, chip, gc, ct->handler, NULL, NULL);
	irq_modify_status(virq, dgc->irq_flags_to_clear, dgc->irq_flags_to_set);
	return 0;
}

void irq_unmap_generic_chip(struct irq_domain *d, unsigned int virq)
{
	struct irq_data *data = irq_domain_get_irq_data(d, virq);
	struct irq_domain_chip_generic *dgc = d->gc;
	unsigned int hw_irq = data->hwirq;
	struct irq_chip_generic *gc;
	int irq_idx;

	gc = irq_get_domain_generic_chip(d, hw_irq);
	if (!gc)
		return;

	irq_idx = hw_irq % dgc->irqs_per_chip;

	clear_bit(irq_idx, &gc->installed);
	irq_domain_set_info(d, virq, hw_irq, &no_irq_chip, NULL, NULL, NULL,
			    NULL);

}

const struct irq_domain_ops irq_generic_chip_ops = {
	.map	= irq_map_generic_chip,
	.unmap  = irq_unmap_generic_chip,
	.xlate	= irq_domain_xlate_onetwocell,
};
EXPORT_SYMBOL_GPL(irq_generic_chip_ops);

/**
 * irq_setup_generic_chip - Setup a range of interrupts with a generic chip
 * @gc:		Generic irq chip holding all data
 * @msk:	Bitmask holding the irqs to initialize relative to gc->irq_base
 * @flags:	Flags for initialization
 * @clr:	IRQ_* bits to clear
 * @set:	IRQ_* bits to set
 *
 * Set up max. 32 interrupts starting from gc->irq_base. Note, this
 * initializes all interrupts to the primary irq_chip_type and its
 * associated handler.
 */
void irq_setup_generic_chip(struct irq_chip_generic *gc, u32 msk,
			    enum irq_gc_flags flags, unsigned int clr,
			    unsigned int set)
{
	struct irq_chip_type *ct = gc->chip_types;
	struct irq_chip *chip = &ct->chip;
	unsigned int i;

	scoped_guard (raw_spinlock, &gc_lock)
		list_add_tail(&gc->list, &gc_list);

	irq_gc_init_mask_cache(gc, flags);

	for (i = gc->irq_base; msk; msk >>= 1, i++) {
		if (!(msk & 0x01))
			continue;

		if (flags & IRQ_GC_INIT_NESTED_LOCK)
			irq_set_lockdep_class(i, &irq_nested_lock_class,
					      &irq_nested_request_class);

		if (!(flags & IRQ_GC_NO_MASK)) {
			struct irq_data *d = irq_get_irq_data(i);

			if (chip->irq_calc_mask)
				chip->irq_calc_mask(d);
			else
				d->mask = 1 << (i - gc->irq_base);
		}
		irq_set_chip_and_handler(i, chip, ct->handler);
		irq_set_chip_data(i, gc);
		irq_modify_status(i, clr, set);
	}
	gc->irq_cnt = i - gc->irq_base;
}
EXPORT_SYMBOL_GPL(irq_setup_generic_chip);

/**
 * irq_setup_alt_chip - Switch to alternative chip
 * @d:		irq_data for this interrupt
 * @type:	Flow type to be initialized
 *
 * Only to be called from chip->irq_set_type() callbacks.
 */
int irq_setup_alt_chip(struct irq_data *d, unsigned int type)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = gc->chip_types;
	unsigned int i;

	for (i = 0; i < gc->num_ct; i++, ct++) {
		if (ct->type & type) {
			d->chip = &ct->chip;
			irq_data_to_desc(d)->handle_irq = ct->handler;
			return 0;
		}
	}
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(irq_setup_alt_chip);

/**
 * irq_remove_generic_chip - Remove a chip
 * @gc:		Generic irq chip holding all data
 * @msk:	Bitmask holding the irqs to initialize relative to gc->irq_base
 * @clr:	IRQ_* bits to clear
 * @set:	IRQ_* bits to set
 *
 * Remove up to 32 interrupts starting from gc->irq_base.
 */
void irq_remove_generic_chip(struct irq_chip_generic *gc, u32 msk,
			     unsigned int clr, unsigned int set)
{
	unsigned int i, virq;

	scoped_guard (raw_spinlock, &gc_lock)
		list_del(&gc->list);

	for (i = 0; msk; msk >>= 1, i++) {
		if (!(msk & 0x01))
			continue;

		/*
		 * Interrupt domain based chips store the base hardware
		 * interrupt number in gc::irq_base. Otherwise gc::irq_base
		 * contains the base Linux interrupt number.
		 */
		if (gc->domain) {
			virq = irq_find_mapping(gc->domain, gc->irq_base + i);
			if (!virq)
				continue;
		} else {
			virq = gc->irq_base + i;
		}

		/* Remove handler first. That will mask the irq line */
		irq_set_handler(virq, NULL);
		irq_set_chip(virq, &no_irq_chip);
		irq_set_chip_data(virq, NULL);
		irq_modify_status(virq, clr, set);
	}
}
EXPORT_SYMBOL_GPL(irq_remove_generic_chip);

static struct irq_data *irq_gc_get_irq_data(struct irq_chip_generic *gc)
{
	unsigned int virq;

	if (!gc->domain)
		return irq_get_irq_data(gc->irq_base);

	/*
	 * We don't know which of the irqs has been actually
	 * installed. Use the first one.
	 */
	if (!gc->installed)
		return NULL;

	virq = irq_find_mapping(gc->domain, gc->irq_base + __ffs(gc->installed));
	return virq ? irq_get_irq_data(virq) : NULL;
}

#ifdef CONFIG_PM
static int irq_gc_suspend(void)
{
	struct irq_chip_generic *gc;

	list_for_each_entry(gc, &gc_list, list) {
		struct irq_chip_type *ct = gc->chip_types;

		if (ct->chip.irq_suspend) {
			struct irq_data *data = irq_gc_get_irq_data(gc);

			if (data)
				ct->chip.irq_suspend(data);
		}

		if (gc->suspend)
			gc->suspend(gc);
	}
	return 0;
}

static void irq_gc_resume(void)
{
	struct irq_chip_generic *gc;

	list_for_each_entry(gc, &gc_list, list) {
		struct irq_chip_type *ct = gc->chip_types;

		if (gc->resume)
			gc->resume(gc);

		if (ct->chip.irq_resume) {
			struct irq_data *data = irq_gc_get_irq_data(gc);

			if (data)
				ct->chip.irq_resume(data);
		}
	}
}
#else
#define irq_gc_suspend NULL
#define irq_gc_resume NULL
#endif

static void irq_gc_shutdown(void)
{
	struct irq_chip_generic *gc;

	list_for_each_entry(gc, &gc_list, list) {
		struct irq_chip_type *ct = gc->chip_types;

		if (ct->chip.irq_pm_shutdown) {
			struct irq_data *data = irq_gc_get_irq_data(gc);

			if (data)
				ct->chip.irq_pm_shutdown(data);
		}
	}
}

static struct syscore_ops irq_gc_syscore_ops = {
	.suspend = irq_gc_suspend,
	.resume = irq_gc_resume,
	.shutdown = irq_gc_shutdown,
};

static int __init irq_gc_init_ops(void)
{
	register_syscore_ops(&irq_gc_syscore_ops);
	return 0;
}
device_initcall(irq_gc_init_ops);
