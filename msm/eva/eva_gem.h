/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __EVA_GEM_H__
#define __EVA_GEM_H__

#include <drm/drm_gem.h>
#include "msm_cvp_buf.h"

struct drm_device;
struct drm_file;
struct dma_buf;
struct msm_cvp_inst;
struct msm_cvp_smem;
struct msm_cvp_platform_resources;

/**
 * struct eva_gem_obj - EVA GEM buffer object (DRM wrapper over msm_cvp_smem)
 *
 * Wraps a dma-buf import for Path 6 (user persist) and Path 10 (frame buffer).
 * msm_cvp_smem owns the dma-buf attach/map state; this struct owns the DRM
 * object lifetime and the PRIME handle namespace.
 *
 * pages/num_pages/sgt are only used by the internal-alloc path (imported ==
 * false, Path 1/2/3/5); sgt is the master page table and is never passed
 * directly to dma_map_sgtable() — every real mapping operates on a dup, see
 * eva_gem_dup_sg_table().
 */

enum eva_gem_type {
	EVA_GEM_INTERNAL = 0,	/* internal-alloc, Path 1/2/3/5; imported == false */
	EVA_GEM_USER_PERSIST,	/* imported user-persist buffer, Path 6 */
	EVA_GEM_FRAME,		/* imported frame buffer, Path 10 */
};

struct eva_gem_obj {
	struct drm_gem_object base;	/* must be first member */
	struct msm_cvp_smem *smem;	/* IOVA, dma_buf, cache state */
	struct msm_cvp_inst *inst;
	bool imported;
	enum eva_gem_type type;
	/* internal-alloc path only (imported == false) */
	struct page **pages;
	u32 num_pages;
	struct sg_table *sgt;
};

#define to_eva_gem(obj) container_of(obj, struct eva_gem_obj, base)

void eva_gem_free_object(struct drm_gem_object *obj);

// int eva_prime_fd_to_handle(struct drm_device *dev, struct drm_file *file_priv,
// 			   int prime_fd, u32 *handle);

int eva_gem_get_imported(struct msm_cvp_inst *inst, struct drm_file *file_priv,
			struct cvp_buf_type *buf, enum eva_gem_type gem_type, u32 pkt_type, struct drm_gem_object **obj_out);
struct msm_cvp_smem *eva_gem_smem(struct drm_gem_object *obj);
u32 eva_gem_iova(struct drm_gem_object *obj);
void eva_gem_put(struct drm_gem_object *obj);

struct eva_gem_obj *eva_gem_create_internal(struct msm_cvp_platform_resources *res,
					     size_t size);
struct dma_buf *eva_gem_export_dma_buf(struct eva_gem_obj *gobj);

int eva_gem_map_iova(struct eva_gem_obj *gobj,
		      struct msm_cvp_platform_resources *res);
int eva_gem_unmap_iova(struct eva_gem_obj *gobj);

int eva_gem_vmap(struct eva_gem_obj *gobj, struct iosys_map *map);
void eva_gem_vunmap(struct eva_gem_obj *gobj, struct iosys_map *map);
void eva_gem_get(struct drm_gem_object *obj);

#endif /* __EVA_GEM_H__ */