// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/of.h>
// #include <soc/qcom/of_common.h>
#include <linux/of_fdt.h>
#include "msm_cvp_internal.h"
#include "msm_cvp_debug.h"
#include "cvp_hfi_api.h"
#include "cvp_hfi.h"
#include <dt-bindings/clock/qcom,kaanapali-evacc.h>
#include <dt-bindings/clock/qcom,kaanapali-gcc.h>

extern int of_fdt_get_ddrtype(void);

#define UBWC_CONFIG(mco, mlo, hbo, bslo, bso, rs, mc, ml, hbb, bsl, bsp) \
{	\
	.override_bit_info.max_channel_override = mco,	\
	.override_bit_info.mal_length_override = mlo,	\
	.override_bit_info.hb_override = hbo,	\
	.override_bit_info.bank_swzl_level_override = bslo,	\
	.override_bit_info.bank_spreading_override = bso,	\
	.override_bit_info.reserved = rs,	\
	.max_channels = mc,	\
	.mal_length = ml,	\
	.highest_bank_bit = hbb,	\
	.bank_swzl_level = bsl,	\
	.bank_spreading = bsp,	\
}

struct msm_cvp_hfi_defs cvp_hfi_defs_v2[MAX_PKT_IDX];
struct msm_cvp_hfi_defs cvp_hfi_msg_defs_v2[MAX_PKT_IDX];

static struct msm_cvp_common_data sm8850_common_data[] = {
	{
		.key = "qcom,pm-qos-latency-us",
		.value = 50,
	},
	{
		.key = "qcom,sw-power-collapse",
		.value = 1,
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 0,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 2, /*
					* As per design driver allows 3rd
					* instance as well since the secure
					* flags were updated later for the
					* current instance. Hence total
					* secure sessions would be
					* max-secure-instances + 1.
					*/
	},
	{
		.key = "qcom,max-supported-instances",
		.value = 32,
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 3000,
	},
	{
		.key = "qcom,hw-resp-timeout",
		.value = 2000,
	},
	{
		.key = "CVP_GDSC_FRAMEWORK_TYPE",
		.value = 1,    /* framework-type: 0x0 --> Regulator framework, 0x1 --> GenPD framework */
	}
};

/* Default UBWC config for LPDDR5 */
static struct msm_cvp_ubwc_config_data kona_ubwc_data[] = {
	UBWC_CONFIG(1, 1, 1, 0, 0, 0, 8, 32, 16, 0, 0),
};


static struct msm_cvp_qos_setting pakala_noc_qos = {
	.axi_qos = 0x99,
	.prioritylut_low = 0x33333333,
	.prioritylut_high = 0x33333333,
	.urgency_low = 0x1033,
	.urgency_low_ro = 0x1003,
	.dangerlut_low = 0x0,
	.safelut_low = 0xffff,
};


/*upstream properties*/
static const char * const cvp_kaanapali_cache_slice_names[] = {
    "cvpfw",
    "cvp",
};

static const struct cvp_subcache_desc cvp_kaanapali_desc = {
    .num_slices  = ARRAY_SIZE(cvp_kaanapali_cache_slice_names),
    .cache_slice_names = cvp_kaanapali_cache_slice_names,
};

// Order of entries in cvp_bus_kaanapali_descs should be same as interconnects in DT
static const struct cvp_bus_desc cvp_bus_kaanapali_descs[] = {  
    {
        .name = "eva-cfg",
        .governor = "performance",
        .min_bw = 1000,
        .max_bw = 1000,
    },
    {
        .name = "eva-ddr",
        .governor = "performance",
        .min_bw = 1000,
        .max_bw = 6533000,
    },
};

static const struct reg_value_pair kaanapali_reg_presets_data[] = {
    { .reg = 0xB0088, .value = 0x0 },
};

static const struct cvp_reg_presets cvp_kaanapali_reg_presets = {
    .count = ARRAY_SIZE(kaanapali_reg_presets_data),
    .tbl   = kaanapali_reg_presets_data,
};

static const struct cvp_ipcc_reg_region cvp_kaanapali_ipcc_reg = {
    .base = 0x400000,
    .size   = 0x100000,
};

static const u32 cvp_kaanapali_reset_power_states[] = {
    0x0,  /* cvp_core_reset/core */
};

static const struct cvp_reset_power_set_desc cvp_kaanapali_reset_power_set_desc = {
    .count  = ARRAY_SIZE(cvp_kaanapali_reset_power_states),
    .pwr_stats = cvp_kaanapali_reset_power_states,
};

static const u32 kaanapali_pd_hw_pc[] = { 0x0, 0x1 };

static const struct cvp_power_domains kaanapali_power_domains = {
    .pd_count = ARRAY_SIZE(kaanapali_pd_hw_pc),
    .power_domain_idx = 2,
    .gdsc_has_hw_pc = kaanapali_pd_hw_pc,
};

static const u32 kaanapali_clock_props_data[] = {
    0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x1 // eva0 and core0
};

static const struct cvp_clock_props kaanapali_clock_props = {
    .clock_props = kaanapali_clock_props_data,
    .count_clkProps     = ARRAY_SIZE(kaanapali_clock_props_data),
};

static const u32 kaanapali_allowed_clock_rates[] = {
    350000000,
    400000000,
    450000000,
    500000000,
    550000000,
};

static const struct cvp_allowed_clock_rates kaanapali_allowed_clocks = {
    .clk_rates = kaanapali_allowed_clock_rates,
    .count = ARRAY_SIZE(kaanapali_allowed_clock_rates),
};

/*
* Register OPP clocks: core0 (EVA_CC_MVS0_CLK) and eva0 (EVA_CC_MVS0C_CLK)
* are scaled together via the OPP framework using dev_pm_opp_config_clks_simple.
* The opp-hz values in DT map: first value -> core0, second value -> eva0.
*/
static const char *const kaanapali_eva_opp_clk_names[] = {
	"core0",
	"eva0",
	NULL,
};

/*
 * OPP voltage rail domains — attached with PD_FLAG_DEV_LINK_ON | PD_FLAG_REQUIRED_OPP
 * so the OPP framework automatically votes the correct voltage corner when
 * dev_pm_opp_set_opp() is called. Matches iris sm8550_opp_pd_table pattern.
 */
static const char *const kaanapali_opp_pd_table[] = { "mxc", "mmcx" };

static const struct cvp_iommu_context_bank kaanapali_cvp_context_banks[] = {
    {
        .name = "eva_hlos",
        .buffer_type = 0xfff,
        .iova_start = 0x4b000000,
        .iova_size  = 0x90000000,
        .vmid = 0,   // secure = false
    },
    {
        .name = "eva_sec_nonpixel",
        .buffer_type = 0x741,
        .iova_start = 0x01000000,
        .iova_size  = 0x25800000,
        .vmid = 0xB,   // secure = true 
    },
    {
        .name = "eva_sec_pixel",
        .buffer_type = 0x106,
        .iova_start = 0x26800000,
        .iova_size  = 0x24800000,
        .vmid = 0xA,   // secure = true 
    },
};

/* clock-ids should have same sequence as clocks in DT*/
static const u32 eva_kaanapali_clock_ids[] = {
        GCC_EVA_AXI0C_CLK,
        EVA_CC_SLEEP_CLK,
        EVA_CC_MVS0C_FREERUN_CLK,
        EVA_CC_MVS0C_CLK,
        GCC_EVA_AXI0_CLK,
        EVA_CC_MVS0_FREERUN_CLK,
        EVA_CC_MVS0_CLK,
};

static struct msm_cvp_platform_data sm8850_data = {
	.common_data = sm8850_common_data,
	.common_data_length = ARRAY_SIZE(sm8850_common_data),
	.ubwc_config = kona_ubwc_data,	/*Reuse Kona setting*/
	.noc_qos = &pakala_noc_qos,
	.cvp_hfi = cvp_hfi_defs_v2,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v2,
	.hfi_ver = 2,
	.pas_id = 26,
	.ipcc_regs = &cvp_kaanapali_ipcc_reg,
	.reg_presets = &cvp_kaanapali_reg_presets,
	.reset_power_sets = &cvp_kaanapali_reset_power_set_desc,
	.power_domains = &kaanapali_power_domains,
	.clock_props = &kaanapali_clock_props,
	.clock_ids = eva_kaanapali_clock_ids, 
	.num_clock_ids = ARRAY_SIZE(eva_kaanapali_clock_ids),
	.allowed_clk_rates = &kaanapali_allowed_clocks,
	.opp_clk_tbl = kaanapali_eva_opp_clk_names,
	.opp_pd_tbl = kaanapali_opp_pd_table,
	.opp_pd_tbl_size = ARRAY_SIZE(kaanapali_opp_pd_table),
	.bus_descs = cvp_bus_kaanapali_descs,
	.subcache_desc = &cvp_kaanapali_desc,
	.cb_data = kaanapali_cvp_context_banks,
	.cb_data_size = ARRAY_SIZE(kaanapali_cvp_context_banks),
};

static const struct of_device_id msm_cvp_dt_match[] = {
	{
		.compatible = "qcom,kaanapali-eva",
		.data = &sm8850_data,
	},
	{},
};

struct msm_cvp_hfi_defs *cvp_hfi_defs;
struct msm_cvp_hfi_defs *cvp_hfi_msg_defs;

/*
 * WARN: name field CAN NOT hold more than 63 chars
 *	 excluding the ending '\0'
 *
 * NOTE: the def entry index for the command packet is
 *	 "the packet type - HFI_CMD_SESSION_CVP_START"
 */

struct msm_cvp_hfi_defs cvp_hfi_defs_v2[MAX_PKT_IDX] = {
	[HFI_CMD_SESSION_EVA_SCALER_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_SCALER_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_SCALER_FRAME",
		},
	[HFI_CMD_SESSION_EVA_SCALER_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_SCALER_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_SCALER_CONFIG",
		},
};

/*
 * Below are for msg packet
 */
struct msm_cvp_hfi_defs cvp_hfi_msg_defs_v2[MAX_PKT_IDX] = {
	[HFI_MSG_SESSION_CVP_OPERATION_CONFIG  - HFI_MSG_SESSION_EVA_OFFSET] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_OPERATION_CONFIG,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_OPERATION_CONFIG",
	},
	[HFI_MSG_SESSION_EVA_SCALER  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_SCALER,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_SCALER",
	},
};

int get_pkt_index(struct cvp_hal_session_cmd_pkt *hdr)
{
	int pkt_idx;
	u32 thirteenth_bit;
	u32 fourteenth_bit;

	if (!hdr)
		return -EINVAL;

	thirteenth_bit = (hdr->packet_type >> 12) & 1;
	fourteenth_bit = (hdr->packet_type >> 13) & 1;

	if (thirteenth_bit && fourteenth_bit)
		pkt_idx = hdr->packet_type - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET;
	else if (!thirteenth_bit && fourteenth_bit)
		pkt_idx = hdr->packet_type - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET;
	else if (thirteenth_bit && !fourteenth_bit)
		pkt_idx = hdr->packet_type - HFI_CMD_SESSION_EVA_CTRL_OFFSET + CTRL_OFFSET;
	else
		pkt_idx = hdr->packet_type - HFI_CMD_SESSION_EVA_OFFSET;

	if ((pkt_idx < 0) || pkt_idx >= (MAX_PKT_IDX))
		return -EINVAL;

	if (cvp_hfi_defs[pkt_idx].size)
		return pkt_idx;

	return -EINVAL;
	
}

int get_pkt_index_from_type(u32 pkt_type)
{
	int pkt_idx;
	u32 thirteenth_bit;
	u32 fourteenth_bit;

	thirteenth_bit = (pkt_type >> 12) & 1;
	fourteenth_bit = (pkt_type >> 13) & 1;

	if (thirteenth_bit && fourteenth_bit)
		pkt_idx = pkt_type - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET;
	else if (!thirteenth_bit && fourteenth_bit)
		pkt_idx = pkt_type - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET;
	else if (thirteenth_bit && !fourteenth_bit)
		pkt_idx = pkt_type - HFI_CMD_SESSION_EVA_CTRL_OFFSET + CTRL_OFFSET;
	else
		pkt_idx = pkt_type - HFI_CMD_SESSION_EVA_OFFSET;

	if ((pkt_idx < 0) || pkt_idx >= (MAX_PKT_IDX))
		return -EINVAL;

	if (cvp_hfi_defs[pkt_idx].size)
		return pkt_idx;
	return -EINVAL;
}

bool is_config_pkt(struct cvp_hal_session_cmd_pkt *hdr)
{
	int pkt_idx;

	pkt_idx = get_pkt_index(hdr);
	if (pkt_idx < 0) {
		dprintk(CVP_ERR, "%s incorrect packet type %x\n", __func__,
				hdr->packet_type);
		return false;
	}
	return cvp_hfi_defs[pkt_idx].is_config_pkt;
}

const char *get_pkt_name_from_type(u32 pkt_type)
{
	u32 mask;
	int pkt_idx;

	if ((pkt_type & 0x03000000) == HFI_CMD_OFFSET) {
		int pkt_idx = get_pkt_index_from_type(pkt_type);

		if ((pkt_idx < 0) || pkt_idx >= (MAX_PKT_IDX))
			return "";
		else
			return cvp_hfi_defs[pkt_idx].name;
	} else if (((pkt_type & 0x03000000) == HFI_MSG_OFFSET)) {
		mask = pkt_type & 0x3000;
		pkt_idx = -EINVAL;

		if (mask == 0x3000)
			pkt_idx = pkt_type - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX;
		else if (mask == 0x1000)
			pkt_idx = pkt_type - HFI_MSG_SESSION_EVA_CTRL_OFFSET
				+ MSG_SESSION_EVA_CTRL_INDEX;
		else if (mask == 0)
			pkt_idx = pkt_type - HFI_MSG_SESSION_EVA_OFFSET;

		if ((pkt_idx < 0) || pkt_idx >= (MAX_PKT_IDX))
			return "";
		else
			return cvp_hfi_msg_defs[pkt_idx].name;
	}
	return "";
}

MODULE_DEVICE_TABLE(of, msm_cvp_dt_match);

void *cvp_get_drv_data(struct device *dev)
{
	struct msm_cvp_platform_data *driver_data;
	const struct of_device_id *match;
	uint32_t ddr_type = DDR_TYPE_LPDDR5;

	if (!IS_ENABLED(CONFIG_OF) || !dev->of_node)
		goto exit;

	match = of_match_node(msm_cvp_dt_match, dev->of_node);

	if (!match)
		return NULL;

	driver_data = (struct msm_cvp_platform_data *)match->data;
exit:
	return driver_data;
}
