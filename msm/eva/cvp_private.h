/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_V4L2_PRIVATE_H_
#define _MSM_V4L2_PRIVATE_H_

#include <drm/drm_ioctl.h>
#include <media/msm_eva_private.h>
#include "msm_cvp_debug.h"

int eva_ioctl_get_session_info(struct drm_device *dev, void *data, struct drm_file *file);
int eva_ioctl_update_power(struct drm_device *dev, void *data, struct drm_file *file);
int eva_ioctl_send_cmd_pkt(struct drm_device *dev, void *data, struct drm_file *file);
int eva_ioctl_receive_msg_pkt(struct drm_device *dev, void *data, struct drm_file *file);
int eva_ioctl_flush_all(struct drm_device *dev, void *data, struct drm_file *file);
int eva_ioctl_session_ctrl(struct drm_device *dev, void *data, struct drm_file *file);
int eva_ioctl_set_sysprop(struct drm_device *dev, void *data, struct drm_file *file);
int eva_ioctl_get_sysprop(struct drm_device *dev, void *data, struct drm_file *file);
int eva_ioctl_flush_frame(struct drm_device *dev, void *data, struct drm_file *file);

#endif
