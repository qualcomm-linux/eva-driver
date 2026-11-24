// SPDX-License-Identifier: GPL-2.0-only

#include <dt-bindings/clock/qcom,kaanapali-evacc.h>
#include <dt-bindings/clock/qcom,kaanapali-gcc.h>

#include "cvp_clock_ids.h"

/* Clock order must match the Kaanapali/Canoe CVP device-tree node. */
const u32 cvp_kaanapali_clock_ids[CVP_KAANAPALI_CLOCK_IDS_COUNT] = {
	GCC_EVA_AXI0C_CLK,
	EVA_CC_SLEEP_CLK,
	EVA_CC_MVS0C_FREERUN_CLK,
	EVA_CC_MVS0C_CLK,
	GCC_EVA_AXI0_CLK,
	EVA_CC_MVS0_FREERUN_CLK,
	EVA_CC_MVS0_CLK,
};
