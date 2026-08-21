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
#include "cvp_presil.h"
#include "cvp_hfi.h"
#include "eva_gem.h"

extern bool trigger_smmu_fault;

void cvp_buf_map_set_vaddr(struct cvp_dma_buf_vmap *vmap, void *vaddr)
{
	#if (KERNEL_VERSION(5, 16, 0) > LINUX_VERSION_CODE)
		dma_buf_map_set_vaddr(&vmap->map, vaddr);
	#else
		iosys_map_set_vaddr(&vmap->map, vaddr);
	#endif
}

void msm_cvp_print_inst_bufs(struct msm_cvp_inst *inst, bool log);

int print_smem(u32 tag, const char *str, struct msm_cvp_inst *inst,
		struct msm_cvp_smem *smem)
{
	int i;
	char name[PKT_NAME_LEN] = "Unknown";


	if (!(tag & msm_cvp_debug))
		return 0;

	if (!inst || !smem) {
		dprintk(CVP_ERR, "Invalid inst 0x%llx or smem 0x%llx\n",
				inst, smem);
		return -EINVAL;
	}

	if (smem->dma_buf) {
		i = get_pkt_index_from_type(smem->pkt_type);
		if (i > 0)
			strscpy(name, cvp_hfi_defs[i].name, PKT_NAME_LEN);

		if (!atomic_read(&smem->refcount)) {
			dprintk(tag,
				"UNUSED mapping %s: 0x%llx size %x iova %#x\n",
				str, smem->dma_buf, smem->size, smem->device_addr);

			dprintk(tag,
				"pkt_type %s buf_idx %#x fd %d cached %d buf_name %s\n",
				 name, smem->buf_idx, smem->fd, smem->cached, smem->dma_buf->name);
		} else {
			dprintk(tag,
				"%s: %x : 0x%llx size %x flags %#x iova %#x\n",
				str, inst->sess_id, smem->dma_buf,
				smem->size, smem->flags, smem->device_addr);

			dprintk(tag,
				"ref %d pkt_type %s buf_idx %#x fd %d cached %d buf_name %s\n",
				atomic_read(&smem->refcount), name, smem->buf_idx,
				smem->fd, smem->cached, smem->dma_buf->name);
		}
	}
	return 0;
}

int print_smem_no_instance(u32 tag, const char *str,
		struct msm_cvp_smem *smem)
{
	int i;
	char name[PKT_NAME_LEN] = "Unknown";


	if (!(tag & msm_cvp_debug))
		return 0;

	if (!smem) {
		dprintk(CVP_ERR, "Invalid smem 0x%llx\n", smem);
		return -EINVAL;
	}

	if (smem->dma_buf) {
		i = get_pkt_index_from_type(smem->pkt_type);
		if (i > 0)
			strscpy(name, cvp_hfi_defs[i].name, PKT_NAME_LEN);

		if (!atomic_read(&smem->refcount)) {
			dprintk(tag,
				"UNUSED mapping %s: 0x%llx size %x iova %#x\n",
				str, smem->dma_buf, smem->size, smem->device_addr);

			dprintk(tag,
				"pkt_type %s buf_idx %#x fd %d cached %d\n",
				 name, smem->buf_idx, smem->fd, smem->cached);
		} else {
			dprintk(tag,
				"%s: 0x%llx size %x flags %#x iova %#x\n",
				str, smem->dma_buf, smem->size,
				smem->flags, smem->device_addr);

			dprintk(tag,
				"ref %d pkt_type %s buf_idx %#x fd %d cached %d\n",
				atomic_read(&smem->refcount), name, smem->buf_idx,
				smem->fd, smem->cached);
		}
	}
	return 0;
}


static void print_internal_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct cvp_internal_buf *cbuf)
{
	struct msm_cvp_smem *smem;

	if (!(tag & msm_cvp_debug) || !inst || !cbuf)
		return;
	
	smem = eva_gem_smem(cbuf->gem);
	if (!smem)
		return;

	if (smem->dma_buf) {
		dprintk(tag,
		"%s: %x : fd %d off %d 0x%llx %s size %d iova %#x\n",
		str, inst->sess_id, cbuf->fd,
		cbuf->offset, smem->dma_buf, smem->dma_buf->name,
		cbuf->size, smem->device_addr);
	} else {
		dprintk(tag,
		"%s: %x : idx %2d fd %d off %d size %d iova %#x\n",
		str, inst->sess_id, cbuf->index, cbuf->fd,
		cbuf->offset, cbuf->size, smem->device_addr);
	}
}

void print_cvp_buffer(u32 tag, const char *str, struct msm_cvp_inst *inst,
		struct cvp_internal_buf *cbuf)
{
	struct msm_cvp_smem *smem;

	if (!inst || !cbuf) {
		dprintk(CVP_ERR,
			"%s Invalid params inst %pK, cbuf %pK\n",
			str, inst, cbuf);
		return;
	}

	smem = eva_gem_smem(cbuf->gem);
	if (smem)
		print_smem(tag, str, inst, smem);
}

static void _log_smem(struct inst_snapshot *snapshot, struct msm_cvp_inst *inst,
		struct msm_cvp_smem *smem, bool logging)
{

	if (print_smem(CVP_ERR, "bufdump", inst, smem))
		return;
	if (!logging || !snapshot)
		return;
	if (snapshot && snapshot->smem_index < MAX_ENTRIES) {
		struct smem_data *s;
		s = &snapshot->smem_log[snapshot->smem_index];
		snapshot->smem_index++;
		s->size = smem->size;
		s->cached = smem->cached;
		s->flags = smem->flags;
		s->device_addr = smem->device_addr;
		s->refcount = atomic_read(&smem->refcount);
		s->pkt_type = smem->pkt_type;
		s->buf_idx = smem->buf_idx;
	}
}

static void _log_buf(struct inst_snapshot *snapshot, enum smem_prop prop,
		struct msm_cvp_inst *inst, struct cvp_internal_buf *cbuf,
		bool logging)
{
	struct cvp_buf_data *buf = NULL;
	struct msm_cvp_smem *smem;
	u32 index;

	smem = eva_gem_smem(cbuf->gem);
	if (!smem)
		return;

	if (prop == SMEM_CDSP && smem->pkt_type == 0)
		return;
	print_cvp_buffer(CVP_ERR, "bufdump", inst, cbuf);
	if (!logging)
		return;
	if (snapshot) {
		if (prop == SMEM_CDSP && snapshot->dsp_index < MAX_ENTRIES) {
			index = snapshot->dsp_index;
			buf = &snapshot->dsp_buf_log[index];
			snapshot->dsp_index++;
		} else if (prop == SMEM_PERSIST &&
				snapshot->persist_index < MAX_ENTRIES) {
			index = snapshot->persist_index;
			buf = &snapshot->persist_buf_log[index];
			snapshot->persist_index++;
		}
		if (buf) {
			buf->device_addr = smem->device_addr;
			buf->size = cbuf->size;
		}
	}
}

void print_client_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct eva_kmd_buffer *cbuf)
{
	if (!(tag & msm_cvp_debug) || !str || !inst || !cbuf)
		return;

	dprintk(tag,
		"%s: %x : idx %2d fd %d off %d size %d type %d flags 0x%x"
		" reserved[0] %u\n",
		str, inst->sess_id, cbuf->index, cbuf->fd,
		cbuf->offset, cbuf->size, cbuf->type, cbuf->flags,
		cbuf->reserved[0]);
}

void print_persist_buffer_info(u32 tag, const char *str, u32 buffer_size,
		struct msm_cvp_inst *inst, struct eva_kmd_hfi_packet *pkt)
{
	struct cvp_hfi_persist_buffer_packet *persist_pkt =
			(struct cvp_hfi_persist_buffer_packet *) pkt;

	if (!(tag & msm_cvp_debug) || !str || !inst)
		return;

	if (persist_pkt == NULL)
		dprintk(tag, "%s size %d total persist size = %d for session %s (%x)",
			str, buffer_size, atomic_read(&inst->persist_usage),
			inst->prop.session_name, inst->sess_id);
	else {
		dprintk(tag, "Feature: %s :{Persist 1 %lu Persist 2 %lu Persist 3 %lu}",
			get_feature_name_from_type(persist_pkt->nCVKernelType),
			persist_pkt->nPersist1Buffer.size, persist_pkt->nPersist2Buffer.size,
			persist_pkt->nPersist3Buffer.size);
	}
}

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

void msm_cvp_cache_operations(struct msm_cvp_smem *smem, u32 type,
				u32 offset, u32 size)
{
	enum smem_cache_ops cache_op;

	if (msm_cvp_cacheop_disabled)
		return;

	if (!smem) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return;
	}

	switch (type) {
	case EVA_KMD_BUFTYPE_INPUT:
		cache_op = SMEM_CACHE_CLEAN;
		break;
	case EVA_KMD_BUFTYPE_OUTPUT:
		cache_op = SMEM_CACHE_INVALIDATE;
		break;
	default:
		cache_op = SMEM_CACHE_CLEAN_INVALIDATE;
	}

	dprintk(CVP_MEM,
		"%s: cache operation enabled for dma_buf: %llx, cache_op: %d, offset: %d, size: %d\n",
		__func__, smem->dma_buf, cache_op, offset, size);
	msm_cvp_smem_cache_operations(smem->dma_buf, cache_op, offset, size);
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
		print_smem(CVP_MEM, "found in cache", inst, smem);
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
			print_smem(CVP_MEM, "found in persist", inst, smem);
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
				print_smem(CVP_MEM, "found in frame",
					inst, smem);
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
			print_smem(CVP_MEM, "free", inst, smem);
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
static struct msm_cvp_smem *msm_cvp_session_get_smem(struct msm_cvp_inst *inst,
						struct cvp_buf_type *buf,
						bool is_persist,
						u32 pkt_type)
{
	int rc = 0, found = 1;
	struct msm_cvp_smem *smem = NULL;
	struct dma_buf *dma_buf = NULL;

	if (buf->fd < 0) {
		dprintk(CVP_ERR, "%s: Invalid fd = %d", __func__, buf->fd);
		return NULL;
	}

	dma_buf = msm_cvp_smem_get_dma_buf(buf->fd);
	if (!dma_buf) {
		dprintk(CVP_ERR, "%s: Invalid fd = %d", __func__, buf->fd);
		return NULL;
	}

	if (is_persist) {
		smem = cvp_kmem_cache_zalloc(&cvp_driver->smem_cache, GFP_KERNEL);
		if (!smem)
			return NULL;

		smem->dma_buf = dma_buf;
		smem->pkt_type = pkt_type;
		smem->cached = false;
		smem->flags |= SMEM_PERSIST;
		smem->fd = buf->fd;
		atomic_inc(&smem->refcount);
		rc = msm_cvp_map_smem(inst, smem, "map cpu");
		if (rc)
			goto exit;
		if (!IS_CVP_BUF_VALID(buf, smem)) {
			dprintk(CVP_ERR,
				"%s: invalid offset %d or size %d persist\n",
				__func__, buf->offset, buf->size);
			goto exit2;
		}
		return smem;
	}
	smem = msm_cvp_session_find_smem(inst, dma_buf, pkt_type);
	if (!smem) {
		found = 0;
		smem = cvp_kmem_cache_zalloc(&cvp_driver->smem_cache, GFP_KERNEL);
		if (!smem)
			return NULL;

		smem->dma_buf = dma_buf;
		smem->cached = false;
		smem->pkt_type = pkt_type;
		smem->fd = buf->fd;
		if (is_params_pkt(pkt_type))
			smem->flags |= SMEM_PERSIST;
		rc = msm_cvp_map_smem(inst, smem, "map cpu");
		if (rc)
			goto exit;
		if (!IS_CVP_BUF_VALID(buf, smem)) {
			dprintk(CVP_ERR,
				"%s: invalid buf %d %d fd %d dma 0x%llx %s %d type %#x\n",
				__func__, buf->offset, buf->size, buf->fd,
				dma_buf, dma_buf->name, dma_buf->size, pkt_type);
			goto exit2;
		}
		rc = msm_cvp_session_add_smem(inst, smem);
		if (rc && rc != -ENOMEM)
			goto exit2;
		return smem;
	}
	if (!IS_CVP_BUF_VALID(buf, smem)) {
		dprintk(CVP_ERR, "%s: invalid offset %d or size %d found\n",
			__func__, buf->offset, buf->size);
		if (found) {
			mutex_lock(&inst->dma_cache.lock);
			atomic_dec(&smem->refcount);
			mutex_unlock(&inst->dma_cache.lock);
			return NULL;
		}
		goto exit2;
	}

	return smem;

exit2:
	rc = msm_cvp_unmap_smem(inst, smem, "unmap cpu");
	if (rc)
		dprintk(CVP_ERR, "%s: Fail to unmap smem 0x%x, error %d\n",
			__func__, smem, rc);
	else {
		msm_cvp_smem_put_dma_buf(dma_buf);
		cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
		smem = NULL;
		return smem;
	}

exit:
	msm_cvp_smem_put_dma_buf(dma_buf);
	cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
	smem = NULL;
	return smem;
}

static int msm_cvp_unmap_user_persist_buf(struct msm_cvp_inst *inst,
				struct cvp_buf_type *buf,
				u32 pkt_type, u32 buf_idx, u32 *iova,
				struct eva_kmd_hfi_packet *in_pkt)
{
	struct list_head *ptr;
	int rc = 0;
	struct list_head *next;
	struct cvp_internal_buf *pbuf;
	struct msm_cvp_smem *smem = NULL;
	struct dma_buf *dma_buf;
	int ret = -EINVAL;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dma_buf = msm_cvp_smem_get_dma_buf(buf->fd);
	if (!dma_buf)
		return -EINVAL;

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		pbuf = list_entry(ptr, struct cvp_internal_buf, list);

		if (pbuf->gem) {
			/* GEM-backed persist: check by GEM object identity */
			struct msm_cvp_smem *psmem = eva_gem_smem(pbuf->gem);
			if (psmem && psmem->dma_buf == dma_buf) {
				if (eva_gem_refcount_is_one(pbuf->gem)) {
					*iova = psmem->device_addr;
					dprintk(CVP_MEM,
						"Unmap persist fd %d, dma_buf %#llx iova %#x\n",
						pbuf->fd, smem->dma_buf, *iova);
					list_del(&pbuf->list);
					atomic_sub(pbuf->size, &inst->persist_usage);
					eva_gem_put(pbuf->gem);
					pbuf->gem = NULL;
					cvp_kmem_cache_free(&cvp_driver->buf_cache, pbuf);
					ret = 0;
					goto exit;
				} else {
					eva_gem_put(pbuf->gem);
				}
			}
		} else {
			smem = pbuf->smem;
			if (!smem || smem->dma_buf != dma_buf ||
			    !(smem->flags & SMEM_PERSIST))
				continue;

			if (atomic_dec_and_test(&smem->refcount)) {
				*iova = smem->device_addr;
				list_del(&pbuf->list);
				atomic_sub(pbuf->size, &inst->persist_usage);
				print_persist_buffer_info(CVP_MEM, "FREE user persist",
					smem->size, inst, in_pkt);
				rc = msm_cvp_unmap_smem(inst, smem, "unmap user persist");
				if (rc)
					dprintk(CVP_ERR,
					"%s: Fail to unmap smem 0x%x error %d\n",
					__func__, smem, rc);
				else
					msm_cvp_smem_put_dma_buf(smem->dma_buf);
				smem->device_addr = 0;
				cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
				pbuf->smem = NULL;
				cvp_kmem_cache_free(&cvp_driver->buf_cache, pbuf);
				ret = 0;
				break;
			}
			dprintk(CVP_INFO, "%s - pbuf in use, smem refcount: %d",
					__func__, atomic_read(&smem->refcount));
			ret = -EAGAIN;
			break;
		}
	}
exit:
	mutex_unlock(&inst->persistbufs.lock);
	dma_buf_put(dma_buf);

	return ret;
}

#ifndef CONFIG_EVA_SUN
enum cp_context_bank msm_cvp_get_cb(u32 flags)
{
	enum cp_context_bank buf_cb;
	switch (flags) {
		case (SMEM_SECURE | SMEM_PIXEL):
			buf_cb = CP_CB_3;
			break;
		case (SMEM_SECURE | SMEM_NON_PIXEL):
			buf_cb = CP_CB_4;
			break;
		case (SMEM_SECURE | SMEM_CAMERA):
			buf_cb = CP_CB_7;
			break;
		default:
			buf_cb = CP_CB_0;
	}
	return buf_cb;
}
#endif

static int msm_cvp_map_user_persist_buf(struct msm_cvp_inst *inst,
				struct cvp_buf_type *buf,
				u32 pkt_type, u32 buf_idx, u32 *iova,
				struct eva_kmd_hfi_packet *in_pkt)
{
	struct list_head *ptr;
	struct list_head *next;
	struct cvp_internal_buf *pbuf;
	struct drm_gem_object *obj = NULL;
	struct msm_cvp_smem *smem;
	int ret;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	ret = eva_gem_get_imported(inst, inst->file_priv, buf, EVA_GEM_USER_PERSIST, pkt_type, &obj);

	if (ret) {
		dprintk(CVP_ERR, "%s: gem get failed fd %d ret %d\n",
			__func__, buf->fd, ret);
		return ret;
	}
	smem = eva_gem_smem(obj);
	if (!smem || !IS_CVP_BUF_VALID(buf, smem)) {
		dprintk(CVP_ERR, "%s: invalid buf or smem fd %d\n",
			__func__, buf->fd);
		eva_gem_put(obj);
		return -EINVAL;
	}

	pbuf = cvp_kmem_cache_zalloc(&cvp_driver->buf_cache, GFP_KERNEL);
	if (!pbuf) {
		dprintk(CVP_ERR, "%s failed to allocate kmem obj\n",
			__func__);
		eva_gem_put(obj);
		return -ENOMEM;
	}

#ifndef CONFIG_EVA_SUN
	buf->context_bank_id = msm_cvp_get_cb(smem->flags);
#endif
	smem->buf_idx = buf_idx;
	pbuf->smem = NULL;
	pbuf->gem = obj;
	pbuf->fd = buf->fd;
	pbuf->size = buf->size;
	pbuf->offset = buf->offset;
	pbuf->ownership = CLIENT;

	atomic_add(pbuf->size, &inst->persist_usage);
	print_persist_buffer_info(CVP_MEM, "MAP user persist", pbuf->size,
		inst, NULL);
	mutex_lock(&inst->persistbufs.lock);
	list_add_tail(&pbuf->list, &inst->persistbufs.list);

	print_internal_buffer(CVP_MEM, "map persist", inst, pbuf);

#ifdef USE_PRESIL42
	presil42_send_map_user_persist_buffer(smem, iova, pbuf);
#endif

	*iova = smem->device_addr + buf->offset;
	mutex_unlock(&inst->persistbufs.lock);

	return 0;

exit:
	cvp_kmem_cache_free(&cvp_driver->buf_cache, pbuf);
	return ret;
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
	u32 ipcc_reg_base_iova;
	u32 ipcc_reg_size;
	int ret;

	core = cvp_driver->cvp_core;
	if (core) {
		ops_tbl = core->dev_ops;
		if (ops_tbl)
			dev = ops_tbl->hfi_device_data;
	}

	if (!dev)
		return -EINVAL;

	ipcc_reg_base_iova = dev->res->ipcc_reg_base_iova;
	ipcc_reg_size = dev->res->ipcc_reg_size;

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

#ifndef CONFIG_EVA_SUN
	buf->context_bank_id = msm_cvp_get_cb(smem->flags);
#endif
	smem->buf_idx = buf_idx;

	frame->bufs[nr].fd = buf->fd;
	frame->bufs[nr].smem = NULL;
	frame->bufs[nr].gem = obj;
	frame->bufs[nr].size = buf->size;
	frame->bufs[nr].offset = buf->offset;

	print_internal_buffer(CVP_MEM, "map cpu", inst, &frame->bufs[nr]);
	atomic_add(buf->size, &inst->frame_usage);

	frame->nr++;

	type = EVA_KMD_BUFTYPE_INPUT | EVA_KMD_BUFTYPE_OUTPUT;
	msm_cvp_cache_operations(smem, type, buf->offset, buf->size);

#ifdef USE_PRESIL42
	presil42_send_map_frame_buffer(smem, iova, buf);
#endif

	iova = smem->device_addr + buf->offset;

	if (trigger_smmu_fault) {
		frame_count++;
		if (frame_count % 200 == 0) {
			iova -= 0x1000000;
			if ((iova >= ipcc_reg_base_iova) &&
				(iova <= ipcc_reg_base_iova + ipcc_reg_size))
				iova += ipcc_reg_size * 2;
			frame_count = 0;
			trigger_smmu_fault = false;
			dprintk(CVP_ERR, "generating fault address %#x", iova);
		}
	}

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
		msm_cvp_cache_operations(smem, type, buf->offset, buf->size);

#ifdef USE_PRESIL42
	presil42_unmap_frame_buf(smem, buf);
#endif
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

static void backup_frame_buffers(struct msm_cvp_inst *inst,
			struct msm_cvp_frame *frame)
{
	/* Save frame buffers before unmap them */
	int i = frame->nr;
	struct msm_cvp_smem *smem;

	if (i == 0 || i > MAX_FRAME_BUFFER_NUMS)
		return;

	inst->last_frame.ktid = frame->ktid;
	inst->last_frame.nr = frame->nr;

	do {
		i--;
		smem = frame->bufs[i].gem ?
			eva_gem_smem(frame->bufs[i].gem) : frame->bufs[i].smem;
		if (smem->cached) {
			/*
			 * Frame buffer info can be found in dma_cache table,
			 * Skip saving
			 */
			inst->last_frame.nr = 0;
			return;
		}

		inst->last_frame.smem[i] = *smem;
	} while (i);
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
			backup_frame_buffers(inst, frame);
			msm_cvp_unmap_frame_buf(inst, frame);
			break;
		}
	}
	mutex_unlock(&inst->frames.lock);

	if (!found)
		dprintk(CVP_CMD, "%s frame %llx not found!\n", __func__, ktid);
}

/*
 * Unmap persistent buffer before sending RELEASE_PERSIST_BUFFERS to FW
 * This packet is sent after SESSION_STOP. The assumption is FW/HW will
 * NOT access any of the 3 persist buffer.
 */
int msm_cvp_unmap_user_persist(struct msm_cvp_inst *inst,
			struct eva_kmd_hfi_packet *in_pkt,
			unsigned int offset, unsigned int buf_num)
{
	struct cvp_buf_type *buf;
	struct cvp_hfi_cmd_session_hdr *cmd_hdr;
	int i, ret;
	u32 iova;

	dprintk(CVP_ERR, "%s: Unsupported request\n", __func__);
	return -EINVAL;

	if (!offset || !buf_num)
		return 0;

	if (offset < (sizeof(struct cvp_hfi_cmd_session_hdr)/sizeof(u32))) {
		dprintk(CVP_ERR, "%s: Incorrect offset in cmd %d\n", __func__, offset);
		return -EINVAL;
	}
	cmd_hdr = (struct cvp_hfi_cmd_session_hdr *)in_pkt;
	for (i = 0; i < buf_num; i++) {
		buf = (struct cvp_buf_type *)&in_pkt->pkt_data[offset];
		offset += sizeof(*buf) >> 2;

		if (buf->fd < 0 || !buf->size)
			continue;

		ret = msm_cvp_unmap_user_persist_buf(inst, buf,
				cmd_hdr->header.packet_type, i, &iova, in_pkt);
		if (ret) {
			dprintk(CVP_ERR,
				"%s: buf %d unmap failed.\n",
				__func__, i);

			return ret;
		}
		buf->fd = iova;
	}
	return 0;
}

int msm_cvp_map_user_persist(struct msm_cvp_inst *inst,
			struct eva_kmd_hfi_packet *in_pkt,
			unsigned int offset, unsigned int buf_num, uint32_t *fd_arr)
{
	struct cvp_buf_type *buf;
	struct cvp_hfi_cmd_session_hdr *cmd_hdr;
	int i, ret;
	u32 iova;
	u64 ktid;

	if (!offset || !buf_num)
		return 0;
	if (offset < (sizeof(struct cvp_hfi_cmd_session_hdr)/sizeof(u32))) {
		dprintk(CVP_ERR, "%s: Incorrect offset in cmd %d\n", __func__, offset);
		return -EINVAL;
	}

	/*Add kernel transaction ID for persist packet*/
	ktid = atomic64_inc_return(&inst->core->kernel_trans_id);
	ktid &= (FENCE_BIT - 1);
	cmd_hdr = (struct cvp_hfi_cmd_session_hdr *)in_pkt;
	cmd_hdr->header.client_data.kdata = ktid;
	for (i = 0; i < buf_num; i++) {
		buf = (struct cvp_buf_type *)&in_pkt->pkt_data[offset];
		offset += sizeof(*buf) >> 2;

		if (buf->fd < 0 || !buf->size) {
			continue;
		}

		ret = msm_cvp_map_user_persist_buf(inst, buf,
				cmd_hdr->header.packet_type, i, &iova, in_pkt);
		if (ret) {
			dprintk(CVP_ERR,
				"%s: buf %d map failed.\n",
				__func__, i);

			return ret;
		}

		fd_arr[i] = buf->fd;
		buf->fd = iova;

#ifdef USE_PRESIL42
		presil42_set_buf_fd(buf, iova, "cvp_map_user_persist");
#endif
	}
	print_persist_buffer_info(CVP_MEM, "MAP user persist", 0,
		inst, in_pkt);

	return 0;
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
	struct msm_cvp_inst *instance = (struct  msm_cvp_inst *)0xdeadbeef;
	struct msm_cvp_core *core = NULL;
	struct list_head *ptr = NULL, *next = NULL;

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
				list_for_each_entry(instance, &core->instances, list) {
					msm_cvp_print_inst_bufs(instance, false);
				}
				mutex_unlock(&core->lock);
				msm_cvp_unmap_frame_buf(inst, frame);
				return -EINVAL;
			}
		}
exit:
#ifdef USE_PRESIL42
		presil42_set_buf_fd(buf, iova, "cvp_map_frame");
#else
		buf->fd = iova;
#endif
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
	struct cvp_internal_buf *cbuf, *dummy;
	struct rb_node *node;
	struct msm_cvp_frame *frame = (struct msm_cvp_frame *)0xdeadbeef, *dummy1;
	struct msm_cvp_smem *smem;
	struct cvp_hal_session *session;
	struct eva_kmd_buffer buf;
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
			if (cbuf->ownership != DRIVER) {
				dprintk(CVP_MEM,
				"%s: sess_id %x : fd %d size %d (GEM)",
				"free user persistent", inst->sess_id, cbuf->fd,
				cbuf->size);
				atomic_sub(cbuf->size, &inst->persist_usage);
				print_persist_buffer_info(CVP_MEM, "FREE user persist",
							cbuf->size, inst, NULL);
				list_del(&cbuf->list);
				// deinit delete: diectly invoke the free ops instead of call put
				eva_gem_free_object(cbuf->gem);
				cbuf->gem = NULL;
				cvp_kmem_cache_free(&cvp_driver->buf_cache, cbuf);
			}
			continue;
		}
		// TODO: we can delete following code
		smem = cbuf->smem;
		if (!smem) {
			dprintk(CVP_ERR, "%s invalid persist smem\n", __func__);
			mutex_unlock(&inst->persistbufs.lock);
			return -EINVAL;
		}
		if (cbuf->ownership != DRIVER) {
			dprintk(CVP_MEM,
			"%s: sess_id %x : fd %d %pK size %d",
			"free user persistent", inst->sess_id, cbuf->fd,
			smem->dma_buf, cbuf->size);
			atomic_sub(cbuf->size, &inst->persist_usage);
			print_persist_buffer_info(CVP_MEM, "FREE user persist", cbuf->size,
						inst, NULL);
			list_del(&cbuf->list);
			if (smem->cached == false) {
				/*
				 * don't care refcount, has to remove mapping
				 * this is user persistent buffer
				 */
				if (smem->device_addr) {
					rc = msm_cvp_unmap_smem(inst, smem,
						"unmap persist");
					if (rc)
						dprintk(CVP_ERR, "%s: unmap smem 0x%x,error %d\n",
							__func__, smem, rc);
					else
						msm_cvp_smem_put_dma_buf(cbuf->smem->dma_buf);
					smem->device_addr = 0;
				}
				cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
				cbuf->smem = NULL;
				cvp_kmem_cache_free(&cvp_driver->buf_cache, cbuf);
			} else {
				/*
				 * DMM_PARAMS and WAP_NCC_PARAMS cases
				 * Leave dma_cache cleanup to unmap
				 */
				cbuf->smem = NULL;
				cvp_kmem_cache_free(&cvp_driver->buf_cache, cbuf);
			}
		}
	}
	mutex_unlock(&inst->persistbufs.lock);

	mutex_lock(&inst->dma_cache.lock);
	node = rb_first(&inst->dma_cache.rbtree);

	while (node && inst->dma_cache.nr > 0) {
		smem = rb_entry(node, struct msm_cvp_smem, node);

		if (atomic_read(&smem->refcount) == 0)
			print_smem(CVP_MEM, "free", inst, smem);

		else if (!(smem->flags & SMEM_PERSIST))
			print_smem(CVP_WARN, "in use", inst, smem);

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

#define MAX_NUM_FRAMES_DUMP 4
void msm_cvp_print_inst_bufs(struct msm_cvp_inst *inst, bool log)
{
	struct msm_cvp_smem *smem;
	struct cvp_internal_buf *buf = (struct cvp_internal_buf *)0xdeadbeef;
	struct msm_cvp_frame *frame = (struct msm_cvp_frame *)0xdeadbeef;
	struct msm_cvp_core *core;
	struct rb_node *node;
	struct inst_snapshot *snap = NULL;
	int i = 0, c = 0;

	// DSP trace related variables
	struct cvp_hal_session *session;
	u32 session_id;

	if (!inst) {
		dprintk(CVP_ERR, "%s - invalid param %pK\n",
			__func__, inst);
		return;
	}
	session = (struct cvp_hal_session *)inst->session;
	session_id = inst->sess_id;

	core = cvp_driver->cvp_core;
	if (core->kmd_trace.kmd_debug_log.log) {
		if (log && core->kmd_trace.kmd_debug_log.log->snapshot_index < 16) {
			snap = &core->kmd_trace.kmd_debug_log.log->snapshot[
				core->kmd_trace.kmd_debug_log.log->snapshot_index];
			snap->session = inst->sess_id;
			core->kmd_trace.kmd_debug_log.log->snapshot_index++;
		}
	}

	dprintk(CVP_ERR,
			"---Buffer details for inst: %pK %s of type: %d---\n",
			inst, inst->proc_name, inst->session_type);

	dprintk(CVP_ERR, "dma_cache entries %d frame_usage 0x%x, watermark 0x%x smem_count %x\n",
			inst->dma_cache.nr, atomic_read(&inst->frame_usage),
			atomic_read(&inst->va_inst_watermark),
			atomic_read(&inst->smem_count));

	mutex_lock(&inst->dma_cache.lock);

	for (node = rb_first(&inst->dma_cache.rbtree); node; node = rb_next(node)) {
		smem = rb_entry(node, struct msm_cvp_smem, node);
		if (smem)
			_log_smem(snap, inst, smem, log);
	}
	mutex_unlock(&inst->dma_cache.lock);

	i = 0;
	dprintk(CVP_ERR, "frame buffer list\n");
	mutex_lock(&inst->frames.lock);
	list_for_each_entry(frame, &inst->frames.list, list) {
		i++;
		if (i <= MAX_NUM_FRAMES_DUMP) {
			dprintk(CVP_ERR, "frame no %d tid %llx bufs\n",
					i, frame->ktid);
			for (c = 0; c < frame->nr; c++){
				struct msm_cvp_smem *fsmem = eva_gem_smem(frame->bufs[c].gem);
				_log_smem(snap, inst, fsmem, log);
			}
		}
	}
	if (i > MAX_NUM_FRAMES_DUMP)
		dprintk(CVP_ERR, "Skipped %d frames' buffers\n",
				(i - MAX_NUM_FRAMES_DUMP));
	mutex_unlock(&inst->frames.lock);

	mutex_lock(&inst->persistbufs.lock);
	dprintk(CVP_ERR, "persist buffer list:\n");
	list_for_each_entry(buf, &inst->persistbufs.list, list)
		_log_buf(snap, SMEM_PERSIST, inst, buf, log);
	mutex_unlock(&inst->persistbufs.lock);

	dprintk(CVP_ERR, "last frame ktid %llx\n", inst->last_frame.ktid);
	for (i = 0; i < inst->last_frame.nr; i++)
		_log_smem(snap, inst, &inst->last_frame.smem[i], log);

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

#ifdef USE_PRESIL42
	presil42_set_smem_flags(&smem_flags);
#endif
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
	print_persist_buffer_info(CVP_MEM, "MAP ARP buffer", buf->size,
				inst, NULL);

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
			print_persist_buffer_info(CVP_MEM, "FREE ARP buffer",
						buf->size, inst, NULL);
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

int cvp_allocate_dsp_bufs(struct cvp_internal_buf *buf,
			u32 buffer_size,
			u32 secure_type,
			const char *buf_name)
{
	u32 smem_flags = SMEM_UNCACHED;
	int rc = 0;

	if (!buf)
		return -EINVAL;

	if (!buffer_size)
		return -EINVAL;

	switch (secure_type) {
	case 0:
		break;
	case 1:
		smem_flags |= SMEM_SECURE | SMEM_PIXEL;
		break;
	case 2:
		smem_flags |= SMEM_SECURE | SMEM_NON_PIXEL;
		break;
	default:
		dprintk(CVP_ERR, "%s Invalid secure_type %d\n",
			__func__, secure_type);
		return -EINVAL;
	}

	dprintk(CVP_MEM, "%s smem_flags 0x%x\n", __func__, smem_flags);
	buf->smem = cvp_kmem_cache_zalloc(&cvp_driver->smem_cache, GFP_KERNEL);
	if (!buf->smem) {
		dprintk(CVP_ERR, "%s Out of memory\n", __func__);
		goto fail_kzalloc_smem_cache;
	}

	buf->smem->flags = smem_flags;
	rc = msm_cvp_smem_alloc(buffer_size, 1, 0,
			&(cvp_driver->cvp_core->resources), buf->smem, 0, buf_name);
	if (rc) {
		dprintk(CVP_ERR, "Failed to allocate DSP buf\n");
		goto err_no_mem;
	}

	buf->smem->pkt_type = buf->smem->buf_idx = 0;
	atomic_inc(&buf->smem->refcount);

	dprintk(CVP_MEM, "%s dma_buf %pK\n", __func__, buf->smem->dma_buf);

	buf->size = buf->smem->size;
	buf->type = HFI_BUFFER_INTERNAL_PERSIST_1;
	buf->ownership = DSP;

	return rc;

err_no_mem:
	cvp_kmem_cache_free(&cvp_driver->smem_cache, buf->smem);
fail_kzalloc_smem_cache:
	return rc;
}

int cvp_release_dsp_buffers(struct cvp_internal_buf *buf)
{
	struct msm_cvp_smem *smem;
	int rc = 0;

	if (!buf) {
		dprintk(CVP_ERR, "Invalid buffer pointer = %pK\n", buf);
		return -EINVAL;
	}

	smem = buf->smem;
	if (!smem) {
		dprintk(CVP_ERR, "%s invalid smem\n", __func__);
		return -EINVAL;
	}

	if (buf->ownership == DSP) {
		dprintk(CVP_MEM,
			"%s: fd %x %s size %d",
			__func__, buf->fd,
			smem->dma_buf ? smem->dma_buf->name : "internal", buf->size);
		if (atomic_dec_and_test(&smem->refcount)) {
			msm_cvp_smem_free(smem);
			cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
		}
	} else {
		dprintk(CVP_ERR,
			"%s: wrong owner %d : fd %x %s size %d",
			__func__, buf->ownership, buf->fd,
			smem->dma_buf ? smem->dma_buf->name : "internal", buf->size);
	}

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

