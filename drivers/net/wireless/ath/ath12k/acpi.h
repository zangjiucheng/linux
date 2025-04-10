/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef ATH12K_ACPI_H
#define ATH12K_ACPI_H

#include <linux/acpi.h>

#define ATH12K_ACPI_DSM_FUNC_SUPPORT_FUNCS	0
#define ATH12K_ACPI_DSM_FUNC_DISABLE_FLAG	2
#define ATH12K_ACPI_DSM_FUNC_BDF_EXT		3
#define ATH12K_ACPI_DSM_FUNC_BIOS_SAR		4
#define ATH12K_ACPI_DSM_FUNC_GEO_OFFSET		5
#define ATH12K_ACPI_DSM_FUNC_INDEX_CCA		6
#define ATH12K_ACPI_DSM_FUNC_TAS_CFG		8
#define ATH12K_ACPI_DSM_FUNC_TAS_DATA		9
#define ATH12K_ACPI_DSM_FUNC_INDEX_BAND_EDGE		10

#define ATH12K_ACPI_FUNC_BIT_DISABLE_FLAG		BIT(1)
#define ATH12K_ACPI_FUNC_BIT_BDF_EXT			BIT(2)
#define ATH12K_ACPI_FUNC_BIT_BIOS_SAR			BIT(3)
#define ATH12K_ACPI_FUNC_BIT_GEO_OFFSET			BIT(4)
#define ATH12K_ACPI_FUNC_BIT_CCA			BIT(5)
#define ATH12K_ACPI_FUNC_BIT_TAS_CFG			BIT(7)
#define ATH12K_ACPI_FUNC_BIT_TAS_DATA			BIT(8)
#define ATH12K_ACPI_FUNC_BIT_BAND_EDGE_CHAN_POWER	BIT(9)

#define ATH12K_ACPI_NOTIFY_EVENT			0x86
#define ATH12K_ACPI_FUNC_BIT_VALID(_acdata, _func)	(((_acdata).func_bit) & (_func))
#define ATH12K_ACPI_CHEK_BIT_VALID(_acdata, _func)	(((_acdata).bit_flag) & (_func))

#define ATH12K_ACPI_TAS_DATA_VERSION		0x1
#define ATH12K_ACPI_TAS_DATA_ENABLE		0x1
#define ATH12K_ACPI_POWER_LIMIT_VERSION		0x1
#define ATH12K_ACPI_POWER_LIMIT_ENABLE_FLAG	0x1
#define ATH12K_ACPI_CCA_THR_VERSION		0x1
#define ATH12K_ACPI_CCA_THR_ENABLE_FLAG		0x1
#define ATH12K_ACPI_BAND_EDGE_VERSION		0x1
#define ATH12K_ACPI_BAND_EDGE_ENABLE_FLAG	0x1

#define ATH12K_ACPI_GEO_OFFSET_DATA_OFFSET	1
#define ATH12K_ACPI_DBS_BACKOFF_DATA_OFFSET	2
#define ATH12K_ACPI_CCA_THR_OFFSET_DATA_OFFSET	5
#define ATH12K_ACPI_BIOS_SAR_DBS_BACKOFF_LEN	10
#define ATH12K_ACPI_POWER_LIMIT_DATA_OFFSET	12
#define ATH12K_ACPI_BIOS_SAR_GEO_OFFSET_LEN	18
#define ATH12K_ACPI_BIOS_SAR_TABLE_LEN		22
#define ATH12K_ACPI_CCA_THR_OFFSET_LEN		36

#define ATH12K_ACPI_DSM_TAS_DATA_SIZE			69
#define ATH12K_ACPI_DSM_BAND_EDGE_DATA_SIZE		100
#define ATH12K_ACPI_DSM_TAS_CFG_SIZE			108

#define ATH12K_ACPI_DSM_FUNC_MIN_BITMAP_SIZE	1
#define ATH12K_ACPI_DSM_FUNC_MAX_BITMAP_SIZE	4

#define ATH12K_ACPI_DSM_DISABLE_11BE_BIT	BIT(0)
#define ATH12K_ACPI_DSM_DISABLE_RFKILL_BIT	BIT(2)

#define ATH12K_ACPI_BDF_ANCHOR_STRING_LEN	3
#define ATH12K_ACPI_BDF_ANCHOR_STRING		"BDF"
#define ATH12K_ACPI_BDF_MAX_LEN			100

#define ATH12K_ACPI_DSM_GEO_OFFSET_DATA_SIZE (ATH12K_ACPI_GEO_OFFSET_DATA_OFFSET + \
					      ATH12K_ACPI_BIOS_SAR_GEO_OFFSET_LEN)
#define ATH12K_ACPI_DSM_BIOS_SAR_DATA_SIZE (ATH12K_ACPI_POWER_LIMIT_DATA_OFFSET + \
					    ATH12K_ACPI_BIOS_SAR_TABLE_LEN)
#define ATH12K_ACPI_DSM_CCA_DATA_SIZE (ATH12K_ACPI_CCA_THR_OFFSET_DATA_OFFSET + \
				       ATH12K_ACPI_CCA_THR_OFFSET_LEN)

#ifdef CONFIG_ACPI

int ath12k_acpi_start(struct ath12k_base *ab);
void ath12k_acpi_stop(struct ath12k_base *ab);
bool ath12k_acpi_get_disable_rfkill(struct ath12k_base *ab);
bool ath12k_acpi_get_disable_11be(struct ath12k_base *ab);
void ath12k_acpi_set_dsm_func(struct ath12k_base *ab);
int ath12k_acpi_check_bdf_variant_name(struct ath12k_base *ab);

#else

static inline int ath12k_acpi_start(struct ath12k_base *ab)
{
	return 0;
}

static inline void ath12k_acpi_stop(struct ath12k_base *ab)
{
}

static inline bool ath12k_acpi_get_disable_rfkill(struct ath12k_base *ab)
{
	return false;
}

static inline bool ath12k_acpi_get_disable_11be(struct ath12k_base *ab)
{
	return false;
}

static inline void ath12k_acpi_set_dsm_func(struct ath12k_base *ab)
{
}

static inline int ath12k_acpi_check_bdf_variant_name(struct ath12k_base *ab)
{
	return 0;
}

#endif /* CONFIG_ACPI */

#endif /* ATH12K_ACPI_H */
