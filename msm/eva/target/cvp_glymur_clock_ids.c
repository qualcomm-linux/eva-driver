// SPDX-License-Identifier: GPL-2.0-only

#include <dt-bindings/clock/qcom,glymur-evacc.h>
#include <dt-bindings/clock/qcom,glymur-gcc.h>

#include "cvp_clock_ids.h"

/* Clock order must match the Glymur CVP device-tree node. */
const u32 cvp_glymur_clock_ids[CVP_GLYMUR_CLOCK_IDS_COUNT] = {
	GCC_EVA_AXI0C_CLK,
	EVA_CC_SLEEP_CLK,
	EVA_CC_MVS0C_FREERUN_CLK,
	EVA_CC_MVS0C_CLK,
	GCC_EVA_AXI0_CLK,
	EVA_CC_MVS0_FREERUN_CLK,
	EVA_CC_MVS0_CLK,
};
