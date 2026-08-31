// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/pid.h>
#include <linux/fdtable.h>
#include <linux/rcupdate.h>
#include <linux/fs.h>
#include <linux/dma-buf.h>
#include <linux/sched/task.h>
#include <linux/version.h>
#include "msm_cvp_common.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_core.h"
#include "cvp_core_hfi.h"
#include "cvp_hfi.h"
#include "eva_gem.h"

static bool is_mapped_persist_buf(struct msm_cvp_inst *inst, struct cvp_buf_type *buf, u32 *iova)
{
	struct cvp_internal_buf *pbuf = (struct cvp_internal_buf *)0xdeadbeef;
	struct list_head *ptr = (struct list_head *)0xdead;
	struct list_head *next = (struct list_head *)0xdead;
	struct msm_cvp_smem *smem;

	if (!inst || !inst->core || buf->fd < 0) {
		dprintk(CVP_ERR, "%s: invalid params", __func__);
		return false;
	}

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		if (!ptr) {
			mutex_unlock(&inst->persistbufs.lock);
			return false;
		}
		pbuf = list_entry(ptr, struct cvp_internal_buf, list);
		if (pbuf->fd == buf->fd && pbuf->size == buf->size) {
			smem = pbuf->gem ? eva_gem_smem(pbuf->gem) : pbuf->smem;
			if (smem)
				*iova = smem->device_addr;
			mutex_unlock(&inst->persistbufs.lock);
			return smem != NULL;
		}
	}
	mutex_unlock(&inst->persistbufs.lock);
	return false;
}

static struct msm_cvp_smem *find_smem_rb_node(struct msm_cvp_inst *inst, struct dma_buf *dma_buf)
{
	struct rb_node *entry_node = inst->dma_cache.rbtree.rb_node;

	while (entry_node) {
		struct msm_cvp_smem *data = rb_entry(entry_node, struct msm_cvp_smem, node);

		if (dma_buf < data->dma_buf)
			entry_node = entry_node->rb_left;
		else if (dma_buf > data->dma_buf)
			entry_node = entry_node->rb_right;
		else
			return data;
	}
	return NULL;
}
struct msm_cvp_smem *msm_cvp_session_find_smem(struct msm_cvp_inst *inst,
				struct dma_buf *dma_buf,
				u32 pkt_type)
{
	struct msm_cvp_smem *smem = NULL;
	struct msm_cvp_frame *frame = (struct msm_cvp_frame *)0xdeadbeef;
	struct cvp_internal_buf *buf = (struct cvp_internal_buf *)0xdeadbeef;
	int i;

	mutex_lock(&inst->dma_cache.lock);
	smem = find_smem_rb_node(inst, dma_buf);
	if (smem) {
		smem->pkt_type = pkt_type;
		smem->cached = true;
		atomic_inc(&smem->refcount);
		msm_cvp_smem_put_dma_buf(smem->dma_buf);
		mutex_unlock(&inst->dma_cache.lock);
		return smem;
	}
	mutex_unlock(&inst->dma_cache.lock);

	/* earch persist list */
	mutex_lock(&inst->persistbufs.lock);
	list_for_each_entry(buf, &inst->persistbufs.list, list) {
		smem = buf->smem;
		if (smem && smem->dma_buf == dma_buf) {
			atomic_inc(&smem->refcount);
			mutex_unlock(&inst->persistbufs.lock);
			return smem;
		}
	}
	mutex_unlock(&inst->persistbufs.lock);

	/* Search frame list */
	mutex_lock(&inst->frames.lock);
	list_for_each_entry(frame, &inst->frames.list, list) {
		for (i = 0; i < frame->nr; i++) {
			smem = frame->bufs[i].smem;
			if (smem && smem->dma_buf == dma_buf) {
				atomic_inc(&smem->refcount);
				mutex_unlock(&inst->frames.lock);
				return smem;
			}
		}
	}
	mutex_unlock(&inst->frames.lock);
	return NULL;
}

static int msm_cvp_free_unused_mapping(struct msm_cvp_inst *inst)
{
	struct msm_cvp_core *core = cvp_driver->cvp_core;
	struct rb_node *node;
	int rc = 0;
	int num_freed_mappings = 0;

	mutex_lock(&inst->dma_cache.lock);
	node = rb_first(&inst->dma_cache.rbtree);
	while (node) {
		struct rb_node *next = rb_next(node);
		struct msm_cvp_smem *smem = rb_entry(node, struct msm_cvp_smem, node);

		if (atomic_read(&smem->refcount) == 0) {
			rb_erase(&smem->node, &inst->dma_cache.rbtree);
			rc = eva_gem_unmap_iova(to_eva_gem(smem->gem));
			if (rc)
				dprintk(CVP_ERR, "%s: Fail to unmap smem 0x%x, error %d\n",
					__func__, smem, rc);
			else {
				msm_cvp_smem_put_dma_buf(smem->dma_buf);
				atomic_sub(smem->size, &inst->va_inst_watermark);
				atomic_sub(smem->size, &core->va_watermark);
			}
			cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
			inst->dma_cache.nr--;
			num_freed_mappings++;
		}
		node = next;
	}
	mutex_unlock(&inst->dma_cache.lock);
	return num_freed_mappings;
}

static void msm_cvp_add_smem_rb_node(struct msm_cvp_inst *inst,
			struct msm_cvp_smem *smem)
{
	struct rb_node **node, *parent = NULL;
	struct msm_cvp_smem *smem2;
	struct msm_cvp_core *core = cvp_driver->cvp_core;

	mutex_lock(&inst->dma_cache.lock);
	node = &inst->dma_cache.rbtree.rb_node;
	while (*node != NULL) {
		parent = *node;
		smem2 = rb_entry(parent, struct msm_cvp_smem, node);

		if (smem->dma_buf < smem2->dma_buf)
			node = &parent->rb_left;
		else
			node = &parent->rb_right;
	}
	smem->cached = true;
	/* Insert node as a child at the bottom of the tree and then sort tree*/
	rb_link_node(&smem->node, parent, node);
	rb_insert_color(&smem->node, &inst->dma_cache.rbtree);
	inst->dma_cache.nr++;
	mutex_unlock(&inst->dma_cache.lock);
	atomic_add(smem->size, &inst->va_inst_watermark);
	atomic_add(smem->size, &core->va_watermark);
}

int msm_cvp_session_add_smem(struct msm_cvp_inst *inst,
				struct msm_cvp_smem *smem)
{
	struct msm_cvp_core *core;
	int freed_mappings = 0;

	core = cvp_driver->cvp_core;
	if (inst->dma_cache.nr < inst->dma_cache.max_capacity) {
		msm_cvp_add_smem_rb_node(inst, smem);
	} else {

		freed_mappings = msm_cvp_free_unused_mapping(inst);

		if(inst->dma_cache.max_capacity < 256)
			inst->dma_cache.max_capacity = 256;

		if(freed_mappings == 0)
		{
			atomic_inc(&smem->refcount);
			dprintk(CVP_MEM,
				"%s: reached limit, fallback to buf mapping list\n", __func__);
			dprintk(CVP_MEM,
				"%s: fd %d, dma_buf %#llx, smem->refcount %d\n",
				__func__, smem->fd, smem->dma_buf, atomic_read(&smem->refcount));
			return -ENOMEM;
		}
		else
			msm_cvp_add_smem_rb_node(inst, smem);
	}
	atomic_inc(&smem->refcount);
	dprintk(CVP_MEM, "%s: Added entry into cache total %d\n", __func__, inst->dma_cache.nr);
	dprintk(CVP_MEM, "%s: fd %d, dma_buf %#llx, smem->refcount %d\n",
		__func__, smem->fd, smem->dma_buf, atomic_read(&smem->refcount));

	if (atomic_read(&core->va_watermark) > IOVA_THRESHOLD) {
		/* Only schedule if not already pending */
		if (!work_pending(&core->iova_cleanup_work))
			schedule_work(&core->iova_cleanup_work);
	}

	CVPKERNEL_ATRACE_END("msm_cvp_add_smem_call");

	return 0;
}

void msm_cvp_iova_cleanup_handler(struct work_struct *work)
{
	CVPKERNEL_ATRACE_BEGIN("msm_cvp_cleanup_handler");
	struct msm_cvp_core *core;
	struct msm_cvp_inst *inst = NULL, *inst_temp;
	int freed_mappings = 0;

	if (!work)
		return;

	core = container_of(work, struct msm_cvp_core, iova_cleanup_work);

	if (!core) {
		dprintk(CVP_ERR, "%s: Invalid params\n", __func__);
		return;
	}
	dprintk(CVP_WARN, "%s: reached IOVA threashold %x. cleaning up ...\n",
		__func__,  atomic_read(&core->va_watermark));

	mutex_lock(&core->lock);
	list_for_each_entry_safe(inst, inst_temp, &core->instances, list) {
            freed_mappings += msm_cvp_free_unused_mapping(inst);
	}
	mutex_unlock(&core->lock);

	CVPKERNEL_ATRACE_END("msm_cvp_cleanup_handler");

	dprintk(CVP_WARN,
		"%s: post clean up IOVA 0x%x\n", __func__, atomic_read(&core->va_watermark));
}
/* for trigger smmu fault */
static u32 frame_count;

static u32 msm_cvp_map_frame_buf(struct msm_cvp_inst *inst,
			struct cvp_buf_type *buf,
			struct msm_cvp_frame *frame,
			u32 pkt_type, u32 buf_idx)
{
	u32 iova = 0;
	struct msm_cvp_smem *smem = NULL;
	struct drm_gem_object *obj = NULL;
	struct msm_cvp_core *core;
	struct cvp_hfi_ops *ops_tbl;
	struct iris_hfi_device *dev = NULL;
	u32 nr;
	u32 type;
	int ret;

	core = cvp_driver->cvp_core;
	if (core) {
		ops_tbl = core->dev_ops;
		if (ops_tbl)
			dev = ops_tbl->hfi_device_data;
	}

	if (!dev)
		return -EINVAL;

	if (!inst || !frame) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return 0;
	}

	nr = frame->nr;
	if (nr == MAX_FRAME_BUFFER_NUMS) {
		dprintk(CVP_ERR, "%s: max frame buffer reached\n", __func__);
		return 0;
	}

	ret = eva_gem_get_imported(inst, inst->file_priv, buf, EVA_GEM_FRAME, pkt_type, &obj);
	if (ret) {
		dprintk(CVP_ERR, "%s: gem get failed fd %d ret %d\n",
			__func__, buf->fd, ret);
		return 0;
	}
	smem = eva_gem_smem(obj);
	if (!smem || !IS_CVP_BUF_VALID(buf, smem)) {
		dprintk(CVP_ERR, "%s: invalid buf or smem fd %d\n",
			__func__, buf->fd);
		eva_gem_put(obj);
		return 0;
	}


	smem->buf_idx = buf_idx;

	frame->bufs[nr].fd = buf->fd;
	frame->bufs[nr].smem = NULL;
	frame->bufs[nr].gem = obj;
	frame->bufs[nr].size = buf->size;
	frame->bufs[nr].offset = buf->offset;

	atomic_add(buf->size, &inst->frame_usage);

	frame->nr++;

	type = EVA_KMD_BUFTYPE_INPUT | EVA_KMD_BUFTYPE_OUTPUT;
	iova = smem->device_addr + buf->offset;

	return iova;
}

static void msm_cvp_unmap_frame_buf(struct msm_cvp_inst *inst,
			struct msm_cvp_frame *frame)
{
	u32 i;
	u32 type;
	int rc = 0;
	struct msm_cvp_smem *smem = NULL;
	struct cvp_internal_buf *buf;

	type = EVA_KMD_BUFTYPE_OUTPUT;

	for (i = 0; i < frame->nr; ++i) {
		buf = &frame->bufs[i];
		smem = eva_gem_smem(buf->gem);

		if (!smem) {
			dprintk(CVP_ERR, "%s: Invalid smem\n", __func__);
			continue;
		}
			if (smem->cached == true) {
				eva_gem_put(buf->gem);
			} else {
				if (atomic_dec_and_test(&smem->refcount)) {
					rc = eva_gem_unmap_iova(to_eva_gem(smem->gem));
					if (rc)
						dprintk(CVP_ERR, "%s:unmap smem 0x%x,error %d\n",
							__func__, smem, rc);
					else
						msm_cvp_smem_put_dma_buf(smem->dma_buf);
					smem->buf_idx |= 0xdead0000;
					cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
					buf->smem = NULL;
				}
			}
			atomic_sub(buf->size, &inst->frame_usage);
	}
	cvp_kmem_cache_free(&cvp_driver->frame_cache, frame);
}

void msm_cvp_unmap_frame(struct msm_cvp_inst *inst, u64 ktid)
{
	struct msm_cvp_frame *frame = (struct msm_cvp_frame *)0xdeadbeef, *dummy1;
	bool found;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return;
	}

	ktid &= (FENCE_BIT - 1);
	dprintk(CVP_MEM, "%s: (%#x) unmap frame %llx\n",
			__func__, inst->sess_id, ktid);

	found = false;
	mutex_lock(&inst->frames.lock);
	list_for_each_entry_safe(frame, dummy1, &inst->frames.list, list) {
		if (frame->ktid == ktid) {
			found = true;
			list_del(&frame->list);
			dprintk(CVP_CMD, "%s: pkt_type %08x sess_id %08x trans_id <> ktid %llx\n",
				__func__, frame->pkt_type,
				inst->sess_id,
				frame->ktid);
			/* Save the previous frame mappings for debug */
			msm_cvp_unmap_frame_buf(inst, frame);
			break;
		}
	}
	mutex_unlock(&inst->frames.lock);

	if (!found)
		dprintk(CVP_CMD, "%s frame %llx not found!\n", __func__, ktid);
}

int msm_cvp_map_frame(struct msm_cvp_inst *inst,
		struct eva_kmd_hfi_packet *in_pkt,
		unsigned int offset, unsigned int buf_num)
{
	struct cvp_buf_type *buf;
	int i;
	u32 iova;
	u64 ktid;
	struct msm_cvp_frame *frame;
	struct cvp_hfi_cmd_session_hdr *cmd_hdr;
	struct msm_cvp_core *core = NULL;

	core = cvp_driver->cvp_core;
	if (!core)
		return -EINVAL;

	/*Add kernel transaction ID for config packet*/
	ktid = atomic64_inc_return(&inst->core->kernel_trans_id);
	ktid &= (FENCE_BIT - 1);
	cmd_hdr = (struct cvp_hfi_cmd_session_hdr *)in_pkt;
	cmd_hdr->header.client_data.kdata = ktid;

	if (!offset || !buf_num)
		return 0;

	if (offset < (sizeof(struct cvp_hfi_cmd_session_hdr)/sizeof(u32))) {
		dprintk(CVP_ERR, "%s: Incorrect offset in cmd %d\n", __func__, offset);
		return -EINVAL;
	}

	dprintk(CVP_CMD, "%s: pkt_type %08x sess_id %08x trans_id %u ktid %llx\n",
		__func__, cmd_hdr->header.packet_type,
		cmd_hdr->header.session_id,
		cmd_hdr->header.client_data.transaction_id,
		cmd_hdr->header.client_data.kdata & (FENCE_BIT - 1));

	frame = cvp_kmem_cache_zalloc(&cvp_driver->frame_cache, GFP_KERNEL);
	if (!frame)
		return -ENOMEM;
	frame->ktid = ktid;
	frame->nr = 0;
	frame->pkt_type = cmd_hdr->header.packet_type;
	frame->stream_idx = cmd_hdr->header.stream_idx;

	for (i = 0; i < buf_num; i++) {
		buf = (struct cvp_buf_type *)&in_pkt->pkt_data[offset];
		offset += sizeof(*buf) >> 2;

		if (buf->fd < 0 || !buf->size) {
			buf->fd = 0;
			buf->size = 0;
			continue;
		}

		/* if config packet and the buffer is in persist list */
		if (is_config_pkt((struct cvp_hal_session_cmd_pkt *)cmd_hdr) &&
					is_mapped_persist_buf(inst, buf, &iova)) {
			goto exit;
		} else {
			iova = msm_cvp_map_frame_buf(inst, buf,
						frame, cmd_hdr->header.packet_type, i);
			if (!iova) {
				dprintk(CVP_ERR,
					"%s: buf %d register failed.\n",
					__func__, i);
				dprintk(CVP_ERR, "smem_leak_count %d\n", core->smem_leak_count);
				mutex_lock(&core->lock);
				mutex_unlock(&core->lock);
				msm_cvp_unmap_frame_buf(inst, frame);
				return -EINVAL;
			}
		}
exit:
		buf->fd = iova;
	}

	mutex_lock(&inst->frames.lock);
	list_add_tail(&frame->list, &inst->frames.list);
	mutex_unlock(&inst->frames.lock);
	dprintk(CVP_MEM, "%s: map frame %llx\n", __func__, ktid);
	return 0;
}

int msm_cvp_session_deinit_buffers(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct cvp_internal_buf *cbuf;
	struct rb_node *node;
	struct msm_cvp_frame *frame = (struct msm_cvp_frame *)0xdeadbeef, *dummy1;
	struct msm_cvp_smem *smem;
	struct cvp_hal_session *session;
	struct list_head *ptr = (struct list_head *)0xdead;
	struct list_head *next = (struct list_head *)0xdead;
	struct msm_cvp_core *core = cvp_driver->cvp_core;

	session = (struct cvp_hal_session *)inst->session;

	mutex_lock(&inst->frames.lock);
	list_for_each_entry_safe(frame, dummy1, &inst->frames.list, list) {
		list_del(&frame->list);
		msm_cvp_unmap_frame_buf(inst, frame);
	}
	mutex_unlock(&inst->frames.lock);

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		if (!ptr)
			return -EINVAL;
		cbuf = list_entry(ptr, struct cvp_internal_buf, list);
		if (cbuf->gem) {
			continue;
		}
	}
	mutex_unlock(&inst->persistbufs.lock);

	mutex_lock(&inst->dma_cache.lock);
	node = rb_first(&inst->dma_cache.rbtree);

	while (node && inst->dma_cache.nr > 0) {
		smem = rb_entry(node, struct msm_cvp_smem, node);
		rb_erase(&smem->node, &inst->dma_cache.rbtree);
		rc = msm_cvp_unmap_smem(inst, smem, "unmap cpu cache");
		if (rc)
			dprintk(CVP_ERR, "%s: Fail to unmap smem 0x%x, error %d\n",
				__func__, smem, rc);
		else {
			msm_cvp_smem_put_dma_buf(smem->dma_buf);
			atomic_sub(smem->size, &inst->va_inst_watermark);
			atomic_sub(smem->size, &core->va_watermark);
		}
		cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
		node = rb_first(&inst->dma_cache.rbtree);
		inst->dma_cache.nr--;
	}
	mutex_unlock(&inst->dma_cache.lock);

	return rc;
}


struct cvp_internal_buf *cvp_allocate_arp_bufs(struct msm_cvp_inst *inst,
			u32 buffer_size)
{
	struct cvp_internal_buf *buf;
	struct msm_cvp_list *buf_list;
	struct msm_cvp_smem *smem;
	u32 smem_flags = SMEM_UNCACHED;
	int rc = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s Invalid input\n", __func__);
		return NULL;
	}

	buf_list = &inst->persistbufs;

	if (!buffer_size)
		return NULL;

	/* If PERSIST buffer requires secure mapping, uncomment
	 * below flags setting
	 * smem_flags |= SMEM_SECURE | SMEM_NON_PIXEL;
	 */

	buf = cvp_kmem_cache_zalloc(&cvp_driver->buf_cache, GFP_KERNEL);
	if (!buf) {
		dprintk(CVP_ERR, "%s Out of memory\n", __func__);
		goto fail_kzalloc;
	}

	smem = cvp_kmem_cache_zalloc(&cvp_driver->smem_cache, GFP_KERNEL);
	if (!smem) {
		dprintk(CVP_ERR, "%s Out of memory\n", __func__);
		goto err_no_smem;
	}

	smem->flags = smem_flags;
	rc = msm_cvp_smem_alloc(buffer_size, 1, 0, /* 0: no mapping in kernel space */
		&(inst->core->resources), smem, 0, "ARP");
	if (rc) {
		dprintk(CVP_ERR, "Failed to allocate ARP memory\n");
		goto err_no_mem;
	}

	smem->pkt_type = smem->buf_idx = 0;
	buf->gem = smem->gem;
	// atomic_inc(&buf->smem->refcount);
	buf->size = smem->size;
	buf->type = HFI_BUFFER_INTERNAL_PERSIST_1;
	buf->ownership = DRIVER;
	atomic_add(buf->size, &inst->persist_usage);

	mutex_lock(&buf_list->lock);
	list_add_tail(&buf->list, &buf_list->list);
	mutex_unlock(&buf_list->lock);
	return buf;

err_no_mem:
	cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
err_no_smem:
	cvp_kmem_cache_free(&cvp_driver->buf_cache, buf);
fail_kzalloc:
	return NULL;
}

int cvp_release_arp_buffers(struct msm_cvp_inst *inst)
{
	// struct msm_cvp_smem *smem;
	struct msm_cvp_smem *smem;
	struct list_head *ptr = (struct list_head *)0xdead;
	struct list_head *next = (struct list_head *)0xdead;
	struct cvp_internal_buf *buf;
	int rc = 0;
	struct msm_cvp_core *core;
	struct cvp_hfi_ops *ops_tbl;

	if (!inst) {
		dprintk(CVP_ERR, "Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}

	core = inst->core;
	if (!core) {
		dprintk(CVP_ERR, "Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	ops_tbl = core->dev_ops;
	if (!ops_tbl) {
		dprintk(CVP_ERR, "Invalid device pointer = %pK\n", ops_tbl);
		return -EINVAL;
	}

	dprintk(CVP_MEM, "release persist buffer!\n");

	mutex_lock(&inst->persistbufs.lock);
	/* Workaround for FW: release buffer means release all */
	if (inst->state > MSM_CVP_CORE_INIT_DONE && inst->state <= MSM_CVP_CLOSE_DONE) {
		rc = call_hfi_op(ops_tbl, session_release_buffers,
				(void *)inst->session);
		if (!rc) {
			mutex_unlock(&inst->persistbufs.lock);
			rc = wait_for_sess_signal_receipt(inst,
				HAL_SESSION_RELEASE_BUFFER_DONE);
			if (rc)
				dprintk(CVP_WARN,
				"%s: wait release_arp signal failed, rc %d\n",
				__func__, rc);
			mutex_lock(&inst->persistbufs.lock);
		} else {
			dprintk_rl(CVP_WARN, "Fail to send Rel prst buf\n");
		}
	}

	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		if (!ptr)
			return -EINVAL;
		buf = list_entry(ptr, struct cvp_internal_buf, list);
		smem = to_eva_gem(buf->gem)->smem;
		if (!smem) {
			dprintk(CVP_ERR, "%s invalid smem\n", __func__);
			mutex_unlock(&inst->persistbufs.lock);
			return -EINVAL;
		}

		if (buf->ownership == DRIVER) {
			// dprintk(CVP_MEM,
			// "%s: %x : fd %d %pK size %d",
			// "free arp", inst->sess_id, buf->fd,
			// smem->dma_buf, buf->size);
			atomic_sub(buf->size, &inst->persist_usage);
			list_del(&buf->list);
			// atomic_dec(&smem->refcount);
			// invoke following API for internal buffers which will free smem and put gem
			msm_cvp_smem_free(smem);
			cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
			// buf->smem = NULL;
			buf->gem = NULL;
			cvp_kmem_cache_free(&cvp_driver->buf_cache, buf);
		}
	}
	mutex_unlock(&inst->persistbufs.lock);
	return rc;
}

int msm_cvp_dma_buf_vmap(struct dma_buf *dmabuf, struct cvp_dma_buf_vmap *vmap)
{
	int ret = 0;

	#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
		ret = dma_buf_vmap_unlocked(dmabuf, &vmap->map);
		vmap->vaddr = vmap->map.vaddr;
	#elif (KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE)
		ret = dma_buf_vmap(dmabuf, &vmap->map);
		vmap->vaddr = vmap->map.vaddr;
	#else
		vmap->vaddr = dma_buf_vmap(dmabuf);
		if (!vmap->vaddr)
			ret = -EINVAL;
	#endif

	return ret;
}

void msm_cvp_dma_buf_vunmap(struct dma_buf *dmabuf, struct cvp_dma_buf_vmap *vmap)
{
	#if (KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE)
		dma_buf_vunmap_unlocked(dmabuf, &vmap->map);
	#elif (KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE)
		dma_buf_vunmap(dmabuf, &vmap->map);
	#else
		dma_buf_vunmap(dmabuf, vmap->vaddr);
	#endif
}

