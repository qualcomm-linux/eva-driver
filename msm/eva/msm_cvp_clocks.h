/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */


#ifndef _MSM_CVP_CLOCKS_H_
#define _MSM_CVP_CLOCKS_H_
#include "msm_cvp_internal.h"
#include "cvp_core_hfi.h"

int msm_cvp_set_fmax(struct msm_cvp_core *core);
int msm_cvp_set_clocks(struct msm_cvp_core *core);

int msm_cvp_opp_set_rate(struct iris_hfi_device *device, u64 freq);
#endif
int msm_cvp_scale_clocks(struct iris_hfi_device *device);
int msm_cvp_prepare_enable_clk(struct iris_hfi_device *device,
		const char *name);
int msm_cvp_disable_unprepare_clk(struct iris_hfi_device *device,
		const char *name);
int msm_cvp_init_clocks(struct iris_hfi_device *device);
void msm_cvp_deinit_clocks(struct iris_hfi_device *device);
int msm_cvp_set_bw(struct msm_cvp_core *core, struct bus_info *bus, unsigned long bw);
int cvp_set_bw(struct bus_info *bus, unsigned long bw);
