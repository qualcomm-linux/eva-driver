/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/* Nordau EVA register offsets. */

#ifndef __CVP_NORDAU_IO_H__
#define __CVP_NORDAU_IO_H__

#include <linux/io.h>

/*
 * --------------------------------------------------------------------------
 * §1 TOP registers
 * --------------------------------------------------------------------------
 */
/* addr-verified: nordau_modules.csv row
 * "EVA_CVP0_CVP0_CVP_TOP,0x0AB00000,8192"
 */
#define CVP_TOP_BASE_OFFS			0x00000000
/* source: nordau_registers.csv cvp0_cvp_top,CVP_SS_IDLE_STATUS,0x00000050 */
#define CVP_SS_IDLE_STATUS		(CVP_TOP_BASE_OFFS + 0x50)
/* source: nordau_registers.csv cvp0_cvp_top,CVP_SS_IRQ_MASK,0x00000004 */
#define CVP_SS_IRQ_MASK    (CVP_TOP_BASE_OFFS + 0x04)
/* source: nordau_bitfields.csv cvp0_cvp_top,CVP_SS_IRQ_MASK,WD_IRQ,8,8,0x100 */
#define CVP_SS_INTR_BMASK  (0x100)

/*
 * --------------------------------------------------------------------------
 * §2 CPU registers
 * --------------------------------------------------------------------------
 */
/* addr-verified: nordau_modules.csv row
 * "EVA_CVP0_CVP0_CVP_CPU_SS_CS,0x0ABA0000,4096"
 *
 * DISCREPANCY vs the onboarding INPUTS table (recorded per the "TRUST THE CSV"
 * instruction): that table listed CVP_CPU_BASE_OFFS = 0x00090000, mapped to the
 * SWI module EVA_CVP_CVP_CVP_CPU_WD @ 0x0AB90000. That module is the CPU
 * WATCHDOG (its registers are VPU_CPU_WD_WDOGLOAD/VALUE/CONTROL/INTCLR/RIS/MIS/
 * LOCK/ITCR only) — it is NOT the CPU_CS command/status block that every
 * CVP_CPU_CS_* macro here addresses. All of SCIACMD, SCIACMDARG0-3, SCIBCMD,
 * SCIBARG1-3, VMIMSG*, AXI4_QOS, X2RPMH and AHB_BRIDGE_SYNC_RESET live in
 * nordau_registers.csv module cvp0_cvp_cpu_ss_cs at absolute 0x0ABA0xxx, i.e.
 * EVA-relative 0x000A0000. Using 0x00090000 would point the whole HFI mailbox
 * at the watchdog. The CSV value is therefore used.
 * (It happens to equal pakala's 0x000A0000, but is derived from the CSV — the
 * INPUTS warning not to copy the parent's value still holds; the agreement is
 * coincidental, not inherited.)
 */
#define CVP_CPU_BASE_OFFS			0x000A0000
/* addr-verified: nordau_modules.csv row
 * "EVA_CVP_AON_CVP_AON_CVP_AON,0x0ABE0000,61440"
 */
#define CVP_AON_BASE_OFFS			0x000E0000

/* inferred: driver "A2H*SOFTINT*" <-> SWI "VPU_CPU_CS_X2H*SOFTINT*" at the
 * same offsets (A2H = Xtensa-to-Host = X2H). All four offsets are
 * source: nordau_registers.csv 0x000A0010/0014/0018/001C.
 */
#define CVP_CPU_CS_A2HSOFTINTEN		(CVP_CPU_BASE_OFFS + 0x10)
#define CVP_CPU_CS_A2HSOFTINTENCLR	(CVP_CPU_BASE_OFFS + 0x14)
#define CVP_CPU_CS_A2HSOFTINT		(CVP_CPU_BASE_OFFS + 0x18)
#define CVP_CPU_CS_A2HSOFTINTCLR	(CVP_CPU_BASE_OFFS + 0x1C)
/* source: nordau_registers.csv VPU_CPU_CS_VMIMSG..VMIMSGAG3, 0x000A0034-0044 */
#define CVP_CPU_CS_VMIMSG		(CVP_CPU_BASE_OFFS + 0x34)
#define CVP_CPU_CS_VMIMSGAG0		(CVP_CPU_BASE_OFFS + 0x38)
#define CVP_CPU_CS_VMIMSGAG1		(CVP_CPU_BASE_OFFS + 0x3C)
#define CVP_CPU_CS_VMIMSGAG2		(CVP_CPU_BASE_OFFS + 0x40)
#define CVP_CPU_CS_VMIMSGAG3		(CVP_CPU_BASE_OFFS + 0x44)
/* source: nordau_registers.csv VPU_CPU_CS_SCIACMD,0x000A0048 */
#define CVP_CPU_CS_SCIACMD			(CVP_CPU_BASE_OFFS + 0x48)
/* source: nordau_registers.csv VPU_CPU_CS_AXI4_QOS,0x000A013C (reset 0x99) */
#define CVP_CPU_CS_AXI4_QOS		(CVP_CPU_BASE_OFFS + 0x13C)
/* source: nordau_registers.csv VPU_CPU_CS_H2XSOFTINTEN,0x000A0148 */
#define CVP_CPU_CS_H2XSOFTINTEN		(CVP_CPU_BASE_OFFS + 0x148)

/* CVP_CTRL_STATUS */
/* source: nordau_registers.csv VPU_CPU_CS_SCIACMDARG0,0x000A004C */
#define CVP_CPU_CS_SCIACMDARG0		(CVP_CPU_BASE_OFFS + 0x4C)
/* verbatim from cvp_pakala_io.h — the HFI software protocol encoding of
 * SCIACMDARG0. nordau_bitfields.csv models the register as one flat
 * 31:0 field (SCIACMDARG0), so the sub-field split is firmware ABI, not
 * hardware; unchanged for NORDAU and not independently verified.
 */
#define CVP_CPU_CS_SCIACMDARG0_BMSK	0xff
#define CVP_CPU_CS_SCIACMDARG0_SHFT	0x0
#define CVP_CPU_CS_SCIACMDARG0_HFI_CTRL_ERROR_STATUS_BMSK	0xfe
#define CVP_CPU_CS_SCIACMDARG0_HFI_CTRL_ERROR_STATUS_SHFT	0x1
#define CVP_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_STATUS_BMSK	0x1
#define CVP_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_STATUS_SHFT	0x0
#define CVP_CPU_CS_SCIACMDARG0_HFI_CTRL_PC_READY           0x100

/* HFI_QTBL_INFO */
/* source: nordau_registers.csv VPU_CPU_CS_SCIACMDARG1,0x000A0050 */
#define CVP_CPU_CS_SCIACMDARG1		(CVP_CPU_BASE_OFFS + 0x50)

/* HFI_QTBL_ADDR */
/* source: nordau_registers.csv VPU_CPU_CS_SCIACMDARG2,0x000A0054 */
#define CVP_CPU_CS_SCIACMDARG2		(CVP_CPU_BASE_OFFS + 0x54)

/* HFI_VERSION_INFO */
/* source: nordau_registers.csv VPU_CPU_CS_SCIACMDARG3,0x000A0058 */
#define CVP_CPU_CS_SCIACMDARG3		(CVP_CPU_BASE_OFFS + 0x58)

/*TCSR_SOC_HW_VERSION */
/* source: nordau_registers.csv VPU_CPU_CS_SCIBARG3,0x000A006C
 * (driver keeps two names for the one register — see SCIBARG3 below)
 */
#define CVP_CPU_CS_SCIBCMDARG3		(CVP_CPU_BASE_OFFS + 0x6C)

/* CVP_SFR_ADDR */
/* source: nordau_registers.csv VPU_CPU_CS_SCIBCMD,0x000A005C */
#define CVP_CPU_CS_SCIBCMD		(CVP_CPU_BASE_OFFS + 0x5C)

/* CVP_MMAP_ADDR */
/* source: nordau_registers.csv VPU_CPU_CS_SCIBCMDARG0,0x000A0060 */
#define CVP_CPU_CS_SCIBCMDARG0		(CVP_CPU_BASE_OFFS + 0x60)

/* CVP_UC_REGION_ADDR */
/* source: nordau_registers.csv VPU_CPU_CS_SCIBARG1,0x000A0064 */
#define CVP_CPU_CS_SCIBARG1		(CVP_CPU_BASE_OFFS + 0x64)

/* CVP_UC_REGION_SIZE */
/* source: nordau_registers.csv VPU_CPU_CS_SCIBARG2,0x000A0068 */
#define CVP_CPU_CS_SCIBARG2		(CVP_CPU_BASE_OFFS + 0x68)

/* source: nordau_registers.csv VPU_CPU_CS_SCIBARG3,0x000A006C */
#define CVP_CPU_CS_SCIBARG3		(CVP_CPU_BASE_OFFS + 0x6C)

/* inferred: driver "H2A*SOFTINT*" <-> SWI "VPU_CPU_CS_H2X*SOFTINT*"
 * (H2A = Host-to-Xtensa = H2X). Offsets are
 * source: nordau_registers.csv 0x000A0148/014C/0150/0154.
 */
#define CVP_CPU_CS_H2ASOFTINTEN		(CVP_CPU_BASE_OFFS + 0x148)
#define CVP_CPU_CS_H2ASOFTINTENCLR	(CVP_CPU_BASE_OFFS + 0x14c)
#define CVP_CPU_CS_H2ASOFTINT		(CVP_CPU_BASE_OFFS + 0x150)
#define CVP_CPU_CS_H2ASOFTINTCLR	(CVP_CPU_BASE_OFFS + 0x154)

/* source: nordau_registers.csv VPU_CPU_CS_AHB_BRIDGE_SYNC_RESET,0x000A0160 */
#define CVP_AHB_BRIDGE_SYNC_RESET	(CVP_CPU_BASE_OFFS + 0x160)
/* source: nordau_registers.csv
 * VPU_CPU_CS_AHB_BRIDGE_SYNC_RESET_STATUS,0x000A0164 (access R)
 */
#define CVP_AHB_BRIDGE_SYNC_RESET_STATUS	(CVP_CPU_BASE_OFFS + 0x164)

/* FAL10 Feature Control */
/* source: nordau_registers.csv VPU_CPU_CS_X2RPMH,0x000A0168 */
#define CVP_CPU_CS_X2RPMh		(CVP_CPU_BASE_OFFS + 0x168)
/* source: nordau_bitfields.csv VPU_CPU_CS_X2RPMH,MASK0,0,0,0x1 */
#define CVP_CPU_CS_X2RPMh_MASK0_BMSK	0x1
#define CVP_CPU_CS_X2RPMh_MASK0_SHFT	0x0
/* source: nordau_bitfields.csv VPU_CPU_CS_X2RPMH,MASK1,1,1,0x2 */
#define CVP_CPU_CS_X2RPMh_MASK1_BMSK	0x2
#define CVP_CPU_CS_X2RPMh_MASK1_SHFT	0x1
/* source: nordau_bitfields.csv VPU_CPU_CS_X2RPMH,SW_OVERRIDE,2,2,0x4 */
#define CVP_CPU_CS_X2RPMh_SWOVERRIDE_BMSK	0x4
/* verbatim from cvp_pakala_io.h — pakala's SHFT of 0x3 disagrees with
 * nordau_bitfields.csv (SW_OVERRIDE is bit 2, so the shift should be 0x2).
 * Left unchanged because the driver only ever uses the _BMSK; correcting the
 * SHFT is a cross-target cleanup, not a NORDAU bringup change.
 * TODO(non-blocking): reconcile SWOVERRIDE_SHFT across all targets.
 */
#define CVP_CPU_CS_X2RPMh_SWOVERRIDE_SHFT	0x3

/* source: nordau_registers.csv VPU_CPU_CS_X2RPMH_STATUS,0x000A0170 */
#define CVP_CPU_CS_X2RPMh_STATUS	(CVP_CPU_BASE_OFFS + 0x170)

/*
 * --------------------------------------------------------------------------
 * §3 MODULE: cvp_wrapper
 * --------------------------------------------------------------------------
 */
/* addr-verified: nordau_modules.csv row
 * "EVA_CVP_CVP_CVP_VPU_WRAPPER_TOP,0x0ABB0000,8192"
 */
#define CVP_WRAPPER_BASE_OFFS		0x000B0000

/* source: nordau_registers.csv VPU_WRAPPER_HW_VERSION,0x000B0000 */
#define CVP_WRAPPER_HW_VERSION		(CVP_WRAPPER_BASE_OFFS + 0x00)
/* verbatim from cvp_pakala_io.h — nordau_bitfields.csv gives
 * MAJOR_VERSION as 31:28 (mask 0xF0000000); the driver deliberately uses
 * 0x78000000 so that bit 31 is excluded from the major-version compare.
 * Kept identical to every other target for HAL-version consistency.
 * TODO(non-blocking): confirm bit 31 semantics in the NORDAU EVA HPG.
 */
#define CVP_WRAPPER_HW_VERSION_MAJOR_VERSION_MASK  0x78000000
/* source: nordau_bitfields.csv VPU_WRAPPER_HW_VERSION,MAJOR_VERSION,31,28 */
#define CVP_WRAPPER_HW_VERSION_MAJOR_VERSION_SHIFT 28
/* source: nordau_bitfields.csv VPU_WRAPPER_HW_VERSION,MINOR_VERSION,27,16 */
#define CVP_WRAPPER_HW_VERSION_MINOR_VERSION_MASK  0xFFF0000
#define CVP_WRAPPER_HW_VERSION_MINOR_VERSION_SHIFT 16
/* source: nordau_bitfields.csv VPU_WRAPPER_HW_VERSION,STEP_VERSION,15,0 */
#define CVP_WRAPPER_HW_VERSION_STEP_VERSION_MASK   0xFFFF

/* source: nordau_registers.csv VPU_WRAPPER_INTR_STATUS,0x000B000C */
#define CVP_WRAPPER_INTR_STATUS	(CVP_WRAPPER_BASE_OFFS + 0x0C)
/* inferred: driver A2HWD <-> SWI CPUSS_WD_INTR (bit 3, 0x8)
 * source: nordau_bitfields.csv VPU_WRAPPER_INTR_STATUS,CPUSS_WD_INTR,3,3
 */
#define CVP_WRAPPER_INTR_STATUS_A2HWD_BMSK	0x8
/* inferred: driver A2H <-> SWI CPUSS_INTR (bit 2, 0x4)
 * source: nordau_bitfields.csv VPU_WRAPPER_INTR_STATUS,CPUSS_INTR,2,2
 */
#define CVP_WRAPPER_INTR_STATUS_A2H_BMSK	0x4

/* source: nordau_registers.csv VPU_WRAPPER_INTR_MASK,0x000B0010 */
#define CVP_WRAPPER_INTR_MASK		(CVP_WRAPPER_BASE_OFFS + 0x10)
#define CVP_FATAL_INTR_BMSK	(CVP_WRAPPER_INTR_MASK_CPU_NOC_BMSK | \
				CVP_WRAPPER_INTR_MASK_CORE_NOC_BMSK )
/* source: nordau_bitfields.csv VPU_WRAPPER_INTR_MASK,SS_CPU_NOC_INTR_MASK,6,6 */
#define CVP_WRAPPER_INTR_MASK_CPU_NOC_BMSK	0x40
/* inferred: driver CORE_NOC <-> SWI SS_NOC_INTR_MASK
 * source: nordau_bitfields.csv VPU_WRAPPER_INTR_MASK,SS_NOC_INTR_MASK,5,5
 */
#define CVP_WRAPPER_INTR_MASK_CORE_NOC_BMSK	0x20
/* inferred: driver A2HWD <-> SWI CPUSS_WD_INTR_MASK
 * source: nordau_bitfields.csv VPU_WRAPPER_INTR_MASK,CPUSS_WD_INTR_MASK,3,3
 */
#define CVP_WRAPPER_INTR_MASK_A2HWD_BMSK	0x8
/* inferred: driver A2HCPU <-> SWI CPUSS_INTR_MASK
 * source: nordau_bitfields.csv VPU_WRAPPER_INTR_MASK,CPUSS_INTR_MASK,2,2
 */
#define CVP_WRAPPER_INTR_MASK_A2HCPU_BMSK	0x4
#define CVP_WRAPPER_INTR_MASK_A2HCPU_SHFT	0x2

/* inferred: driver INTR_CLEAR <-> SWI VPU_WRAPPER_INTR_CLR
 * source: nordau_registers.csv VPU_WRAPPER_INTR_CLR,0x000B0014
 */
#define CVP_WRAPPER_INTR_CLEAR		(CVP_WRAPPER_BASE_OFFS + 0x14)

/*
 * --------------------------------------------------------------------------
 * §4 Wrapper TZ registers
 * --------------------------------------------------------------------------
 */
/* addr-verified: nordau_modules.csv row
 * "EVA_CVP_TSW_CVP_TSW_CVP_VPU_WRAPPER_TSW,0x0ABC0000,8192"
 * inferred: driver "TZ" (TrustZone) <-> SWI "TSW" (trusted software) — same
 * block, two naming conventions; see generation log §1.
 */
#define CVP_WRAPPER_TZ_BASE_OFFS		0x000C0000

/* source: nordau_registers.csv VPU_WRAPPER_TSW_CPU_CLOCK_CONFIG,0x000C0000 */
#define CVP_WRAPPER_TZ_CPU_CLOCK_CONFIG	(CVP_WRAPPER_TZ_BASE_OFFS)
/* verbatim from cvp_pakala_io.h — NORDAU's VPU_WRAPPER_INTR_CLR bit map
 * (nordau_bitfields.csv) has no A2HWD/A2H bits at all; bit 4 is RAM_SEC and
 * bit 2 is LBIST_DONE. These four constants are currently unreferenced by any
 * .c file, so they are carried over rather than deleted.
 * TODO(non-blocking): delete or re-derive from the NORDAU EVA HPG.
 */
#define CVP_WRAPPER_INTR_CLEAR_A2HWD_BMSK	0x10
#define CVP_WRAPPER_INTR_CLEAR_A2HWD_SHFT	0x4
#define CVP_WRAPPER_INTR_CLEAR_A2H_BMSK	0x4
#define CVP_WRAPPER_INTR_CLEAR_A2H_SHFT	0x2
/* source: nordau_registers.csv VPU_WRAPPER_TSW_CPU_STATUS,0x000C0010 */
#define CVP_WRAPPER_CPU_STATUS		(CVP_WRAPPER_TZ_BASE_OFFS + 0x10)
/* inferred: driver AXI_CLOCK_CONFIG <-> SWI TSW_CTL_AXI_CLOCK_CONFIG
 * source: nordau_registers.csv 0x000C0014
 */
#define CVP_WRAPPER_AXI_CLOCK_CONFIG	(CVP_WRAPPER_TZ_BASE_OFFS + 0x14)

/* TODO: NORDAU EVA HPG — no CPU CGC-disable register exists in the NORDAU
 * wrapper. nordau_registers.csv has no entry at wrapper+0x2010, and the
 * wrapper aperture is only 8 KiB (0x2000) per nordau_modules.csv, so pakala's
 * offset 0x2010 is provably outside the block. Unreferenced by any .c file.
 * Placeholder, see generation log section 4.
 */
#define CVP_WRAPPER_CPU_CGC_DIS		0xDEADBEEF

/* inferred: driver CPU_CLOCK_CONFIG <-> SWI VPU_WRAPPER_EVA_CTL_CORE_CLOCK_CONFIG
 * source: nordau_registers.csv 0x000B0050 (reset 0x1)
 */
#define CVP_WRAPPER_CPU_CLOCK_CONFIG	(CVP_WRAPPER_BASE_OFFS + 0x50)
/* source: nordau_registers.csv VPU_WRAPPER_DEBUG_BRIDGE_LPI_CONTROL,0x000B0054 */
#define CVP_WRAPPER_DEBUG_BRIDGE_LPI_CONTROL	(CVP_WRAPPER_BASE_OFFS + 0x54)
/* source: nordau_registers.csv VPU_WRAPPER_DEBUG_BRIDGE_LPI_STATUS,0x000B0058 */
#define CVP_WRAPPER_DEBUG_BRIDGE_LPI_STATUS	(CVP_WRAPPER_BASE_OFFS + 0x58)
/* inferred: driver CPU_NOC_LPI_* <-> SWI VPU_WRAPPER_EVA_CPU_NOC_LPI_*
 * source: nordau_registers.csv 0x000B005C / 0x000B0060
 */
#define CVP_WRAPPER_CPU_NOC_LPI_CONTROL		(CVP_WRAPPER_BASE_OFFS + 0x5C)
#define CVP_WRAPPER_CPU_NOC_LPI_STATUS		(CVP_WRAPPER_BASE_OFFS + 0x60)
/* source: nordau_registers.csv VPU_WRAPPER_SPARE_0,0x000B0078 */
#define CVP_WRAPPER_SPARE_0			(CVP_WRAPPER_BASE_OFFS + 0x78)
/* source: nordau_registers.csv VPU_WRAPPER_CORE_POWER_STATUS,0x000B0080 */
#define CVP_WRAPPER_CORE_POWER_STATUS  (CVP_WRAPPER_BASE_OFFS + 0x80)
/* source: nordau_registers.csv VPU_WRAPPER_CORE_POWER_CONTROL,0x000B0084 */
#define CVP_WRAPPER_CORE_POWER_CONTROL  (CVP_WRAPPER_BASE_OFFS + 0x84)
/* source: nordau_registers.csv VPU_WRAPPER_CORE_CLOCK_CONFIG,0x000B0088 */
#define CVP_WRAPPER_CORE_CLOCK_CONFIG		(CVP_WRAPPER_BASE_OFFS + 0x88)

/* source: nordau_registers.csv VPU_WRAPPER_GPIO_IN,0x000B0028 — this register
 * DOES exist on NORDAU (it was a 0xDEADBEEF placeholder in the previous
 * generation, before the register-level export was available).
 */
#define CVP_WRAPPER_GPIO_IN		(CVP_WRAPPER_BASE_OFFS + 0x28)
/* source: nordau_registers.csv VPU_WRAPPER_GPIO_OUT,0x000B002C */
#define CVP_WRAPPER_GPIO_OUT		(CVP_WRAPPER_BASE_OFFS + 0x2C)

/*
 * --------------------------------------------------------------------------
 * §10 Aliases — map generic HFI names to CPU_CS registers
 * --------------------------------------------------------------------------
 * verbatim from cvp_pakala_io.h — pure macro aliasing, no hardware value;
 * the aliased registers are individually document-sourced above.
 */
#define CVP_CTRL_INIT		CVP_CPU_CS_SCIACMD

#define CVP_CTRL_STATUS	CVP_CPU_CS_SCIACMDARG0
#define CVP_CTRL_INIT_STATUS__M \
		CVP_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_STATUS_BMSK
#define CVP_CTRL_ERROR_STATUS__M \
		CVP_CPU_CS_SCIACMDARG0_HFI_CTRL_ERROR_STATUS_BMSK
#define CVP_CTRL_INIT_IDLE_MSG_BMSK \
		CVP_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_IDLE_MSG_BMSK
#define CVP_CTRL_STATUS_PC_READY \
		CVP_CPU_CS_SCIACMDARG0_HFI_CTRL_PC_READY


#define CVP_QTBL_INFO		CVP_CPU_CS_SCIACMDARG1

#define CVP_QTBL_ADDR		CVP_CPU_CS_SCIACMDARG2

#define CVP_VERSION_INFO	CVP_CPU_CS_SCIACMDARG3

#define CVP_SFR_ADDR		CVP_CPU_CS_SCIBCMD
#define CVP_MMAP_ADDR		CVP_CPU_CS_SCIBCMDARG0
#define CVP_UC_REGION_ADDR	CVP_CPU_CS_SCIBARG1
#define CVP_UC_REGION_SIZE	CVP_CPU_CS_SCIBARG2

/* HFI_DSP_QTBL_ADDR
 * 31:3 - HFI_DSP_QTBL_ADDR
 *        4-byte aligned Address
 */
#define HFI_DSP_QTBL_ADDR	CVP_CPU_CS_VMIMSG

/* HFI_DSP_UC_REGION_ADDR
 * 31:20 - HFI_DSP_UC_REGION_ADDR
 *         1MB aligned address.
 *         Uncached Region start Address. This region covers
 *         HFI DSP QTable,
 *         HFI DSP Queue Headers,
 *         HFI DSP Queues,
 */
#define HFI_DSP_UC_REGION_ADDR	CVP_CPU_CS_VMIMSGAG0

/* HFI_DSP_UC_REGION_SIZE
 * 31:20 - HFI_DSP_UC_REGION_SIZE
 *         Multiples of 1MB.
 *         Size of the DSP_UC_REGION Uncached Region
 */
#define HFI_DSP_UC_REGION_SIZE	CVP_CPU_CS_VMIMSGAG1

 /*
  * --------------------------------------------------------------------------
  * MODULE: cvp_wrapper_xtss
  * --------------------------------------------------------------------------
  */
/* addr-verified: aliases the Wrapper TSW base 0x0ABC0000 (nordau_modules.csv) */
#define CVP_WRAPPER_XTS_OFFS		0x000C0000
/* source: nordau_registers.csv VPU_WRAPPER_TSW_XTSS_SW_RESET_RO,0x000C0020
 * (access R, reset 0x1 — matches the "in reset" check in the HAL)
 */
#define CVP_WRAPPER_XTSS_SW_RESET_RO	(CVP_WRAPPER_XTS_OFFS + 0x20)

/*
 * --------------------------------------------------------------------------
 * §6 MODULE: vcodec noc error log registers (CPU-side NOC)
 * --------------------------------------------------------------------------
 */
/* addr-verified: nordau_modules.csv row
 * "EVA_CVP_CVP_IRIS_CPU_B_NOC_UFLOW,0x0ABD0000,20608"
 * inferred: driver CVP_NOC_* (CPU side) <-> SWI IRIS_CPU_B_NOC_*
 */
#define CVP_NOC_BASE_OFFS		0x000D0000
/* source: nordau_registers.csv IRIS_CPU_B_NOC_ERRORLOGGER_* ,
 * 0x000D0000..0x000D003C (all thirteen offsets below)
 */
#define CVP_NOC_ERR_SWID_LOW_OFFS	(CVP_NOC_BASE_OFFS + 0x0)
#define CVP_NOC_ERR_SWID_HIGH_OFFS	(CVP_NOC_BASE_OFFS + 0x4)
#define CVP_NOC_ERR_MAINCTL_LOW_OFFS	(CVP_NOC_BASE_OFFS + 0x8)
#define CVP_NOC_ERR_ERRVLD_LOW_OFFS	(CVP_NOC_BASE_OFFS + 0x10)
#define CVP_NOC_ERR_ERRCLR_LOW_OFFS	(CVP_NOC_BASE_OFFS + 0x18)
#define CVP_NOC_ERR_ERRLOG0_LOW_OFFS	(CVP_NOC_BASE_OFFS + 0x20)
#define CVP_NOC_ERR_ERRLOG0_HIGH_OFFS	(CVP_NOC_BASE_OFFS + 0x24)
#define CVP_NOC_ERR_ERRLOG1_LOW_OFFS	(CVP_NOC_BASE_OFFS + 0x28)
#define CVP_NOC_ERR_ERRLOG1_HIGH_OFFS	(CVP_NOC_BASE_OFFS + 0x2C)
#define CVP_NOC_ERR_ERRLOG2_LOW_OFFS	(CVP_NOC_BASE_OFFS + 0x30)
#define CVP_NOC_ERR_ERRLOG2_HIGH_OFFS	(CVP_NOC_BASE_OFFS + 0x34)
#define CVP_NOC_ERR_ERRLOG3_LOW_OFFS	(CVP_NOC_BASE_OFFS + 0x38)
#define CVP_NOC_ERR_ERRLOG3_HIGH_OFFS	(CVP_NOC_BASE_OFFS + 0x3C)
/* source: nordau_registers.csv IRIS_CPU_B_NOC_SBM_FAULTINEN0_LOW,0x000D0240 */
#define CVP_NOC_SBM_FAULTINEN0_LOW      (CVP_NOC_BASE_OFFS + 0x240)
/* source: nordau_registers.csv IRIS_CPU_B_NOC_SBM_FAULTINSTATUS0_LOW,0x000D0248 */
#define CVP_NOC_SBM_FAULTINSTATUS0_LOW  (CVP_NOC_BASE_OFFS + 0x248)
/* inferred: driver SBM_SENSELN0 <-> SWI SBM_SENSEIN0 (SENSE-IN, driver typo)
 * source: nordau_registers.csv IRIS_CPU_B_NOC_SBM_SENSEIN0_LOW,0x000D0300
 */
#define CVP_NOC_SBM_SENSELN0_LOW	(CVP_NOC_BASE_OFFS + 0x300)

/*
 * --------------------------------------------------------------------------
 * §7 MODULE: NOC Core registers (VIDEO_B_NOC)
 * --------------------------------------------------------------------------
 * This block DIVERGES from cvp_pakala_io.h. pakala's video_b_noc had three
 * AMM_DMA_A/B/C channels; NORDAU has three differently-named channels and a
 * different internal layout. Every offset below is re-derived from
 * nordau_registers.csv (module cvp_noc_video_b_noc).
 *
 * CHANNEL MAPPING (name inference — see generation log section 1):
 *   driver "A" -> NORDAU DMA      (NIU 0x1A200, QoSgen 0x15008)
 *   driver "B" -> NORDAU PPU_XRA  (NIU 0x1A400, QoSgen 0x17008)
 *   driver "C" -> NORDAU CDM      (NIU 0x1A000, QoSgen 0x16008)
 * Rationale: the HAL writes UBWC NIU DECCTL/ENCCTL for channels A and B only,
 * and in nordau_registers.csv only the DMA and PPU_XRA NIUs expose
 * DECCTL/ENCCTL — the CDM NIU has none. So the two DECCTL/ENCCTL-capable
 * channels must be A and B, leaving CDM as C.
 * TODO(blocking for perf, not for bringup): confirm this A/B/C assignment and
 * the resulting DT reg-preset / QoS LUT ordering against the NORDAU EVA HPG
 * NOC chapter before trusting the QoS values.
 */
/* addr-verified: nordau_modules.csv row
 * "EVA_CVP_NOC_CVP_NOC_VIDEO_B_NOC,0x0AB10000,58880"
 * inferred: driver CVP_NOC_CORE_* <-> SWI VIDEO_B_NOC_*
 */
#define CVP_NOC_CORE_BASE_OFFS			0x00010000

/* --- NIU UBWC encode/decode control --- */
/* source: nordau_registers.csv VIDEO_B_NOC_DMA_NIU_DECCTL_LOW,0x0001A308 */
#define CVP_NOC_A_NIU_DECCTL_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA308)
/* source: nordau_registers.csv VIDEO_B_NOC_DMA_NIU_ENCCTL_LOW,0x0001A388 */
#define CVP_NOC_A_NIU_ENCCTL_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA388)
/* source: nordau_registers.csv VIDEO_B_NOC_PPU_XRA_NIU_DECCTL_LOW,0x0001A508 */
#define CVP_NOC_B_NIU_DECCTL_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA508)
/* source: nordau_registers.csv VIDEO_B_NOC_PPU_XRA_NIU_ENCCTL_LOW,0x0001A588 */
#define CVP_NOC_B_NIU_ENCCTL_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA588)

/* --- Core NOC sideband manager --- */
/* source: nordau_registers.csv
 * VIDEO_B_NOC_SIDEBANDMANAGER_MAIN_SIDEBANDMANAGER_FAULTINEN0_LOW,0x00010240
 */
#define CVP_NOC_MAIN_SIDEBANDMANAGER_FAULTINEN0_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0x240)
/* inferred: driver SENSELN* <-> SWI SENSEIN* (SENSE-IN; driver typo).
 * source: nordau_registers.csv
 * ..._SIDEBANDMANAGER_SENSEIN0_LOW/HIGH,SENSEIN1_LOW/HIGH,SENSEIN2_LOW at
 * 0x00010300/0304/0308/030C/0310
 */
#define CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN0_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0x300)
#define CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN0_HIGH \
		(CVP_NOC_CORE_BASE_OFFS + 0x304)
#define CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN1_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0x308)
#define CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN1_HIGH \
		(CVP_NOC_CORE_BASE_OFFS + 0x30C)
#define CVP_NOC_MAIN_SIDEBANDMANAGER_SENSELN2_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0x310)

/* --- Core NOC error logger --- */
/* source: nordau_registers.csv
 * VIDEO_B_NOC_ERL_ERRORLOGGER_MAIN_ERRORLOGGER_* , 0x00010000..0x0001003C
 */
#define CVP_NOC_CORE_ERR_SWID_LOW_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x000)
#define CVP_NOC_CORE_ERR_SWID_HIGH_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x004)
#define CVP_NOC_CORE_ERR_MAINCTL_LOW_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x008)
#define CVP_NOC_CORE_ERR_ERRVLD_LOW_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x010)
#define CVP_NOC_CORE_ERR_ERRCLR_LOW_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x018)
#define CVP_NOC_CORE_ERR_ERRLOG0_LOW_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x020)
#define CVP_NOC_CORE_ERR_ERRLOG0_HIGH_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x024)
#define CVP_NOC_CORE_ERR_ERRLOG1_LOW_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x028)
#define CVP_NOC_CORE_ERR_ERRLOG1_HIGH_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x02C)
#define CVP_NOC_CORE_ERR_ERRLOG2_LOW_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x030)
#define CVP_NOC_CORE_ERR_ERRLOG2_HIGH_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x034)
#define CVP_NOC_CORE_ERR_ERRLOG3_LOW_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x038)
#define CVP_NOC_CORE_ERR_ERRLOG3_HIGH_OFFS \
		(CVP_NOC_CORE_BASE_OFFS + 0x03C)

/* --- NOC RCG clock gating --- */
/* inferred: driver RCG_VNOC_NOC_CLK_FORCECLOCKON <-> SWI
 * ..._CSRUNIT_RCG_VNOCB_NOC_CLK_FORCECLKON_LOW (FORCECLOCKON = FORCECLKON)
 * source: nordau_registers.csv 0x0001E418
 */
#define CVP_NOC_RCG_VNOC_NOC_CLK_FORCECLOCKON_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xE418)
/* source: nordau_registers.csv
 * VIDEO_B_NOC_CSRUNIT_RCG_CHICKEN_BIT_VNOC_CSRUNIT_RCG_VNOCB_NOC_CLK_ENABLE_LOW,
 * 0x0001E428
 */
#define CVP_NOC_RCG_VNOC_NOC_CLK_ENABLE_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xE428)

/* --- NoC QoS registers --- */
/* source: nordau_registers.csv VIDEO_B_NOC_DMA_NIU_PRIORITYLUT_LOW/HIGH,
 * URGENCY_LOW, DANGERLUT_LOW, SAFELUT_LOW at
 * 0x0001A230/A234/A238/A240/A248  (driver channel A = NORDAU DMA)
 */
#define CVP_NOC_A_PRIORITYLUT_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA230)
#define CVP_NOC_A_PRIORITYLUT_HIGH \
		(CVP_NOC_CORE_BASE_OFFS + 0xA234)
#define CVP_NOC_A_URGENCY_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA238)
#define CVP_NOC_A_DANGERLUT_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA240)
#define CVP_NOC_A_SAFELUT_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA248)
/* source: nordau_registers.csv VIDEO_B_NOC_PPU_XRA_NIU_* at
 * 0x0001A430/A434/A438/A440/A448  (driver channel B = NORDAU PPU_XRA)
 */
#define CVP_NOC_B_PRIORITYLUT_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA430)
#define CVP_NOC_B_PRIORITYLUT_HIGH \
		(CVP_NOC_CORE_BASE_OFFS + 0xA434)
#define CVP_NOC_B_URGENCY_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA438)
#define CVP_NOC_B_DANGERLUT_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA440)
#define CVP_NOC_B_SAFELUT_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA448)
/* source: nordau_registers.csv VIDEO_B_NOC_CDM_NIU_* at
 * 0x0001A030/A034/A038/A040/A048  (driver channel C = NORDAU CDM)
 */
#define CVP_NOC_C_PRIORITYLUT_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA030)
#define CVP_NOC_C_PRIORITYLUT_HIGH \
		(CVP_NOC_CORE_BASE_OFFS + 0xA034)
#define CVP_NOC_C_URGENCY_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA038)
#define CVP_NOC_C_DANGERLUT_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA040)
#define CVP_NOC_C_SAFELUT_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xA048)

/* source: nordau_registers.csv VIDEO_B_NOC_DMA_QOSGEN_MAINCTL_LOW,0x00015008 */
#define CVP_VIDEO_B_NOC_A_QOSGEN_MAINCTL_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0x5008)
/* source: nordau_registers.csv VIDEO_B_NOC_PPU_XRA_QOSGEN_MAINCTL_LOW,0x00017008 */
#define CVP_VIDEO_B_NOC_B_QOSGEN_MAINCTL_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0x7008)
/* source: nordau_registers.csv VIDEO_B_NOC_CDM_QOSGEN_MAINCTL_LOW,0x00016008 */
#define CVP_VIDEO_B_NOC_C_QOSGEN_MAINCTL_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0x6008)
/* End of NoC Qos */

/* --- NOC RCG controller --- */
/* TODO: NORDAU EVA HPG NOC chapter — the NORDAU
 * VIDEO_B_NOC_VNOCB_NOC_CLK_RCGCONTROLLER instance exposes only SWID_LOW/HIGH,
 * HYSTERESIS_LOW, WAKEUP_LOW and NANOSTATUS_LOW in nordau_registers.csv; there
 * is no MAINCTL_LOW. Unreferenced by cvp_nordau_hal.c.
 * Placeholder, see generation log section 4.
 */
#define CVP_NOC_RCGCONTROLLER_MAINCTL_LOW	0xDEADBEEF
/* source: nordau_registers.csv
 * VIDEO_B_NOC_VNOCB_NOC_CLK_RCGCONTROLLER_HYSTERESIS_LOW,0x0001E010
 */
#define CVP_NOC_RCGCONTROLLER_HYSTERESIS_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xE010)
/* source: nordau_registers.csv
 * VIDEO_B_NOC_VNOCB_NOC_CLK_RCGCONTROLLER_WAKEUP_LOW,0x0001E018
 */
#define CVP_NOC_RCGCONTROLLER_WAKEUP_LOW \
		(CVP_NOC_CORE_BASE_OFFS + 0xE018)

/* --- NOC reset (separate SWI module, addressed off the NOC core base) --- */
/* addr-verified + source: nordau_modules.csv row
 * "EVA_AON_CVP_NOC_RESET,0x0AB1F000,4096"; nordau_registers.csv
 * aon_cvp_noc_reset,AON_WRAPPER_CVP_NOC_RESET_REQ,0x0001F000
 */
#define CVP_NOC_RESET_REQ \
		(CVP_NOC_CORE_BASE_OFFS + 0xf000)
/* source: nordau_registers.csv
 * aon_cvp_noc_reset,AON_WRAPPER_CVP_NOC_RESET_ACK,0x0001F004
 */
#define CVP_NOC_RESET_ACK \
		(CVP_NOC_CORE_BASE_OFFS + 0xf004)
/* TODO: NORDAU EVA HPG — the aon_cvp_noc_reset module contains only
 * RESET_REQ (0x1F000), RESET_ACK (0x1F004) and FUSA_STATUS_REGISTER (0x1F010)
 * in nordau_registers.csv; there is no SYNCRST register at 0x1F008.
 * Unreferenced by cvp_nordau_hal.c (only hawi/kaanapali use it).
 * Placeholder, see generation log section 4.
 */
#define CVP_NOC_RESET_SYNCRST	0xDEADBEEF

/*
 * --------------------------------------------------------------------------
 * §5 MODULE: AON registers (core NOC LPI, video CTL NOC LPI, ARCG)
 * --------------------------------------------------------------------------
 */
/* addr-verified base declared above with the CPU section (CVP_AON @ 0x0ABE0000) */
/* source: nordau_registers.csv AON_WRAPPER_CVP_NOC_LPI_CONTROL,0x000E0008 */
#define CVP_AON_WRAPPER_CVP_NOC_LPI_CONTROL	(CVP_AON_BASE_OFFS + 0x8)
/* source: nordau_registers.csv AON_WRAPPER_CVP_NOC_LPI_STATUS,0x000E000C */
#define CVP_AON_WRAPPER_CVP_NOC_LPI_STATUS	(CVP_AON_BASE_OFFS + 0xC)
/* source: nordau_registers.csv AON_WRAPPER_CVP_NOC_ARCG_CONTROL,0x000E0014
 * (reset 0x1 — the HAL clears it to 0x0 in __set_registers)
 */
#define CVP_AON_WRAPPER_CVP_NOC_ARCG_CONTROL	(CVP_AON_BASE_OFFS + 0x14)
/* source: nordau_registers.csv AON_WRAPPER_CVP_NOC_CORE_CLK_CONTROL,0x000E0024 */
#define CVP_AON_WRAPPER_CVP_NOC_CORE_CLK_CONTROL	(CVP_AON_BASE_OFFS + 0x24)
/* source: nordau_registers.csv AON_WRAPPER_CVP_NOC_CORE_SW_RESET,0x000E001C */
#define CVP_AON_WRAPPER_CVP_NOC_CORE_SW_RESET	(CVP_AON_BASE_OFFS + 0x1C)
/* source: nordau_registers.csv
 * AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_CONTROL,0x000E002C
 */
#define CVP_AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_CONTROL	(CVP_AON_BASE_OFFS + 0x2C)
/* source: nordau_registers.csv
 * AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_STATUS,0x000E0030
 */
#define CVP_AON_WRAPPER_CVP_VIDEO_CTL_NOC_LPI_STATUS	(CVP_AON_BASE_OFFS + 0x30)

/*
 * --------------------------------------------------------------------------
 * §8 MODULE: EVA CC registers
 * --------------------------------------------------------------------------
 * NOTE: NORDAU's EVA_CC register layout DIVERGES from pakala's — every GDSCR
 * and CBCR offset below moved. All are now taken from nordau_registers.csv
 * (module eva_cc_eva_cc_reg).
 */
/* addr-verified: nordau_modules.csv row
 * "EVA_CVP_EVA_CC_EVA_CC_EVA_CC_REG,0x0ABF8000,32768"
 */
#define CVP_CC_BASE_OFFS			0x000F8000
/* source: nordau_registers.csv MVS0C_GDSCR,0x000F804C (reset 0x00226801) */
#define CVP_CC_MVS0C_GDSCR			(CVP_CC_BASE_OFFS + 0x4C)
/* source: nordau_registers.csv MVS0C_CBCR,0x000F8064 (reset 0x88000000) */
#define CVP_CC_MVS0C_CBCR			(CVP_CC_BASE_OFFS + 0x64)
/* source: nordau_registers.csv MVS0_GDSCR,0x000F807C (reset 0x00226801) */
#define CVP_CC_MVS0_GDSCR			(CVP_CC_BASE_OFFS + 0x7C)
/* source: nordau_registers.csv MVS0_CBCR,0x000F8090 (reset 0x88000000) */
#define CVP_CC_MVS0_CBCR			(CVP_CC_BASE_OFFS + 0x90)
/* source: nordau_registers.csv AHB_CBCR,0x000F80B4 */
#define CVP_CC_AHB_CBCR				(CVP_CC_BASE_OFFS + 0xB4)
/* source: nordau_registers.csv SLEEP_CBCR,0x000F8114 */
#define CVP_CC_SLEEP_CBCR			(CVP_CC_BASE_OFFS + 0x114)
/* source: nordau_registers.csv SPARE1,0x000F9F24 (unchanged from pakala) */
#define CVP_CC_SPARE1				(CVP_CC_BASE_OFFS + 0x1F24)
/* source: nordau_registers.csv MVS0_FREERUN_CBCR,0x000F80A0 */
#define CVP_CC_MVS0_FREERUN_CBCR		(CVP_CC_BASE_OFFS + 0xA0)
/* source: nordau_registers.csv MVS0C_FREERUN_CBCR,0x000F8074 */
#define CVP_CC_MVS0C_FREERUN_CBCR		(CVP_CC_BASE_OFFS + 0x74)
/* source: nordau_registers.csv XO_CBCR,0x000F80F0 — exists on NORDAU (was a
 * 0xDEADBEEF placeholder before the register-level export).
 */
#define CVP_CC_XO_CBCR				(CVP_CC_BASE_OFFS + 0xF0)
/* source: nordau_registers.csv MVS0_CFG_GDSCR,0x000F8080 */
#define CVP_EVA_CC_MVS0_CFG_GDSCR		(CVP_CC_BASE_OFFS + 0x80)
/* inferred: driver MVS0C_DEBUG_CBCR <-> SWI DEBUG_CBCR. NORDAU has a single
 * un-scoped DEBUG_CBCR, not an MVS0C-scoped one, so the name equivalence is a
 * convention guess at a document-sourced address.
 * source: nordau_registers.csv DEBUG_CBCR,0x000F80BC
 */
#define CVP_CC_MVS0C_DEBUG_CBCR			(CVP_CC_BASE_OFFS + 0xBC)

/* --- GDSC CFG variants and hypervisor GDSC shadows ---
 * Not referenced by the current HAL, but exported here so the NORDAU-only
 * power-sequence work (and any GDSC-mode debugging) has document-sourced
 * addresses rather than magic numbers.
 *
 * NOTE: NORDAU has NO MVS1 GDSC domain — eva_cc_eva_cc_reg contains only the
 * MVS0C (controller) and MVS0 (core) domains, so no MVS1 macro is emitted.
 */
/* source: nordau_registers.csv MVS0C_CFG_GDSCR,0x000F8050 */
#define CVP_CC_MVS0C_CFG_GDSCR			(CVP_CC_BASE_OFFS + 0x50)
/* source: nordau_registers.csv MVS0C_CFG2_GDSCR,0x000F8054 */
#define CVP_CC_MVS0C_CFG2_GDSCR			(CVP_CC_BASE_OFFS + 0x54)
/* source: nordau_registers.csv MVS0C_CFG3_GDSCR,0x000F8058 */
#define CVP_CC_MVS0C_CFG3_GDSCR			(CVP_CC_BASE_OFFS + 0x58)
/* source: nordau_registers.csv MVS0C_CFG4_GDSCR,0x000F805C */
#define CVP_CC_MVS0C_CFG4_GDSCR			(CVP_CC_BASE_OFFS + 0x5C)
/* source: nordau_registers.csv MVS0_CFG_GDSCR,0x000F8080
 * (same register as CVP_EVA_CC_MVS0_CFG_GDSCR above; both driver spellings kept)
 */
#define CVP_CC_MVS0_CFG_GDSCR			(CVP_CC_BASE_OFFS + 0x80)
/* source: nordau_registers.csv MVS0_CFG2_GDSCR,0x000F8084 */
#define CVP_CC_MVS0_CFG2_GDSCR			(CVP_CC_BASE_OFFS + 0x84)
/* source: nordau_registers.csv MVS0_CFG3_GDSCR,0x000F8088 */
#define CVP_CC_MVS0_CFG3_GDSCR			(CVP_CC_BASE_OFFS + 0x88)
/* source: nordau_registers.csv MVS0_CFG4_GDSCR,0x000F808C */
#define CVP_CC_MVS0_CFG4_GDSCR			(CVP_CC_BASE_OFFS + 0x8C)
/* source: nordau_registers.csv HYP_MVS0_SGDSCR,0x000F9000 (reset 0x00000006) */
#define CVP_CC_HYP_MVS0_SGDSCR			(CVP_CC_BASE_OFFS + 0x1000)
/* source: nordau_registers.csv HYP_MVS0C_SGDSCR,0x000F9004 (reset 0x00000006) */
#define CVP_CC_HYP_MVS0C_SGDSCR			(CVP_CC_BASE_OFFS + 0x1004)

/*
 * --------------------------------------------------------------------------
 * §8.1 ECC AUTO_MEM_INIT registers — NORDAU-ONLY memory-init sequence
 * --------------------------------------------------------------------------
 * The NORDAU HPG adds a "Memory initialization sequence" that the behavioural
 * parent (pakala) does not have, so NOTHING here is inherited — these offsets
 * exist so cvp_nordau_hal.c can table-drive __mem_init_nordau() instead of
 * hard-coding addresses.
 *
 * Every offset is source: nordau_registers.csv (18 AUTO_MEM_INIT rows total).
 * On every block AUTO_MEM_INIT is RW with reset 0x00000005 and
 * AUTO_MEM_INIT_STATUS is R with reset 0x00000000.
 *
 * Per-core stride is +0x20000 for FDU/MPU/DMA/PPU, confirmed by comparing the
 * core0 and core1 rows rather than assumed (e.g. FDU 0x0000A08C -> 0x0002A08C).
 * XRA has only a core0 instance in the export — there is no XRA1.
 */
/* derived: core0 -> core1 delta, from the CSV row pairs listed below */
#define CVP_ECC_CORE_STRIDE			0x20000

/* --- core 0 --- */
/* source: nordau_registers.csv
 * cvp_fdu0_ecc_cvp_fdu_ecc,FDU0_ECC_AUTO_MEM_INIT,0x0000A08C
 */
#define CVP_FDU0_ECC_AUTO_MEM_INIT		0x0000A08C
/* source: nordau_registers.csv FDU0_ECC_AUTO_MEM_INIT_STATUS,0x0000A04C */
#define CVP_FDU0_ECC_AUTO_MEM_INIT_STATUS	0x0000A04C
/* source: nordau_registers.csv
 * cvp_mpu0_ecc_cvp_mpu_ecc,MPU0_ECC_AUTO_MEM_INIT,0x0000A88C
 */
#define CVP_MPU0_ECC_AUTO_MEM_INIT		0x0000A88C
/* source: nordau_registers.csv MPU0_ECC_AUTO_MEM_INIT_STATUS,0x0000A84C */
#define CVP_MPU0_ECC_AUTO_MEM_INIT_STATUS	0x0000A84C
/* source: nordau_registers.csv
 * cvp_dma0_ecc_cvp_dma_ecc,DMA0_ECC_AUTO_MEM_INIT,0x0000C88C
 */
#define CVP_DMA0_ECC_AUTO_MEM_INIT		0x0000C88C
/* source: nordau_registers.csv DMA0_ECC_AUTO_MEM_INIT_STATUS,0x0000C84C */
#define CVP_DMA0_ECC_AUTO_MEM_INIT_STATUS	0x0000C84C
/* source: nordau_registers.csv
 * cvp_ppu0_ecc_cvp_ppu_ecc,PPU0_ECC_AUTO_MEM_INIT,0x000550A8
 * NOTE the irregular init/status spacing on PPU (0xA8 / 0x88, not 0x8C / 0x4C).
 */
#define CVP_PPU0_ECC_AUTO_MEM_INIT		0x000550A8
/* source: nordau_registers.csv PPU0_ECC_AUTO_MEM_INIT_STATUS,0x00055088 */
#define CVP_PPU0_ECC_AUTO_MEM_INIT_STATUS	0x00055088
/* source: nordau_registers.csv
 * cvp_xra0_ecc_cvp_xra_ecc,XRA0_ECC_AUTO_MEM_INIT,0x0006608C
 * XRA is single-instance: present on core0 only.
 */
#define CVP_XRA0_ECC_AUTO_MEM_INIT		0x0006608C
/* source: nordau_registers.csv XRA0_ECC_AUTO_MEM_INIT_STATUS,0x0006604C */
#define CVP_XRA0_ECC_AUTO_MEM_INIT_STATUS	0x0006604C

/* --- core 1 (each row independently verified, not computed from the stride) --- */
/* source: nordau_registers.csv cvp_fdu1_ecc_cvp_fdu_ecc,...AUTO_MEM_INIT,0x0002A08C */
#define CVP_FDU1_ECC_AUTO_MEM_INIT		0x0002A08C
/* source: nordau_registers.csv cvp_fdu1_ecc...AUTO_MEM_INIT_STATUS,0x0002A04C */
#define CVP_FDU1_ECC_AUTO_MEM_INIT_STATUS	0x0002A04C
/* source: nordau_registers.csv cvp_mpu1_ecc_cvp_mpu_ecc,...AUTO_MEM_INIT,0x0002A88C */
#define CVP_MPU1_ECC_AUTO_MEM_INIT		0x0002A88C
/* source: nordau_registers.csv cvp_mpu1_ecc...AUTO_MEM_INIT_STATUS,0x0002A84C */
#define CVP_MPU1_ECC_AUTO_MEM_INIT_STATUS	0x0002A84C
/* source: nordau_registers.csv cvp_dma1_ecc_cvp_dma_ecc,...AUTO_MEM_INIT,0x0002C88C */
#define CVP_DMA1_ECC_AUTO_MEM_INIT		0x0002C88C
/* source: nordau_registers.csv cvp_dma1_ecc...AUTO_MEM_INIT_STATUS,0x0002C84C */
#define CVP_DMA1_ECC_AUTO_MEM_INIT_STATUS	0x0002C84C
/* source: nordau_registers.csv cvp_ppu1_ecc_cvp_ppu_ecc,...AUTO_MEM_INIT,0x000750A8 */
#define CVP_PPU1_ECC_AUTO_MEM_INIT		0x000750A8
/* source: nordau_registers.csv cvp_ppu1_ecc...AUTO_MEM_INIT_STATUS,0x00075088 */
#define CVP_PPU1_ECC_AUTO_MEM_INIT_STATUS	0x00075088

/*
 * --------------------------------------------------------------------------
 * §9 MODULE: GCC registers (absolute addresses)
 * --------------------------------------------------------------------------
 * verbatim from cvp_pakala_io.h — the 0x8F0xx GCC branch window lies OUTSIDE
 * the EVA aperture, so neither nordau_registers.csv nor nordau_modules.csv
 * (both EVA-only exports) can confirm or contradict these. Read through
 * __read_gcc_register(), i.e. via the separate "qcom,gcc-reg" mapping.
 * TODO(non-blocking): verify against the NORDAU GCC SWI /
 * dt-bindings/clock/qcom,nordau-gcc.h.
 */
/*Added to debug HW Hang*/
#define CVP_GCC_EVA_AHB_CBCR			(0x8F004)
#define CVP_GCC_EVA_AXI0_CBCR			(0x8F008)
#define CVP_GCC_EVA_AXI0C_CBCR			(0x8F018)
#define CVP_GCC_EVA_XO_CBCR			(0x8F01C)

/*
 * verbatim from cvp_pakala_io.h — the TCSR window is outside the EVA aperture
 * and absent from both NORDAU EVA exports; read through
 * __read_tcsr_register().
 * TODO(non-blocking): verify TCSR base/size and the SOC_HW_VERSION offset
 * against the NORDAU TCSR SWI.
 */
#define TCSR_REG_BASE					0x01F40000
#define TCSR_REG_SIZE					0xC0000

#define TCSR_SOW_HW_VERSION				0x88000

/*
 * --------------------------------------------------------------------------
 * §11 Placeholders — registers that genuinely do not exist on NORDAU.
 * Each is confirmed ABSENT from nordau_registers.csv (a 3607-register export
 * of the whole EVA aperture), so this is "no such register", not "no source".
 * Kept 0xDEADBEEF so the shared HAL/HFI code stays platform independent.
 * See generation log section 4.
 * --------------------------------------------------------------------------
 */
/* TODO: NORDAU EVA HPG — no AXI0_CX_INT / MM_INT GDSC in eva_cc_eva_cc_reg;
 * NORDAU's EVA_CC has only the MVS0 and MVS0C power domains. hawi-only.
 */
#define CVP_CC_AXI0_CX_INT_GDSCR	0xDEADBEEF
#define CVP_CC_MM_INT_GDSCR		0xDEADBEEF
/* TODO: NORDAU EVA HPG — the core/CX NOC LPI handshake lives in the AON block
 * on NORDAU (see CVP_AON_WRAPPER_CVP_NOC_LPI_* in section 5), not in the
 * wrapper. No CVP_NOC_LPI or CVP_NOC_CX_LPI register exists in
 * cvp_cvp_vpu_wrapper_top. hawi-only.
 */
#define CVP_WRAPPER_CVP_NOC_LPI_CONTROL		0xDEADBEEF
#define CVP_WRAPPER_CVP_NOC_LPI_STATUS		0xDEADBEEF
#define CVP_WRAPPER_CVP_NOC_CX_LPI_CONTROL	0xDEADBEEF
#define CVP_WRAPPER_CVP_NOC_CX_LPI_STATUS	0xDEADBEEF
/* TODO: NORDAU EVA HPG — no debug-chain XO branch in eva_cc_eva_cc_reg.
 * hawi-only.
 */
#define CVP_CC_DBGCH_XO_CBCR			0xDEADBEEF
/* TODO: NORDAU EVA HPG — NORDAU has MVS0C_FREERUN_CBCR (0x000F8074) but no
 * separate MVS0C_CTL freerun branch. hawi-only.
 */
#define CVP_CC_CC_MVS0C_CTL_FREERUN_CBCR	0xDEADBEEF

#endif
