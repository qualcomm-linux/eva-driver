/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "cvp_comm_def.h"

/*
 * Common HFI code must not select a target register map at compile time.
 * The DT match provides one immutable table through platform data instead.
 * Target IO macro headers remain private to their HAL implementation files.
 */
struct cvp_hfi_io_registers {
	u32 ctrl_init;
	u32 ctrl_status;
	u32 ctrl_init_status_mask;
	u32 ctrl_error_status_mask;
	u32 ctrl_status_pc_ready;
	u32 cpu_cs_h2xsoftinten;
	u32 cpu_cs_x2rpmh;
	u32 cpu_cs_h2asoftint;
	u32 cpu_cs_a2hsoftint;
	u32 cpu_cs_a2hsoftintclr;
	u32 cpu_cs_sciacmdarg0;
	u32 cpu_cs_scibcmdarg3;
	u32 wrapper_hw_version;
	u32 wrapper_cpu_status;
	u32 wrapper_intr_status;
	u32 wrapper_intr_mask;
	u32 wrapper_intr_mask_a2hcpu;
	u32 wrapper_intr_mask_cpu_noc;
	u32 wrapper_intr_mask_core_noc;
	u32 wrapper_intr_status_a2hwd;
	u32 wrapper_cpu_clock_config;
	u32 wrapper_core_clock_config;
	u32 wrapper_tz_cpu_clock_config;
	u32 wrapper_spare_0;
	u32 wrapper_debug_bridge_lpi_control;
	u32 fatal_intr_mask;
	u32 noc_main_sidebandmanager_faultinen0_low;
	u32 noc_sbm_faultinen0_low;
	u32 noc_err_mainctl_low;
	u32 cc_spare1;
	u32 cc_mvs0c_gdscr;
	u32 cc_mvs0c_cbcr;
	u32 cc_mvs0_gdscr;
	u32 cc_mvs0_cbcr;
	u32 cc_ahb_cbcr;
	u32 cc_sleep_cbcr;
	u32 cc_mvs0_freerun_cbcr;
	u32 cc_mvs0c_freerun_cbcr;
	u32 gcc_eva_axi0_cbcr;
	u32 gcc_eva_axi0c_cbcr;
	u32 gcc_eva_ahb_cbcr;
	u32 gcc_eva_xo_cbcr;
	u32 mmap_addr;
	u32 qtbl_addr;
	u32 qtbl_info;
	u32 sfr_addr;
	u32 uc_region_addr;
	u32 uc_region_size;
	u32 version_info;
	u32 video_b_noc_a_qosgen_mainctl_low;
	u32 video_b_noc_b_qosgen_mainctl_low;
	u32 video_b_noc_c_qosgen_mainctl_low;
	u32 tcsr_soc_hw_version;
	u32 tcsr_reg_base;
	u32 tcsr_reg_size;
};

/* Expand only in cvp_{glymur,kaanapali,nordau}_hal.c after its target IO include. */
#define CVP_HFI_IO_REGISTERS_INIT \
	.ctrl_init = CVP_CTRL_INIT, \
	.ctrl_status = CVP_CTRL_STATUS, \
	.ctrl_init_status_mask = CVP_CTRL_INIT_STATUS__M, \
	.ctrl_error_status_mask = CVP_CTRL_ERROR_STATUS__M, \
	.ctrl_status_pc_ready = CVP_CTRL_STATUS_PC_READY, \
	.cpu_cs_h2xsoftinten = CVP_CPU_CS_H2XSOFTINTEN, \
	.cpu_cs_x2rpmh = CVP_CPU_CS_X2RPMh, \
	.cpu_cs_h2asoftint = CVP_CPU_CS_H2ASOFTINT, \
	.cpu_cs_a2hsoftint = CVP_CPU_CS_A2HSOFTINT, \
	.cpu_cs_a2hsoftintclr = CVP_CPU_CS_A2HSOFTINTCLR, \
	.cpu_cs_sciacmdarg0 = CVP_CPU_CS_SCIACMDARG0, \
	.cpu_cs_scibcmdarg3 = CVP_CPU_CS_SCIBCMDARG3, \
	.wrapper_hw_version = CVP_WRAPPER_HW_VERSION, \
	.wrapper_cpu_status = CVP_WRAPPER_CPU_STATUS, \
	.wrapper_intr_status = CVP_WRAPPER_INTR_STATUS, \
	.wrapper_intr_mask = CVP_WRAPPER_INTR_MASK, \
	.wrapper_intr_mask_a2hcpu = CVP_WRAPPER_INTR_MASK_A2HCPU_BMSK, \
	.wrapper_intr_mask_cpu_noc = CVP_WRAPPER_INTR_MASK_CPU_NOC_BMSK, \
	.wrapper_intr_mask_core_noc = CVP_WRAPPER_INTR_MASK_CORE_NOC_BMSK, \
	.wrapper_intr_status_a2hwd = CVP_WRAPPER_INTR_STATUS_A2HWD_BMSK, \
	.wrapper_cpu_clock_config = CVP_WRAPPER_CPU_CLOCK_CONFIG, \
	.wrapper_core_clock_config = CVP_WRAPPER_CORE_CLOCK_CONFIG, \
	.wrapper_tz_cpu_clock_config = CVP_WRAPPER_TZ_CPU_CLOCK_CONFIG, \
	.wrapper_spare_0 = CVP_WRAPPER_SPARE_0, \
	.wrapper_debug_bridge_lpi_control = CVP_WRAPPER_DEBUG_BRIDGE_LPI_CONTROL, \
	.fatal_intr_mask = CVP_FATAL_INTR_BMSK, \
	.noc_main_sidebandmanager_faultinen0_low = CVP_NOC_MAIN_SIDEBANDMANAGER_FAULTINEN0_LOW, \
	.noc_sbm_faultinen0_low = CVP_NOC_SBM_FAULTINEN0_LOW, \
	.noc_err_mainctl_low = CVP_NOC_ERR_MAINCTL_LOW_OFFS, \
	.cc_spare1 = CVP_CC_SPARE1, \
	.cc_mvs0c_gdscr = CVP_CC_MVS0C_GDSCR, \
	.cc_mvs0c_cbcr = CVP_CC_MVS0C_CBCR, \
	.cc_mvs0_gdscr = CVP_CC_MVS0_GDSCR, \
	.cc_mvs0_cbcr = CVP_CC_MVS0_CBCR, \
	.cc_ahb_cbcr = CVP_CC_AHB_CBCR, \
	.cc_sleep_cbcr = CVP_CC_SLEEP_CBCR, \
	.cc_mvs0_freerun_cbcr = CVP_CC_MVS0_FREERUN_CBCR, \
	.cc_mvs0c_freerun_cbcr = CVP_CC_MVS0C_FREERUN_CBCR, \
	.gcc_eva_axi0_cbcr = CVP_GCC_EVA_AXI0_CBCR, \
	.gcc_eva_axi0c_cbcr = CVP_GCC_EVA_AXI0C_CBCR, \
	.gcc_eva_ahb_cbcr = CVP_GCC_EVA_AHB_CBCR, \
	.gcc_eva_xo_cbcr = CVP_GCC_EVA_XO_CBCR, \
	.mmap_addr = CVP_MMAP_ADDR, \
	.qtbl_addr = CVP_QTBL_ADDR, \
	.qtbl_info = CVP_QTBL_INFO, \
	.sfr_addr = CVP_SFR_ADDR, \
	.uc_region_addr = CVP_UC_REGION_ADDR, \
	.uc_region_size = CVP_UC_REGION_SIZE, \
	.version_info = CVP_VERSION_INFO, \
	.video_b_noc_a_qosgen_mainctl_low = CVP_VIDEO_B_NOC_A_QOSGEN_MAINCTL_LOW, \
	.video_b_noc_b_qosgen_mainctl_low = CVP_VIDEO_B_NOC_B_QOSGEN_MAINCTL_LOW, \
	.video_b_noc_c_qosgen_mainctl_low = CVP_VIDEO_B_NOC_C_QOSGEN_MAINCTL_LOW, \
	.tcsr_soc_hw_version = TCSR_SOW_HW_VERSION, \
	.tcsr_reg_base = TCSR_REG_BASE, \
	.tcsr_reg_size = TCSR_REG_SIZE

extern const struct cvp_hfi_io_registers cvp_glymur_io_registers;
extern const struct cvp_hfi_io_registers cvp_kaanapali_io_registers;
extern const struct cvp_hfi_io_registers cvp_nordau_io_registers;
