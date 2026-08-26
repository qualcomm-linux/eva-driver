/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_COMM_DEF_H_
#define _MSM_COMM_DEF_H_

#include <linux/types.h>

enum op_mode {
	OP_NORMAL,
	OP_DRAINING,
	OP_FLUSH,
	OP_INVALID,
};

enum queue_state {
	QUEUE_INIT,
	QUEUE_ACTIVE = 1,
	QUEUE_START,
	QUEUE_STOP,
	QUEUE_INVALID,
};

#define TEMP_WORKAROUND

#ifdef CONFIG_EVA_TVM

#else	/* LA target starts here */

#define DEFAULT_HAL_VER 1
#define KNP_HAL_VER 2
#define HAWI_HAL_VER 3

#ifdef CONFIG_EVA_CANOE
#define CVP_GENPD_ENABLED 1
#define CVP_OPP_ENABLED 1
#endif /* End of CONFIG_EVA_CANOE*/



#endif	/* End CONFIG_EVA_TVM */

#endif
