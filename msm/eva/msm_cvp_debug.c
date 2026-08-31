// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/debugfs.h>
#include "msm_cvp_debug.h"
#include "msm_cvp_common.h"
#include "cvp_core_hfi.h"
#include "cvp_hfi_api.h"

#define MAX_SSR_STRING_LEN 10

int msm_cvp_debug = CVP_ERR | CVP_WARN | CVP_FW;
EXPORT_SYMBOL(msm_cvp_debug);

int msm_cvp_debug_out = CVP_OUT_PRINTK;
EXPORT_SYMBOL(msm_cvp_debug_out);


int msm_cvp_fw_low_power_mode = 1;
bool msm_cvp_syscache_disable = !true;
bool msm_cvp_auto_pil = true;
int msm_cvp_fw_debug = 0x10018;
int msm_cvp_fw_debug_mode = 1;
bool msm_cvp_fw_coverage = !true;

static void put_inst_helper(struct kref *kref)
{
	struct msm_cvp_inst *inst;

	if (!kref)
		return;
	inst = container_of(kref, struct msm_cvp_inst, kref);

	msm_cvp_destroy(inst);
}