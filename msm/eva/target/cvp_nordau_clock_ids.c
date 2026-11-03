// SPDX-License-Identifier: GPL-2.0-only

#include <dt-bindings/clock/qcom,nord-evacc.h>
#include <dt-bindings/clock/qcom,nord-gcc.h>
#include <dt-bindings/clock/qcom,nord-nwgcc.h>

#include "cvp_clock_ids.h"

/* Clock order must match the Nordau CVP device-tree node. */
const u32 cvp_nordau_clock_ids[CVP_NORDAU_CLOCK_IDS_COUNT] = {
	NW_GCC_EVA_AXI0C_CLK,
	NW_GCC_EVA_AXI0_CLK,
	EVA_CC_SLEEP_CLK,
	EVA_CC_MVS0C_FREERUN_CLK,
	EVA_CC_MVS0_FREERUN_CLK,
	EVA_CC_MVS0C_CLK,
	EVA_CC_MVS0_CLK,
};
