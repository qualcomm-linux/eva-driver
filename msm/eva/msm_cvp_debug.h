/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __MSM_CVP_DEBUG__
#define __MSM_CVP_DEBUG__
#include <linux/debugfs.h>
#include <linux/delay.h>
#include "msm_cvp_internal.h"
#include "msm_cvp_events.h"

#ifndef CVP_DBG_LABEL
#define CVP_DBG_LABEL "msm_cvp"
#endif

#define CVP_DBG_TAG CVP_DBG_LABEL ": %4s: "
#define CVP_PID_TAG "[%d,%d] " CVP_DBG_LABEL ": %4s: "

/* To enable messages OR these values and
 * echo the result to debugfs file.
 *
 * To enable all messages set debug_level = 0x101F
 */

enum cvp_msg_prio {
	CVP_ERR   = 0x000001,
	CVP_WARN  = 0x000002,
	CVP_INFO  = 0x000004,
	CVP_CMD   = 0x000008,
	CVP_PROF  = 0x000010,
	CVP_PKT   = 0x000020,
	CVP_MEM   = 0x000040,
	CVP_SYNX  = 0x000080,
	CVP_CORE  = 0x000100,
	CVP_REG   = 0x000200,
	CVP_PWR   = 0x000400,
	CVP_DSP   = 0x000800,
	CVP_FW    = 0x001000,
	CVP_SESS  = 0x002000,
	CVP_HFI   = 0x004000,
	CVP_VM    = 0x008000,
	CVP_TRACE = 0x010000,
	CVP_PERF  = 0x020000,
	CVP_DBG  = CVP_MEM | CVP_SYNX | CVP_CORE | CVP_REG | CVP_CMD |
		CVP_PWR | CVP_DSP | CVP_SESS | CVP_HFI | CVP_PKT | CVP_VM,
};

enum cvp_msg_out {
	CVP_OUT_PRINTK = 0,
};

enum msm_cvp_debugfs_event {
	MSM_CVP_DEBUGFS_EVENT_ETB,
	MSM_CVP_DEBUGFS_EVENT_EBD,
	MSM_CVP_DEBUGFS_EVENT_FTB,
	MSM_CVP_DEBUGFS_EVENT_FBD,
};

extern int msm_cvp_debug;
extern int msm_cvp_debug_out;
extern int msm_cvp_fw_debug;
extern int msm_cvp_fw_debug_mode;
extern int msm_cvp_fw_low_power_mode;
extern bool msm_cvp_fw_coverage;
extern bool msm_cvp_auto_pil;
extern bool msm_cvp_syscache_disable;

#define dprintk(__level, __fmt, arg...)	\
	do { \
		if (msm_cvp_debug & __level) { \
			if (msm_cvp_debug_out == CVP_OUT_PRINTK) { \
				if (__level == CVP_ERR || __level == CVP_WARN) { \
					pr_info(CVP_PID_TAG __fmt, \
						current->pid, current->tgid, \
						get_debug_level_str(__level), \
						## arg); \
				} \
				else { \
					pr_info(CVP_DBG_TAG __fmt, \
						get_debug_level_str(__level), \
						## arg); \
				} \
			} \
		} \
	} while (0)

/* dprintk_rl is designed for printing frequent recurring errors */
#define dprintk_rl(__level, __fmt, arg...)	\
	do { \
		if (msm_cvp_debug & __level) { \
			if (msm_cvp_debug_out == CVP_OUT_PRINTK) { \
				pr_info_ratelimited(CVP_DBG_TAG __fmt, \
					get_debug_level_str(__level),   \
					## arg); \
			} \
		} \
	} while (0)

#define MSM_CVP_ERROR(value)					\
	do {	if (value)					\
			dprintk(CVP_ERR, "WarnOn");		\
		WARN_ON(value);					\
	} while (0)


static inline char *get_debug_level_str(int level)
{
	switch (level) {
	case CVP_ERR:
		return "err";
	case CVP_WARN:
		return "warn";
	case CVP_INFO:
		return "info";
	case CVP_CMD:
		return "cmd";
	case CVP_DBG:
		return "dbg";
	case CVP_PROF:
		return "prof";
	case CVP_PKT:
		return "pkt";
	case CVP_MEM:
		return "mem";
	case CVP_SYNX:
		return "synx";
	case CVP_CORE:
		return "core";
	case CVP_REG:
		return "reg";
	case CVP_PWR:
		return "pwr";
	case CVP_DSP:
		return "dsp";
	case CVP_FW:
		return "fw";
	case CVP_SESS:
		return "sess";
	case CVP_HFI:
		return "hfi";
	case CVP_VM:
		return "vm";
	case CVP_PERF:
		return "perf";
	default:
		return "???";
	}
}

#endif
