/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Target-specific CVP clock IDs.
 *
 * The dt-binding clock headers intentionally use common macro names (for
 * example EVA_CC_MVS0_CLK). Keep each binding header in its corresponding
 * .c file so IDs from one target cannot redefine IDs for another target.
 */
#ifndef __CVP_CLOCK_IDS_H__
#define __CVP_CLOCK_IDS_H__

#include <linux/types.h>

#define CVP_GLYMUR_CLOCK_IDS_COUNT		7
#define CVP_KAANAPALI_CLOCK_IDS_COUNT	7
#define CVP_NORDAU_CLOCK_IDS_COUNT		7

extern const u32 cvp_glymur_clock_ids[CVP_GLYMUR_CLOCK_IDS_COUNT];
extern const u32 cvp_kaanapali_clock_ids[CVP_KAANAPALI_CLOCK_IDS_COUNT];
extern const u32 cvp_nordau_clock_ids[CVP_NORDAU_CLOCK_IDS_COUNT];

#endif /* __CVP_CLOCK_IDS_H__ */
