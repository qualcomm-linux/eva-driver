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
#include "cvp_hfi_io.h"
#include "target/cvp_clock_ids.h"

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

struct msm_cvp_hfi_defs cvp_hfi_defs_v1[MAX_PKT_IDX];
struct msm_cvp_hfi_defs cvp_hfi_msg_defs_v1[MAX_PKT_IDX];
struct msm_cvp_hfi_defs cvp_hfi_defs_v2[MAX_PKT_IDX];
struct msm_cvp_hfi_defs cvp_hfi_msg_defs_v2[MAX_PKT_IDX];

static struct msm_cvp_common_data default_common_data[] = {
	{
		.key = "qcom,auto-pil",
		.value = 1,
	},
};

static struct msm_cvp_common_data sm8450_common_data[] = {
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
		.value = 1,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 2,             /*
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
		.value = 16,
    
	},
	{
		.key = "qcom,max-ssr-allowed",
		.value = 1,		/*
					 * Maxinum number of SSR before BUG_ON
					 */
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
		.key = "qcom,dsp-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dsp-enabled",
		.value = 1,
	}
};

static struct msm_cvp_common_data sm8550_common_data[] = {
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
		.value = 2,             /*
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
		.value = 16,
	},
	{
		.key = "qcom,max-ssr-allowed",
		.value = 1,		/*
					 * Maxinum number of SSR before BUG_ON
					 */
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
		.key = "qcom,dsp-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dsp-enabled",
		.value = 1,
	}
};

static struct msm_cvp_common_data sm8550_tvm_common_data[] = {
	{
		.key = "qcom,pm-qos-latency-us",
		.value = 50,
	},
	{
		.key = "qcom,sw-power-collapse",
		.value = 0,
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 0,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 2,             /*
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
		.value = 16,
	},
	{
		.key = "qcom,max-ssr-allowed",
		.value = 1,		/*
					 * Maxinum number of SSR before BUG_ON
					 */
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
		.key = "qcom,dsp-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dsp-enabled",
		.value = 0,
	}
};

static struct msm_cvp_common_data sm8650_common_data[] = {
	{
		.key = "qcom,pm-qos-latency-us",
		.value = 50,
	},
	{
		.key = "qcom,sw-power-collapse",
		.value = 0,
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
		.value = 16,
	},
	{
		.key = "qcom,max-ssr-allowed",
		.value = 1,	/*
					* Maxinum number of SSR before BUG_ON
					*/
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 3000,
	},
	{
		.key = "qcom,hw-resp-timeout",
		.value = 100000,
	},
	{
		.key = "qcom,dsp-resp-timeout",
		.value = 100000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dsp-enabled",
		.value = 1,
	}
};

static struct msm_cvp_common_data sm8750_common_data[] = {
	{
		.key = "qcom,pm-qos-latency-us",
		.value = 50,
	},
	{
		.key = "qcom,sw-power-collapse",
#ifdef USE_PRESIL
		.value = 0,
#else
		.value = 1,
#endif
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
		.value = 16,
	},
	{
		.key = "qcom,max-ssr-allowed",
		.value = 1,	/*
					* Maxinum number of SSR before BUG_ON
					*/
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 3000,
	},
	{
		.key = "qcom,hw-resp-timeout",
#ifdef USE_PRESIL
		.value = 180000,
#else
		.value = 2000,
#endif
	},
	{
		.key = "qcom,dsp-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dsp-enabled",
		.value = 1,
	}
};

static struct msm_cvp_common_data sm8735_common_data[] = {
	{
		.key = "qcom,pm-qos-latency-us",
		.value = 50,
	},
	{
		.key = "qcom,sw-power-collapse",
#ifdef USE_PRESIL
		.value = 0,
#else
		.value = 1,
#endif
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 0,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 2,
	},
	{
		.key = "qcom,max-supported-instances",
		.value = 16,
	},
	{
		.key = "qcom,max-ssr-allowed",
		.value = 1,
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 3000,
	},
	{
		.key = "qcom,hw-resp-timeout",
#ifdef USE_PRESIL
		.value = 180000,
#else
		.value = 2000,
#endif
	},
	{
		.key = "qcom,dsp-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dsp-enabled",
		.value = 1,
	},
	{
		.key = "qcom,rcg_vnoc_clk_en_low",
		.value = 0x8,
	}
};

static struct msm_cvp_common_data sm8850_common_data[] = {
	{
		.key = "qcom,pm-qos-latency-us",
		.value = 50,
	},
	{
		.key = "qcom,sw-power-collapse",
#ifdef USE_PRESIL
		.value = 0,
#else
		.value = 0,
#endif
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
		.key = "qcom,max-ssr-allowed",
		.value = 1,	/*
					* Maxinum number of SSR before BUG_ON
					*/
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 3000,
	},
	{
		.key = "qcom,hw-resp-timeout",
#ifdef USE_PRESIL
		.value = 180000,
#else
		.value = 2000,
#endif
	},
	{
		.key = "qcom,dsp-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dsp-enabled",
		.value = 0,
	},
	{
		.key = "CVP_GDSC_FRAMEWORK_TYPE",
		.value = 1,    /* framework-type: 0x0 --> Regulator framework, 0x1 --> GenPD framework */
	}
};

static struct msm_cvp_common_data sm8845_common_data[] = {
	{
		.key = "qcom,pm-qos-latency-us",
		.value = 50,
	},
	{
		.key = "qcom,sw-power-collapse",
#ifdef USE_PRESIL
		.value = 0,
#else
		.value = 1,
#endif
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 0,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 2,
	},
	{
		.key = "qcom,max-supported-instances",
		.value = 32,
	},
	{
		.key = "qcom,max-ssr-allowed",
		.value = 1,
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 3000,
	},
	{
		.key = "qcom,hw-resp-timeout",
#ifdef USE_PRESIL
		.value = 180000,
#else
		.value = 2000,
#endif
	},
	{
		.key = "qcom,dsp-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dsp-enabled",
		.value = 1,
	},
	{
		.key = "qcom,qos_noc_urgency_low_a_bitmask",
		.value = 0x30,
	},
	{
		.key = "qcom,qos_noc_urgency_low_b_bitmask",
		.value = 0x3,
	}
};

static struct msm_cvp_common_data sm8975_common_data[] = {
	{
		.key = "qcom,pm-qos-latency-us",
		.value = 50,
	},
	{
		.key = "qcom,sw-power-collapse",
#ifdef USE_PRESIL
		.value = 0,
#else
		.value = 1,
#endif
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 0,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 2,
	},
	{
		.key = "qcom,max-supported-instances",
		.value = 32,
	},
	{
		.key = "qcom,max-ssr-allowed",
		.value = 1,
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 3000,
	},
	{
		.key = "qcom,hw-resp-timeout",
#ifdef USE_PRESIL
		.value = 60000,
#else
		.value = 2000,
#endif
	},
	{
		.key = "qcom,dsp-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dsp-enabled",
		.value = 1,
	},
	{
		.key = "qcom,qos_noc_urgency_low_a_bitmask",
		.value = 0x30,
	},
	{
		.key = "qcom,qos_noc_urgency_low_b_bitmask",
		.value = 0x3,
	}
};

static struct msm_cvp_common_data sm8950_common_data[] = {
	{
		.key = "qcom,pm-qos-latency-us",
		.value = 50,
	},
	{
		.key = "qcom,sw-power-collapse",
#ifdef USE_PRESIL
		.value = 0,
#else
		.value = 1,
#endif
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 0,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 2,
	},
	{
		.key = "qcom,max-supported-instances",
		.value = 32,
	},
	{
		.key = "qcom,max-ssr-allowed",
		.value = 1,
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 3000,
	},
	{
		.key = "qcom,hw-resp-timeout",
#ifdef USE_PRESIL
		.value = 60000,
#else
		.value = 2000,
#endif
	},
	{
		.key = "qcom,dsp-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dsp-enabled",
#ifdef CVP_DSP_ENABLED
		.value = 1,
#else
		.value = 0,
#endif
	},
	{
		.key = "qcom,qos_noc_urgency_low_a_bitmask",
		.value = 0x30,
	},
	{
		.key = "qcom,qos_noc_urgency_low_b_bitmask",
		.value = 0x3,
	},
	{
		.key = "qcom,core_noc_cx_pd_disable",
		.value = 1,
	}
};


static struct msm_cvp_common_data sa8797p_common_data[] = {
	{
		.key = "qcom,pm-qos-latency-us",
		.value = 50,		/* inherited sm8850 — verify w/ SA8797P power team */
	},
	{
		.key = "qcom,sw-power-collapse",
#ifdef USE_PRESIL
		.value = 0,
#else
		.value = 0,
#endif
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 0,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 2,
	},
	{
		.key = "qcom,max-supported-instances",
		.value = 32,		/* inherited sm8850 */
	},
	{
		.key = "qcom,max-ssr-allowed",
		.value = 1,	/*
					* Maxinum number of SSR before BUG_ON
					*/
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 3000,
	},
	{
		.key = "qcom,hw-resp-timeout",
#ifdef USE_PRESIL
		.value = 180000,
#else
		.value = 2000,
#endif
	},
	{
		.key = "qcom,dsp-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dsp-enabled",
		.value = 0,		/* inherited sm8850 */
	},
	{
		.key = "CVP_GDSC_FRAMEWORK_TYPE",
		.value = 1,    /* framework-type: 0x0 --> Regulator framework, 0x1 --> GenPD framework */
	},
	{
      .key = "qcom,rcg_vnoc_clk_en_low",
      .value = 0x8,
   }
};
/* Default UBWC config for LPDDR5 */
static struct msm_cvp_ubwc_config_data kona_ubwc_data[] = {
	UBWC_CONFIG(1, 1, 1, 0, 0, 0, 8, 32, 16, 0, 0),
};

static struct msm_cvp_qos_setting waipio_noc_qos = {
	.axi_qos = 0x99,
	.prioritylut_low = 0x22222222,
	.prioritylut_high = 0x33333333,
	.urgency_low = 0x1022,
	.dangerlut_low = 0x0,
	.safelut_low = 0xffff,
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
static const char * const cvp_glymur_cache_slice_names[] = {
    "cvp",
};


static struct msm_cvp_qos_setting nordau_noc_qos = {
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


static const struct cvp_subcache_desc cvp_glymur_desc = {
    .num_slices  = ARRAY_SIZE(cvp_glymur_cache_slice_names),
    .cache_slice_names = cvp_glymur_cache_slice_names,
};


static const struct cvp_subcache_desc cvp_kaanapali_desc = {
    .num_slices  = ARRAY_SIZE(cvp_kaanapali_cache_slice_names),
    .cache_slice_names = cvp_kaanapali_cache_slice_names,
};

// Order of entries in cvp_bus_glymur_descs should be same as interconnects in DT
static const struct cvp_bus_desc cvp_bus_glymur_descs[] = {  
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

static const struct reg_value_pair glymur_reg_presets_data[] = {
    { .reg = 0xB0088, .value = 0x0 },
};

static const struct reg_value_pair kaanapali_reg_presets_data[] = {
    { .reg = 0xB0088, .value = 0x0 },
};


static const struct cvp_reg_presets cvp_glymur_reg_presets = {
    .count = ARRAY_SIZE(glymur_reg_presets_data),
    .tbl   = glymur_reg_presets_data,
};

static const struct cvp_reg_presets cvp_kaanapali_reg_presets = {
    .count = ARRAY_SIZE(kaanapali_reg_presets_data),
    .tbl   = kaanapali_reg_presets_data,
};

const struct cvp_gcc_reg_region cvp_glymur_gcc_reg = {
    .base = 0x110000,
    .size   = 0x90000,
};

const struct cvp_gcc_reg_region cvp_kaanapali_gcc_reg = {
    .base = 0x110000,
    .size   = 0x90000,
};

static const struct cvp_ipcc_reg_region cvp_glymur_ipcc_reg = {
    .base = 0x400000,
    .size   = 0x100000,
};

static const struct cvp_ipcc_reg_region cvp_kaanapali_ipcc_reg = {
    .base = 0x400000,
    .size   = 0x100000,
};

static const u32 cvp_glymur_reset_power_states[] = {
    0x0,  /* cvp_core_reset/core */
};

static const u32 cvp_kaanapali_reset_power_states[] = {
    0x0,  /* cvp_core_reset/core */
};

static const struct cvp_reset_power_set_desc cvp_glymur_reset_power_set_desc = {
    .count  = ARRAY_SIZE(cvp_glymur_reset_power_states),
    .pwr_stats = cvp_glymur_reset_power_states,
};

static const struct cvp_reset_power_set_desc cvp_kaanapali_reset_power_set_desc = {
    .count  = ARRAY_SIZE(cvp_kaanapali_reset_power_states),
    .pwr_stats = cvp_kaanapali_reset_power_states,
};


static const struct cvp_regspace_mappings cvp_glymur_regspace = {
    .hwmutex = {
        .iova = 0xFFB00000,
        .size = 0x2000,
        .phys = 0x1f4a000,
    },
    .aon = {
        .iova = 0xFF80F000,
        .size = 0x1000,
        .phys = 0x0ABE0000,
    },
    .aon_timer = {
        .iova = 0xFFA00000,
        .size = 0x1000,
        .phys = 0xc220000,
    },
};

static const u32 glymur_pd_hw_pc[] = { 0x0, 0x1 };

static const struct cvp_power_domains glymur_power_domains = {
    .pd_count = ARRAY_SIZE(glymur_pd_hw_pc),
    .power_domain_idx = 2,
    .gdsc_has_hw_pc = glymur_pd_hw_pc,
};

static const u32 glymur_clock_props_data[] = {
    0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x1
};

static const struct cvp_clock_props glymur_clock_props = {
    .clock_props = glymur_clock_props_data,
    .count_clkProps     = ARRAY_SIZE(glymur_clock_props_data),
};

static const u32 glymur_allowed_clock_rates[] = {
    350000000,
    400000000,
    450000000,
    500000000,
    550000000,
};

static const struct cvp_allowed_clock_rates glymur_allowed_clocks = {
    .clk_rates = glymur_allowed_clock_rates,
    .count = ARRAY_SIZE(glymur_allowed_clock_rates),
};

static const struct cvp_regspace_mappings cvp_kaanapali_regspace = {
    .hwmutex = {
        .iova = 0xFFB00000,
        .size = 0x2000,
        .phys = 0x1f4a000,
    },
    .aon = {
        .iova = 0xFF80F000,
        .size = 0x1000,
        .phys = 0x0ABE0000,
    },
    .aon_timer = {
        .iova = 0xFFA00000,
        .size = 0x1000,
        .phys = 0xc220000,
    },
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

static const char *const glymur_eva_opp_clk_names[] = {
	"core0",
	"eva0",
	NULL,
};

/*
 * OPP voltage rail domains — attached with PD_FLAG_DEV_LINK_ON | PD_FLAG_REQUIRED_OPP
 * so the OPP framework automatically votes the correct voltage corner when
 * dev_pm_opp_set_opp() is called. Matches iris sm8550_opp_pd_table pattern.
 */
static const char *const glymur_opp_pd_table[] = { "mxc", "mmcx" };

static const char *const kaanapali_opp_pd_table[] = { "mxc", "mmcx" };

static const struct cvp_ipclite_mappings cvp_kaanapali_ipclite_desc = {
    .iova_start = 0xFE500000,
};

static const struct cvp_ipclite_mappings cvp_glymur_ipclite_desc = {
    .iova_start = 0xFE500000,
};

static const struct cvp_iommu_context_bank glymur_cvp_context_banks[] = {
    {
        .name = "eva_hlos",
        .buffer_type = 0xfff,
        .iova_start = 0x4b000000,
        .iova_size  = 0x90000000,
        .vmid = 0,
    },
    {
        .name = "eva_sec_nonpixel",
        .buffer_type = 0x741,
        .iova_start = 0x01000000,
        .iova_size  = 0x25800000,
        .vmid = 0xB,
    },
    {
        .name = "eva_sec_pixel",
        .buffer_type = 0x106,
        .iova_start = 0x26800000,
        .iova_size  = 0x24800000,
        .vmid = 0xA,
    },
};

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

static const char * const cvp_nordau_cache_slice_names[] = {
    "cvp",
};

static const struct cvp_subcache_desc cvp_nordau_desc = {
    .num_slices = 0,
    .cache_slice_names = NULL,
};


static const struct cvp_bus_desc cvp_bus_nordau_descs[] = {
    {
        .name = "eva-cfg",		/* [DT dtsi:43] */
        .governor = "performance",	/* [INH] */
        .min_bw = 1000,			/* [INH] */
        .max_bw = 1000,			/* [INH] */
    },
    {
        .name = "eva-ddr",		/* [DT dtsi:43] */
        .governor = "performance",	/* [INH] */
        .min_bw = 1000,			/* [INH] */
        .max_bw = 6533000,		/* [INH] SoC DDR ceiling — verify for SA8797P */
    },
};


static const struct reg_value_pair nordau_reg_presets_data[] = {
    { .reg = 0xB0088, .value = 0x0 },
};

static const struct cvp_reg_presets cvp_nordau_reg_presets = {
    .count = ARRAY_SIZE(nordau_reg_presets_data),
    .tbl   = nordau_reg_presets_data,
};


const struct cvp_gcc_reg_region cvp_nordau_gcc_reg = {
    .base = 0x110000,
    .size   = 0x90000,
};


static const struct cvp_ipcc_reg_region cvp_nordau_ipcc_reg = {
    .base = 0x400000,
    .size   = 0x100000,
};


static const u32 cvp_nordau_reset_power_states[] = {
    0x0,  /* reset-names "core" -> EVA_CC_MVS0C_CLK_ARES (nordau-cvp.dtsi:34) */
};

static const struct cvp_reset_power_set_desc cvp_nordau_reset_power_set_desc = {
    .count  = ARRAY_SIZE(cvp_nordau_reset_power_states),
    .pwr_stats = cvp_nordau_reset_power_states,
};


static const struct cvp_regspace_mappings cvp_nordau_regspace = {
    .hwmutex = {
        .iova = 0xFFB00000,	/* [INH] SoC SMMU VA — unverified */
        .size = 0x2000,		/* [INH] */
        .phys = 0x1f4a000,	/* [INH] SA8797P TCSR hwmutex — unverified */
    },
    .aon = {
        .iova = 0xFF80F000,	/* [INH] SoC SMMU VA — unverified */

        .size = 0xF000,
        .phys = 0x0ABE0000,	/* [CSV] nordau_modules.csv:51 EVA_CVP_AON — VERIFIED */
    },
    .aon_timer = {
        .iova = 0xFFA00000,	/* [INH] SoC SMMU VA — unverified */
        .size = 0x1000,		/* [INH] */
        .phys = 0xc220000,	/* [INH] SA8797P timer — unverified */
    },
};


static const u32 nordau_pd_hw_pc[] = {
    0x0,  /* "eva"  / EVA_CC_MVS0C_GDSC — controller: no HW power collapse [INH] */
    0x1,  /* "core" / EVA_CC_MVS0_GDSC  — core: HW power collapse capable  [INH] */
};

static const struct cvp_power_domains nordau_power_domains = {
    .pd_count = ARRAY_SIZE(nordau_pd_hw_pc),	/* [DT] 2 GDSCs, dtsi:14-15 */
    .power_domain_idx = 2,			/* [DT] OPP rails start at index 2 */
    .gdsc_has_hw_pc = nordau_pd_hw_pc,
};


static const u32 nordau_clock_props_data[] = {
    0x0,  /* [0] eva_iface     GCC_EVA_AXI0C_CLK        — fixed */
    0x0,  /* [1] core_iface    GCC_EVA_AXI0_CLK         — fixed */
    0x0,  /* [2] sleep         EVA_CC_SLEEP_CLK         — fixed 32 kHz */
    0x0,  /* [3] eva_freerun   EVA_CC_MVS0C_FREERUN_CLK — fixed */
    0x0,  /* [4] core_freerun  EVA_CC_MVS0_FREERUN_CLK  — fixed */
    0x1,  /* [5] eva0          EVA_CC_MVS0C_CLK         — HAS_SCALING [CSV:77-81] */
    0x1,  /* [6] core0         EVA_CC_MVS0_CLK          — HAS_SCALING [CSV:47-51] */
};

static const struct cvp_clock_props nordau_clock_props = {
    .clock_props = nordau_clock_props_data,
    .count_clkProps     = ARRAY_SIZE(nordau_clock_props_data),
};

static const u32 nordau_allowed_clock_rates[] = {
    280000000,	/* [CSV:48] SVS    */
    500000000,	/* [CSV:49] SVS_L1 */
    600000000,	/* [CSV:47] NOM    */
    625000000,	/* [CSV:50,51] TURBO == TURBO_L1 — deduped */
};

static const struct cvp_allowed_clock_rates nordau_allowed_clocks = {
    .clk_rates = nordau_allowed_clock_rates,
    .count = ARRAY_SIZE(nordau_allowed_clock_rates),
};


static const char *const nordau_eva_opp_clk_names[] = {
	"core0",	/* [DT dtsi:32] EVA_CC_MVS0_CLK  — 1st opp-hz value */
	"eva0",		/* [DT dtsi:31] EVA_CC_MVS0C_CLK — 2nd opp-hz value */
	NULL,
};


static const char *const nordau_opp_pd_table[] = { "mxc", "mmcx" };


static const struct cvp_ipclite_mappings cvp_nordau_ipclite_desc = {
    .iova_start = 0xFE500000,
};


static const struct cvp_iommu_context_bank nordau_cvp_context_banks[] = {
    {
        .name = "eva_hlos",             /* [DT dtsi:46] EVA_NON_SECURE SID 0x0920 */
        .buffer_type = 0xfff,           /* [INH] */
        .iova_start = 0x4b000000,       /* [INH] */
        .iova_size  = 0x90000000,       /* [INH] */
        .vmid = 0,                      /* [INH] non-secure */
    },
    {
        .name = "eva_sec_nonpixel",     /* [DT dtsi:47] EVA_SECURE_NONPIXEL SID 0x0924 */
        .buffer_type = 0x741,           /* [INH] */
        .iova_start = 0x01000000,       /* [INH] */
        .iova_size  = 0x25800000,       /* [INH] */
        .vmid = 0xB,                    /* [INH] secure — confirm w/ security team */
    },
    {
        .name = "eva_sec_pixel",        /* [DT dtsi:48] EVA_SECURE_PIXEL SID 0x0923 */
        .buffer_type = 0x106,           /* [INH] */
        .iova_start = 0x26800000,       /* [INH] */
        .iova_size  = 0x24800000,       /* [INH] */
        .vmid = 0xA,                    /* [INH] secure — confirm w/ security team */
    },
};


static struct msm_cvp_platform_data default_data = {
	.common_data = default_common_data,
	.common_data_length =  ARRAY_SIZE(default_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = 0x0,
	.noc_qos = 0x0,
	.vm_id = 1,
	.cvp_hfi = cvp_hfi_defs_v1,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v1,
	.hfi_ver = 1,
	.hal_version = DEFAULT_HAL_VER,
};

static struct msm_cvp_platform_data sm8450_data = {
	.common_data = sm8450_common_data,
	.common_data_length =  ARRAY_SIZE(sm8450_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,
	.noc_qos = &waipio_noc_qos,
	.vm_id = 1,
	.cvp_hfi = cvp_hfi_defs_v1,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v1,
	.hfi_ver = 1,
	.hal_version = DEFAULT_HAL_VER,
};

static struct msm_cvp_platform_data sm8550_data = {
	.common_data = sm8550_common_data,
	.common_data_length =  ARRAY_SIZE(sm8550_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,	/*Reuse Kona setting*/
	.noc_qos = &waipio_noc_qos,	/*Reuse Waipio setting*/
	.vm_id = 1,
	.cvp_hfi = cvp_hfi_defs_v1,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v1,
	.hfi_ver = 1,
	.hal_version = DEFAULT_HAL_VER,
};

static struct msm_cvp_platform_data sm8550_tvm_data = {
	.common_data = sm8550_tvm_common_data,
	.common_data_length =  ARRAY_SIZE(sm8550_tvm_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,	/*Reuse Kona setting*/
	.noc_qos = &waipio_noc_qos,	/*Reuse Waipio setting*/
	.vm_id = 2,
	.cvp_hfi = cvp_hfi_defs_v1,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v1,
	.hfi_ver = 1,
	.hal_version = DEFAULT_HAL_VER,
};

static struct msm_cvp_platform_data sm8650_data = {
	.common_data = sm8650_common_data,
	.common_data_length = ARRAY_SIZE(sm8650_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,	/*Reuse Kona setting*/
	.noc_qos = &waipio_noc_qos,	/*Reuse Waipio setting*/
	.vm_id = 1,
	.cvp_hfi = cvp_hfi_defs_v1,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v1,
	.hfi_ver = 1,
	.hal_version = DEFAULT_HAL_VER,
};

static struct msm_cvp_platform_data sm8750_data = {
	.common_data = sm8750_common_data,
	.common_data_length = ARRAY_SIZE(sm8650_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,	/*Reuse Kona setting*/
	.noc_qos = &pakala_noc_qos,
	.vm_id = 1,
	.cvp_hfi = cvp_hfi_defs_v1,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v1,
	.hfi_ver = 1,
	.hal_version = DEFAULT_HAL_VER,
};

static struct msm_cvp_platform_data sm8735_data = {
	.common_data = sm8735_common_data,
	.common_data_length = ARRAY_SIZE(sm8735_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,	/*Reuse Kona setting*/
	.noc_qos = &pakala_noc_qos,
	.vm_id = 1,
	.cvp_hfi = cvp_hfi_defs_v1,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v1,
	.hfi_ver = 1,
	.hal_version = DEFAULT_HAL_VER,
};

static struct msm_cvp_platform_data sm8850_data = {
	.io_registers = &cvp_kaanapali_io_registers,
	.common_data = sm8850_common_data,
	.common_data_length = ARRAY_SIZE(sm8850_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,	/*Reuse Kona setting*/
	.noc_qos = &pakala_noc_qos,
	.vm_id = 1,
	.cvp_hfi = cvp_hfi_defs_v2,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v2,
	.hfi_ver = 2,
	.hal_version = KNP_HAL_VER,
	.pas_id = 26,
	.gcc_regs = &cvp_kaanapali_gcc_reg,
	.ipcc_regs = &cvp_kaanapali_ipcc_reg,
	.regspace_mappings = &cvp_kaanapali_regspace,
	.reg_presets = &cvp_kaanapali_reg_presets,
	.reset_power_sets = &cvp_kaanapali_reset_power_set_desc,
	.power_domains = &kaanapali_power_domains,
	.clock_props = &kaanapali_clock_props,
	.clock_ids = cvp_kaanapali_clock_ids,
	.num_clock_ids = CVP_KAANAPALI_CLOCK_IDS_COUNT,
	.allowed_clk_rates = &kaanapali_allowed_clocks,
	.opp_clk_tbl = kaanapali_eva_opp_clk_names,
	.opp_pd_tbl = kaanapali_opp_pd_table,
	.opp_pd_tbl_size = ARRAY_SIZE(kaanapali_opp_pd_table),
	.bus_descs = cvp_bus_kaanapali_descs,
	.subcache_desc = &cvp_kaanapali_desc,
	.ipclite_mappings = &cvp_kaanapali_ipclite_desc,
	.cb_data = kaanapali_cvp_context_banks,
	.cb_data_size = ARRAY_SIZE(kaanapali_cvp_context_banks),
};

static struct msm_cvp_platform_data glymur_data = {
	.io_registers = &cvp_glymur_io_registers,
	.common_data = sm8850_common_data,
	.common_data_length = ARRAY_SIZE(sm8850_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,	/*Reuse Kona setting*/
	.noc_qos = &pakala_noc_qos,
	.vm_id = 1,
	.cvp_hfi = cvp_hfi_defs_v2,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v2,
	.hfi_ver = 2,
	.hal_version = GLYMUR_HAL_VER,
	.pas_id = 26,
	.gcc_regs = &cvp_glymur_gcc_reg,
    .ipcc_regs = &cvp_glymur_ipcc_reg,
    .regspace_mappings = &cvp_glymur_regspace,
    .reg_presets = &cvp_glymur_reg_presets,
    .reset_power_sets = &cvp_glymur_reset_power_set_desc,
    .power_domains = &glymur_power_domains,
    .clock_props = &glymur_clock_props,
	.clock_ids = cvp_glymur_clock_ids,
	.num_clock_ids = CVP_GLYMUR_CLOCK_IDS_COUNT,
    .allowed_clk_rates = &glymur_allowed_clocks,
    .opp_clk_tbl = glymur_eva_opp_clk_names,
    .opp_pd_tbl = glymur_opp_pd_table,
    .opp_pd_tbl_size = ARRAY_SIZE(glymur_opp_pd_table),
    .bus_descs = cvp_bus_glymur_descs,
    .subcache_desc = &cvp_glymur_desc,
    .ipclite_mappings = &cvp_glymur_ipclite_desc,
    .cb_data = glymur_cvp_context_banks,
    .cb_data_size = ARRAY_SIZE(glymur_cvp_context_banks),
};



static struct msm_cvp_platform_data sm8845_data = {
	.common_data = sm8845_common_data,
	.common_data_length = ARRAY_SIZE(sm8845_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,	/*Reuse Kona setting*/
	.noc_qos = &pakala_noc_qos,
	.vm_id = 1,
	.cvp_hfi = cvp_hfi_defs_v2,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v2,
	.hfi_ver = 2,
	.hal_version = KNP_HAL_VER,
};

static struct msm_cvp_platform_data sm8975_data = {
	.common_data = sm8975_common_data,
	.common_data_length = ARRAY_SIZE(sm8975_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,	/*Reuse Kona setting*/
	.noc_qos = &pakala_noc_qos,
	.vm_id = 1,
	.cvp_hfi = cvp_hfi_defs_v2,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v2,
	.hfi_ver = 2,
	.hal_version = HAWI_HAL_VER,
};

static struct msm_cvp_platform_data sm8950_data = {
	.common_data = sm8950_common_data,
	.common_data_length = ARRAY_SIZE(sm8950_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,	/*Reuse Kona setting*/
	.noc_qos = &pakala_noc_qos,
	.vm_id = 1,
	.cvp_hfi = cvp_hfi_defs_v2,
	.cvp_hfi_msg = cvp_hfi_msg_defs_v2,
	.hfi_ver = 2,
	.hal_version = HAWI_HAL_VER,
};


static struct msm_cvp_platform_data nordau_data = {
	.io_registers = &cvp_nordau_io_registers,
	.common_data = sa8797p_common_data,			/* [INH] SoC=SA8797P, array cloned from sm8850 — verify w/ power team */
	.common_data_length = ARRAY_SIZE(sa8797p_common_data),
	.sku_version = 0,					/* user-confirmed */
	.vpu_ver = VPU_VERSION_5,				/* [INH] VPU gen unchanged — confirm NORDAU is VPU5 */
	.ubwc_config = kona_ubwc_data,				/* [INH] reuse Kona UBWC — confirm NORDAU UBWC/DDR settings */
	.noc_qos = &nordau_noc_qos,				/* [INH] ⚠️ HIGHEST RISK — see nordau_noc_qos comment */
	.vm_id = 1,						/* [INH] pvm=1 — SoC-specific, confirm SA8797P integration guide */
	.cvp_hfi = cvp_hfi_defs_v2,				/* [INH] HFI protocol v2 unchanged */
	.cvp_hfi_msg = cvp_hfi_msg_defs_v2,			/* [INH] HFI protocol v2 unchanged */
	.hfi_ver = 2,						/* [INH] HFI protocol v2 unchanged */
	.hal_version = NORDAU_HAL_VER,				/* = 5, cvp_comm_def.h:40 (set by build-wiring) */
	.pas_id = 26,						/* [INH] confirm w/ security team */
	.gcc_regs = &cvp_nordau_gcc_reg,			/* [INH] needs SA8797P GCC map */
	.ipcc_regs = &cvp_nordau_ipcc_reg,			/* [INH] needs SA8797P IPCC map */
	.regspace_mappings = &cvp_nordau_regspace,		/* [CSV] aon.phys only; rest [INH] */
	.reg_presets = &cvp_nordau_reg_presets,			/* [CSV] offset verified; value [INH] */
	.reset_power_sets = &cvp_nordau_reset_power_set_desc,	/* [DT] 1 reset; value [INH] */
	.power_domains = &nordau_power_domains,			/* [DT]+[CSV] pd_count=2, idx=2; hw_pc [INH] */
	.clock_props = &nordau_clock_props,			/* [DT]+[CSV] re-indexed for NORDAU order */
	.clock_ids = cvp_nordau_clock_ids,			/* [DT] dtsi:18-24 order */
	.num_clock_ids = CVP_NORDAU_CLOCK_IDS_COUNT,	/* 7 — must equal DT clock count */
	.allowed_clk_rates = &nordau_allowed_clocks,		/* [CSV] eva_cc_mvs0_clk, deduped */
	.opp_clk_tbl = nordau_eva_opp_clk_names,		/* [DT] core0,eva0 per opp-hz pair order */
	.opp_pd_tbl = nordau_opp_pd_table,			/* [DT] mxc,mmcx dtsi:14-15 */
	.opp_pd_tbl_size = ARRAY_SIZE(nordau_opp_pd_table),	/* 2 */
	.bus_descs = cvp_bus_nordau_descs,			/* [DT] order dtsi:38-43; bw [INH] */
	.subcache_desc = &cvp_nordau_desc,			/* [INH] slice list not in QLI DT */
	.ipclite_mappings = &cvp_nordau_ipclite_desc,		/* [INH] needs SMMU VA map */
	.cb_data = nordau_cvp_context_banks,			/* [DT] SIDs/count; iova+vmid [INH] */
	.cb_data_size = ARRAY_SIZE(nordau_cvp_context_banks),	/* 3 — matches dtsi:46-48 */
};

static const struct of_device_id msm_cvp_dt_match[] = {
	{
		.compatible = "qcom,waipio-cvp",
		.data = &sm8450_data,
	},
	{
		.compatible = "qcom,kalama-cvp",
		.data = &sm8550_data,
	},
	{
		.compatible = "qcom,kalama-cvp-tvm",
		.data = &sm8550_tvm_data,
	},
	{
		.compatible = "qcom,pineapple-cvp",
		.data = &sm8650_data,
	},
	{
		.compatible = "qcom,sun-cvp",
		.data = &sm8750_data,
	},
	{
		.compatible = "qcom,tuna-cvp",
		.data = &sm8735_data,
	},
	{
		.compatible = "qcom,canoe-cvp",
		.data = &sm8850_data,
	},
	{
		.compatible = "qcom,kaanapali-eva",
		.data = &sm8850_data,
	},
	{
		.compatible = "qcom,glymur-eva",
		.data = &glymur_data,
	},
	{
		.compatible = "qcom,alor-cvp",
		.data = &sm8845_data,
	},
	{
		.compatible = "qcom,art-cvp",
		.data = &sm8975_data,
	},
	{
		.compatible = "qcom,pebble-cvp",
		.data = &sm8950_data,
	},
    {

		.compatible = "qcom,nordau-eva",
		.data = &nordau_data,
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
#ifdef CONFIG_EVA_SUN
struct msm_cvp_hfi_defs cvp_hfi_defs_v1[MAX_PKT_IDX] = {
	[HFI_CMD_SESSION_CVP_DFS_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_DFS_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DFS_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DFS_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_DFS_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_DFS_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DFS_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DFS_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_SGM_OF_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SGM_OF_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SGM_OF_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_SGM_OF_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SGM_OF_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SGM_OF_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_WARP_NCC_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_WARP_NCC_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_WARP_NCC_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_WARP_NCC_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_WARP_NCC_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_WARP_NCC_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_WARP_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_WARP_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_WARP_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_WARP_DS_PARAMS - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_WARP_DS_PARAMS,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_WARP_DS_PARAMS",
		},
	[HFI_CMD_SESSION_CVP_WARP_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_WARP_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_WARP_FRAME",
			.force_kernel_fence = true,
		},
	[HFI_CMD_SESSION_CVP_DMM_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_DMM_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DMM_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DMM_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_DMM_PARAMS - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_DMM_PARAMS,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DMM_PARAMS",
		},
	[HFI_CMD_SESSION_CVP_DMM_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_DMM_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DMM_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DMM_FRAME",
			.force_kernel_fence = true,
		},
	[HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_PERSIST_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS",
		},
	[HFI_CMD_SESSION_CVP_RELEASE_PERSIST_BUFFERS - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xffffffff,
			.type = HFI_CMD_SESSION_CVP_RELEASE_PERSIST_BUFFERS,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_RELEASE_PERSIST_BUFFERS",
		},
	[HFI_CMD_SESSION_CVP_DS_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_DS_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DS_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DS_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_DS - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_DS_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DS,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DS",
		},
	[HFI_CMD_SESSION_CVP_CV_TME_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_OF_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_CV_TME_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_CV_TME_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_CV_TME_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_OF_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_CV_TME_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_CV_TME_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_CV_ODT_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_ODT_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_CV_ODT_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_CV_ODT_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_CV_ODT_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_ODT_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_CV_ODT_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_CV_ODT_FRAME",
		},
	[HFI_CMD_SESSION_CVP_CV_OD_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_OD_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_CV_OD_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_CV_OD_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_CV_OD_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_OD_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_CV_OD_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_CV_OD_FRAME",
		},
	[HFI_CMD_SESSION_CVP_NCC_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_NCC_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_NCC_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_NCC_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_NCC_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_NCC_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_NCC_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_NCC_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_ICA_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_ICA_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_ICA_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_ICA_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_ICA_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_ICA_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_ICA_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_ICA_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_HCD_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_HCD_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_HCD_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_HCD_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_HCD_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_HCD_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_HCD_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_HCD_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_DC_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_DCM_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DC_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DC_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_DC_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_DCM_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DC_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DC_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_DCM_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_DCM_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DCM_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DCM_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_DCM_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_DCM_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DCM_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DCM_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_PYS_HCD_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_PYS_HCD_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_PYS_HCD_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_PYS_HCD_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_PYS_HCD_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = HFI_PYS_HCD_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_PYS_HCD_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_PYS_HCD_FRAME",
			.force_kernel_fence = true,
		},
	[HFI_CMD_SESSION_CVP_SET_MODEL_BUFFERS - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SET_MODEL_BUFFERS,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SET_MODEL_BUFFERS",
		},
	[HFI_CMD_SESSION_CVP_SET_SNAPSHOT_BUFFERS - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SET_SNAPSHOT_BUFFERS,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SET_SNAPSHOT_BUFFERS",
		},
	[HFI_CMD_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS",
		},
	[HFI_CMD_SESSION_CVP_SET_SNAPSHOT_MODE - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SET_SNAPSHOT_MODE,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SET_SNAPSHOT_MODE",
		},
	[HFI_CMD_SESSION_CVP_SNAPSHOT_WRITE_DONE - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SNAPSHOT_WRITE_DONE,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SNAPSHOT_WRITE_DONE",
		},
	[HFI_CMD_SESSION_CVP_FD_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_FD_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_FD_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_FD_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_FD_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_FD_FRAME",
		},
	[HFI_CMD_SESSION_CVP_XRA_BLOB_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_XRA_BLOB_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_XRA_BLOB_FRAME",
		},
	[HFI_CMD_SESSION_CVP_XRA_BLOB_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_XRA_BLOB_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_XRA_BLOB_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_XRA_MATCH_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_XRA_MATCH_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_XRA_MATCH_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_XRA_MATCH_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_XRA_MATCH_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_XRA_MATCH_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_RGE_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_RGE_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_RGE_FRAME",
			.force_kernel_fence = true,
		},
	[HFI_CMD_SESSION_CVP_RGE_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_RGE_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_RGE_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_ITOF_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_ITOF_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_ITOF_FRAME",
			.force_kernel_fence = true,
		},
	[HFI_CMD_SESSION_EVA_ITOF_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_ITOF_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_ITOF_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_SCALER_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_SCALER_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_SCALER_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_SCALER_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_SCALER_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_SCALER_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_DESCRIPTOR_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DESCRIPTOR_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DESCRIPTOR_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_DESCRIPTOR_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DESCRIPTOR_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DESCRIPTOR_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_DLFD_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DLFD_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DLFD_FRAME",
		},
	[HFI_CMD_SESSION_EVA_DLFD_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DLFD_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DLFD_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_DLFL_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DLFL_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DLFL_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_DLFL_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DLFL_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DLFL_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_SYNX - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SYNX,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SYNX",
		},
	[HFI_CMD_SESSION_EVA_DME_ONLY_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DME_ONLY_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DME_ONLY_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_DME_ONLY_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DME_ONLY_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DME_ONLY_FRAME",
			.force_kernel_fence = true,
		},
	[HFI_CMD_SESSION_CVP_GME_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_GME_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_GME_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_GME_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_GME_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_GME_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_LME_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_LME_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_LME_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_LME_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_LME_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_LME_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_SPSTAT_CONFIG - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_SPSTAT_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_SPSTAT_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_SPSTAT_FRAME - HFI_CMD_SESSION_CVP_START] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_SPSTAT_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_SPSTAT_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_DFS_FRAME - HFI_CMD_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_EVA_DFS_FRAME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_CMD_SESSION_EVA_DFS_FRAME",
		.force_kernel_fence = false,
	},
	[HFI_CMD_SESSION_EVA_DFS_CONFIG - HFI_CMD_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_EVA_DFS_CONFIG,
		.is_config_pkt = true,
		.resp = HAL_NO_RESP,
		.name = "HFI_CMD_SESSION_EVA_DFS_CONFIG",
		.force_kernel_fence = false,
	},
	[HFI_CMD_SESSION_EVA_LME_CONFIG - HFI_CMD_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_EVA_LME_CONFIG,
		.is_config_pkt = true,
		.resp = HAL_NO_RESP,
		.name = "HFI_CMD_SESSION_EVA_LME_CONFIG",
		.force_kernel_fence = false,
	},
	[HFI_CMD_SESSION_EVA_LME_FRAME - HFI_CMD_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_EVA_LME_FRAME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_CMD_SESSION_EVA_LME_FRAME",
		.force_kernel_fence = false,
	},
	[HFI_CMD_SESSION_EVA_MATCH_CONFIG - HFI_CMD_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_EVA_MATCH_CONFIG,
		.is_config_pkt = true,
		.resp = HAL_NO_RESP,
		.name = "HFI_CMD_SESSION_EVA_MATCH_CONFIG",
		.force_kernel_fence = false,
	},
	[HFI_CMD_SESSION_EVA_MATCH_FRAME - HFI_CMD_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_EVA_MATCH_FRAME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_CMD_SESSION_EVA_MATCH_FRAME",
		.force_kernel_fence = false,
	},
	[HFI_CMD_SESSION_CVP_FPX_CONFIG - HFI_CMD_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_CVP_FPX_CONFIG,
		.is_config_pkt = true,
		.resp = HAL_NO_RESP,
		.name = "HFI_CMD_SESSION_CVP_FPX_CONFIG",
		.force_kernel_fence = false,
	},
	[HFI_CMD_SESSION_CVP_FPX_FRAME - HFI_CMD_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_CVP_FPX_FRAME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_CMD_SESSION_CVP_FPX_FRAME",
		.force_kernel_fence = false,
	},
	[HFI_CMD_SESSION_EVA_GME_CONFIG - HFI_CMD_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_EVA_GME_CONFIG,
		.is_config_pkt = true,
		.resp = HAL_NO_RESP,
		.name = "HFI_CMD_SESSION_EVA_GME_CONFIG",
		.force_kernel_fence = false,
	},
	[HFI_CMD_SESSION_EVA_GME_FRAME - HFI_CMD_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_CMD_SESSION_EVA_GME_FRAME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_CMD_SESSION_EVA_GME_FRAME",
		.force_kernel_fence = false,
	}
};

/*
 * Below are for msg packet
 *  "the packet type - HFI_MSG_SESSION_CVP_START"
 */

struct msm_cvp_hfi_defs cvp_hfi_msg_defs_v1[MAX_PKT_IDX] = {
	[HFI_MSG_SESSION_CVP_FPX - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_FPX,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_FPX",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_SET_BUFFERS - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SET_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SET_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_RELEASE_BUFFERS  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_RELEASE_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_RELEASE_BUFFERS ",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_DS - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_DS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_DS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_CV_HOG - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_CV_HOG,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_CV_HOG",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_DFS - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_DFS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_DFS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_SVM - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SVM,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SVM",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_NCC - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_NCC,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_NCC",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_TME - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_TME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_TME",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_SPSTAT - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_SPSTAT,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_SPSTAT",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_ICA - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_ICA,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_ICA",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_DME - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_DME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_DME",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_DME_ONLY - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_DME_ONLY,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_DME_ONLY",
		.force_kernel_fence = true,
	},
	[HFI_MSG_SESSION_CVP_OPERATION_CONFIG  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_OPERATION_CONFIG,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_OPERATION_CONFIG",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_SET_PERSIST_BUFFERS  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SET_PERSIST_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SET_PERSIST_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_SET_MODEL_BUFFERS  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SET_MODEL_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SET_MODEL_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_RELEASE_PERSIST_BUFFERS  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_RELEASE_PERSIST_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_RELEASE_PERSIST_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_RELEASE_MODEL_BUFFERS  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_RELEASE_MODEL_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_RELEASE_MODEL_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_SGM_OF  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SGM_OF,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SGM_OF",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_GCE  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_GCE,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_OPERATION_CONFIG",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_WARP_NCC  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_WARP_NCC,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_WARP_NCC",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_DMM  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_DMM,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_DMM",
		.force_kernel_fence = true,
	},
	[HFI_MSG_SESSION_CVP_SGM_DFS  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SGM_DFS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SGM_DFS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_WARP  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_WARP,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_WARP",
		.force_kernel_fence = true,
	},
	[HFI_MSG_SESSION_CVP_DMM_PARAMS  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_DMM_PARAMS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_DMM_PARAMS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_WARP_DS_PARAMS  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_WARP_DS_PARAMS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_WARP_DS_PARAMS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_SET_SNAPSHOT_BUFFERS  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SET_SNAPSHOT_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SET_SNAPSHOT_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_EVENT_NOTIFY_SNAPSHOT_READY  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_EVENT_NOTIFY_SNAPSHOT_READY,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_EVENT_NOTIFY_SNAPSHOT_READY",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_WARP  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_WARP,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_WARP",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_LME   - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_LME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_LME",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_FLUSH  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_FLUSH,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_FLUSH",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_START  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_START,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_START",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_STOP  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_STOP,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_STOP",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_PYS_HCD  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_PYS_HCD,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_PYS_HCD",
		.force_kernel_fence = true,
	},
	[HFI_MSG_SESSION_EVA_DESCRIPTOR  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_DESCRIPTOR,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_DESCRIPTOR",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_MATCH  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_MATCH,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_MATCH",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_GME  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_GME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_GME",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_SCALER  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_SCALER,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_SCALER",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_FPX  - HFI_MSG_SESSION_CVP_START] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_FPX,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_FPX",
		.force_kernel_fence = false,
	}
};
#else
struct msm_cvp_hfi_defs cvp_hfi_defs_v2[MAX_PKT_IDX] = {
	[HFI_CMD_SESSION_EVA_DFS_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = HFI_DFS_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_EVA_DFS_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DFS_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_DFS_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = HFI_DFS_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_EVA_DFS_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DFS_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_SGM_OF_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SGM_OF_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SGM_OF_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_SGM_OF_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SGM_OF_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SGM_OF_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_WARP_NCC_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_WARP_NCC_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_WARP_NCC_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_WARP_NCC_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_WARP_NCC_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_WARP_NCC_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_WARP_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_WARP_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_WARP_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_WARP_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_WARP_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_WARP_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_DMM_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = HFI_DMM_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DMM_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DMM_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_DMM_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = HFI_DMM_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DMM_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DMM_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS - HFI_CMD_SESSION_EVA_CTRL_OFFSET
								+ CTRL_OFFSET] = {
			.size = HFI_PERSIST_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS",
		},
	[HFI_CMD_SESSION_CVP_RELEASE_PERSIST_BUFFERS - HFI_CMD_SESSION_EVA_CTRL_OFFSET
								+ CTRL_OFFSET] = {
			.size = 0xffffffff,
			.type = HFI_CMD_SESSION_CVP_RELEASE_PERSIST_BUFFERS,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_RELEASE_PERSIST_BUFFERS",
		},
	[HFI_CMD_SESSION_CVP_NCC_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = HFI_NCC_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_NCC_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_NCC_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_NCC_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = HFI_NCC_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_NCC_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_NCC_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_DC_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = HFI_DCM_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DC_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DC_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_DC_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = HFI_DCM_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DC_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DC_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_DCM_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = HFI_DCM_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DCM_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DCM_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_DCM_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = HFI_DCM_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_DCM_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_DCM_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_PYS_HCD_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = HFI_PYS_HCD_CONFIG_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_PYS_HCD_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_PYS_HCD_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_PYS_HCD_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = HFI_PYS_HCD_FRAME_CMD_SIZE,
			.type = HFI_CMD_SESSION_CVP_PYS_HCD_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_PYS_HCD_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_SET_SNAPSHOT_BUFFERS - HFI_CMD_SESSION_EVA_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SET_SNAPSHOT_BUFFERS,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SET_SNAPSHOT_BUFFERS",
		},
	[HFI_CMD_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS - HFI_CMD_SESSION_EVA_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS",
		},
	[HFI_CMD_SESSION_CVP_SET_SNAPSHOT_MODE - HFI_CMD_SESSION_EVA_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SET_SNAPSHOT_MODE,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SET_SNAPSHOT_MODE",
		},
	[HFI_CMD_SESSION_CVP_SNAPSHOT_WRITE_DONE - HFI_CMD_SESSION_EVA_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SNAPSHOT_WRITE_DONE,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SNAPSHOT_WRITE_DONE",
		},
	[HFI_CMD_SESSION_EVA_BLOB_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_BLOB_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "EVA_BLOB_FRAME",
		},
	[HFI_CMD_SESSION_EVA_BLOB_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_BLOB_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "EVA_BLOB_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_MATCH_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_MATCH_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "EVA_MATCH_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_MATCH_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_MATCH_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "EVA_MATCH_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_RGE_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_RGE_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_RGE_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_RGE_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_RGE_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_RGE_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_ITOF_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_ITOF_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_ITOF_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_ITOF_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_ITOF_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_ITOF_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_SCALER_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_SCALER_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_SCALER_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_SCALER_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_SCALER_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_SCALER_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_DESCRIPTOR_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DESCRIPTOR_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DESCRIPTOR_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_DESCRIPTOR_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET
								+ CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DESCRIPTOR_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DESCRIPTOR_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_DLFD_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DLFD_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DLFD_FRAME",
		},
	[HFI_CMD_SESSION_EVA_DLFD_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DLFD_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DLFD_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_DLFL_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DLFL_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DLFL_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_DLFL_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DLFL_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DLFL_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_SYNX - HFI_CMD_SESSION_EVA_CTRL_OFFSET + CTRL_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_SYNX,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_SYNX",
		},
	[HFI_CMD_SESSION_EVA_DME_ONLY_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DME_ONLY_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DME_ONLY_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_DME_ONLY_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_DME_ONLY_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_DME_ONLY_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_GME_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_GME_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_GME_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_GME_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_GME_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_GME_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_LME_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_LME_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_LME_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_LME_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_LME_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_CVP_LME_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_EVA_SPSTAT_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_SPSTAT_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_SPSTAT_CONFIG",
		},
	[HFI_CMD_SESSION_EVA_SPSTAT_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_EVA_SPSTAT_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "HFI_CMD_SESSION_EVA_SPSTAT_FRAME",
			.force_kernel_fence = false,
		},
	[HFI_CMD_SESSION_CVP_FPX_CONFIG - HFI_CMD_SESSION_CONFIG_OFFSET + CONFIG_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_FPX_CONFIG,
			.is_config_pkt = true,
			.resp = HAL_NO_RESP,
			.name = "FPX_CONFIG",
		},
	[HFI_CMD_SESSION_CVP_FPX_FRAME - HFI_CMD_SESSION_FRAME_OFFSET + FRAME_OFFSET] = {
			.size = 0xFFFFFFFF,
			.type = HFI_CMD_SESSION_CVP_FPX_FRAME,
			.is_config_pkt = false,
			.resp = HAL_NO_RESP,
			.name = "FPX_FRAME",
			.force_kernel_fence = false,
		},

};

/*
 * Below are for msg packet
 */
struct msm_cvp_hfi_defs cvp_hfi_msg_defs_v2[MAX_PKT_IDX] = {
	[HFI_MSG_SESSION_CVP_FPX - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_FPX,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_FPX",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_SET_BUFFERS - HFI_MSG_SESSION_EVA_CTRL_OFFSET
					+ MSG_SESSION_EVA_CTRL_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SET_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SET_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_RELEASE_BUFFERS  - HFI_MSG_SESSION_EVA_CTRL_OFFSET
					+ MSG_SESSION_EVA_CTRL_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_RELEASE_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_RELEASE_BUFFERS ",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_DFS - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_DFS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_DFS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_NCC - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_NCC,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_NCC",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_SPSTAT - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_SPSTAT,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_SPSTAT",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_DME - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_DME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_DME",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_DME_ONLY - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_DME_ONLY,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_DME_ONLY",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_OPERATION_CONFIG  - HFI_MSG_SESSION_EVA_OFFSET] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_OPERATION_CONFIG,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_OPERATION_CONFIG",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_SET_PERSIST_BUFFERS  - HFI_MSG_SESSION_EVA_CTRL_OFFSET
							+ MSG_SESSION_EVA_CTRL_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SET_PERSIST_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SET_PERSIST_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_RELEASE_PERSIST_BUFFERS  - HFI_MSG_SESSION_EVA_CTRL_OFFSET
							+ MSG_SESSION_EVA_CTRL_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_RELEASE_PERSIST_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_RELEASE_PERSIST_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_SGM_OF  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SGM_OF,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SGM_OF",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_GCE  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_GCE,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_OPERATION_CONFIG",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_WARP_NCC  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_WARP_NCC,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_WARP_NCC",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_DMM  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_DMM,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_DMM",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_SGM_DFS  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SGM_DFS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SGM_DFS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_SET_SNAPSHOT_BUFFERS  - HFI_MSG_SESSION_EVA_OFFSET] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_SET_SNAPSHOT_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_SET_SNAPSHOT_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS  - HFI_MSG_SESSION_EVA_OFFSET] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_RELEASE_SNAPSHOT_BUFFERS",
		.force_kernel_fence = false,
	},
	[HFI_MSG_EVENT_NOTIFY_SNAPSHOT_READY  - HFI_MSG_SESSION_EVA_OFFSET] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_EVENT_NOTIFY_SNAPSHOT_READY,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_EVENT_NOTIFY_SNAPSHOT_READY",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_WARP  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_WARP,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_WARP",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_LME   - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_LME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_LME",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_FLUSH  - HFI_MSG_SESSION_EVA_CTRL_OFFSET
					+ MSG_SESSION_EVA_CTRL_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_FLUSH,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_FLUSH",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_START_DONE  - HFI_MSG_SESSION_EVA_CTRL_OFFSET
					+ MSG_SESSION_EVA_CTRL_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_START_DONE,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_START_DONE",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_STOP_DONE  - HFI_MSG_SESSION_EVA_CTRL_OFFSET
					+ MSG_SESSION_EVA_CTRL_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_STOP_DONE,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_STOP_DONE",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_PYS_HCD  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_PYS_HCD,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_PYS_HCD",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_DESCRIPTOR  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_DESCRIPTOR,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_DESCRIPTOR",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_MATCH  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_MATCH,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_MATCH",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_GME  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_GME,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_GME",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_EVA_SCALER  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_EVA_SCALER,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_EVA_SCALER",
		.force_kernel_fence = false,
	},
	[HFI_MSG_SESSION_CVP_FPX  - HFI_MSG_SESSION_OFFSET + MSG_SESSION_INDEX] = {
		.size = 0xFFFFFFFF,
		.type = HFI_MSG_SESSION_CVP_FPX,
		.is_config_pkt = false,
		.resp = HAL_NO_RESP,
		.name = "HFI_MSG_SESSION_CVP_FPX",
		.force_kernel_fence = false,
	}
};
#endif

int get_pkt_index(struct cvp_hal_session_cmd_pkt *hdr)
{
	struct msm_cvp_platform_data *pdata = cvp_driver->cvp_core->platform_data;
	uint32_t hfi_ver = pdata->hfi_ver;

	if (hfi_ver == 1) {
		if (!hdr || (hdr->packet_type < HFI_CMD_SESSION_CVP_START)
			|| hdr->packet_type >= (HFI_CMD_SESSION_CVP_START + MAX_PKT_IDX))
			return -EINVAL;

		if (cvp_hfi_defs[hdr->packet_type - HFI_CMD_SESSION_CVP_START].size)
			return (hdr->packet_type - HFI_CMD_SESSION_CVP_START);

		return -EINVAL;
	} else {
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
}

int get_pkt_fenceoverride(struct cvp_hal_session_cmd_pkt *hdr)
{
	struct msm_cvp_platform_data *pdata = cvp_driver->cvp_core->platform_data;
	uint32_t hfi_ver = pdata->hfi_ver;

	if (hfi_ver == 1)
		return cvp_hfi_defs[hdr->packet_type -
			HFI_CMD_SESSION_CVP_START].force_kernel_fence;
	else {
		int pkt_idx;
		u32 thirteenth_bit;
		u32 fourteenth_bit;

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


		return cvp_hfi_defs[pkt_idx].force_kernel_fence;
	}
}

int get_pkt_index_from_type(u32 pkt_type)
{
	struct msm_cvp_platform_data *pdata = cvp_driver->cvp_core->platform_data;
	uint32_t hfi_ver = pdata->hfi_ver;

	if (hfi_ver == 1) {
		if ((pkt_type < HFI_CMD_SESSION_CVP_START) ||
			pkt_type >= (HFI_CMD_SESSION_CVP_START + MAX_PKT_IDX))
			return -EINVAL;

		if (cvp_hfi_defs[pkt_type - HFI_CMD_SESSION_CVP_START].size)
			return (pkt_type - HFI_CMD_SESSION_CVP_START);
	} else {
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
	}

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
	struct msm_cvp_platform_data *pdata = cvp_driver->cvp_core->platform_data;
	uint32_t hfi_ver = pdata->hfi_ver;

	if (hfi_ver == 1) {
		if (pkt_type > HFI_CMD_SESSION_CVP_START &&
			pkt_type <= (HFI_CMD_SESSION_CVP_START + MAX_PKT_IDX))
			return cvp_hfi_defs[pkt_type - HFI_CMD_SESSION_CVP_START].name;
		else if (pkt_type > HFI_MSG_SESSION_CVP_START &&
			pkt_type <= (HFI_MSG_SESSION_CVP_START + MAX_PKT_IDX))
			return cvp_hfi_msg_defs[pkt_type - HFI_MSG_SESSION_CVP_START].name;
		else
			return "";
	} else {
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
}

const char *get_feature_name_from_type(u32 pkt_type)
{
	switch (pkt_type) {
	case HFI_CV_KERNEL_FPX:
		return "FPX";
	case HFI_CV_KERNEL_WARP:
		return "WARP";
	case HFI_CV_KERNEL_DESCRIPTOR:
		return "DESCRIPTOR";
	case HFI_CV_KERNEL_NCC:
		return "NCC";
	case HFI_CV_KERNEL_DFS:
		return "DFS";
	case HFI_CV_KERNEL_WARP_NCC:
		return "WARP NCC";
	case HFI_CV_KERNEL_ORB:
		return "ORB";
	case HFI_CV_KERNEL_PYS_HCD:
		return "Pyramid HCD";
	case HFI_CV_KERNEL_ICA:
		return "ICA";
	case HFI_CV_KERNEL_GCX:
		return "GSX";
	case HFI_CV_KERNEL_XRA:
		return "XRA";
	case HFI_CV_KERNEL_CSC:
		return "CSC";
	case HFI_CV_KERNEL_LSR:
		return "LSR";
	case HFI_CV_KERNEL_ITOF:
		return "ITOF";
	case HFI_CV_KERNEL_RGE:
		return "RGE";
	case HFI_CV_KERNEL_LME:
		return "LME";
	case HFI_CV_KERNEL_SPSTAT:
		return "Spatial Stats";
	case HFI_CV_KERNEL_GME:
		return "GME";
	case HFI_CV_KERNEL_SCALER:
		return "SCALER";
	case HFI_CV_KERNEL_MATCH:
		return "MATCH";
	case HFI_CV_KERNEL_BLOB:
		return "BLOB";
	default:
		return " ";
	}
}

MODULE_DEVICE_TABLE(of, msm_cvp_dt_match);

int cvp_of_fdt_get_ddrtype(void)
{
#if defined(FIXED_DDR_TYPE) || defined(CVP_FIX_DDR_TYPE)
	/* of_fdt_get_ddrtype() is usually unavailable during pre-sil */
	return DDR_TYPE_LPDDR5;
#else
	// return of_fdt_get_ddrtype();
	return DDR_TYPE_LPDDR5;
#endif
}

void *cvp_get_drv_data(struct device *dev)
{
	struct msm_cvp_platform_data *driver_data;
	const struct of_device_id *match;
	uint32_t ddr_type = DDR_TYPE_LPDDR5;

	driver_data = &default_data;

	if (!IS_ENABLED(CONFIG_OF) || !dev->of_node)
		goto exit;

	match = of_match_node(msm_cvp_dt_match, dev->of_node);

	if (!match)
		return NULL;

	driver_data = (struct msm_cvp_platform_data *)match->data;

	if (!strcmp(match->compatible, "qcom,waipio-cvp")) {
		ddr_type = cvp_of_fdt_get_ddrtype();
		if (ddr_type == -ENOENT) {
			dprintk(CVP_ERR,
				"Failed to get ddr type, use LPDDR5\n");
		}

		if (driver_data->ubwc_config &&
			(ddr_type == DDR_TYPE_LPDDR4 ||
			ddr_type == DDR_TYPE_LPDDR4X))
			driver_data->ubwc_config->highest_bank_bit = 15;
		dprintk(CVP_CORE, "DDR Type 0x%x hbb 0x%x\n",
			ddr_type, driver_data->ubwc_config ?
			driver_data->ubwc_config->highest_bank_bit : -1);
	}
exit:
	return driver_data;
}
