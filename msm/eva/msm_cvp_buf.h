/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.​
 */

#ifndef _MSM_CVP_BUF_H_
#define _MSM_CVP_BUF_H_
#include <linux/version.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/refcount.h>
#include <media/msm_eva_private.h>
#include "cvp_comm_def.h"

#define MAX_FRAME_BUFFER_NUMS 40
#ifdef USE_PRESIL42
#define IOVA_THRESHOLD 0x30000000
#else
#define IOVA_THRESHOLD 2147483648
#endif
#define IS_CVP_BUF_VALID(buf, smem) \
	((buf->size <= smem->size) && \
	(buf->size <= smem->size - buf->offset))

struct msm_cvp_inst;
struct drm_gem_object;
struct msm_cvp_platform_resources;
struct msm_cvp_list;

enum smem_cache_ops {
	SMEM_CACHE_CLEAN,
	SMEM_CACHE_INVALIDATE,
	SMEM_CACHE_CLEAN_INVALIDATE,
};

enum smem_prop {
	SMEM_UNCACHED = 0x1,
	SMEM_CACHED = 0x2,
	SMEM_SECURE = 0x4,
	SMEM_CDSP = 0x8,
	SMEM_NON_PIXEL = 0x10,
	SMEM_PIXEL = 0x20,
	SMEM_CAMERA = 0x40,
	SMEM_PERSIST = 0x100,
};

enum cp_context_bank {
	CP_CB_0 = 0,
	CP_CB_3 = 3,
	CP_CB_4 = 4,
	CP_CB_7 = 7,
};

struct msm_cvp_list {
	struct list_head list;
	struct mutex lock;
};

static inline void INIT_MSM_CVP_LIST(struct msm_cvp_list *mlist)
{
	mutex_init(&mlist->lock);
	INIT_LIST_HEAD(&mlist->list);
}

static inline void DEINIT_MSM_CVP_LIST(struct msm_cvp_list *mlist)
{
	mutex_destroy(&mlist->lock);
}

struct cvp_dma_mapping_info {
	struct device *dev;
	struct iommu_domain *domain;
	struct sg_table *table;
	struct dma_buf_attachment *attach;
	struct dma_buf *buf;
	void *cb_info;
};

struct cvp_dma_buf_vmap {
#if (KERNEL_VERSION(5, 18, 0) <= LINUX_VERSION_CODE)
	struct iosys_map map;
#elif (KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE)
	struct dma_buf_map map;
#endif
	void *vaddr;
};

struct msm_cvp_smem {
	struct rb_node node;
	struct list_head list;
	atomic_t refcount;
	struct dma_buf *dma_buf;
	struct drm_gem_object *gem;
	void *kvaddr;
	struct cvp_dma_buf_vmap vmap;
	u32 device_addr;
	dma_addr_t dma_handle;
	u32 size;
	bool cached;
	u32 flags;
	u32 pkt_type;
	u32 buf_idx;
	u32 fd;
	struct cvp_dma_mapping_info mapping_info;
};

struct cvp_dmamap_cache {
	struct mutex lock;
	struct rb_root rbtree;
	unsigned int nr;
	unsigned int max_capacity;
};

static inline void INIT_DMAMAP_CACHE(struct cvp_dmamap_cache *cache)
{
	mutex_init(&cache->lock);
	cache->rbtree = RB_ROOT;
	cache->nr = 0;
	cache->max_capacity = 128;
}

static inline void DEINIT_DMAMAP_CACHE(struct cvp_dmamap_cache *cache)
{
	mutex_destroy(&cache->lock);
	cache->rbtree = RB_ROOT;
	cache->nr = 0;
	cache->max_capacity = 128;
}

#define INPUT_FENCE_BITMASK 0x1
#define OUTPUT_FENCE_BITMASK 0x2

/* Track source of dma_buf allocator/owner */
enum buffer_owner {
	DRIVER,		/* Allocated by KMD, for CPU driver */
	CLIENT,		/* Allocated by Client (DSP or CPU) */
	DSP,		/* Allocated by KMD, for DSP driver */
	MAX_OWNER
};

struct cvp_internal_buf {
	struct list_head list;
	s32 fd;
	u32 size;
	u32 offset;
	u32 type;
	u32 index;
	u64 ktid;
	enum buffer_owner ownership;
	struct msm_cvp_smem *smem;
	struct drm_gem_object *gem;
};

struct msm_cvp_frame {
	struct list_head list;
	struct cvp_internal_buf bufs[MAX_FRAME_BUFFER_NUMS];
	u32 nr;
	u64 ktid;
	u32 pkt_type;
	u32 stream_idx;
};

struct cvp_frame_bufs {
	u64 ktid;
	u32 nr;
	struct msm_cvp_smem smem[MAX_FRAME_BUFFER_NUMS];
};

void print_cvp_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst,
		struct cvp_internal_buf *cbuf);
void print_cvp_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst,
		struct cvp_internal_buf *cbuf);
void print_client_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst,
		struct eva_kmd_buffer *cbuf);
int print_smem(u32 tag, const char *str,
		struct msm_cvp_inst *inst,
		struct msm_cvp_smem *smem);
int print_smem_no_instance(u32 tag, const char *str,
		struct msm_cvp_smem *smem);
/*Kernel DMA buffer and IOMMU mapping functions*/
int msm_cvp_smem_alloc(size_t size, u32 align, int map_kernel,
			void  *res, struct msm_cvp_smem *smem, int user_access,
			const char *buf_name);
int msm_cvp_smem_free(struct msm_cvp_smem *smem);
struct context_bank_info *msm_cvp_smem_get_context_bank(
				struct msm_cvp_platform_resources *res,
				unsigned int flags);
int msm_cvp_map_smem(struct msm_cvp_inst *inst,
			struct msm_cvp_smem *smem,
			const char *str);
int msm_cvp_unmap_smem(struct msm_cvp_inst *inst,
			struct msm_cvp_smem *smem,
			const char *str);
struct dma_buf *msm_cvp_smem_get_dma_buf(int fd);
void msm_cvp_smem_put_dma_buf(void *dma_buf);
int msm_cvp_smem_cache_operations(struct dma_buf *dbuf,
				enum smem_cache_ops cache_op,
				unsigned long offset,
				unsigned long size);
int msm_cvp_map_ipcc_regs(u32 *iova);
int msm_cvp_unmap_ipcc_regs(u32 iova);

/* CVP driver internal buffer management functions*/
struct cvp_internal_buf *cvp_allocate_arp_bufs(struct msm_cvp_inst *inst,
					u32 buffer_size);
int cvp_release_arp_buffers(struct msm_cvp_inst *inst);
void msm_cvp_cache_operations(struct msm_cvp_smem *smem,
			u32 type, u32 offset, u32 size);
int msm_cvp_unmap_user_persist(struct msm_cvp_inst *inst,
			struct eva_kmd_hfi_packet *in_pkt,
			unsigned int offset, unsigned int buf_num);
int msm_cvp_map_user_persist(struct msm_cvp_inst *inst,
			struct eva_kmd_hfi_packet *in_pkt,
			unsigned int offset, unsigned int buf_num, uint32_t *fd_arr);
int msm_cvp_map_frame(struct msm_cvp_inst *inst,
		struct eva_kmd_hfi_packet *in_pkt,
		unsigned int offset, unsigned int buf_num);
void msm_cvp_unmap_frame(struct msm_cvp_inst *inst, u64 ktid);
int msm_cvp_session_deinit_buffers(struct msm_cvp_inst *inst);
void msm_cvp_print_inst_bufs(struct msm_cvp_inst *inst, bool log);
int cvp_allocate_dsp_bufs(struct cvp_internal_buf *buf,
			u32 buffer_size,
			u32 secure_type,
			const char *buf_name);
int cvp_release_dsp_buffers(struct cvp_internal_buf *buf);
void cvp_buf_map_set_vaddr(struct cvp_dma_buf_vmap *vmap, void *vaddr);
int msm_cvp_dma_buf_vmap(struct dma_buf *dmabuf, struct cvp_dma_buf_vmap *vmap);
void msm_cvp_dma_buf_vunmap(struct dma_buf *dmabuf, struct cvp_dma_buf_vmap *vmap);
enum cp_context_bank msm_cvp_get_cb(u32 flags);
struct msm_cvp_smem *msm_cvp_session_find_smem(struct msm_cvp_inst *inst, struct dma_buf *dma_buf, u32 pkt_type);
int msm_cvp_session_add_smem(struct msm_cvp_inst *inst, struct msm_cvp_smem *smem);
void print_persist_buffer_info(u32 tag, const char *str, u32 buffer_size,
		struct msm_cvp_inst *inst, struct eva_kmd_hfi_packet *pkt);

#endif
