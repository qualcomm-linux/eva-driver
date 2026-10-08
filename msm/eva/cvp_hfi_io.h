/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "cvp_comm_def.h"

#ifdef CONFIG_EVA_PINEAPPLE
#include "target/cvp_lanai_io.h"
#endif

#ifdef CONFIG_EVA_SUN
#include "target/cvp_pakala_io.h"
#endif

#ifdef CONFIG_EVA_CANOE
#include "target/cvp_kaanapali_io.h"
#endif

#ifdef CONFIG_EVA_ART
#include "target/cvp_hawi_io.h"
#endif

#ifdef CONFIG_EVA_GLYMUR
#include "target/cvp_glymur_io.h"
#endif
