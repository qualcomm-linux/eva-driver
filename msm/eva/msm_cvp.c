// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "msm_cvp.h"
#include "cvp_hfi.h"
#include "cvp_core_hfi.h"
#include "msm_cvp_buf.h"
#include "cvp_comm_def.h"
#include "cvp_power.h"
#include "cvp_hfi_api.h"
#include "eva_gem.h"
/*
 * only need #define CREATE_TRACE_POINTS in one source file
 * but every source file which add CVPKERNEL_ATRACE_BEGIN/CVPKERNEL_ATRACE_END
 * should include "msm_cvp_events.h"
 */
#define CREATE_TRACE_POINTS
#include "msm_cvp_events.h"

static int cvp_enqueue_pkt(struct msm_cvp_inst* inst,
	struct eva_kmd_hfi_packet *in_pkt,
	unsigned int in_offset,
	unsigned int in_buf_num);

void *get_sess_from_idr(struct msm_cvp_inst *inst)
{
	void *sess = NULL;
	struct msm_cvp_core *core = NULL;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return NULL;
	}

	core = inst->core;
	mutex_lock(&core->idr_lock);
	sess = idr_find(&core->sess_idr, inst->sess_id);
	mutex_unlock(&core->idr_lock);
	if (!sess)
		dprintk(CVP_ERR, "%s: Could not find the sess obj for given idr id\n",
				__func__);

	return sess;
}

int msm_cvp_get_session_info(struct msm_cvp_inst *inst, u32 *session)
{
	int rc = 0;
	struct msm_cvp_inst *s;
	struct msm_cvp_core *core = NULL;

	CVPKERNEL_ATRACE_BEGIN("msm_cvp_get_session_info");
	dprintk(CVP_WARN, "%s: invoked");

	if (!inst || !session) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = cvp_driver->cvp_core;
	if (!core) {
		dprintk(CVP_ERR, "%s: core is NULL", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(core, inst);
	if (!s)
		return -ECONNRESET;

	*session = inst->sess_id;
	dprintk(CVP_SESS, "%s: id 0x%x\n", __func__, *session);

	cvp_put_inst(s);
	CVPKERNEL_ATRACE_END("msm_cvp_get_session_info");
	return rc;
}



static bool cvp_msg_pending(struct cvp_session_queue *sq,
				struct cvp_session_msg **msg, u64 *ktid)
{
	struct cvp_session_msg *mptr = NULL, *dummy;
	bool result = false;
	CVPKERNEL_ATRACE_BEGIN("cvp_msg_pending");
	if (!sq)
		return false;
	spin_lock(&sq->lock);
	if (sq->state == QUEUE_INIT || sq->state == QUEUE_INVALID) {
		/* The session is being deleted */
		spin_unlock(&sq->lock);
		*msg = NULL;
		return true;
	}
	result = list_empty(&sq->msgs);
	if (!result) {
		mptr = list_first_entry(&sq->msgs,
				struct cvp_session_msg,
				node);
		if (!ktid) {
			if (mptr) {
				list_del_init(&mptr->node);
				sq->msg_count--;
			}
		} else {
			result = true;
			list_for_each_entry_safe(mptr, dummy, &sq->msgs, node) {
				if (*ktid == mptr->pkt.header.client_data.kdata) {
					list_del_init(&mptr->node);
					sq->msg_count--;
					result = false;
					break;
				}
			}
			if (result)
				mptr = NULL;
		}
	}
	spin_unlock(&sq->lock);
	*msg = mptr;
	CVPKERNEL_ATRACE_END("cvp_msg_pending");
	return !result;
}

static int cvp_wait_process_message(struct msm_cvp_inst *inst,
				struct cvp_session_queue *sq, u64 *ktid,
				unsigned long timeout,
				struct eva_kmd_hfi_packet *out)
{
	struct cvp_session_msg *msg = NULL;
	struct cvp_hfi_msg_session_hdr *hdr;
	int rc = 0;

	CVPKERNEL_ATRACE_BEGIN("cvp_wait_process_message");

	if (!inst) {
		dprintk(CVP_ERR, "%s: Invalid inst", __func__);
		goto exit;
	}

	if (wait_event_timeout(sq->wq,
		cvp_msg_pending(sq, &msg, ktid), timeout) == 0) {
		dprintk(CVP_WARN,
			"Frames: session queue wait timeout and session_id = %#x sq %pK, sq->wq %pK\n",
			inst->sess_id, sq, &sq->wq);

		rc = -ETIMEDOUT;
		handle_session_timeout(inst, true);
		goto exit;
	}

	if (msg == NULL) {
		dprintk(CVP_WARN, "%s: queue state %d, msg cnt %d\n", __func__,
					sq->state, sq->msg_count);

		if (inst->state >= MSM_CVP_CLOSE_DONE ||
				(sq->state != QUEUE_ACTIVE &&
				sq->state != QUEUE_START)) {
			rc = -ECONNRESET;
			goto exit;
		}

		msm_cvp_comm_kill_session(inst);
		goto exit;
	}

	if (!out) {
		cvp_kmem_cache_free(&cvp_driver->msg_cache, msg);
		goto exit;
	}

	hdr = (struct cvp_hfi_msg_session_hdr *)&msg->pkt;
	CVPKERNEL_ATRACE_BEGIN("before and after memcpy");
	memcpy(out, &msg->pkt, get_msg_size(hdr));
	CVPKERNEL_ATRACE_END("before and after memcpy");
	if (hdr->header.client_data.kdata >= MAX_PKT_IDX)
		msm_cvp_unmap_frame(inst, hdr->header.client_data.kdata);
	
	cvp_kmem_cache_free(&cvp_driver->msg_cache, msg);

exit:
	CVPKERNEL_ATRACE_END("cvp_wait_process_message");
	return rc;
}

int msm_cvp_session_receive_hfi(struct msm_cvp_inst *inst,
			struct eva_kmd_hfi_packet *out_pkt)
{
	unsigned long wait_time;
	struct cvp_session_queue *sq;
	struct msm_cvp_inst *s;
	int rc = 0;
	struct cvp_hfi_msg_session_hdr *msg_hdr = NULL;
	struct msm_cvp_core *core = NULL;
	CVPKERNEL_ATRACE_BEGIN("msm_cvp_session_receive_hfi");

	if (!inst) {
		dprintk(CVP_ERR, "%s invalid session\n", __func__);
		return -EINVAL;
	}

	core = cvp_driver->cvp_core;
	if (!core) {
		dprintk(CVP_ERR, "%s: core is NULL", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(core, inst);
	if (!s)
		return -ECONNRESET;

	wait_time = msecs_to_jiffies(
		inst->core->resources.msm_cvp_hw_rsp_timeout);
	sq = &inst->session_queue;

	rc = cvp_wait_process_message(inst, sq, NULL, wait_time, out_pkt);

	msg_hdr = (struct cvp_hfi_msg_session_hdr *)out_pkt;
	msm_cvp_msg_tracing_from_sw(msg_hdr, "EVA_KMD_REV_END");

	cvp_put_inst(inst);
	CVPKERNEL_ATRACE_END("msm_cvp_session_receive_hfi");
	return rc;
}

int msm_cvp_session_process_hfi(
	struct msm_cvp_inst *inst,
	struct eva_kmd_hfi_packet *in_pkt,
	unsigned int in_offset,
	unsigned int in_buf_num)
{
	int pkt_idx, rc = 0;

	unsigned int offset = 0, buf_num = 0, signal;
	struct cvp_session_queue *sq;
	struct cvp_hfi_cmd_session_hdr *pkt_hdr;
	bool is_config_pkt;
	struct cvp_hfi_cmd_session_hdr *cmd_hdr = NULL;
	struct msm_cvp_core *core = NULL;

	CVPKERNEL_ATRACE_BEGIN("msm_cvp_session_process_hfi");

	if (!inst || !in_pkt) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = cvp_driver->cvp_core;
	if (!core) {
		dprintk(CVP_ERR, "%s: core is NULL", __func__);
		return -EINVAL;
	}

	inst = cvp_get_inst_validate(core, inst);
	if (!inst)
		return -ECONNRESET;

	if (inst->state == MSM_CVP_CORE_INVALID) {
		dprintk(CVP_ERR, "sess %pK INVALIDim reject new HFIs\n", inst);
		rc = -ECONNRESET;
		goto exit;
	}

	sq = &inst->session_queue;
	spin_lock(&sq->lock);
	if (sq->state > QUEUE_STOP) {
		spin_unlock(&sq->lock);
		dprintk(CVP_ERR, "Invalid session %pK cannot accept HFI commands\n", inst);
		rc = -EINVAL;
		goto exit;
	}
	spin_unlock(&sq->lock);

	pkt_hdr = (struct cvp_hfi_cmd_session_hdr *)in_pkt;
	dprintk(CVP_CMD, "%s: pkt_type %08x sess_id %08x trans_id %u ktid %llx\n",
		__func__, pkt_hdr->header.packet_type,
		pkt_hdr->header.session_id,
		pkt_hdr->header.client_data.transaction_id,
		pkt_hdr->header.client_data.kdata & (FENCE_BIT - 1));

	pkt_idx = get_pkt_index((struct cvp_hal_session_cmd_pkt *)in_pkt);
	if (pkt_idx < 0) {
		dprintk(CVP_ERR, "%s incorrect packet %d, %x\n", __func__,
				in_pkt->pkt_data[0],
				in_pkt->pkt_data[1]);
		goto exit;
	} else {
		signal = cvp_hfi_defs[pkt_idx].resp;
		is_config_pkt = cvp_hfi_defs[pkt_idx].is_config_pkt;
	}

	if (is_config_pkt)
		pr_info_ratelimited(CVP_PID_TAG "inst %pK config %s\n",
			current->pid, current->tgid, "sess",
			inst, cvp_hfi_defs[pkt_idx].name);

	if (signal == HAL_NO_RESP) {
		/* Frame packets are not allowed before session starts*/
		spin_lock(&sq->lock);
		if (sq->state != QUEUE_START && !is_config_pkt) {
			/*
			 * A init packet is allowed in case of
			 * QUEUE_ACTIVE, QUEUE_START, QUEUE_STOP
			 * A frame packet is only allowed in case of
			 * QUEUE_START
			 */
			spin_unlock(&sq->lock);
			dprintk(CVP_ERR, "%s: invalid queue state %d\n",
				__func__, sq->state);
			rc = -EINVAL;
			goto exit;
		}
		spin_unlock(&sq->lock);
	}

	if (in_offset && in_buf_num) {
		offset = in_offset;
		buf_num = in_buf_num;
	}
	if (!is_buf_param_valid(buf_num, offset)) {
		dprintk(CVP_ERR, "Incorrect buffer num and offset in cmd\n");
		rc = -EINVAL;
		goto exit;
	}

	cmd_hdr = (struct cvp_hfi_cmd_session_hdr *)in_pkt;
	msm_cvp_cmd_tracing_from_sw(cmd_hdr, "EVA_KMD_FWD_BEGIN");

	rc = cvp_enqueue_pkt(inst, in_pkt, offset, buf_num);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to enqueue pkt, inst %pK pkt_type %08x ktid %llx trans_id %u\n",
			inst, pkt_hdr->header.packet_type,
			pkt_hdr->header.client_data.kdata,
			pkt_hdr->header.client_data.transaction_id);
	}

exit:
	cvp_put_inst(inst);
	CVPKERNEL_ATRACE_END("msm_cvp_session_process_hfi");
	return rc;
}

static int cvp_enqueue_pkt(struct msm_cvp_inst* inst,
	struct eva_kmd_hfi_packet *in_pkt,
	unsigned int in_offset,
	unsigned int in_buf_num)
{
	struct cvp_hfi_ops *ops_tbl;
	struct cvp_hfi_cmd_session_hdr *cmd_hdr;
	int rc = 0;

	CVPKERNEL_ATRACE_BEGIN("cvp_enqueue_pkt");

	if (in_offset > MAX_HFI_PKT_SIZE ||
			in_buf_num > MAX_HFI_PKT_SIZE) {
		dprintk(CVP_ERR, "%s: Offset:%d or Buf num:%d incorrect",
				__func__, in_offset, in_buf_num);
		rc = -EINVAL;
		return rc;
	}

	ops_tbl = inst->core->dev_ops;

	cmd_hdr = (struct cvp_hfi_cmd_session_hdr *)in_pkt;
	/* The kdata will be overriden by transaction ID if the cmd has buf */
	cmd_hdr->header.client_data.kdata = 0;

	rc = msm_cvp_map_frame(inst, in_pkt, in_offset, in_buf_num);

	if (rc)
		goto exit;

	CVPKERNEL_ATRACE_BEGIN(" call_hfi_op  -- session_send");
	rc = call_hfi_op(ops_tbl, session_send, (void *)inst->session,
		in_pkt);
	CVPKERNEL_ATRACE_END(" call_hfi_op  -- session_send");
	if (rc) {
		dprintk(CVP_ERR,"%s: Failed in call_hfi_op %d, %x\n",
				__func__, in_pkt->pkt_data[0],
				in_pkt->pkt_data[1]);
		msm_cvp_unmap_frame(inst, cmd_hdr->header.client_data.kdata);
		
	}
	CVPKERNEL_ATRACE_END("cvp_enqueue_pkt");
exit:
	return rc;
}

static inline int div_by_1dot5(unsigned int a)
{
	unsigned long i = a << 1;

	return (unsigned int) i/3;
}

int msm_cvp_session_delete(struct msm_cvp_inst *inst)
{
	return 0;
}

int msm_cvp_session_create(struct msm_cvp_inst *inst)
{
	int rc = 0, rc1 = 0;
	struct cvp_session_queue *sq;

	struct msm_cvp_core *core = NULL;

	core = cvp_driver->cvp_core;

	CVPKERNEL_ATRACE_BEGIN("msm_cvp_session_create");

	if (!inst || !inst->core)
		return -EINVAL;

	if (inst->state >= MSM_CVP_CLOSE_DONE)
		return -ECONNRESET;

	if (inst->state != MSM_CVP_CORE_INIT_DONE ||
		inst->state > MSM_CVP_OPEN_DONE) {
		dprintk(CVP_ERR,
			"%s Incorrect CVP state %d to create session\n",
			__func__, inst->state);
		return -EINVAL;
	}

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_OPEN_DONE);
	if (rc) {
		dprintk(CVP_ERR, "Failed to move instance to open done state\n");
		goto fail_create;
	}

	rc = cvp_comm_set_arp_buffers(inst);
	if (rc) {
		dprintk(CVP_ERR,
				"Failed to set ARP buffers\n");
		goto fail_init;
	}

	sq = &inst->session_queue;
	spin_lock(&sq->lock);
	sq->state = QUEUE_ACTIVE;
	spin_unlock(&sq->lock);
	CVPKERNEL_ATRACE_END("msm_cvp_session_create");
	return rc;

fail_init:
	rc1 = msm_cvp_comm_try_state(inst, MSM_CVP_CLOSE_DONE);
	if (rc1)
		dprintk(CVP_ERR, "%s: close failed\n", __func__);
fail_create:
	return rc;
}

int session_state_check_init(struct msm_cvp_inst *inst)
{
	mutex_lock(&inst->lock);
	if (inst->state == MSM_CVP_OPEN || inst->state == MSM_CVP_OPEN_DONE) {
		mutex_unlock(&inst->lock);
		return 0;
	}
	mutex_unlock(&inst->lock);

	return msm_cvp_session_create(inst);
}

int msm_cvp_session_start(struct msm_cvp_inst *inst,
		struct eva_kmd_arg *arg)
{
	struct cvp_session_queue *sq;
	struct cvp_hfi_ops *ops_tbl;
	struct iris_hfi_device *device;
	int rc;
	enum queue_state old_state;
	u64 ktid;

	CVPKERNEL_ATRACE_BEGIN("msm_cvp_session_start");

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	sq = &inst->session_queue;
	spin_lock(&sq->lock);
	if (sq->msg_count) {
		dprintk(CVP_ERR, "session start failed queue not empty%d\n",
			sq->msg_count);
		spin_unlock(&sq->lock);
		rc = -EINVAL;
		goto exit;
	}
	old_state = sq->state;
	sq->state = QUEUE_START;
	spin_unlock(&sq->lock);

	ops_tbl = inst->core->dev_ops;
	device = (struct iris_hfi_device *)ops_tbl->hfi_device_data;
	call_iris_op(device, pm_qos_update, device);

	/* Send SESSION_START command */
	ktid = atomic64_inc_return(&inst->core->kernel_trans_id);
	ktid &= (FENCE_BIT - 1);
	rc = call_hfi_op(ops_tbl, session_start, (void *)inst->session, ktid);
	if (rc) {
		dprintk(CVP_WARN, "%s: session start failed rc %d\n",
				__func__, rc);
		goto restore_state;
	}

	/* Wait for FW response */
	rc = wait_for_sess_signal_receipt(inst, HAL_SESSION_START_DONE);
	if (rc) {
		dprintk(CVP_WARN, "%s: wait for signal failed, rc %d\n",
				__func__, rc);
		goto restore_state;
	}

	pr_info_ratelimited(CVP_PID_TAG "session %llx (%#x) started\n",
		current->pid, current->tgid, "sess", inst, inst->sess_id);
	CVPKERNEL_ATRACE_END("msm_cvp_session_start");

	return 0;

restore_state:

	spin_lock(&sq->lock);
	sq->state = old_state;
	spin_unlock(&sq->lock);
exit:
	return rc;
}

int msm_cvp_session_flush_stop(struct msm_cvp_inst *inst)
{
	struct cvp_session_queue *sq;
	struct msm_cvp_inst *s;
	struct cvp_hfi_ops *ops_tbl;
	u64 ktid;
	int rc;
	struct msm_cvp_core *core = NULL;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = cvp_driver->cvp_core;
	if (!core) {
		dprintk(CVP_ERR, "%s: core is NULL", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(core, inst);
	if (!s)
		return -ECONNRESET;

	sq = &inst->session_queue;

	spin_lock(&sq->lock);

	if (sq->state == QUEUE_STOP) {
		dprintk(CVP_WARN, "Session %llx (%#x) already stopped\n",
			inst, inst->sess_id);
		spin_unlock(&sq->lock);
		rc = 0;
		goto exit;
	}

	if (sq->state < QUEUE_START) {
		dprintk(CVP_WARN, "Session %llx (%#x) not started yet, session state: %d\n",
			inst, inst->sess_id, sq->state);
		spin_unlock(&sq->lock);
		rc = 0;
		goto stop_thread;
	}

	spin_unlock(&sq->lock);

	ops_tbl = inst->core->dev_ops;

	/*Flush all pending cmds for the error EVA session*/
	pr_info_ratelimited(CVP_PID_TAG "flush stop session: %pK session_id = %#x\n",
		current->pid, current->tgid, "sess",
		inst, inst->sess_id);
	rc = cvp_session_flush_all(inst);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: cannot flush session %llx (%#x) rc %d\n",
			__func__, inst, inst->sess_id, rc);
		goto stop_thread;
	}

	/* Send SESSION_STOP command */
	ktid = atomic64_inc_return(&inst->core->kernel_trans_id);
	ktid &= (FENCE_BIT - 1);
	rc = call_hfi_op(ops_tbl, session_stop, (void *)inst->session, ktid);
	if (rc) {
		dprintk(CVP_WARN, "%s: session stop failed rc %d\n",
				__func__, rc);
		goto stop_thread;
	}

	/* Wait for FW response */
	rc = wait_for_sess_signal_receipt(inst, HAL_SESSION_STOP_DONE);
	if (rc) {
		dprintk(CVP_WARN, "%s: wait for signal failed, rc %d and session_id = %#x\n",
				__func__, rc, inst->sess_id);
		goto stop_thread;
	}

stop_thread:
	spin_lock(&sq->lock);
	if (!rc)
		sq->state = QUEUE_STOP;
	else
		sq->state = QUEUE_INVALID;
	spin_unlock(&sq->lock);

	wake_up_all(&inst->session_queue.wq);

exit:
	cvp_put_inst(s);
	return rc;
}

int msm_cvp_session_stop(struct msm_cvp_inst *inst,
		struct eva_kmd_arg *arg)
{
	struct cvp_session_queue *sq;
	struct eva_kmd_session_control *sc = NULL;
	struct msm_cvp_inst *s;
	struct cvp_hfi_ops *ops_tbl;
	struct iris_hfi_device *device;
	u64 ktid;
	struct msm_cvp_core *core = NULL;
	int rc;

	CVPKERNEL_ATRACE_BEGIN("msm_cvp_session_stop");

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = cvp_driver->cvp_core;
	if (!core) {
		dprintk(CVP_ERR, "%s: core is NULL", __func__);
		return -EINVAL;
	}

	if (arg)
		sc = &arg->data.session_ctrl;

	s = cvp_get_inst_validate(core, inst);
	if (!s)
		return -ECONNRESET;

	sq = &inst->session_queue;

	spin_lock(&sq->lock);
	if (sq->state == QUEUE_STOP) {
		dprintk(CVP_WARN, "Session %llx (%#x) already stopped\n",
			inst, inst->sess_id);
		spin_unlock(&sq->lock);
		rc = 0;
		goto exit;
	}
	if (sq->state != QUEUE_INVALID && sq->msg_count) {
		dprintk(CVP_ERR, "session stop incorrect: queue not empty%d\n",
			sq->msg_count);
		if (sc)
			sc->ctrl_data[0] = sq->msg_count;
		spin_unlock(&sq->lock);
		rc = -EUCLEAN;
		goto exit;
	}

	pr_info_ratelimited(CVP_PID_TAG "Stop session: %pK session_id = %#x\n",
			current->pid, current->tgid, "sess",
			inst, inst->sess_id);
	spin_unlock(&sq->lock);

	ops_tbl = inst->core->dev_ops;

	/* Send SESSION_STOP command */
	ktid = atomic64_inc_return(&inst->core->kernel_trans_id);
	ktid &= (FENCE_BIT - 1);
	rc = call_hfi_op(ops_tbl, session_stop, (void *)inst->session, ktid);
	if (rc) {
		dprintk(CVP_WARN, "%s: session stop failed rc %d\n",
				__func__, rc);
		goto stop_thread;
	}

	/* Wait for FW response */
	rc = wait_for_sess_signal_receipt(inst, HAL_SESSION_STOP_DONE);
	if (rc) {
		dprintk(CVP_WARN,
			"%s: wait for signal failed, rc %d and session_id = %#x, retry flush_stop\n",
			__func__, rc, inst->sess_id);
		rc = msm_cvp_session_flush_stop(inst);
		goto exit;
	}
stop_thread:
	spin_lock(&sq->lock);
	if (!rc)
		sq->state = QUEUE_STOP;
	else
		sq->state = QUEUE_INVALID;
	spin_unlock(&sq->lock);

	wake_up_all(&inst->session_queue.wq);

	device = (struct iris_hfi_device *)ops_tbl->hfi_device_data;
	call_iris_op(device, pm_qos_update, device);

exit:
	pr_info_ratelimited(CVP_PID_TAG "Stop session done for session_id = %#x\n",
			current->pid, current->tgid, "sess",
			inst->sess_id);
	cvp_put_inst(s);
	CVPKERNEL_ATRACE_END("msm_cvp_session_stop");
	return rc;
}


int msm_cvp_session_ctrl(struct msm_cvp_inst *inst,
		struct eva_kmd_arg *arg)
{
	struct eva_kmd_session_control *ctrl = &arg->data.session_ctrl;
	int rc = 0;
	unsigned int ctrl_type;
	CVPKERNEL_ATRACE_BEGIN("msm_cvp_session_ctrl");

	ctrl_type = ctrl->ctrl_type;

	if (!inst && ctrl_type != SESSION_CREATE) {
		dprintk(CVP_ERR, "%s invalid session\n", __func__);
		return -EINVAL;
	}

	switch (ctrl_type) {
	case SESSION_STOP:
		rc = msm_cvp_session_stop(inst, arg);
		break;
	case SESSION_START:
		rc = msm_cvp_session_start(inst, arg);
		break;
	case SESSION_CREATE:
		rc = msm_cvp_session_create(inst);
		break;
	case SESSION_DELETE:
		rc = msm_cvp_session_delete(inst);
		break;
	case SESSION_INFO:
	default:
		dprintk(CVP_ERR, "%s Unsupported session ctrl%d\n",
			__func__, ctrl->ctrl_type);
		rc = -EINVAL;
	}
	CVPKERNEL_ATRACE_END("msm_cvp_session_ctrl");
	return rc;
}

int msm_cvp_get_sysprop(struct msm_cvp_inst *inst,
		struct eva_kmd_arg *arg)
{
	struct eva_kmd_sys_properties *props = &arg->data.sys_properties;
	struct cvp_hfi_ops *ops_tbl;
	struct iris_hfi_device *hfi;
	struct cvp_session_prop *session_prop;
	int i, rc = 0;
	CVPKERNEL_ATRACE_BEGIN("msm_cvp_get_sysprop");

	if (!inst || !inst->core || !inst->core->dev_ops) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	ops_tbl = inst->core->dev_ops;
	hfi = ops_tbl->hfi_device_data;

	if (props->prop_num > MAX_KMD_PROP_NUM_PER_PACKET) {
		dprintk(CVP_ERR, "Too many properties %d to get\n",
			props->prop_num);
		return -E2BIG;
	}

	session_prop = &inst->prop;

	for (i = 0; i < props->prop_num; i++) {
		switch (props->prop_data[i].prop_type) {
		case EVA_KMD_PROP_HFI_VERSION:
		{
			props->prop_data[i].data = hfi->version;
			break;
		}
		case EVA_KMD_PROP_SESSION_STATE:
		{
			props->prop_data[i].data = inst->session_error_code;
			break;
		}
		case EVA_KMD_PROP_PWR_FDU:
		{
			props->prop_data[i].data =
				msm_cvp_get_hw_aggregate_cycles(HFI_HW_FDU);
			break;
		}
		case EVA_KMD_PROP_PWR_ICA:
		{
			props->prop_data[i].data =
				msm_cvp_get_hw_aggregate_cycles(HFI_HW_ICA);
			break;
		}
		case EVA_KMD_PROP_PWR_OD:
		{
			props->prop_data[i].data =
				msm_cvp_get_hw_aggregate_cycles(HFI_HW_OD);
			break;
		}
		case EVA_KMD_PROP_PWR_MPU:
		{
			props->prop_data[i].data =
				msm_cvp_get_hw_aggregate_cycles(HFI_HW_MPU);
			break;
		}
		case EVA_KMD_PROP_PWR_VADL:
		{
			props->prop_data[i].data =
				msm_cvp_get_hw_aggregate_cycles(HFI_HW_VADL);
			break;
		}
		case EVA_KMD_PROP_PWR_TOF:
		{
			props->prop_data[i].data =
				msm_cvp_get_hw_aggregate_cycles(HFI_HW_TOF);
			break;
		}
		case EVA_KMD_PROP_PWR_RGE:
		{
			props->prop_data[i].data =
				msm_cvp_get_hw_aggregate_cycles(HFI_HW_RGE);
			break;
		}
		case EVA_KMD_PROP_PWR_XRA:
		{
			props->prop_data[i].data =
				msm_cvp_get_hw_aggregate_cycles(HFI_HW_XRA);
			break;
		}
		case EVA_KMD_PROP_PWR_LSR:
		{
			props->prop_data[i].data =
				msm_cvp_get_hw_aggregate_cycles(HFI_HW_LSR);
			break;
		}
		case EVA_KMD_PROP_SOC_HW_VERSION:
		{
			props->prop_data[i].data = inst->core->soc_hw_version;
			break;
		}
		default:
			dprintk(CVP_ERR, "unrecognized sys property %d\n",
				props->prop_data[i].prop_type);
			rc = -EFAULT;
		}
	}
	CVPKERNEL_ATRACE_END("msm_cvp_get_sysprop");
	return rc;
}

static int cvp_session_name_copy_u32(u32 idx,
		struct cvp_session_prop *session_prop,
		u32 *val)
{
	int rc = 0;

	if (idx > SESSION_NAME_MAX_LEN - 4) {
		dprintk(CVP_ERR, "Session name exceed maximum length %d\n",
				SESSION_NAME_MAX_LEN);
		memset(session_prop->session_name, 0x00, SESSION_NAME_MAX_LEN);
		rc = -E2BIG;
	} else {
		memcpy(&session_prop->session_name[idx],
			val, sizeof(u32));
	}
	return rc;
}

int msm_cvp_set_sysprop_sess(struct msm_cvp_inst *inst,
		struct eva_kmd_sys_property *prop_array, int i)
{
	struct cvp_session_prop *session_prop;
	int rc = 0;

	session_prop = &inst->prop;
	switch (prop_array->prop_type) {
		case EVA_KMD_PROP_SESSION_TYPE:
			session_prop->type = prop_array->data;
			break;
		case EVA_KMD_PROP_SESSION_KERNELMASK:
			session_prop->kernel_mask = prop_array->data;
			break;
		case EVA_KMD_PROP_SESSION_PRIORITY:
			session_prop->priority = prop_array->data;
			break;
		case EVA_KMD_PROP_SESSION_SECURITY:
			session_prop->is_secure = prop_array->data;
			break;
		case EVA_KMD_PROP_SESSION_LATENCY:
			inst->pm_qos_latency = prop_array->data;
			dprintk(CVP_INFO, "inst %pK - New latency value from user %d\n",
				inst, inst->pm_qos_latency);
			break;
		case EVA_KMD_PROP_PKT_CONCURRENCY:
			session_prop->pkt_concurrency = prop_array->data;
			break;
		case EVA_KMD_PROP_SET_NAME:
		{
			u32 idx = i * 4;

			rc = cvp_session_name_copy_u32(idx, session_prop,
								(u32 *)&(prop_array->data));
			break;
		}
		default:
			rc = -EFAULT;
	}
	return rc;
}

static int msm_cvp_set_sysprop_pwr_hw(struct msm_cvp_inst *inst,
		struct eva_kmd_sys_property *prop_array)
{
	struct cvp_session_prop *session_prop;
	int rc = 0;

	session_prop = &inst->prop;

	switch (prop_array->prop_type) {
		case EVA_KMD_PROP_PWR_FDU:
			session_prop->cycles[HFI_HW_FDU] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_ICA:
			session_prop->cycles[HFI_HW_ICA] =
				div_by_1dot5(prop_array->data);
			break;
		case EVA_KMD_PROP_PWR_OD:
			session_prop->cycles[HFI_HW_OD] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_MPU:
			session_prop->cycles[HFI_HW_MPU] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_VADL:
			session_prop->cycles[HFI_HW_VADL] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_TOF:
			session_prop->cycles[HFI_HW_TOF] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_RGE:
			session_prop->cycles[HFI_HW_RGE] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_XRA:
			session_prop->cycles[HFI_HW_XRA] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_LSR:
			session_prop->cycles[HFI_HW_LSR] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_FW:
			session_prop->fw_cycles =
				div_by_1dot5(prop_array->data);
			break;
		case EVA_KMD_PROP_PWR_DDR:
			session_prop->ddr_bw = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_SYSCACHE:
			session_prop->ddr_cache = prop_array->data;
			break;
		default:
			rc = -EFAULT;
	}
	return rc;
}

static int msm_cvp_set_sysprop_pwr_op(struct msm_cvp_inst *inst,
		struct eva_kmd_sys_property *prop_array)
{
	struct cvp_session_prop *session_prop;
	int rc = 0;

	session_prop = &inst->prop;

	switch (prop_array->prop_type) {
		case EVA_KMD_PROP_PWR_FDU_OP:
			session_prop->op_cycles[HFI_HW_FDU] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_ICA_OP:
			session_prop->op_cycles[HFI_HW_ICA] =
				div_by_1dot5(prop_array->data);
			break;
		case EVA_KMD_PROP_PWR_OD_OP:
			session_prop->op_cycles[HFI_HW_OD] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_MPU_OP:
			session_prop->op_cycles[HFI_HW_MPU] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_VADL_OP:
			session_prop->op_cycles[HFI_HW_VADL] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_TOF_OP:
			session_prop->op_cycles[HFI_HW_TOF] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_RGE_OP:
			session_prop->op_cycles[HFI_HW_RGE] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_XRA_OP:
			session_prop->op_cycles[HFI_HW_XRA] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_LSR_OP:
			session_prop->op_cycles[HFI_HW_LSR] = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_FW_OP:
			session_prop->fw_op_cycles =
				div_by_1dot5(prop_array->data);
			break;
		case EVA_KMD_PROP_PWR_DDR_OP:
			session_prop->ddr_op_bw = prop_array->data;
			break;
		case EVA_KMD_PROP_PWR_SYSCACHE_OP:
			session_prop->ddr_op_cache = prop_array->data;
			break;
		default:
			rc = -EFAULT;
	}
	return rc;
}

int msm_cvp_set_sysprop(struct msm_cvp_inst *inst,
		struct eva_kmd_arg *arg)
{
	struct eva_kmd_sys_properties *props = &arg->data.sys_properties;
	struct eva_kmd_sys_property *prop_array;
	int i, rc = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (props->prop_num > MAX_KMD_PROP_NUM_PER_PACKET) {
		dprintk(CVP_ERR, "Too many properties %d to set\n",
			props->prop_num);
		return -E2BIG;
	}

	prop_array = &arg->data.sys_properties.prop_data[0];

	for (i = 0; i < props->prop_num; i++) {
		if (msm_cvp_set_sysprop_sess(inst, &prop_array[i], i)) {
			if (msm_cvp_set_sysprop_pwr_hw(inst, &prop_array[i])) {
				if (msm_cvp_set_sysprop_pwr_op(inst, &prop_array[i])) {
					dprintk(CVP_ERR,
						"unrecognized sys property to set %d\n",
						prop_array[i].prop_type);
					rc = -EFAULT;
				}
			}
		}
	}

	return rc;
}


int cvp_session_flush_all(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct msm_cvp_inst *s;
	struct cvp_hfi_ops *ops_tbl;
	u64 ktid;
	struct msm_cvp_core *core = NULL;

	CVPKERNEL_ATRACE_BEGIN("cvp_session_flush_all");

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = cvp_driver->cvp_core;
	if (!core) {
		dprintk(CVP_ERR, "%s: core is NULL", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(core, inst);
	if (!s)
		return -ECONNRESET;

	dprintk(CVP_SESS, "session %llx (%#x)flush all starts\n",
			inst, inst->sess_id);
	ops_tbl = inst->core->dev_ops;

	/* Boost EVA clock frequency before sending flush to FW*/
	msm_cvp_set_fmax(inst->core);

	dprintk(CVP_SESS, "%s: (%#x) send flush to fw\n",
			__func__, inst->sess_id);

	ktid = atomic64_inc_return(&inst->core->kernel_trans_id);
	ktid &= (FENCE_BIT - 1);
	rc = call_hfi_op(ops_tbl, session_flush, (void *)inst->session, ktid);
	if (rc) {
		dprintk(CVP_ERR, "%s: continue flush without fw. rc %d\n",
		__func__, rc);
		goto exit;
	}

	/* Wait for FW response */
	rc = wait_for_sess_signal_receipt(inst, HAL_SESSION_FLUSH_DONE);
	if (rc)
		dprintk(CVP_ERR, "%s: wait for signal failed, rc %d\n",
			__func__, rc);
	else
		dprintk(CVP_SESS, "%s: (%#x) received flush from fw\n",
			__func__, inst->sess_id);

exit:
	/* Restore original EVA clock freq */
	msm_cvp_set_clocks(inst->core);
	cvp_put_inst(s);
	CVPKERNEL_ATRACE_END("cvp_session_flush_all");
	return rc;
}

int msm_cvp_session_deinit(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct cvp_hal_session *session;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	dprintk(CVP_SESS, "%s: inst %pK (%#x)\n", __func__,
		inst, inst->sess_id);

	session = (struct cvp_hal_session *)get_sess_from_idr(inst);
	if (!session || session != inst->session)
		return rc;

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CLOSE_DONE);
	if (rc)
		dprintk(CVP_ERR, "%s: close failed\n", __func__);

	rc = msm_cvp_session_deinit_buffers(inst);
	return rc;
}

int msm_cvp_session_init(struct msm_cvp_inst *inst)
{
	int rc = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dprintk(CVP_SESS, "%s: inst %pK (%#x)\n", __func__,
		inst, inst->sess_id);

	/* set default frequency */
	inst->clk_data.min_freq = 1000;
	inst->clk_data.ddr_bw = 1000;
	inst->clk_data.sys_cache_bw = 1000;

	inst->prop.type = 1;
	inst->prop.kernel_mask = 0xFFFFFFFF;
	inst->prop.priority = 0;
	inst->prop.is_secure = 0;
	inst->prop.dsp_mask = 0;
	inst->prop.fthread_nr = 3;

	return rc;
}
