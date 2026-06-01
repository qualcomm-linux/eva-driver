/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MSM_CVP_DMA_BUF_H_
#define _MSM_CVP_DMA_BUF_H_

#include <linux/types.h>
#include <linux/platform_device.h>

struct dma_buf *msm_cvp_alloc_dma_buffer(size_t len, unsigned int flags);

void msm_cvp_free_dma_buffer(struct dma_buf *dmabuf);

#endif /* _CAM_BUF_MGR_H_ */