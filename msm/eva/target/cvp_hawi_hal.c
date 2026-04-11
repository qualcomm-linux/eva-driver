// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */


#include "cvp_hawi_hal.h"

extern struct cvp_hal_ops hal_ops;

static int iris_pm_qos_aggregate_hawi(struct iris_hfi_device *device)
{
	struct iris_hfi_device *dev = NULL;
	struct msm_cvp_core *core = NULL;
	struct msm_cvp_inst *inst = NULL;
	struct cvp_session_queue *sq = NULL;
	u32 min_pm_qos_latency = PM_QOS_RESUME_LATENCY_DEFAULT_VALUE;

	if (!device) {
		dprintk(CVP_ERR, "%s Invalid device\n", __func__);
		return -ENODEV;
	}

	dev = device;
	core = cvp_driver->cvp_core;
	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		sq = &inst->session_queue;
		spin_lock(&sq->lock);
		/* Consider the latency for aggregation only if session is in start state */
		if (sq->state == QUEUE_START)
			min_pm_qos_latency = min_pm_qos_latency < inst->pm_qos_latency
				? min_pm_qos_latency : inst->pm_qos_latency;
		spin_unlock(&sq->lock);
	}
	mutex_unlock(&core->lock);

	if (min_pm_qos_latency != dev->global_pm_qos_latency_us) {
		mutex_lock(&dev->lock);
		dprintk(CVP_PWR, "%s New aggregated minmum latency %d\n",
				__func__, min_pm_qos_latency);
		/* Put a threshold on user latency so that user can only use the latency
		 * to acheive power saving. Malicius user must not be allowed to keep the
		 * apps core away from LPM.
		 */
		if (min_pm_qos_latency > core->resources.pm_qos.latency_us) {
			dev->global_pm_qos_latency_us = min_pm_qos_latency;
			cvp_pm_qos_update(dev, true);
		}
		mutex_unlock(&dev->lock);
	}

	return 0;
}

/*
 * Based on fal10_veto, X2RPMh, core_pwr_on and PWAitMode value, infer
 * value of xtss_sw_reset. xtss_sw_reset is a TZ register bit. Driver
 * cannot access it directly.
 *
 * In __boot_firmware() function, the caller of this function. It checks
 * "core_pwr_on" == false, basically core powered off. So this function
 * doesn't check core_pwr_on. Assume core_pwr_on = false.
 *
 * fal10_veto = VPU_CPU_CS_X2RPMh[2] |
 *		( ~VPU_CPU_CS_X2RPMh[1] & core_pwr_on ) |
 *		( ~VPU_CPU_CS_X2RPMh[0] & ~( xtss_sw_reset | PWaitMode ) ) ;
 */

static void __check_tensilica_in_reset_hawi(struct iris_hfi_device *device)
{
	u32 xtss_reset_ro = 1;

	dprintk(CVP_WARN, "tensilica xtss_reset_ro %#x\n", xtss_reset_ro);
}

static void __enter_cpu_noc_lpi(struct iris_hfi_device *device,
				enum enter_noc_lpi_caller caller)
{
	u32 lpi_status, count = 0, max_count = 2000;

	/* HPG Section 3.7 Step 5 */
	__write_register(device, CVP_WRAPPER_CPU_NOC_LPI_CONTROL, 0x1);

	/* HPG Section 3.7 Step 6 */
	while (count < max_count) {
		lpi_status =
		    __read_register(device, CVP_WRAPPER_CPU_NOC_LPI_STATUS);
		if (((lpi_status & BIT(1)) || (lpi_status & BIT(2))) &&
			(!(lpi_status & BIT(0)))) {
			__write_register(device,
					 CVP_WRAPPER_CPU_NOC_LPI_CONTROL, 0x0);
			usleep_range(10, 20);
			__write_register(device,
					 CVP_WRAPPER_CPU_NOC_LPI_CONTROL, 0x1);
			usleep_range(1000, 1200);
			count++;
		} else {
			break;
		}
	}

	dprintk(CVP_PWR, "%s, CPU Noc: lpi_status %x (count %d)\n", __func__,
		lpi_status, count);

	/* HPG section 3.7 Step 7 */
	count = 0;
	while (count < max_count) {
		lpi_status = __read_register(device,
					CVP_WRAPPER_CPU_NOC_LPI_STATUS);
		if (lpi_status & 0x1)
			break;
		usleep_range(50, 100);
		count++;
	}

	/* HPG section 3.7 Step 8 */
	 __write_register(device, CVP_WRAPPER_CPU_NOC_LPI_CONTROL, 0x0);
	if (count == max_count) {
		u32 pc_ready, wfi_status;

		wfi_status = __read_register(device, CVP_WRAPPER_CPU_STATUS);
		pc_ready = __read_register(device, CVP_CTRL_STATUS);

		dprintk(CVP_WARN,
			"%s - %d, CPU Noc is not in LPI: %x %x %x\n",
			__func__, caller, lpi_status, wfi_status, pc_ready);

		/* Added for debug info purpose, not part of HPG */
		call_iris_op(device, print_sbm_regs, device);
	} else
		dprintk(CVP_INFO,
			"%s, CPU Noc is in LPI: lpi_status %x (count %d)\n",
			__func__, lpi_status, count);
}

static void __power_off_core_noc_hawi(struct iris_hfi_device *device)
{
	/* HPG section 3.4.4.2 Steps 1-9 */
	__disable_gdsc(device, "core_noc_mm_pd");
	if (!device->res->core_noc_cx_pd_disable)
		__disable_gdsc(device, "core_noc_cx_pd");
}

static void __enter_video_ctl_noc_lpi(struct iris_hfi_device *device,
					enum enter_noc_lpi_caller caller)
{
	u32 lpi_status, count = 0, max_count = 2000;

	/* HPG Section 3.7 Step 9*/
	__write_register(device, CVP_AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_CONTROL,
			 0x1);

	/* HPG Section 3.7 Step 10*/
	while (count < max_count) {
		/* Reading the LPI status */
		lpi_status = __read_register(
		    device, CVP_AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_STATUS);
		if (((lpi_status & BIT(1)) || (lpi_status & BIT(2))) &&
			(!(lpi_status & BIT(0)))) {
			__write_register(
			    device,
			    CVP_AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_CONTROL, 0x0);
			usleep_range(10, 20);
			__write_register(
			    device,
			    CVP_AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_CONTROL, 0x1);
			usleep_range(1000, 1200);
			count++;
		} else {
			break;
		}
	}

	dprintk(CVP_PWR, "%s, CVP_VIDEO_CTL Noc: lpi_status %x (count %d)\n",
		__func__, lpi_status, count);

	/* HPG Section 3.7 Step 11*/
	count = 0;
	while (count < max_count) {
		lpi_status = __read_register(device,
					CVP_AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_STATUS);
		if (lpi_status & 0x1)
			break;
		usleep_range(50, 100);
		count++;
	}

	/* HPG Section Step 12 */
	__write_register(device, CVP_AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_CONTROL,
			 0x0);
	if (count == max_count) {
		dprintk(CVP_WARN,
			"%s - %d, CVP_VIDEO_CTL Noc is not in LPI: lpi_status %x\n",
			__func__, caller, lpi_status);

		/* Added for debug info purpose, not part of HPG */
		call_iris_op(device, print_sbm_regs, device);
	} else
		dprintk(CVP_INFO, "%s, CVP_VIDEO_CTL Noc is in LPI: lpi_status %x (count %d)\n",
			__func__, lpi_status, count);
}

static void __noc_lpi_hawi(struct iris_hfi_device *device,
				enum enter_noc_lpi_caller caller)
{
	__enter_cpu_noc_lpi(device, caller);
	__enter_video_ctl_noc_lpi(device, caller);
}

static void setup_dsp_uc_memmap_vpu5_hawi(struct iris_hfi_device *device)
{
#ifdef CVP_DSP_ENABLED
	/* initialize DSP QTBL & UCREGION with CPU queues */
#ifdef USE_PRESIL42
	presil42_setup_dsp_uc_memmap_vpu5(device);
	return;
#endif
	__write_register(device, HFI_DSP_QTBL_ADDR,
			 (u32)device->dsp_iface_q_table.align_device_addr);
	__write_register(device, HFI_DSP_UC_REGION_ADDR,
			 (u32)device->dsp_iface_q_table.align_device_addr);
	__write_register(device, HFI_DSP_UC_REGION_SIZE,
			 device->dsp_iface_q_table.mem_data.size);
#endif
}

static void interrupt_init_iris2_hawi(struct iris_hfi_device *device)
{
	u32 mask_val = 0;

	/* All interrupts should be disabled initially 0x1F6 : Reset value */
	mask_val = __read_register(device, CVP_WRAPPER_INTR_MASK);

	/* Write 0 to unmask CPU and WD interrupts */
	mask_val &= ~(CVP_FATAL_INTR_BMSK | CVP_WRAPPER_INTR_MASK_A2HCPU_BMSK);
	__write_register(device, CVP_WRAPPER_INTR_MASK, mask_val);
	dprintk(CVP_REG, "Init irq: reg: %x, mask value %x\n",
		CVP_WRAPPER_INTR_MASK, mask_val);

	mask_val = 0;
	mask_val = __read_register(device, CVP_SS_IRQ_MASK);
	mask_val &= ~(CVP_SS_INTR_BMASK);
	__write_register(device, CVP_SS_IRQ_MASK, mask_val);
	dprintk(CVP_REG, "Init irq_wd: reg: %x, mask value %x\n",
		CVP_SS_IRQ_MASK, mask_val);
}

static int __check_ctl_power_on_hawi(struct iris_hfi_device *device)
{
	u32 reg;

	reg = __read_register(device, CVP_CC_MVS0C_GDSCR);
	if (!(reg & 0x80000000))
		return -1;

	reg = __read_register(device, CVP_CC_MVS0C_CBCR);
	if (reg & 0x80000000)
		return -2;

	reg = __read_register(device, CVP_CC_MVS0C_DEBUG_CBCR);
	if (reg & 0x80000000)
		return -3;

	return 0;
}

static int __check_core_power_on_hawi(struct iris_hfi_device *device)
{
	u32 reg;

	reg = __read_register(device, CVP_CC_MVS0_GDSCR);
	if (!(reg & 0x80000000))
		return -1;

	reg = __read_register(device, CVP_CC_MVS0_CBCR);
	if (reg & 0x80000000)
		return -2;

	if (!device->res->core_noc_cx_pd_disable) {
		reg = __read_register(device, CVP_CC_AXI0_CX_INT_GDSCR);
		if (!(reg & 0x80000000))
			return -3;
	}

	reg = __read_register(device, CVP_CC_MM_INT_GDSCR);
	if (!(reg & 0x80000000))
		return -4;

	return 0;
}

static void __controller_fallback_mode(struct iris_hfi_device *device)
{
	u32 tcsr_fallback, reg, write_mask;
	u32 count = 0, max_count = 1000;

	reg = __read_register(device, CVP_WRAPPER_GPIO_IN);

	tcsr_fallback = reg & 0x1;

	if (tcsr_fallback) {
		/*Assert SW/FW SHIFTER_CLK_EN: Write VPU_WRAPPER_GPIO_OUT[3] as 1*/
		reg = __read_register(device, CVP_WRAPPER_GPIO_OUT);
		write_mask = reg | 0x8;
		__write_register(device, CVP_WRAPPER_GPIO_OUT, write_mask);

		/* Assert SW/FW CX_INT_DL_REQ: Write VPU_WRAPPER_GPIO_OUT[2] as 1*/
		reg = __read_register(device, CVP_WRAPPER_GPIO_OUT);
		write_mask = reg | 0x4;
		__write_register(device, CVP_WRAPPER_GPIO_OUT, write_mask);

		/* Delay */
		usleep_range(50, 100);

		/* Poll on SW/FW CX_INT DL_Done_Status until it changed from 1 to 0
		 * Poll VPU_WRAPPER_GPIO_IN[5] until it is 0
		 */
		reg = __read_register(device, CVP_WRAPPER_GPIO_IN);
		while (((reg & 0x20) != 0) && count < max_count) {
			reg = __read_register(device, CVP_WRAPPER_GPIO_IN);
			usleep_range(50, 100);
			count++;
		}

		if (count == max_count)
			dprintk(CVP_WARN, "Poll on SW/FW CX_INT DL_Done_Status Failed\n");

		/* De-Assert SW/FW CX_INT_DL_REQ: Write VPU_WRAPPER_GPIO_OUT[2] as 0 */
		reg = __read_register(device, CVP_WRAPPER_GPIO_OUT);
		write_mask = reg | 0xFFFFFFFB;
		__write_register(device, CVP_WRAPPER_GPIO_OUT, write_mask);

		/* De-Assert SW/FW SHIFTER_CLK_EN: Write VPU_WRAPPER_GPIO_OUT [3] as 0 */
		reg = __read_register(device, CVP_WRAPPER_GPIO_OUT);
		write_mask = reg | 0xFFFFFFF7;
		__write_register(device, CVP_WRAPPER_GPIO_OUT, write_mask);

	}

}

static int __power_on_controller_hawi(struct iris_hfi_device *device)
{
	int rc = 0;

	CVPKERNEL_ATRACE_BEGIN("__power_on_controller_v1");

	rc = __enable_gdsc(device, "controller_pd");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable ctrler: %d\n", rc);
		return rc;
	}

	rc = msm_cvp_prepare_enable_clk(device, "sleep_clk");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable sleep clk: %d\n", rc);
		goto fail_reset_sleep;
	}

	rc = msm_cvp_prepare_enable_clk(device, "core_axi_clock");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable axi0 clk: %d\n", rc);
		goto fail_enable_axi0;
	}

	rc = msm_cvp_prepare_enable_clk(device, "cvp_axi_clock");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable axi0c clk: %d\n", rc);
		goto fail_enable_axi0c;
	}

	rc = msm_cvp_prepare_enable_clk(device, "cvp_clk");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable cvp_clk: %d\n", rc);
		goto fail_enable_cvp;
	}

	rc = msm_cvp_prepare_enable_clk(device, "cvp_freerun_clk");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable cvp_freerun_clk: %d\n", rc);
		goto fail_enable_freerun;
	}

	rc = msm_cvp_prepare_enable_clk(device, "cvp_ctl_freerun_clk");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable cvp_ctl_freerun_clk: %d\n", rc);
		goto fail_enable_freerun_ctl;
	}

	rc = msm_cvp_prepare_enable_clk(device, "cvp_debug_clk");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable cvp_debug_clk: %d\n", rc);
		goto fail_enable_cvp_debug;
	}

	__controller_fallback_mode(device);

	dprintk(CVP_PWR, "EVA controller powered on\n");
	CVPKERNEL_ATRACE_END("__power_on_controller_v1");
	return 0;

fail_enable_cvp_debug:
	msm_cvp_disable_unprepare_clk(device, "cvp_ctl_freerun_clk");
fail_enable_freerun_ctl:
	msm_cvp_disable_unprepare_clk(device, "cvp_freerun_clk");
fail_enable_freerun:
	msm_cvp_disable_unprepare_clk(device, "cvp_clk");
fail_enable_cvp:
	msm_cvp_disable_unprepare_clk(device, "cvp_axi_clock");
fail_enable_axi0c:
	msm_cvp_disable_unprepare_clk(device, "core_axi_clock");
fail_enable_axi0:
	msm_cvp_disable_unprepare_clk(device, "sleep_clk");
fail_reset_sleep:
	__disable_gdsc(device, "controller_pd");
	CVPKERNEL_ATRACE_END("__power_on_controller_v1");
	return rc;
}

static int __power_on_core_hawi(struct iris_hfi_device *device)
{
	int rc = 0;

	CVPKERNEL_ATRACE_BEGIN("__power_on_core_v1");

	if (!device->res->core_noc_cx_pd_disable) {
		rc = __enable_gdsc(device, "core_noc_cx_pd");
		if (rc) {
			dprintk(CVP_ERR, "Failed to enable core noc parent:%d\n", rc);
			goto fail_enable_core_noc_parent_gdsc;
		}
	}

	rc = __enable_gdsc(device, "core_noc_mm_pd");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable core noc:%d\n",
			rc);
		goto fail_enable_core_noc_gdsc;
	}

	rc = __enable_gdsc(device, "core_pd");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable core: %d\n", rc);
		goto fail_enable_core_gdsc;
	}

	rc = msm_cvp_prepare_enable_clk(device, "eva_cc_mvs0_clk_src");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable eva_cc_mvs0_clk_src:%d\n",
			rc);
		goto fail_enable_clk_src;
	}

	rc = msm_cvp_prepare_enable_clk(device, "core_clk");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable core_clk: %d\n", rc);
		goto fail_enable_core_clk;
	}

	if (device->res->core_noc_cx_pd_disable) {
		rc = msm_cvp_prepare_enable_clk(device, "core_freerun_clk");
		if (rc) {
			dprintk(CVP_ERR, "Failed to enable core_freerun_clk: %d\n", rc);
			goto fail_enable_freerun;
		}
	}

	dprintk(CVP_PWR, "EVA core powered on\n");
	CVPKERNEL_ATRACE_END("__power_on_core_v1");

	return 0;
fail_enable_freerun:
	msm_cvp_disable_unprepare_clk(device, "core_clk");
fail_enable_core_clk:
	msm_cvp_disable_unprepare_clk(device, "eva_cc_mvs0_clk_src");
fail_enable_clk_src:
	__disable_gdsc(device, "core_pd");
fail_enable_core_gdsc:
	__disable_gdsc(device, "core_noc_mm_pd");
fail_enable_core_noc_gdsc:
	if (!device->res->core_noc_cx_pd_disable)
		__disable_gdsc(device, "core_noc_cx_pd");
fail_enable_core_noc_parent_gdsc:
	return rc;
}

static int __power_off_core_hawi(struct iris_hfi_device *device)
{
	u32 config, value = 0, count = 0;
	u32 max_count = 10;

	value = __read_register(device, CVP_CC_MVS0_GDSCR);
	if (!(value & 0x80000000)) {
		/*
		 * Core has been powered off by f/w.
		 * Check NOC reset registers to ensure
		 * NO outstanding NoC transactions
		 */
		value = __read_register(device, CVP_NOC_RESET_ACK);
		if (value) {
			dprintk(CVP_WARN,
				"Core off with NOC RESET ACK non-zero %x\n",
				value);
			call_iris_op(device, print_sbm_regs, device);
		}

		if (!device->res->core_noc_cx_pd_disable)
			__disable_gdsc(device, "core_noc_cx_pd");
		__disable_gdsc(device, "core_noc_mm_pd");
		__disable_gdsc(device, "core_pd");
		msm_cvp_disable_unprepare_clk(device, "core_clk");
		return 0;
	} else if (!(value & 0x2) && msm_cvp_fw_low_power_mode) {
		/*
		 * HW_CONTROL PC disabled, then core is powered on for
		 * CVP NoC access
		 */
		if (!device->res->core_noc_cx_pd_disable)
			__disable_gdsc(device, "core_noc_cx_pd");
		__disable_gdsc(device, "core_noc_mm_pd");
		__disable_gdsc(device, "core_pd");
		msm_cvp_disable_unprepare_clk(device, "core_clk");
		return 0;
	}

	dprintk(CVP_PWR, "Driver controls Core power off now\n");

	/* HPG 3.4.4 step 1 */
	/*
	 * check to make sure core clock branch enabled else
	 * we cannot read core idle register
	 */
	config = __read_register(device, CVP_WRAPPER_CORE_CLOCK_CONFIG);
	if (config) {
		dprintk(CVP_PWR, "core clock config not enabled\n");
		__write_register(device, CVP_WRAPPER_CORE_CLOCK_CONFIG, 0);
	}

	/*
	 * check to make sure EVA NOC is in idle state
	 */

	do {
		value = __read_register(device, CVP_SS_IDLE_STATUS);
		if (value & 0x400000)
			goto advance;
		else
			usleep_range(1000, 2000);
		count++;
	} while (count < max_count);

	if (count == max_count)
		dprintk(CVP_WARN, "Core fail to go idle %x\n", value);

advance:
	/* Core NOC can be Power Collapsed separately on art */

	/* HPG Section 3.4.4 Steps 2-6 */
	__write_register(device, CVP_NOC_RESET_REQ, 0xffff0000);

	__write_register(device, CVP_NOC_RESET_REQ, 0xffff5a3f);

	count = 0;
	do {
		value = __read_register(device, CVP_NOC_RESET_ACK);
		if (value & 0x5a3f)
			break;
		usleep_range(1000, 2000);
		count++;
	} while (count < max_count);

	if (count == max_count)
		dprintk(CVP_WARN, "Failed to get Partial Reset Ack %x\n",
			value);

	__write_register(device, CVP_NOC_RESET_SYNCRST, 0x5a3f);
	__write_register(device, CVP_NOC_RESET_SYNCRST, 0x0);

	__write_register(device, CVP_NOC_RESET_REQ, 0xffff0000);

	count = 0;
	do {
		value = __read_register(device, CVP_NOC_RESET_ACK);
		if (value == 0x0)
			break;
		usleep_range(1000, 2000);
		count++;
	} while (count < max_count);

	if (count == max_count)
		dprintk(CVP_WARN, "Failed to get de-assert Partial Reset Ack %x\n",
			value);

	__write_register(device, CVP_NOC_RESET_REQ, 0xffff0000);
	__write_register(device, CVP_NOC_RESET_REQ, 0x0);

	/* HPG 3.4.4 step 7 */
	/* Reset both sides of 2 ahb2ahb_bridges (TZ and non-TZ) */
	__write_register(device, CVP_AHB_BRIDGE_SYNC_RESET, 0x3);
	__write_register(device, CVP_AHB_BRIDGE_SYNC_RESET, 0x2);
	__write_register(device, CVP_AHB_BRIDGE_SYNC_RESET, 0x0);

	/* HPG 3.4.4 step 8 */
	__disable_gdsc(device, "core_pd");
	msm_cvp_disable_unprepare_clk(device, "core_clk");

	/* HPG 3.4.4 step 9-10 */
	/* Handled by Clock Driver */

	/* HPG Step 3.4.4 step 11
	 * Power down Core Noc
	 */
	__power_off_core_noc_hawi(device);

	return 0;
}

static int __power_off_controller_hawi(struct iris_hfi_device *device)
{
	u32 lpi_status, count = 0, max_count = 1000;
	u32 lpi_control;
	int rc;

	/* HPG Section 3.7 Step 4  */
	__write_register(device, CVP_CPU_CS_X2RPMh, 0x3);

	/*
	 * HPG Section 3.7 Step 5-8
	 */
	__enter_cpu_noc_lpi(device, POWER_OFF_CNTRL);

	/*
	 * HPG Section 3.7 Step 9-12
	 */
	__enter_video_ctl_noc_lpi(device, POWER_OFF_CNTRL);

	/* HPG Section 3.7 Step 13 */
	__write_register(device, CVP_WRAPPER_DEBUG_BRIDGE_LPI_CONTROL, 0x0);

	/* HPG Section 3.7 Step 14 */
	lpi_status = 0x1;
	count = 0;
	while (lpi_status && count < max_count) {
		lpi_status = __read_register(
		    device, CVP_WRAPPER_DEBUG_BRIDGE_LPI_STATUS);
		usleep_range(50, 100);
		count++;
	}
	dprintk(CVP_PWR, "DBLP Release: lpi_status %d(count %d)\n", lpi_status,
		count);
	if (count == max_count)
		dprintk(CVP_WARN, "DBLP Release: lpi_status %x\n", lpi_status);

	/* HPG Section 3.7 Step 15 */
	lpi_control =
	    __read_register(device, CVP_AON_WRAPPER_CVP_NOC_LPI_CONTROL);
	lpi_control = lpi_control | 0x10;
	__write_register(device, CVP_AON_WRAPPER_CVP_NOC_LPI_CONTROL,
			 lpi_control);

	usleep_range(10, 20);
	lpi_control = lpi_control & (~0x10);
	__write_register(device, CVP_AON_WRAPPER_CVP_NOC_LPI_CONTROL,
			 lpi_control);

	/* HPG Section 3.7 Steps 16-17 */

	if (device->res->core_noc_cx_pd_disable) {
		rc = msm_cvp_disable_unprepare_clk(device, "core_freerun_clk");
		if (rc)
			dprintk(CVP_ERR, "Failed to disable core_freerun_clk: %d\n", rc);
	}

	rc = msm_cvp_disable_unprepare_clk(device, "cvp_debug_clk");
	if (rc)
		dprintk(CVP_ERR, "Failed to disable cvp_debug_clk: %d\n",
			rc);

	rc = msm_cvp_disable_unprepare_clk(device, "cvp_ctl_freerun_clk");
	if (rc)
		dprintk(CVP_ERR, "Failed to disable cvp_ctl_freerun_clk: %d\n", rc);

	rc = msm_cvp_disable_unprepare_clk(device, "cvp_freerun_clk");
	if (rc)
		dprintk(CVP_ERR, "Failed to disable cvp_freerun_clk: %d\n", rc);

	rc = msm_cvp_disable_unprepare_clk(device, "cvp_clk");
	if (rc)
		dprintk(CVP_ERR, "Failed to disable cvp_clk: %d\n", rc);

	rc = msm_cvp_disable_unprepare_clk(device, "sleep_clk");
	if (rc)
		dprintk(CVP_ERR, "Failed to disable sleep clk: %d\n", rc);

	__disable_gdsc(device, "controller_pd");

	/* Disables GCC clks in power on sequence */
	rc = msm_cvp_disable_unprepare_clk(device, "core_axi_clock");
	rc = msm_cvp_disable_unprepare_clk(device, "cvp_axi_clock");

	/****************** TODO RESET ****************************************
	 *
	 *
	 *
	rc = call_iris_op(device, reset_control_assert_name, device,
	"cvp_axi_reset"); if (rc) dprintk(CVP_ERR, "%s: assert cvp_axi_reset
	failed\n", __func__);

	rc = call_iris_op(device, reset_control_assert_name, device,
	"cvp_core_reset"); if (rc) dprintk(CVP_ERR, "%s: assert cvp_core_reset
	failed\n", __func__); usleep_range(1000, 1050);

	rc = call_iris_op(device, reset_control_deassert_name, device,
	"cvp_axi_reset"); if (rc) dprintk(CVP_ERR, "%s: de-assert cvp_axi_reset
	failed\n", __func__);

	rc = call_iris_op(device, reset_control_deassert_name, device,
	"cvp_core_reset"); if (rc) dprintk(CVP_ERR, "%s: de-assert
	cvp_core_reset failed\n", __func__);

	***********************************************************************/
	rc = msm_cvp_disable_unprepare_clk(device, "eva_cc_mvs0_clk_src");
	if (rc) {
		dprintk(CVP_ERR, "Failed to disable eva_cc_mvs0_clk_src: %d\n",
			rc);
	}
	return 0;
}

static void __print_sidebandmanager_regs_hawi(struct iris_hfi_device *device)
{
	u32 sbm_ln0_low, axi_cbcr, val;
	u32 main_sbm_ln0_low = 0xdeadbeef, main_sbm_ln0_high = 0xdeadbeef;
	u32 main_sbm_ln1_high = 0xdeadbeef, cpu_cs_x2rpmh;

	sbm_ln0_low = __read_register(device, CVP_NOC_SBM_SENSELN0_LOW);

	cpu_cs_x2rpmh = __read_register(device, CVP_CPU_CS_X2RPMh);

	__write_register(device, CVP_CPU_CS_X2RPMh,
			 (cpu_cs_x2rpmh | CVP_CPU_CS_X2RPMh_SWOVERRIDE_BMSK));
	usleep_range(500, 1000);
	val = __read_register(device, CVP_CPU_CS_X2RPMh);
	dprintk(CVP_REG, "CVP_CPU_CS_X2RPMh %#x\n", val);
	val = __read_register(device, CVP_CPU_CS_X2RPMh_STATUS);
	dprintk(CVP_REG, "CVP_CPU_CS_X2RPMh_STATUS %#x\n", val);

	cpu_cs_x2rpmh = __read_register(device, CVP_CPU_CS_X2RPMh);
	if (!(cpu_cs_x2rpmh & CVP_CPU_CS_X2RPMh_SWOVERRIDE_BMSK)) {
		dprintk(CVP_WARN, "failed set CVP_CPU_CS_X2RPMH mask %x\n",
			cpu_cs_x2rpmh);
		goto exit;
	}

	axi_cbcr = __read_gcc_register(device, CVP_GCC_EVA_AXI0_CBCR);
	if (axi_cbcr & 0x80000000) {
		dprintk(CVP_WARN, "failed to turn on AXI clock %x\n", axi_cbcr);
		goto exit;
	}

	/* Added by Thomas to debug CPU NoC hang */
	val = __read_register(device, CVP_NOC_ERR_ERRVLD_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRVLD_LOW %#x\n", val);

	val = __read_register(device, CVP_NOC_SBM_FAULTINSTATUS0_LOW);
	dprintk(CVP_ERR, "CVP_NOC_SBM_FAULTINSTATUS0_LOW %#x\n", val);

	val = __read_register(device, CVP_NOC_ERR_ERRLOG0_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG0_LOW %#x\n", val);

	val = __read_register(device, CVP_NOC_ERR_ERRLOG0_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG0_HIGH %#x\n", val);

	val = __read_register(device, CVP_NOC_ERR_ERRLOG1_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG1_LOW %#x\n", val);

	val = __read_register(device, CVP_NOC_ERR_ERRLOG1_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG1_HIGH %#x\n", val);

	val = __read_register(device, CVP_NOC_ERR_ERRLOG2_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG2_LOW %#x\n", val);

	val = __read_register(device, CVP_NOC_ERR_ERRLOG2_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG2_HIGH %#x\n", val);

	val = __read_register(device, CVP_NOC_ERR_ERRLOG3_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG3_LOW %#x\n", val);

	val = __read_register(device, CVP_NOC_ERR_ERRLOG3_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERL_MAIN_ERRLOG3_HIGH %#x\n", val);

	main_sbm_ln0_low =
	    __read_register(device, CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN0_LOW);
	main_sbm_ln0_high =
	    __read_register(device, CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN0_HIGH);
	main_sbm_ln1_high =
	    __read_register(device, CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN1_HIGH);

exit:
	cpu_cs_x2rpmh = cpu_cs_x2rpmh & (~CVP_CPU_CS_X2RPMh_SWOVERRIDE_BMSK);
	__write_register(device, CVP_CPU_CS_X2RPMh, cpu_cs_x2rpmh);
	dprintk(CVP_WARN, "Sidebandmanager regs %x %x %x %x %x\n", sbm_ln0_low,
		main_sbm_ln0_low, main_sbm_ln0_high, main_sbm_ln1_high,
		cpu_cs_x2rpmh);
}

static int gdsc_set_check(struct iris_hfi_device *device, bool check_reg_status,
	u32 reg_name, const char *gdsc_name, enum core_gdsc_dest dest)
{
	int rc = 0, loop = 10;
	u32 reg_gdsc;
	char *power_status;
	u32 power_mask;

	power_status = dest ? "power on" : "power off";
	power_mask = dest ? 0x1 : 0x0;

	rc = switch_core_gdsc_mode(device, dest, gdsc_name);
	if (rc) {
		dprintk(CVP_WARN,
			"%s Vote failed for %s gdsc:%d\n",
			dest, gdsc_name, rc);
		return rc;
	}

	if (check_reg_status)
		goto reg_status;
	else
		return rc;

reg_status:
	while (loop) {
		reg_gdsc = __read_register(device, reg_name);
		if ((reg_gdsc >> 31) == power_mask) {
			usleep_range(100, 200);
			loop--;
		} else
			break;
	}

	if (!loop) {
		dprintk(CVP_ERR, "fail to %s CORE/CORE NOC during resume\n", power_status);
		return -EINVAL;
	}

	return rc;
}

static int switch_all_core_gdsc(struct iris_hfi_device *device, bool check_reg_status,
	enum core_gdsc_dest dest)
{
	int rc = 0;
	int i = 0;
	u32 num_of_gdsc = device->res->core_noc_cx_pd_disable ? 2 : 3;

	u32 reg_gdsc_hw_ctl_2[] = { CVP_CC_MVS0_GDSCR, CVP_CC_MM_INT_GDSCR };
	u32 reg_gdsc_hw_ctl_3[] = { CVP_CC_MVS0_GDSCR, CVP_CC_MM_INT_GDSCR,
		CVP_CC_AXI0_CX_INT_GDSCR };
	static const char *const gdsc_name_hw_ctl_2[] = { "core_pd", "core_noc_mm_pd" };
	static const char *const gdsc_name_hw_ctl_3[] = { "core_pd", "core_noc_mm_pd",
		"core_noc_cx_pd" };
	const u32 *reg_gdsc_hw_ctl = device->res->core_noc_cx_pd_disable ?
		reg_gdsc_hw_ctl_2 : reg_gdsc_hw_ctl_3;
	static const char *const *gdsc_name_hw_ctl;

	u32 reg_gdsc_sw_ctl_2[] = { CVP_CC_MM_INT_GDSCR, CVP_CC_MVS0_GDSCR };
	u32 reg_gdsc_sw_ctl_3[] = { CVP_CC_AXI0_CX_INT_GDSCR, CVP_CC_MM_INT_GDSCR,
		CVP_CC_MVS0_GDSCR };
	static const char *const gdsc_name_sw_ctl_2[] = { "core_noc_mm_pd",
		"core_pd"};
	static const char *const gdsc_name_sw_ctl_3[] = { "core_noc_cx_pd", "core_noc_mm_pd",
		"core_pd"};
	const u32 *reg_gdsc_sw_ctl = device->res->core_noc_cx_pd_disable ?
		reg_gdsc_sw_ctl_2 : reg_gdsc_sw_ctl_3;
	static const char *const *gdsc_name_sw_ctl;

	enum core_gdsc_dest default_gdsc_mode = TO_SW_CTRL;

	gdsc_name_hw_ctl = device->res->core_noc_cx_pd_disable ?
		gdsc_name_hw_ctl_2 : gdsc_name_hw_ctl_3;
	gdsc_name_sw_ctl = device->res->core_noc_cx_pd_disable ?
		gdsc_name_sw_ctl_2 : gdsc_name_sw_ctl_3;

	if (device->res->gdsc_framework_type) {
		for (i = 0; i < num_of_gdsc; i++) {
			if (dest == TO_HW_CTRL)
				rc = gdsc_set_check(device, check_reg_status,
					reg_gdsc_hw_ctl[i], gdsc_name_hw_ctl[i], dest);
			else
				rc = gdsc_set_check(device, check_reg_status,
					reg_gdsc_sw_ctl[i], gdsc_name_sw_ctl[i], dest);
			if (rc)
				goto revert_switch;
		}
	} else {
		dprintk(CVP_WARN, "Failed to switch GDSC, Framework not available\n");
		return -EINVAL;
	}

	return rc;

revert_switch:
	dprintk(CVP_WARN, "Switching GDSC Failed, Reverting back to default mode\n");
	while (i > 0) {
		i--;
		rc = gdsc_set_check(device, false, reg_gdsc_hw_ctl[i], gdsc_name_hw_ctl[i],
			default_gdsc_mode);
		if (rc)
			dprintk(CVP_WARN, "Switching GDSC Failed to default mode\n");
	}
	return -EINVAL;
}

static int __enable_hw_power_collapse_hawi(struct iris_hfi_device *device)
{
	int rc = 0;

	if (!msm_cvp_fw_low_power_mode) {
		dprintk(CVP_PWR, "Not enabling hardware power collapse\n");
		return 0;
	}

	rc = switch_all_core_gdsc(device, true, TO_HW_CTRL);

	if (rc) {
		dprintk(CVP_WARN, "Failed to enable hardware power collapse\n");
		rc = switch_all_core_gdsc(device, false, TO_SW_CTRL);
	}

	return rc;
}

static int __set_registers_hawi(struct iris_hfi_device *device)
{
	struct msm_cvp_core *core;
	struct msm_cvp_platform_data *pdata;
	struct reg_set *reg_set;
	int i;
	u32 arcg = 1;

	if (!device->res) {
		dprintk(CVP_ERR,
			"device resources null, cannot set registers\n");
		return -EINVAL;
	}

	core = cvp_driver->cvp_core;
	pdata = core->platform_data;

	reg_set = &device->res->reg_set;
	for (i = 0; i < reg_set->count; i++) {
		__write_register(device, reg_set->reg_tbl[i].reg,
				 reg_set->reg_tbl[i].value);
		dprintk(CVP_REG, "write_reg offset=%x, val=%x\n",
			reg_set->reg_tbl[i].reg, reg_set->reg_tbl[i].value);
	}

	if (arcg) {
		__write_register(device, CVP_NOC_RCGCONTROLLER_HYSTERESIS_LOW, 0xff);
		__write_register(device, CVP_NOC_RCGCONTROLLER_WAKEUP_LOW, 0x7);
		__write_register(device, CVP_NOC_RCG_VNOC_NOC_CLK_FORCECLOCKON_LOW,
			0x1);
		__write_register(device, CVP_NOC_RCG_VNOC_NOC_CLK_ENABLE_LOW, 0x1);
		usleep_range(5, 10);
		__write_register(device, CVP_NOC_RCG_VNOC_NOC_CLK_FORCECLOCKON_LOW,
			0x0);
		__write_register(device, CVP_AON_WRAPPER_CVP_NOC_ARCG_CONTROL, 0x0);
	} else
		dprintk(CVP_WARN, "Skip ARCG sequence\n");

	__write_register(device, CVP_CPU_CS_AXI4_QOS, pdata->noc_qos->axi_qos);
	__write_register(device, CVP_NOC_A_PRIORITYLUT_LOW,
			 pdata->noc_qos->prioritylut_low);
	__write_register(device, CVP_NOC_A_PRIORITYLUT_HIGH,
			 pdata->noc_qos->prioritylut_high);
	/*
	 * XOR'ed with bitmask to allow the urgency low value
	 * to be modify differently for ALOR and ART
	 */
	__write_register(device, CVP_NOC_A_URGENCY_LOW,
			 (pdata->noc_qos->urgency_low) ^
			 (device->res->qos_noc_urgency_low_a_bitmask));
	__write_register(device, CVP_NOC_A_DANGERLUT_LOW,
			 pdata->noc_qos->dangerlut_low);
	__write_register(device, CVP_NOC_A_SAFELUT_LOW,
			 pdata->noc_qos->safelut_low);
	__write_register(device, CVP_NOC_B_PRIORITYLUT_LOW,
			 pdata->noc_qos->prioritylut_low);
	__write_register(device, CVP_NOC_B_PRIORITYLUT_HIGH,
			 pdata->noc_qos->prioritylut_high);
	/*
	 * XOR'ed with bitmask to allow the urgency low value
	 * to be modify differently for ALOR and ART
	 */
	__write_register(device, CVP_NOC_B_URGENCY_LOW,
			 (pdata->noc_qos->urgency_low) ^
			 (device->res->qos_noc_urgency_low_b_bitmask));
	__write_register(device, CVP_NOC_B_DANGERLUT_LOW,
			 pdata->noc_qos->dangerlut_low);
	__write_register(device, CVP_NOC_B_SAFELUT_LOW,
			 pdata->noc_qos->safelut_low);
	__write_register(device, CVP_NOC_C_PRIORITYLUT_LOW,
			 pdata->noc_qos->prioritylut_low);
	__write_register(device, CVP_NOC_C_PRIORITYLUT_HIGH,
			 pdata->noc_qos->prioritylut_high);
	__write_register(device, CVP_NOC_C_URGENCY_LOW,
			 pdata->noc_qos->urgency_low_ro);
	__write_register(device, CVP_NOC_C_DANGERLUT_LOW,
			 pdata->noc_qos->dangerlut_low);
	__write_register(device, CVP_NOC_C_SAFELUT_LOW,
			 pdata->noc_qos->safelut_low);

	/* Below registers write moved from FW to SW to enable UBWC */
	__write_register(device, CVP_NOC_A_NIU_DECCTL_LOW, 0x1);
	/* ToDo: After cofirming for HAWI, remove these 2 registers completely */
	if (!device->res->core_noc_cx_pd_disable) {
		__write_register(device, CVP_NOC_A_NIU_ENCCTL_LOW, 0x1);
		__write_register(device, CVP_NOC_B_NIU_DECCTL_LOW, 0x1);
	}
	__write_register(device, CVP_NOC_B_NIU_ENCCTL_LOW, 0x1);
	__write_register(device, CVP_NOC_CORE_ERR_MAINCTL_LOW_OFFS, 0x3);
	__write_register(device, CVP_NOC_MAIN_SIDEBANDMANAGER_FAULTINEN0_LOW,
			 0x1);

	return 0;
}

static void __print_reg_details_errlog3_low_hawi(u32 val)
{
	u32 mid, sid;

	mid = (val >> 7) & 0x1F;

	sid = (val >> 2) & 0x7;
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG3_LOW:     %#x\n", val);
	dprintk(CVP_ERR, "Sub-client:%s, SID: %d\n", mid_names_hawi[mid],
		sid);
}

static void __clear_pd_noc_req(struct iris_hfi_device *device)
{
	u32 val = 0, count = 0, max_count = 10;
	u32 lpi_status;

	if (!device->res->core_noc_cx_pd_disable) {
		val = __read_register(device, CVP_WRAPPER_CVP_NOC_CX_LPI_CONTROL);
		val = val & (~0x01);
		__write_register(device, CVP_WRAPPER_CVP_NOC_CX_LPI_CONTROL, val);

		while (count < max_count) {
			lpi_status = __read_register(device,
				CVP_WRAPPER_CVP_NOC_CX_LPI_STATUS);
			if ((lpi_status & 0x1) == 0x0)
				break;
			count++;
		}

		dprintk(CVP_ERR, "%s, CVP_NOC_CX Noc: lpi_status %x (count %d)\n",
			__func__, lpi_status, count);
	}

	val = __read_register(device, CVP_WRAPPER_CVP_NOC_LPI_CONTROL);
	val = val & (~0x01);
	__write_register(device, CVP_WRAPPER_CVP_NOC_LPI_CONTROL, val);

	count = 0;

	while (count < max_count) {
		lpi_status = __read_register(device, CVP_WRAPPER_CVP_NOC_LPI_STATUS);
		if ((lpi_status & 0x1) == 0x0)
			break;
		count++;
	}

	dprintk(CVP_ERR, "%s, CVP_NOC_MM Noc: lpi_status %x (count %d)\n",
		__func__, lpi_status, count);
}

static void __dump_noc_regs_hawi(struct iris_hfi_device *device)
{
	#ifndef USE_PRESIL42
	u32 val = 0, config;
	struct regulator_info *rinfo;
	int rc = 0;

	if (msm_cvp_fw_low_power_mode) {
		if (device->res->gdsc_framework_type) {
			rc = switch_all_core_gdsc(device, true, TO_SW_CTRL);
		} else {
			iris_hfi_for_each_regulator(device, rinfo) {
				if (strcmp(rinfo->name, "cvp-core"))
					continue;
				rc = __acquire_regulator(rinfo, device);
			}
		}
		if (rc)
			dprintk(
			    CVP_WARN,
			    "%s, Failed to acquire core gdsc control to SW\n",
			    __func__);
	}

	__clear_pd_noc_req(device);

	val = __read_register(device, CVP_CC_MVS0_GDSCR);
	dprintk(CVP_ERR, "%s, CVP_CC_MVS0_GDSCR: 0x%x", __func__, val);
	val = __read_register(device, CVP_CC_MM_INT_GDSCR);
	dprintk(CVP_ERR, "%s, CVP_CC_MM_INT_GDSCR: 0x%x", __func__, val);
	if (!device->res->core_noc_cx_pd_disable) {
		val = __read_register(device, CVP_CC_AXI0_CX_INT_GDSCR);
		dprintk(CVP_ERR, "%s, CVP_CC_AXI0_CX_INT_GDSCR: 0x%x", __func__, val);
	}

	val = __read_register(device, CVP_CC_XO_CBCR);
	dprintk(CVP_ERR, "%s, CVP_CC_XO_CBCR: 0x%x", __func__, val);
	val = __read_register(device, CVP_CC_SLEEP_CBCR);
	dprintk(CVP_ERR, "%s, CVP_CC_SLEEP_CBCR: 0x%x", __func__, val);
	val = __read_register(device, CVP_CC_AHB_CBCR);
	dprintk(CVP_ERR, "%s, CVP_CC_AHB_CBCR: 0x%x", __func__, val);
	val = __read_register(device, CVP_CC_DBGCH_XO_CBCR);
	dprintk(CVP_ERR, "%s, CVP_CC_DBGCH_XO_CBCR: 0x%x", __func__, val);
	val = __read_register(device, CVP_CC_CC_MVS0C_CTL_FREERUN_CBCR);
	dprintk(CVP_ERR, "%s, CVP_CC_CC_MVS0C_CTL_FREERUN_CBCR: 0x%x", __func__, val);
	val = __read_register(device, CVP_CC_MVS0C_FREERUN_CBCR);
	dprintk(CVP_ERR, "%s, CVP_CC_MVS0C_FREERUN_CBCR: 0x%x", __func__, val);

	config = __read_register(device, CVP_WRAPPER_CORE_CLOCK_CONFIG);
	dprintk(CVP_ERR, "%s, CVP_WRAPPER_CORE_CLOCK_CONFIG: 0x%x", __func__,
		config);
	if (config) {
		dprintk(CVP_PWR, "core clock config not enabled\n");
		__write_register(device, CVP_WRAPPER_CORE_CLOCK_CONFIG, 0);
	}

	val = __read_register(device, CVP_NOC_A_NIU_DECCTL_LOW);
	dprintk(CVP_ERR, "CVP_NOC_A_NIU_DECCTL_LOW: 0x%x", val);
	val = __read_register(device, CVP_NOC_A_NIU_ENCCTL_LOW);
	dprintk(CVP_ERR, "CVP_NOC_A_NIU_ENCCTL_LOW: 0x%x", val);
	val = __read_register(device, CVP_NOC_B_NIU_DECCTL_LOW);
	dprintk(CVP_ERR, "CVP_NOC_B_NIU_DECCTL_LOW: 0x%x", val);
	val = __read_register(device, CVP_NOC_B_NIU_ENCCTL_LOW);
	dprintk(CVP_ERR, "CVP_NOC_B_NIU_ENCCTL_LOW: 0x%x", val);
	val = __read_register(device,
			      CVP_NOC_MAIN_SIDEBANDMANAGER_FAULTINEN0_LOW);
	dprintk(CVP_ERR, "CVP_NOC_MAIN_SIDEBANDMANAGER_FAULTINEN0_LOW: 0x%x",
		val);
	val =
	    __read_register(device, CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN0_LOW);
	dprintk(CVP_ERR, "CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN0_LOW: 0x%x",
		val);
	val =
	    __read_register(device, CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN0_HIGH);
	dprintk(CVP_ERR, "CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN0_HIGH: 0x%x",
		val);
	val =
	    __read_register(device, CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN1_LOW);
	dprintk(CVP_ERR, "CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN1_LOW: 0x%x",
		val);
	val =
	    __read_register(device, CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN1_HIGH);
	dprintk(CVP_ERR, "CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN1_HIGH: 0x%x",
		val);
	val =
	    __read_register(device, CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN2_LOW);
	dprintk(CVP_ERR, "CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN2_LOW: 0x%x",
		val);

	dprintk(CVP_ERR, "Dumping CPU NoC registers\n");
	val = __read_register(device, CVP_NOC_ERR_MAINCTL_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERR_MAINCTL_LOW_OFFS: 0x%x", val);
	val = __read_register(device, CVP_NOC_ERR_ERRVLD_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERR_ERRVLD_LOW_OFFS: 0x%x", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG0_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERR_ERRLOG0_LOW_OFFS: 0x%x", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG0_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERR_ERRLOG0_HIGH_OFFS: 0x%x", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG1_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERR_ERRLOG1_LOW_OFFS: 0x%x", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG1_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERR_ERRLOG1_HIGH_OFFS: 0x%x", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG2_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERR_ERRLOG2_LOW_OFFS: 0x%x", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG2_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERR_ERRLOG2_HIGH_OFFS: 0x%x", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG3_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERR_ERRLOG3_LOW_OFFS: 0x%x", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG3_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_ERR_ERRLOG3_HIGH_OFFS: 0x%x", val);

	dprintk(CVP_ERR, "Dumping Core NoC registers\n");
	val = __read_register(device, CVP_NOC_CORE_ERR_SWID_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC__CORE_ERL_MAIN_SWID_LOW: 0x%x", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_SWID_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_SWID_HIGH 0x%x", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_MAINCTL_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_MAINCTL_LOW 0x%x", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRVLD_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRVLD_LOW 0x%x", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRCLR_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRCLR_LOW 0x%x", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG0_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG0_LOW 0x%x", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG0_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG0_HIGH 0x%x", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG1_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG1_LOW 0x%x", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG1_HIGH_OFFS);
	__print_reg_details_errlog1_high(val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG2_LOW_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG2_LOW 0x%x", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG2_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG2_HIGH 0x%x", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG3_LOW_OFFS);
	dprintk(CVP_ERR, "CORE ERRLOG3_LOW 0x%x, below details", val);
	__print_reg_details_errlog3_low_hawi(val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG3_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG3_HIGH 0x%x", val);
	__write_register(device, CVP_NOC_CORE_ERR_ERRCLR_LOW_OFFS, 0x1);

	if (msm_cvp_fw_low_power_mode) {
		if (device->res->gdsc_framework_type) {
			rc = switch_all_core_gdsc(device, true, TO_HW_CTRL);
		} else {
			iris_hfi_for_each_regulator(device, rinfo) {
				if (strcmp(rinfo->name, "cvp-core"))
					continue;
				rc = __hand_off_regulator(rinfo);
			}
		}
		if (rc) {
			dprintk(
				CVP_WARN,
				"%s, Failed to hand off core gdsc control to HW\n",
				__func__);
			rc = switch_all_core_gdsc(device, false, TO_SW_CTRL);
		}
	}
	__write_register(device, CVP_WRAPPER_CORE_CLOCK_CONFIG, config);
#endif
}

static void __noc_error_info_iris2_hawi(struct iris_hfi_device *device)
{
	struct msm_cvp_core *core;
	struct cvp_noc_log *noc_log;
	u32 val = 0, regi, regiii;
	bool log_required = false;
	int rc;

	core = cvp_driver->cvp_core;

	if (!core->ssr_count && core->resources.max_ssr_allowed >= 1)
		log_required = true;

	noc_log = &core->kmd_trace.kmd_debug_log.log->noc_log;

	if (noc_log->used) {
		dprintk(CVP_WARN, "Data already in NoC log, skip logging\n");
		return;
	}
	noc_log->used = 1;
	rc = 0;

	if (!device->res->core_noc_cx_pd_disable)
		__disable_hw_power_collapse(device, "core_noc_cx_pd");
	__disable_hw_power_collapse(device, "core_noc_mm_pd");
	__disable_hw_power_collapse(device, "core_pd");


	val = call_iris_op(device, check_core_power_on, device);
	regi =
	    __read_register(device, CVP_AON_WRAPPER_CVP_NOC_CORE_CLK_CONTROL);
	regiii = __read_register(device, CVP_WRAPPER_CORE_CLOCK_CONFIG);
	dprintk(CVP_ERR, "noc reg check: %#x %#x %#x\n", val, regi, regiii);

	val = __read_register(device, CVP_NOC_ERR_SWID_LOW_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_swid_low,
		  "CVP_NOC_ERL_MAIN_SWID_LOW", val);
	val = __read_register(device, CVP_NOC_ERR_SWID_HIGH_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_swid_high,
		  "CVP_NOC_ERL_MAIN_SWID_HIGH", val);
	val = __read_register(device, CVP_NOC_ERR_MAINCTL_LOW_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_mainctl_low,
		  "CVP_NOC_ERL_MAIN_MAINCTL_LOW", val);
	val = __read_register(device, CVP_NOC_ERR_ERRVLD_LOW_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_errvld_low,
		  "CVP_NOC_ERL_MAIN_ERRVLD_LOW", val);
	val = __read_register(device, CVP_NOC_ERR_ERRCLR_LOW_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_errclr_low,
		  "CVP_NOC_ERL_MAIN_ERRCLR_LOW", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG0_LOW_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_errlog0_low,
		  "CVP_NOC_ERL_MAIN_ERRLOG0_LOW", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG0_HIGH_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_errlog0_high,
		  "CVP_NOC_ERL_MAIN_ERRLOG0_HIGH", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG1_LOW_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_errlog1_low,
		  "CVP_NOC_ERL_MAIN_ERRLOG1_LOW", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG1_HIGH_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_errlog1_high,
		  "CVP_NOC_ERL_MAIN_ERRLOG1_HIGH", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG2_LOW_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_errlog2_low,
		  "CVP_NOC_ERL_MAIN_ERRLOG2_LOW", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG2_HIGH_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_errlog2_high,
		  "CVP_NOC_ERL_MAIN_ERRLOG2_HIGH", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG3_LOW_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_errlog3_low,
		  "CVP_NOC_ERL_MAIN_ERRLOG3_LOW", val);
	val = __read_register(device, CVP_NOC_ERR_ERRLOG3_HIGH_OFFS);
	__err_log(log_required, &noc_log->err_ctrl_errlog3_high,
		  "CVP_NOC_ERL_MAIN_ERRLOG3_HIGH", val);

	val = __read_register(device, CVP_NOC_CORE_ERR_SWID_LOW_OFFS);
	__err_log(log_required, &noc_log->err_core_swid_low,
		  "CVP_NOC__CORE_ERL_MAIN_SWID_LOW", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_SWID_HIGH_OFFS);
	__err_log(log_required, &noc_log->err_core_swid_high,
		  "CVP_NOC_CORE_ERL_MAIN_SWID_HIGH", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_MAINCTL_LOW_OFFS);
	__err_log(log_required, &noc_log->err_core_mainctl_low,
		  "CVP_NOC_CORE_ERL_MAIN_MAINCTL_LOW", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRVLD_LOW_OFFS);
	__err_log(log_required, &noc_log->err_core_errvld_low,
		  "CVP_NOC_CORE_ERL_MAIN_ERRVLD_LOW", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRCLR_LOW_OFFS);
	__err_log(log_required, &noc_log->err_core_errclr_low,
		  "CVP_NOC_CORE_ERL_MAIN_ERRCLR_LOW", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG0_LOW_OFFS);
	__err_log(log_required, &noc_log->err_core_errlog0_low,
		  "CVP_NOC_CORE_ERL_MAIN_ERRLOG0_LOW", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG0_HIGH_OFFS);
	__err_log(log_required, &noc_log->err_core_errlog0_high,
		  "CVP_NOC_CORE_ERL_MAIN_ERRLOG0_HIGH", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG1_LOW_OFFS);
	__err_log(log_required, &noc_log->err_core_errlog1_low,
		  "CVP_NOC_CORE_ERL_MAIN_ERRLOG1_LOW", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG1_HIGH_OFFS);
	__err_log(log_required, &noc_log->err_core_errlog1_high,
		  "CVP_NOC_CORE_ERL_MAIN_ERRLOG1_HIGH", val);
	__print_reg_details_errlog1_high(val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG2_LOW_OFFS);
	__err_log(log_required, &noc_log->err_core_errlog2_low,
		  "CVP_NOC_CORE_ERL_MAIN_ERRLOG2_LOW", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG2_HIGH_OFFS);
	__err_log(log_required, &noc_log->err_core_errlog2_high,
		  "CVP_NOC_CORE_ERL_MAIN_ERRLOG2_HIGH", val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG3_LOW_OFFS);
	__err_log(log_required, &noc_log->err_core_errlog3_low,
		  "CORE ERRLOG3_LOW, below details", val);
	__print_reg_details_errlog3_low_hawi(val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG3_HIGH_OFFS);
	__err_log(log_required, &noc_log->err_core_errlog3_high,
		  "CVP_NOC_CORE_ERL_MAIN_ERRLOG3_HIGH", val);
	__write_register(device, CVP_NOC_CORE_ERR_ERRCLR_LOW_OFFS, 0x1);

#define CVP_SS_CLK_HALT 0x8
#define CVP_SS_CLK_EN 0xC
#define CVP_VPU_WRAPPER_CORE_CONFIG 0xB0088
	__write_register(device, CVP_SS_CLK_HALT, 0);
	__write_register(device, CVP_SS_CLK_EN, 0x3f);
	__write_register(device, CVP_VPU_WRAPPER_CORE_CONFIG, 0);
}

int set_hawi_hal_functions(void)
{
	hal_ops.interrupt_init = interrupt_init_iris2_hawi;
	hal_ops.setup_dsp_uc_memmap = setup_dsp_uc_memmap_vpu5_hawi;
	hal_ops.power_off_controller = __power_off_controller_hawi;
	hal_ops.power_off_core = __power_off_core_hawi;
	hal_ops.power_on_controller = __power_on_controller_hawi;
	hal_ops.power_on_core = __power_on_core_hawi;
	hal_ops.noc_error_info = __noc_error_info_iris2_hawi;
	hal_ops.check_ctl_power_on = __check_ctl_power_on_hawi;
	hal_ops.check_core_power_on = __check_core_power_on_hawi;
	hal_ops.print_sbm_regs = __print_sidebandmanager_regs_hawi;
	hal_ops.enable_hw_power_collapse = __enable_hw_power_collapse_hawi;
	hal_ops.set_registers = __set_registers_hawi;
	hal_ops.dump_noc_regs = __dump_noc_regs_hawi;
	hal_ops.check_tensilica_in_reset = __check_tensilica_in_reset_hawi;
	hal_ops.pm_qos_update = iris_pm_qos_aggregate_hawi;
	hal_ops.noc_lpi = __noc_lpi_hawi;
	return 0;
}
