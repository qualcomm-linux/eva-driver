// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.​
 */

#include <drm/drm_ioctl.h>
#include <drm/drm_file.h>
#include "cvp_private.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_common.h"
#include "msm_cvp.h"

int eva_ioctl_get_session_info(struct drm_device *dev, void *data,
				      struct drm_file *file)
{
	struct eva_kmd_arg *karg = data;
	struct msm_cvp_inst *inst = file->driver_priv;
	int rc;

	rc = session_state_check_init(inst);
	if (rc)
		return rc;

	return msm_cvp_get_session_info(inst, &karg->data.session.session_id);
}

int eva_ioctl_update_power(struct drm_device *dev, void *data,
				  struct drm_file *file)
{
	struct msm_cvp_inst *inst = file->driver_priv;
	int rc;
	dprintk(CVP_WARN, "%s: ioctl invoked", __func__);

	rc = session_state_check_init(inst);
	if (rc)
		return rc;

	return msm_cvp_update_power(inst);
}

int eva_ioctl_send_cmd_pkt(struct drm_device *dev, void *data,
				  struct drm_file *file)
{
	struct eva_kmd_arg *karg = data;
	struct msm_cvp_inst *inst = file->driver_priv;
	struct cvp_hal_session_cmd_pkt pkt_hdr;
	int rc;

	pkt_hdr.size        = karg->data.hfi_pkt.pkt_data[0];
	pkt_hdr.packet_type = karg->data.hfi_pkt.pkt_data[1];

	if (get_pkt_index(&pkt_hdr) < 0) {
		dprintk(CVP_ERR, "Invalid HFI packet type 0x%x\n",
			pkt_hdr.packet_type);
		return -EINVAL;
	}

	if (pkt_hdr.size > MAX_HFI_PKT_SIZE * sizeof(unsigned int)) {
		dprintk(CVP_ERR, "HFI packet too large: %u\n", pkt_hdr.size);
		return -EINVAL;
	}

	rc = session_state_check_init(inst);
	if (rc)
		return rc;

	return msm_cvp_session_process_hfi(inst, &karg->data.hfi_pkt,
					   karg->buf_offset, karg->buf_num);
}

int eva_ioctl_receive_msg_pkt(struct drm_device *dev, void *data,
				     struct drm_file *file)
{
	struct eva_kmd_arg *karg = data;
	struct msm_cvp_inst *inst = file->driver_priv;
	int rc;

	rc = session_state_check_init(inst);
	if (rc)
		return rc;

	return msm_cvp_session_receive_hfi(inst, &karg->data.hfi_pkt);
}

int eva_ioctl_flush_all(struct drm_device *dev, void *data,
			       struct drm_file *file)
{
	struct msm_cvp_inst *inst = file->driver_priv;
	int rc;

	rc = session_state_check_init(inst);
	if (rc)
		return rc;

	return cvp_session_flush_all(inst);
}

int eva_ioctl_session_ctrl(struct drm_device *dev, void *data,
				  struct drm_file *file)
{
	struct eva_kmd_arg *karg = data;
	struct msm_cvp_inst *inst = file->driver_priv;

	return msm_cvp_session_ctrl(inst, karg);
}

int eva_ioctl_set_sysprop(struct drm_device *dev, void *data,
				 struct drm_file *file)
{
	struct eva_kmd_arg *karg = data;
	struct msm_cvp_inst *inst = file->driver_priv;

	if (karg->data.sys_properties.prop_num < 1 ||
	    karg->data.sys_properties.prop_num > MAX_KMD_PROP_NUM_PER_PACKET) {
		dprintk(CVP_ERR, "prop_num out of range: %u\n",
			karg->data.sys_properties.prop_num);
		return -EINVAL;
	}

	return msm_cvp_set_sysprop(inst, karg);
}

int eva_ioctl_get_sysprop(struct drm_device *dev, void *data,
				 struct drm_file *file)
{
	struct eva_kmd_arg *karg = data;
	struct msm_cvp_inst *inst = file->driver_priv;

	if (karg->data.sys_properties.prop_num < 1 ||
	    karg->data.sys_properties.prop_num > MAX_KMD_PROP_NUM_PER_PACKET) {
		dprintk(CVP_ERR, "prop_num out of range: %u\n",
			karg->data.sys_properties.prop_num);
		return -EINVAL;
	}

	return msm_cvp_get_sysprop(inst, karg);
}

int eva_ioctl_flush_frame(struct drm_device *dev, void *data,
				 struct drm_file *file)
{
	dprintk(CVP_WARN, "EVA_FLUSH_FRAME ioctl is deprecated\n");
	return 0;
}
