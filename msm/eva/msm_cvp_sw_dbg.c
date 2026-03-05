// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "msm_cvp_internal.h"
#include "msm_cvp_debug.h"
#include "cvp_core_hfi.h"

void eva_kmd_session_dump_to_buf(void *vaddr)
{
#ifdef CVP_SW_DBG_BUF_ENABLED
	struct msm_cvp_core *core = NULL;
	u32 *write_ptr = NULL;
	u8 *align_virtual_addr = vaddr;

	pr_info(CVP_PID_TAG "%s : Session Error dump to SW DBG Buffer from UMD\n",
			current->pid, current->tgid, "info", __func__);
	core = cvp_driver->cvp_core;
	if (!core) {
		dprintk(CVP_ERR, "%s: Core is NULL!\n", __func__);
		return;
	}
	write_ptr = (u32 *)(align_virtual_addr
			+ EVA_SW_DBG_KMD_OFFLINE_DUMP_IDX);
	if (write_ptr < (u32 *)align_virtual_addr ||
			write_ptr > (u32 *)(align_virtual_addr
				+ EVA_SW_DBG_BUF_UMD_OFFSET)) {
		dprintk(CVP_ERR, "%s: write_ptr is OOB\n", __func__);
		return;
	}
	memcpy(write_ptr, core->kmd_trace.kmd_session, sizeof(core->kmd_trace.kmd_session));
#endif
}


void eva_kmd_session_dump(struct msm_cvp_inst *inst)
{
#ifdef CVP_SW_DBG_BUF_ENABLED
	struct msm_cvp_core *core = NULL;
	struct eva_kmd_session *kmd_trace_sess = NULL;
	u32 trace_index = 0;

	pr_info(CVP_PID_TAG "%s : Session Error dump to SW DBG Buffer\n",
			current->pid, current->tgid, "info", __func__);

	if (!inst) {
		dprintk(CVP_ERR, "%s: inst is not valid\n", __func__);
		return;
	}
	core = cvp_driver->cvp_core;
	if (!core) {
		dprintk(CVP_ERR, "%s: Core is NULL!\n", __func__);
		return;
	}

	mutex_lock(&core->kmd_dbg.dbg_lock);
	trace_index = core->kmd_dbg.kmd_sess_cnt;
	core->kmd_dbg.kmd_sess_cnt++;
	if (core->kmd_dbg.kmd_sess_cnt >= TRACE_SESS_SIZE)
		core->kmd_dbg.kmd_sess_cnt = 0;

	mutex_unlock(&core->kmd_dbg.dbg_lock);

	kmd_trace_sess = &(core->kmd_trace.kmd_session[trace_index]);
	kmd_trace_sess->session_id = inst->sess_id;
	kmd_trace_sess->instance_state = inst->state;
	kmd_trace_sess->session_type = inst->session_type;
	kmd_trace_sess->hfi_error_code = inst->hfi_error_code;
	kmd_trace_sess->prev_hfi_error_code = inst->prev_hfi_error_code;
	kmd_trace_sess->session_error_code = inst->session_error_code;
#endif
}


void eva_kmd_debug_log_dump(void *vaddr)
{
#ifdef CVP_SW_DBG_BUF_ENABLED
	struct msm_cvp_core *core = NULL;
	u32 *write_ptr = NULL;
	u8 *align_virtual_addr = vaddr;

	if (align_virtual_addr ==  NULL) {
		dprintk(CVP_ERR, "%s: SW debug buffer is not allocated\n", __func__);
		return;
	}

	pr_info(CVP_PID_TAG "%s : Log Error dump to SW DBG Buffer\n",
			current->pid, current->tgid, "info", __func__);
	core = cvp_driver->cvp_core;
	if (!core) {
		dprintk(CVP_ERR, "%s: Core is NULL!\n", __func__);
		return;
	}

	write_ptr = (u32 *)(align_virtual_addr + EVA_SW_DBG_KMD_OFFLINE_DUMP_IDX
			+ (sizeof(struct eva_kmd_session) * TRACE_SESS_SIZE));
	if (write_ptr < (u32 *)align_virtual_addr ||
			write_ptr > (u32 *)(align_virtual_addr
				+ EVA_SW_DBG_BUF_UMD_OFFSET)) {
		dprintk(CVP_ERR, "%s: write_ptr is OOB for eva_kmd_debug_log\n", __func__);
		return;
	}
	memcpy(write_ptr, &(core->kmd_trace.kmd_debug_log), sizeof(struct eva_kmd_debug_log));
#endif
}

#ifdef CVP_SW_DBG_BUF_ENABLED
static int eva_queue_dump(struct cvp_iface_q_info *iface_q,
		u8 *align_virtual_addr, uint32_t queue_idx)
{

	struct cvp_hfi_queue_header *q_hdr = NULL;
	u32 *write_ptr = NULL;

	q_hdr = (struct cvp_hfi_queue_header *)iface_q->q_hdr;
	if (!q_hdr || (iface_q->q_array.align_virtual_addr == NULL)) {
		dprintk(CVP_ERR, "%s: Cannot read from shared Queue\n", __func__);
		return -1;
	}
	write_ptr = (u32 *)(align_virtual_addr + queue_idx);
	if (write_ptr < (u32 *)align_virtual_addr ||
			write_ptr > (u32 *)(align_virtual_addr
				+ queue_idx + SW_DBG_UMD_KMD_SIZE)) {
		dprintk(CVP_ERR, "%s: write_ptr is OOB for Queue\n", __func__);
		return -1;
	}
	memcpy(write_ptr, q_hdr, sizeof(struct cvp_hfi_queue_header));
	write_ptr = (u32 *)(align_virtual_addr + queue_idx
			+ sizeof(struct cvp_hfi_queue_header));
	memcpy(write_ptr, iface_q->q_array.align_virtual_addr, iface_q->q_array.mem_size);

	return 0;
}
#endif

void eva_cmd_msg_queue_dump(void *vaddr)
{
#ifdef CVP_SW_DBG_BUF_ENABLED
	struct msm_cvp_core *core = NULL;
	struct cvp_iface_q_info *iface_q = NULL;
	struct iris_hfi_device *dev = NULL;
	struct cvp_hfi_ops *ops_tbl = NULL;
	u8 *align_virtual_addr = NULL;
	int rc = 0;
	pr_info(CVP_PID_TAG "%s : CMD Queue dump to SW DBG Buffer\n",
			current->pid, current->tgid, "info", __func__);
	core = cvp_driver->cvp_core;
	if (!core) {
		dprintk(CVP_ERR, "%s: Core is NULL!\n", __func__);
		return;
	}
	ops_tbl = core->dev_ops;
	if (ops_tbl)
		dev = ops_tbl->hfi_device_data;
	if (!dev) {
		dprintk(CVP_ERR, "%s: dev is NULL\n", __func__);
		return;
	}
	align_virtual_addr = vaddr;
	if (align_virtual_addr ==  NULL) {
		dprintk(CVP_ERR, "%s: SW debug buffer is not allocated\n", __func__);
		return;
	}

	/* Dump CMD queue */
	iface_q = &dev->iface_queues[CVP_IFACEQ_CMDQ_IDX];
	rc = eva_queue_dump(iface_q, align_virtual_addr, SW_DBG_CMD_Q_IDX);
	if (rc) {
		dprintk(CVP_ERR, "%s: Cannot read from CMD Queue\n", __func__);
		return;
	}

	/* Dump MSG queue */
	iface_q = &dev->iface_queues[CVP_IFACEQ_MSGQ_IDX];
	rc = eva_queue_dump(iface_q, align_virtual_addr, SW_DBG_MSG_Q_IDX);
	if (rc) {
		dprintk(CVP_ERR, "%s: Cannot read from MSG Queue\n", __func__);
		return;
	}

	/* Dump DSP CMD queue */
	iface_q = &dev->dsp_iface_queues[CVP_IFACEQ_CMDQ_IDX];
	rc = eva_queue_dump(iface_q, align_virtual_addr, SW_DBG_DSP_CMD_Q_IDX);
	if (rc) {
		dprintk(CVP_ERR, "%s: Cannot read from DSP CMD Queue\n", __func__);
		return;
	}

	/* Dump DSP MSG queue */
	iface_q = &dev->dsp_iface_queues[CVP_IFACEQ_MSGQ_IDX];
	rc = eva_queue_dump(iface_q, align_virtual_addr, SW_DBG_DSP_MSG_Q_IDX);
	if (rc) {
		dprintk(CVP_ERR, "%s: Cannot read from DSP MSG Queue\n", __func__);
		return;
	}
#endif
}
