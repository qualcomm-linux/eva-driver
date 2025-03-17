// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "msm_cvp_internal.h"
#include "msm_cvp_debug.h"
#include "cvp_core_hfi.h"

void eva_kmd_buf_dump(struct msm_cvp_inst *inst,
		struct msm_cvp_smem *smem, int buf_map_type)
{
#ifdef CVP_SW_DBG_BUF_ENABLED
	struct msm_cvp_core *core = NULL;
	struct iris_hfi_device *dev = NULL;
	struct cvp_hfi_ops *ops_tbl = NULL;
	u32 *write_ptr = NULL;
	u32 write_idx = 0;
	u32 new_write_idx = 0;
	struct eva_kmd_buf *trace_buf = NULL;

	core = cvp_driver->cvp_core;
	if (core) {
		ops_tbl = core->dev_ops;
		if (ops_tbl)
			dev = ops_tbl->hfi_device_data;
	}
	if (!dev)
		dprintk(CVP_ERR, "%s: dev is NULL\n", __func__);
	else {
		mutex_lock(&core->kmd_dbg.dbg_lock);
		trace_buf = &(core->kmd_trace.kmd_buf[core->kmd_dbg.kmd_buf_cnt]);
		trace_buf->session_id = hash32_ptr(inst->session);
		trace_buf->map_type = buf_map_type;
		trace_buf->iova = smem->device_addr;
		trace_buf->size = smem->size;

		write_idx = core->kmd_dbg.kmd_buf_offset;
		write_ptr = (u32 *)((dev->sw_dbg_buf.align_virtual_addr) + (write_idx << 2));
		if (write_ptr < (u32 *)dev->sw_dbg_buf.align_virtual_addr ||
				write_ptr > (u32 *)(dev->sw_dbg_buf.align_virtual_addr
					+ EVA_SW_DBG_BUF_UMD_OFFSET)) {
			dprintk(CVP_ERR, "%s: write_ptr is OOB for buffer 0x%x\n",
					__func__, trace_buf->iova);
			mutex_unlock(&core->kmd_dbg.dbg_lock);
			return;
		}

		memcpy(write_ptr, trace_buf, sizeof(struct eva_kmd_buf));
		new_write_idx = write_idx + (sizeof(struct eva_kmd_buf) >> 2);
		core->kmd_dbg.kmd_buf_cnt++;
		if (core->kmd_dbg.kmd_buf_cnt >= DBG_BUF_CNT) {
			core->kmd_dbg.kmd_buf_cnt = 0;
			core->kmd_dbg.kmd_buf_offset = 0;
		} else {
			core->kmd_dbg.kmd_buf_offset = new_write_idx;
		}

		mutex_unlock(&core->kmd_dbg.dbg_lock);
	}
#endif
}

void eva_kmd_session_dump(void)
{
#ifdef CVP_SW_DBG_BUF_ENABLED
	struct msm_cvp_core *core = NULL;
	struct eva_kmd_session *kmd_trace_sess = NULL;
	struct msm_cvp_inst *inst = NULL;
	struct iris_hfi_device *dev = NULL;
	struct cvp_hfi_ops *ops_tbl = NULL;
	u32 *write_ptr = NULL;
	u32 write_idx = 0;
	u32 new_write_idx = 0;
	u32 trace_index = 0;

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

	list_for_each_entry(inst, &core->instances, list) {
		if (inst != NULL) {
			if (inst->state > MSM_CVP_OPEN) {
				kmd_trace_sess = &(core->kmd_trace.kmd_session[trace_index]);
				kmd_trace_sess->session_id = hash32_ptr(inst->session);
				kmd_trace_sess->instance_state = inst->state;
				kmd_trace_sess->session_type = inst->session_type;
				kmd_trace_sess->hfi_error_code = inst->hfi_error_code;
				kmd_trace_sess->prev_hfi_error_code = inst->prev_hfi_error_code;
				kmd_trace_sess->session_error_code = inst->session_error_code;


				write_idx = new_write_idx;
				write_ptr = (u32 *)((dev->sw_dbg_buf.align_virtual_addr)
						+ (write_idx << 2)
						+ EVA_SW_DBG_KMD_OFFLINE_DUMP_IDX);
				if (write_ptr < (u32 *)dev->sw_dbg_buf.align_virtual_addr ||
					write_ptr > (u32 *)(dev->sw_dbg_buf.align_virtual_addr
					+ EVA_SW_DBG_BUF_UMD_OFFSET)) {
					dprintk(CVP_ERR, "%s: write_ptr is OOB\n", __func__);
					return;
				}

				memcpy(write_ptr, kmd_trace_sess, sizeof(struct eva_kmd_session));
				new_write_idx = write_idx + (sizeof(struct eva_kmd_session) >> 2);
				trace_index++;
				if (trace_index >= TRACE_SESS_SIZE) {
					dprintk(CVP_ERR, "%s: reached max number of instances %d\n",
							__func__, trace_index);
					break;
				}
			} else
				dprintk(CVP_WARN, "%s: Not dumping as state is %d for inst %pK",
						__func__, inst->state, inst);
		}
	}
#endif
}

void eva_kmd_debug_log_dump(void)
{
#ifdef CVP_SW_DBG_BUF_ENABLED
	struct msm_cvp_core *core = NULL;
	u32 *write_ptr = NULL;
	struct iris_hfi_device *dev = NULL;
	struct cvp_hfi_ops *ops_tbl = NULL;

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

	write_ptr = (u32 *)((dev->sw_dbg_buf.align_virtual_addr) + EVA_SW_DBG_KMD_OFFLINE_DUMP_IDX
			+ (sizeof(struct eva_kmd_session) * TRACE_SESS_SIZE));
	if (write_ptr < (u32 *)dev->sw_dbg_buf.align_virtual_addr ||
			write_ptr > (u32 *)(dev->sw_dbg_buf.align_virtual_addr
				+ EVA_SW_DBG_BUF_UMD_OFFSET)) {
		dprintk(CVP_ERR, "%s: write_ptr is OOB for eva_kmd_debug_log\n", __func__);
		return;
	}
	memcpy(write_ptr, &(core->kmd_trace.kmd_debug_log), sizeof(struct eva_kmd_debug_log));
#endif
}
