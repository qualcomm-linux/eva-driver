// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/* Nordau EVA HAL implementation. */

#include "cvp_nordau_hal.h"
#include "cvp_nordau_io.h"
#include "cvp_hfi_io.h"

extern struct cvp_hal_ops hal_ops;

const struct cvp_hfi_io_registers cvp_nordau_io_registers = {
	CVP_HFI_IO_REGISTERS_INIT,
};

static int iris_pm_qos_update_nordau(struct iris_hfi_device *device)
{
	struct iris_hfi_device *dev;

	if (!device) {
		dprintk(CVP_ERR, "%s Invalid device\n", __func__);
		return -ENODEV;
	}

	dev = device;

	mutex_lock(&dev->lock);
	cvp_pm_qos_update(dev, true);
	mutex_unlock(&dev->lock);

	return 0;
}

static void __check_tensilica_in_reset_nordau(struct iris_hfi_device *device)
{
	u32 xtss_reset_ro = 1;

	/*
	 * [INH from cvp_pakala_hal.c:29-35] body; register via cvp_nordau_io.h
	 * CVP_WRAPPER_XTSS_SW_RESET_RO (source: nordau_registers.csv
	 * VPU_WRAPPER_TSW_XTSS_SW_RESET_RO,0x000C0020).
	 *
	 * BUGFIX vs 9461931.diff:448 — the patch guards this read with
	 * "#ifdef CONFIG_EVA_SUN", a copy-paste leak from another chipset that
	 * silently compiles the read out on every non-SUN target (leaving the
	 * bogus initialiser 1 to be printed). This target-specific read is kept
	 * in the Nordau HAL, which is selected through runtime platform data.
	 */
	xtss_reset_ro = __read_register(device, CVP_WRAPPER_XTSS_SW_RESET_RO);
	dprintk(CVP_WARN, "tensilica xtss_reset_ro %#x\n", xtss_reset_ro);
}

static void __enter_cpu_noc_lpi(struct iris_hfi_device *device)
{
	u32 lpi_status, count = 0, max_count = 2000;

	/* [INH from cvp_pakala_hal.c:37-90] — NORDAU HPG §6.14 "SW based NoC
	 * error recovery sequence" Steps 15-17. §6 numbering did NOT shift
	 * (NORD_EVA_4.6.html:3270 "6.14 M5.8 SW based NoC error recovery
	 * sequence"), so this citation is unchanged from the parent.
	 *
	 * Clear CVP_iris_cpu_noc_errorLogger_ErrVld_Low SWI
	 * by writing 0x1 to CVP_NOC_ERR_ERRCLR_LOW_OFFS.
	 * No resource-name strings in this function.
	 */
	__write_register(device, CVP_NOC_ERR_ERRCLR_LOW_OFFS, 0x1);
	__write_register(device, CVP_WRAPPER_CPU_NOC_LPI_CONTROL, 0x1);
	while (count < max_count) {
		lpi_status =
		    __read_register(device, CVP_WRAPPER_CPU_NOC_LPI_STATUS);
		if ((lpi_status & BIT(1)) ||
		    ((lpi_status & BIT(2)) && (!(lpi_status & BIT(0))))) {
			/*
			 * If QDENY == true, or
			 * If QACTIVE == true && QACCEPT == false
			 * Try again
			 */
			__write_register(device,
					 CVP_WRAPPER_CPU_NOC_LPI_CONTROL, 0x0);
			usleep_range(10, 20);
			__write_register(device, CVP_NOC_ERR_ERRCLR_LOW_OFFS,
					 0x1);
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

	/* HPG Step-7 of section 3.7 */
	__write_register(device, CVP_WRAPPER_CPU_NOC_LPI_CONTROL, 0x0);

	if (count == max_count) {
		u32 pc_ready, wfi_status;

		wfi_status = __read_register(device, CVP_WRAPPER_CPU_STATUS);
		pc_ready = __read_register(device, CVP_CTRL_STATUS);

		dprintk(CVP_WARN,
			"%s, CPU NOC not in qaccept status %x %x %x\n",
			__func__, lpi_status, wfi_status, pc_ready);

		/* Added for debug info purpose, not part of HPG */
		call_iris_op(device, print_sbm_regs, device);
	}
}

static void __enter_core_noc_lpi(struct iris_hfi_device *device)
{
	u32 lpi_status, value, count = 0, max_count = 2000;

	/* [INH from cvp_pakala_hal.c:92-152] — NORDAU HPG §6.14 Steps 4-6
	 * (§6 numbering unshifted).
	 * Clear CVP_NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_ERRVLD_LOW SWI
	 * by writing 0x1 to CVP_NOC_CORE_ERR_ERRCLR_LOW_OFFS.
	 * No resource-name strings in this function.
	 */
	__write_register(device, CVP_NOC_CORE_ERR_ERRCLR_LOW_OFFS, 0x1);
	__write_register(device, CVP_AON_WRAPPER_CVP_NOC_LPI_CONTROL, 0x1);
	while (count < max_count) {
		/* Reading the LPI status */
		lpi_status =
		    __read_register(device, CVP_AON_WRAPPER_CVP_NOC_LPI_STATUS);
		if ((lpi_status & BIT(1)) ||
		    ((lpi_status & BIT(2)) && (!(lpi_status & BIT(0))))) {
			/*
			 * If QDENY == true, or
			 * If QACTIVE == true && QACCEPT == false
			 * Try again
			 */
			__write_register(
			    device, CVP_AON_WRAPPER_CVP_NOC_LPI_CONTROL, 0x0);
			usleep_range(10, 20);
			__write_register(device,
					 CVP_NOC_CORE_ERR_ERRCLR_LOW_OFFS, 0x1);
			__write_register(
			    device, CVP_AON_WRAPPER_CVP_NOC_LPI_CONTROL, 0x1);
			usleep_range(1000, 1200);
			count++;
		} else {
			break;
		}
	}

	dprintk(CVP_PWR, "%s, CORE Noc: lpi_status %x (count %d)\n", __func__,
		lpi_status, count);
	if (count == max_count) {
		dprintk(CVP_WARN, "%s, CORE NOC not in qaccept status %x\n",
			__func__, lpi_status);

		/* Added for debug info purpose, not part of HPG */
		call_iris_op(device, print_sbm_regs, device);
	}

	/*
	 * NORDAU HPG §3.5.4 Step 4-5 (RENUMBERED from pakala §3.4.4 —
	 * "Power off CORE" moved from pakala §3.4.4 to NORDAU §3.5.4 by the
	 * +1 shift that NORDAU's new §3.1 introduces; bodies are byte-identical
	 * in both HPGs).
	 */
	count = 0;
	do {
		value =
		    __read_register(device, CVP_AON_WRAPPER_CVP_NOC_LPI_STATUS);
		if (value & 0x0)
			break;
		usleep_range(1000, 2000);
		count++;
	} while (count < 10);

	__write_register(device, CVP_AON_WRAPPER_CVP_NOC_LPI_CONTROL, 0x0);
}

static void __enter_video_ctl_noc_lpi(struct iris_hfi_device *device)
{
	u32 lpi_status, count = 0, max_count = 2000;

	/* [INH from cvp_pakala_hal.c:154-203] — NORDAU HPG §6.14 Steps 19-21
	 * (§6 numbering unshifted). No resource-name strings in this function.
	 */
	__write_register(device, CVP_AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_CONTROL,
			 0x1);
	while (count < max_count) {
		/* Reading the LPI status */
		lpi_status = __read_register(
		    device, CVP_AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_STATUS);
		if ((lpi_status & BIT(1)) ||
		    ((lpi_status & BIT(2)) && (!(lpi_status & BIT(0))))) {
			/*
			 * If QDENY == true, or
			 * If QACTIVE == true && QACCEPT == false
			 * Try again
			 */
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
	/* NORDAU HPG §6.14 Step-22 (§6 numbering unshifted) */
	__write_register(device, CVP_AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_CONTROL,
			 0x0);
	if (count == max_count) {
		dprintk(
		    CVP_WARN,
		    "%s, CVP_VIDEO_CTL NOC not in qaccept status %x\n",
		    __func__, lpi_status);

		/* Added for debug info purpose, not part of HPG */
		call_iris_op(device, print_sbm_regs, device);
	}
}

/*
 * Wakes the NoC (CPU, video-ctl, core) out of low-power-idle on power-ON,
 * mirroring cvp_kaanapali_hal.c's __noc_lpi_kaanapali (the closest match,
 * since kaanapali also implements all three helpers below; hawi only has
 * two). Wired up after a power-on external abort was traced to
 * __set_registers_nordau touching NOC registers while this was still an
 * empty stub, leaving the bus asleep. NORDAU's helpers take no `caller`
 * argument (unlike hawi/kaanapali), so it's unused here.
 */
static void __noc_lpi_nordau(struct iris_hfi_device *device,
					enum enter_noc_lpi_caller caller)
{
	__enter_cpu_noc_lpi(device);
	__enter_video_ctl_noc_lpi(device);
	__enter_core_noc_lpi(device);
}

static void setup_dsp_uc_memmap_vpu5_nordau(struct iris_hfi_device *device)
{
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
}

static void interrupt_init_iris2_nordau(struct iris_hfi_device *device)
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

static int __check_ctl_power_on_nordau(struct iris_hfi_device *device)
{
	u32 reg;

	reg = __read_register(device, CVP_CC_MVS0C_GDSCR);
	if (!(reg & 0x80000000))
		return -1;

	reg = __read_register(device, CVP_CC_MVS0C_CBCR);
	if (reg & 0x80000000)
		return -2;

	return 0;
}

static int __check_core_power_on_nordau(struct iris_hfi_device *device)
{
	u32 reg;

	reg = __read_register(device, CVP_CC_MVS0_GDSCR);
	if (!(reg & 0x80000000))
		return -1;

	reg = __read_register(device, CVP_CC_MVS0_CBCR);
	if (reg & 0x80000000)
		return -2;

	return 0;
}

static int __power_on_controller_nordau(struct iris_hfi_device *device)
{
	int rc = 0;

	/*
	 * NORDAU HPG §3.4.1 "Power up CVP_CTL" (NORD_EVA_4.6.html:747),
	 * RENUMBERED from pakala §3.3.1 — bodies byte-identical, so the STEP
	 * ORDER below is inherited [INH from cvp_pakala_hal.c:277-349]; every
	 * resource NAME is re-resolved against nordau-cvp.dtsi (a value, not a
	 * behaviour — CLAUDE.md Invariant 2 corollary).
	 */
	CVPKERNEL_ATRACE_BEGIN("__power_on_controller_v1");

	/* name: nordau-cvp.dtsi:15 power-domain-names[0] = "eva" (GenPD
	 * controller domain; was pakala "controller" — 9461931.diff:694)
	 */
	rc = __enable_gdsc(device, "eva");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable ctrler: %d\n", rc);
		return rc;
	}

	/* name: nordau-cvp.dtsi:28 clock-names[2] = "sleep"
	 * (was pakala "sleep_clk" — 9461931.diff:700)
	 */
	rc = msm_cvp_prepare_enable_clk(device, "sleep");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable sleep clk: %d\n", rc);
		goto fail_reset_sleep;
	}
	/*
	 *After adding AXI0C and FREERUN resets in DTSI, changing below code
	 *rc = call_iris_op(device, reset_control_assert_name, device,
	 *"cvp_axi_reset"); if (rc) dprintk(CVP_ERR, "%s: assert cvp_axi_reset
	 *failed\n", __func__);
	 *rc = call_iris_op(device, reset_control_assert_name, device,
	 *"cvp_core_reset"); if (rc) dprintk(CVP_ERR, "%s: assert cvp_core_reset
	 *failed\n", __func__);
	 *usleep_range(300, 400);
	 *rc = call_iris_op(device, reset_control_deassert_name, device,
	 *"cvp_axi_reset"); if (rc) dprintk(CVP_ERR, "%s: de-assert cvp_axi_reset
	 *failed\n", __func__); rc = call_iris_op(device,
	 *reset_control_deassert_name, device, "cvp_core_reset"); if (rc)
	 *   dprintk(CVP_ERR, "%s: de-assert cvp_core_reset failed\n", __func__);
	 */
	/* name: nordau-cvp.dtsi:27 clock-names[1] = "core_iface"
	 * (was pakala "core_axi_clock" — 9461931.diff:720)
	 */
	rc = msm_cvp_prepare_enable_clk(device, "core_iface");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable axi0 clk: %d\n", rc);
		goto fail_enable_axi0;
	}

	/* name: nordau-cvp.dtsi:26 clock-names[0] = "eva_iface"
	 * (was pakala "cvp_axi_clock" — 9461931.diff:726)
	 */
	rc = msm_cvp_prepare_enable_clk(device, "eva_iface");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable axi0c clk: %d\n", rc);
		goto fail_enable_axi0c;
	}

	/* name: nordau-cvp.dtsi:31 clock-names[5] = "eva0"
	 * (was pakala "cvp_clk" — 9461931.diff:732)
	 */
	rc = msm_cvp_prepare_enable_clk(device, "eva0");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable eva0: %d\n", rc);
		goto fail_enable_cvp;
	}

	/* name: nordau-cvp.dtsi:29 clock-names[3] = "eva_freerun"
	 * (was pakala "cvp_freerun_clk" — 9461931.diff:738)
	 */
	rc = msm_cvp_prepare_enable_clk(device, "eva_freerun");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable eva_freerun: %d\n", rc);
		goto fail_enable_freerun;
	}

	dprintk(CVP_PWR, "EVA controller powered on\n");
	CVPKERNEL_ATRACE_END("__power_on_controller_v1");
	return 0;

fail_enable_freerun:
	/*
	 * BUGFIX vs 9461931.diff:749 (and cvp_pakala_hal.c): the clock enabled
	 * at the preceding step is "eva0", not "cvp"/"cvp_clk". "cvp" is not in
	 * nordau-cvp.dtsi clock-names, so the original disable is a no-op leak.
	 */
	msm_cvp_disable_unprepare_clk(device, "eva0");
fail_enable_cvp:
	msm_cvp_disable_unprepare_clk(device, "eva_iface");
fail_enable_axi0c:
	msm_cvp_disable_unprepare_clk(device, "core_iface");
fail_enable_axi0:
	msm_cvp_disable_unprepare_clk(device, "sleep");
fail_reset_sleep:
	__disable_gdsc(device, "eva");
	CVPKERNEL_ATRACE_END("__power_on_controller_v1");
	return rc;
}

static int __power_on_core_nordau(struct iris_hfi_device *device)
{
	int rc = 0;

	/*
	 * NORDAU HPG §3.4.2 "Power up CVP_CORE (SW triggered)"
	 * (NORD_EVA_4.6.html:857), RENUMBERED from pakala §3.3.2.
	 * Step order [INH from cvp_pakala_hal.c:351-394].
	 *
	 * `changed` verdict honoured: pakala §3.3.2 drives CVP_EVA_CC_MVS1_GDSCR;
	 * NORDAU §3.4.2 drives MVS0 — "MVS1_GDSCR" appears 0 times in
	 * NORD_EVA_4.6.html and nordau_registers.csv exposes only MVS0C_*
	 * (controller) / MVS0_* (core). No MVS1 reference is inherited anywhere
	 * in this file.
	 *
	 * NOTE: NORDAU has 2 EVA cores but only 2 GDSC domains (MVS0C + MVS0), so
	 * this stays a SINGLE-domain function. Core count is not a domain count;
	 * the only per-core duplication is the §3.1 ECC mem-init below.
	 */
	CVPKERNEL_ATRACE_BEGIN("__power_on_core_v1");

	/* name: nordau-cvp.dtsi:15 power-domain-names[1] = "core" (GenPD core
	 * domain — the ONE core GDSC; NORDAU has no MVS1 domain)
	 */
	rc = __enable_gdsc(device, "core");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable core: %d\n", rc);
		return rc;
	}

	/* name: nordau-cvp.dtsi:32 clock-names[6] = "core0"
	 * (was pakala "core_clk" — 9461931.diff:774)
	 */
	rc = msm_cvp_prepare_enable_clk(device, "core0");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable core_clk: %d\n", rc);
		goto fail_enable_core;
	}

	/* name: nordau-cvp.dtsi:30 clock-names[4] = "core_freerun"
	 * (was pakala "core_freerun_clk" — 9461931.diff:780)
	 */
	rc = msm_cvp_prepare_enable_clk(device, "core_freerun");
	if (rc) {
		dprintk(CVP_ERR, "Failed to enable core_freerun_clk: %d\n", rc);
		goto fail_enable_freerun;
	}

	dprintk(CVP_PWR, "EVA core powered on\n");
	CVPKERNEL_ATRACE_END("__power_on_core_v1");

	return 0;

fail_enable_freerun:
	/*
	 * Unwind disables exactly what was enabled, once each: "core0" only here,
	 * then the "core" GDSC. Diverges from cvp_pakala_hal.c:387-393, which had
	 * a third rung (fail_enable_clk_src / "eva_cc_mvs0_clk_src"); that clock
	 * does not exist in nordau-cvp.dtsi, so both its enable and its disable
	 * are removed and no unreferenced label is left behind. "core0" is NOT
	 * double-disabled.
	 */
	msm_cvp_disable_unprepare_clk(device, "core0");
fail_enable_core:
	__disable_gdsc(device, "core");
	return rc;
}

static int __power_off_core_nordau(struct iris_hfi_device *device)
{
	u32 config, value = 0, count = 0;
	u32 max_count = 10;

	/*
	 * NORDAU HPG §3.5.4 "Power off CORE" (RENUMBERED from pakala §3.4.4) and
	 * §5 "Power Down Sequences" (numbering IDENTICAL in both HPGs).
	 * Step order [INH from cvp_pakala_hal.c:396-475].
	 *
	 * CAVEAT for reviewers: NORD_EVA_4.6.html's own §5 cross-references are
	 * STALE — they still say "refer to 3.4.4 / 3.6.4 / 3.7 / 3.3.1", which
	 * are pakala's pre-shift numbers. Citations here use NORDAU's ACTUAL
	 * section numbers, not those stale back-references.
	 *
	 * Single GDSC domain (MVS0), not per-core — see __power_on_core_nordau.
	 */
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
		/* names: nordau-cvp.dtsi:15 "core", :32 "core0" */
		__disable_gdsc(device, "core");
		msm_cvp_disable_unprepare_clk(device, "core0");
		return 0;
	} else if (!(value & 0x2) && msm_cvp_fw_low_power_mode) {
		/*
		 * HW_CONTROL PC disabled, then core is powered on for
		 * CVP NoC access
		 */
		__disable_gdsc(device, "core");
		msm_cvp_disable_unprepare_clk(device, "core0");
		msm_cvp_disable_unprepare_clk(device, "core_freerun");
		return 0;
	}

	dprintk(CVP_PWR, "Driver controls Core power off now\n");

	/* NORDAU HPG §3.5.4 step 1 (pakala §3.4.4) */
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
	 * MNoC idle check before collapsing the CORE GDSC domain.
	 * poll for NoC DMA idle -> NORDAU HPG §6.2.1 "Core power collapse
	 * sequence" (same number in NORD_EVA_4.6.html:1 TOC and in pakala; §6
	 * did NOT shift).
	 *
	 * `changed` verdict honoured: pakala's comment said "before collapsing
	 * MVS1" (its §3.3.2 drives CVP_EVA_CC_MVS1_GDSCR). NORDAU has NO MVS1
	 * domain at all — "MVS1_GDSCR" occurs 0 times in NORD_EVA_4.6.html and
	 * nordau_registers.csv exposes only MVS0C_* (controller) and MVS0_*
	 * (core). The domain collapsed here is MVS0 (CVP_CC_MVS0_GDSCR, read
	 * above), so the MVS1 wording is not carried over.
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
	/* put CORE NOC to low power */
	__enter_core_noc_lpi(device);

	/* NORDAU HPG §3.5.4 (pakala §3.4.4) */
	/* Reset both sides of 2 ahb2ahb_bridges (TZ and non-TZ) */
	__write_register(device, CVP_AHB_BRIDGE_SYNC_RESET, 0x3);
	__write_register(device, CVP_AHB_BRIDGE_SYNC_RESET, 0x2);
	__write_register(device, CVP_AHB_BRIDGE_SYNC_RESET, 0x0);
	__write_register(device, CVP_WRAPPER_CORE_CLOCK_CONFIG, config);

	/* NORDAU HPG §3.5.4 step 6-7 (pakala §3.4.4) */
	__disable_gdsc(device, "core");
	msm_cvp_disable_unprepare_clk(device, "core0");
	return 0;
}

static int __power_off_controller_nordau(struct iris_hfi_device *device)
{
	u32 lpi_status, count = 0, max_count = 1000;
	u32 lpi_control;
	int rc;

	/*
	 * NORDAU HPG §3.8 "Power down CVP_CTL (SW/FW)" — RENUMBERED from
	 * pakala §3.7; bodies byte-identical in both HPGs, so every "3.7" in the
	 * parent becomes "3.8" here. Step order
	 * [INH from cvp_pakala_hal.c:477-580].
	 */
	/* NORDAU HPG §3.8 Step 4 (was pakala §3.7 Step 4) */
	__write_register(device, CVP_CPU_CS_X2RPMh, 0x3);

	/* put CPU/Tensilica NOC to low power — NORDAU HPG §6.14 Steps 15-17 */
	__enter_cpu_noc_lpi(device);

	/* put CVP_VIDEO_CTL NOC to low power — NORDAU HPG §6.14 Steps 19-21 */
	__enter_video_ctl_noc_lpi(device);

	/* NORDAU HPG §3.8 step 11 (was pakala §3.7 step 11) */
	__write_register(device, CVP_WRAPPER_DEBUG_BRIDGE_LPI_CONTROL, 0x0);

	/* NORDAU HPG §3.8 step 12 (was pakala §3.7 step 12) */
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

	lpi_control =
	    __read_register(device, CVP_AON_WRAPPER_CVP_NOC_LPI_CONTROL);
	lpi_control = lpi_control | 0x10;
	__write_register(device, CVP_AON_WRAPPER_CVP_NOC_LPI_CONTROL,
			 lpi_control);
	usleep_range(50, 100);
	lpi_control = lpi_control & (~0x10);
	__write_register(device, CVP_AON_WRAPPER_CVP_NOC_LPI_CONTROL,
			 lpi_control);

	/*
	 * Below sequence are missing from HPG §3.8.
	 * It disables EVA_CC clks in power on sequence.
	 * Names resolved to nordau-cvp.dtsi clock-names (9461931.diff:931-946).
	 */
	rc = msm_cvp_disable_unprepare_clk(device, "core_freerun");
	if (rc)
		dprintk(CVP_ERR, "Failed to disable core_freerun: %d\n",
			rc);

	rc = msm_cvp_disable_unprepare_clk(device, "eva_freerun");
	if (rc)
		dprintk(CVP_ERR, "Failed to disable eva_freerun: %d\n", rc);

	rc = msm_cvp_disable_unprepare_clk(device, "eva0");
	if (rc)
		dprintk(CVP_ERR, "Failed to disable eva0: %d\n", rc);

	rc = msm_cvp_disable_unprepare_clk(device, "sleep");
	if (rc)
		dprintk(CVP_ERR, "Failed to disable sleep clk: %d\n", rc);

	/*
	 * NORDAU HPG §3.8 Step 13 and 14 — disable controller GDSC.
	 * BUGFIX vs 9461931.diff:949 (and cvp_pakala_hal.c): power-on enables
	 * the "eva" domain, so power-off must disable "eva", not the pre-QLI
	 * "controller" (which is absent from nordau-cvp.dtsi power-domain-names,
	 * making the original __disable_gdsc a silent no-op).
	 */
	__disable_gdsc(device, "eva");

	/* Below sequence are missing from HPG §3.8.
	 * It disables GCC clks in power on sequence
	 */
	rc = msm_cvp_disable_unprepare_clk(device, "core_iface");
	rc = msm_cvp_disable_unprepare_clk(device, "eva_iface");

	return 0;
}

static void __print_sidebandmanager_regs_nordau(struct iris_hfi_device *device)
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

	/* Added to debug CPU NoC hang */
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

static int __enable_hw_power_collapse_nordau(struct iris_hfi_device *device)
{
	int rc = 0, loop = 10;
	u32 reg_gdsc;

	if (!msm_cvp_fw_low_power_mode) {
		dprintk(CVP_PWR, "Not enabling hardware power collapse\n");
		return 0;
	}

	if (device->res->gdsc_framework_type)
		/* GenPD path: core power-domain name "core" (was pakala
		 * "core_pd"; corrected per 9461931.diff:1070)
		 */
		rc = switch_core_gdsc_mode(device, TO_HW_CTRL, "core");
	else
		rc = __hand_off_regulators(device);

	if (rc) {
		dprintk(CVP_WARN,
			"%s : Failed to enable HW power collapse %d\n",
			__func__, rc);
		return rc;
	}

	while (loop) {
		reg_gdsc = __read_register(device, CVP_CC_MVS0_GDSCR);
		if (reg_gdsc & 0x80000000) {
			usleep_range(100, 200);
			loop--;
		} else {
			break;
		}
	}

	if (!loop) {
		dprintk(CVP_ERR, "fail to power off CORE during resume\n");
		return -EINVAL;
	}

	return rc;
}

static int __set_registers_nordau(struct iris_hfi_device *device)
{
	struct msm_cvp_core *core;
	struct msm_cvp_platform_data *pdata;
	struct reg_set *reg_set;
	int i, val;

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

	/* NORDAU HPG §3.5 Programming security control registers — register
	 * writes inherited verbatim from cvp_pakala_hal.c (behavioural parent);
	 * all register macros resolve through cvp_nordau_io.h.
	 */
	__write_register(device, CVP_NOC_RCGCONTROLLER_HYSTERESIS_LOW, 0xff);
	__write_register(device, CVP_NOC_RCGCONTROLLER_WAKEUP_LOW, 0x7);
	__write_register(device, CVP_NOC_RCG_VNOC_NOC_CLK_FORCECLOCKON_LOW,
			 0x1);
	__write_register(device, CVP_NOC_RCG_VNOC_NOC_CLK_ENABLE_LOW, 0x1);
	__write_register(device, CVP_NOC_RCG_VNOC_NOC_CLK_FORCECLOCKON_LOW, 0x1);
	__write_register(device,
		CVP_NOC_RCG_VNOC_NOC_CLK_ENABLE_LOW + device->res->rcg_vnoc_clk_en_low, 0x1);
	usleep_range(5, 10);
	__write_register(device, CVP_NOC_RCG_VNOC_NOC_CLK_FORCECLOCKON_LOW,
			 0x0);
	__write_register(device, CVP_AON_WRAPPER_CVP_NOC_ARCG_CONTROL, 0x0);

	__write_register(device, CVP_CPU_CS_AXI4_QOS, pdata->noc_qos->axi_qos);
	__write_register(device, CVP_NOC_A_PRIORITYLUT_LOW,
			 pdata->noc_qos->prioritylut_low);
	__write_register(device, CVP_NOC_A_PRIORITYLUT_HIGH,
			 pdata->noc_qos->prioritylut_high);
	__write_register(device, CVP_NOC_A_URGENCY_LOW,
			 pdata->noc_qos->urgency_low);
	__write_register(device, CVP_NOC_A_DANGERLUT_LOW,
			 pdata->noc_qos->dangerlut_low);
	__write_register(device, CVP_NOC_A_SAFELUT_LOW,
			 pdata->noc_qos->safelut_low);
	__write_register(device, CVP_NOC_B_PRIORITYLUT_LOW,
			 pdata->noc_qos->prioritylut_low);
	__write_register(device, CVP_NOC_B_PRIORITYLUT_HIGH,
			 pdata->noc_qos->prioritylut_high);
	__write_register(device, CVP_NOC_B_URGENCY_LOW,
			 pdata->noc_qos->urgency_low);
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
	__write_register(device, CVP_NOC_A_NIU_ENCCTL_LOW, 0x1);
	__write_register(device, CVP_NOC_B_NIU_DECCTL_LOW, 0x1);
	__write_register(device, CVP_NOC_B_NIU_ENCCTL_LOW, 0x1);
	__write_register(device, CVP_NOC_CORE_ERR_MAINCTL_LOW_OFFS, 0x3);
	__write_register(device, CVP_NOC_MAIN_SIDEBANDMANAGER_FAULTINEN0_LOW,
			 0x1);

	return 0;
}

static void __print_reg_details_errlog3_low_nordau(u32 val)
{
	u32 mid, sid;

	/* [INH from cvp_pakala_hal.c:787-797] field extraction */
	mid = (val >> 7) & 0x1F;

	sid = (val >> 2) & 0x7;
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG3_LOW:     %#x\n", val);
	/*
	 * BUGFIX vs cvp_pakala_hal.c:795 and 9461931.diff:1480 (qli-patch-structure
	 * §11 #3): MID is a 5-bit field (0..31) but mid_names_nordau[] has only 25
	 * entries, so an unguarded index reads out of bounds for MID 25..31 and
	 * hands a garbage pointer to %s. Bound-check and fall back to the raw
	 * numeric MID. The array size stays 25 to remain faithful to the patch
	 * (see the WARNING in cvp_nordau_hal.h).
	 */
	if (mid < ARRAY_SIZE(mid_names_nordau))
		dprintk(CVP_ERR, "Sub-client:%s, SID: %d\n",
			mid_names_nordau[mid], sid);
	else
		dprintk(CVP_ERR, "Sub-client:unknown(MID %u), SID: %d\n", mid,
			sid);
}

static void __dump_noc_regs_nordau(struct iris_hfi_device *device)
{
#ifndef USE_PRESIL42
	u32 val = 0, config;
	struct regulator_info *rinfo;
	int rc = 0;

	if (msm_cvp_fw_low_power_mode) {
		if (device->res->gdsc_framework_type) {
			/* GenPD core domain name "core" (was pakala "core_pd") */
			rc = switch_core_gdsc_mode(device, TO_SW_CTRL, "core");
		} else {
			iris_hfi_for_each_regulator(device, rinfo) {
				/* legacy regulator path only (framework_type==0) */
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
	val = __read_register(device, CVP_CC_MVS0_GDSCR);
	dprintk(CVP_ERR, "%s, CVP_CC_MVS0_GDSCR: 0x%x", __func__, val);
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
	__print_reg_details_errlog3_low_nordau(val);
	val = __read_register(device, CVP_NOC_CORE_ERR_ERRLOG3_HIGH_OFFS);
	dprintk(CVP_ERR, "CVP_NOC_CORE_ERL_MAIN_ERRLOG3_HIGH 0x%x", val);
	__write_register(device, CVP_NOC_CORE_ERR_ERRCLR_LOW_OFFS, 0x1);

	if (msm_cvp_fw_low_power_mode) {
		if (device->res->gdsc_framework_type) {
			/* GenPD core domain name "core" (was pakala "core_pd") */
			rc = switch_core_gdsc_mode(device, TO_HW_CTRL, "core");
		} else {
			iris_hfi_for_each_regulator(device, rinfo) {
				/* legacy regulator path only (framework_type==0) */
				if (strcmp(rinfo->name, "cvp-core"))
					continue;
				rc = __hand_off_regulator(rinfo);
			}
		}
		if (rc)
			dprintk(
			    CVP_WARN,
			    "%s, Failed to hand off core gdsc control to HW\n",
			    __func__);
	}
	__write_register(device, CVP_WRAPPER_CORE_CLOCK_CONFIG, config);
#endif
}

static void __noc_error_info_iris2_nordau(struct iris_hfi_device *device)
{
	struct msm_cvp_core *core;
	struct cvp_noc_log *noc_log;
	u32 val = 0, regi, regiii;
	bool log_required = false;
	int rc;

	core = cvp_driver->cvp_core;

	if (core->resources.max_ssr_allowed >= 1)
		log_required = true;

	noc_log = &core->kmd_trace.kmd_debug_log.log->noc_log;

	if (noc_log->used) {
		dprintk(CVP_WARN, "Data already in NoC log, skip logging\n");
		return;
	}
	noc_log->used = 1;
	rc = 0;

	/* GenPD core power-domain name "core" (was pakala "core_pd"; :1327) */
	__disable_hw_power_collapse(device, "core");

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
	__print_reg_details_errlog3_low_nordau(val);
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

int set_nordau_hal_functions(void)
{
	hal_ops.interrupt_init = interrupt_init_iris2_nordau;
	hal_ops.setup_dsp_uc_memmap = setup_dsp_uc_memmap_vpu5_nordau;
	hal_ops.power_off_controller = __power_off_controller_nordau;
	hal_ops.power_off_core = __power_off_core_nordau;
	hal_ops.power_on_controller = __power_on_controller_nordau;
	hal_ops.power_on_core = __power_on_core_nordau;
	hal_ops.noc_error_info = __noc_error_info_iris2_nordau;
	hal_ops.check_ctl_power_on = __check_ctl_power_on_nordau;
	hal_ops.check_core_power_on = __check_core_power_on_nordau;
	hal_ops.print_sbm_regs = __print_sidebandmanager_regs_nordau;
	hal_ops.enable_hw_power_collapse = __enable_hw_power_collapse_nordau;
	hal_ops.set_registers = __set_registers_nordau;
	hal_ops.dump_noc_regs = __dump_noc_regs_nordau;
	hal_ops.check_tensilica_in_reset = __check_tensilica_in_reset_nordau;
	hal_ops.pm_qos_update = iris_pm_qos_update_nordau;
	hal_ops.noc_lpi = __noc_lpi_nordau;
	return 0;
}
