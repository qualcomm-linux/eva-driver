// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <linux/scatterlist.h>
#include <linux/iosys-map.h>
#include <linux/dma-buf.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_prime.h>
#include "msm_cvp_internal.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_buf.h"
#include "eva_gem.h"

extern struct msm_cvp_drv *cvp_driver;

static int eva_gem_object_vmap(struct drm_gem_object *obj, struct iosys_map *map);
static void eva_gem_object_vunmap(struct drm_gem_object *obj, struct iosys_map *map);
static struct sg_table *eva_gem_get_sg_table(struct drm_gem_object *obj);
static int eva_gem_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma);

static const struct vm_operations_struct eva_gem_vm_ops = {
	.open  = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct drm_gem_object_funcs eva_gem_object_funcs = {
	.free         = eva_gem_free_object,
	.get_sg_table = eva_gem_get_sg_table,
	.vmap         = eva_gem_object_vmap,
	.vunmap       = eva_gem_object_vunmap,
	.mmap         = eva_gem_mmap,
	.vm_ops       = &eva_gem_vm_ops,
};

/*
 * eva_gem_alloc_pages_sgt() / eva_gem_free_pages_sgt() - internal-alloc page
 * backing store, migrated from msm_cvp_dma_buf.c (see task3-1.md D8).
 */
static int eva_gem_alloc_pages_sgt(struct eva_gem_obj *gobj, size_t size)
{
	int i, rc;

	gobj->num_pages = DIV_ROUND_UP(size, PAGE_SIZE);
	gobj->pages = kcalloc(gobj->num_pages, sizeof(*gobj->pages), GFP_KERNEL);
	if (!gobj->pages)
		return -ENOMEM;

	for (i = 0; i < gobj->num_pages; ++i) {
		gobj->pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
		if (!gobj->pages[i]) {
			dprintk(CVP_ERR, "Cannot allocate pages[%d]", i);
			rc = -ENOMEM;
			goto err_free_pages;
		}
	}

	gobj->sgt = kzalloc(sizeof(*gobj->sgt), GFP_KERNEL);
	if (!gobj->sgt) {
		rc = -ENOMEM;
		goto err_free_pages;
	}

	rc = sg_alloc_table_from_pages(gobj->sgt, gobj->pages, gobj->num_pages,
					0, size, GFP_KERNEL);
	if (rc)
		goto err_free_sgt;
	return 0;

err_free_sgt:
	kfree(gobj->sgt);
	gobj->sgt = NULL;
err_free_pages:
	for ( i = 0; i < gobj->num_pages; ++i) {
		if (gobj->pages[i])
			__free_page(gobj->pages[i]);
	}
	kfree(gobj->pages);
	gobj->pages = NULL;
	gobj->num_pages = 0;
	return rc;
}

static void eva_gem_free_pages_sgt(struct eva_gem_obj *gobj)
{
	int i;

	sg_free_table(gobj->sgt);
	kfree(gobj->sgt);
	for (i = 0; i < gobj->num_pages; ++i) {
		if (gobj->pages[i])
			__free_page(gobj->pages[i]);
	}
	kfree(gobj->pages);
	gobj->pages = NULL;
	gobj->num_pages = 0;
}

/*
 * eva_gem_dup_sg_table() - duplicate a sg_table.
 *
 * gobj->sgt (the master table) is never passed directly to
 * dma_map_sgtable() — every real mapping (internal eva_gem_map_iova(), or
 * external .get_sg_table) operates on a dup, to avoid two independent
 * map/unmap timelines sharing the same scatterlist entries (see R11).
 */
static struct sg_table *eva_gem_dup_sg_table(struct sg_table *src)
{
	struct sg_table *dst;
	struct scatterlist *sg, *dst_sg;
	int i, rc;

	dst = kzalloc(sizeof(*dst), GFP_KERNEL);
	if (!dst)
		return ERR_PTR(-ENOMEM);

	rc = sg_alloc_table(dst, src->orig_nents, GFP_KERNEL);
	if (rc) {
		kfree(dst);
		return ERR_PTR(rc);
	}

	dst_sg = dst->sgl;
	for_each_sg(src->sgl, sg, src->orig_nents, i) {
		sg_set_page(dst_sg, sg_page(sg), sg->length, 0);
		dst_sg = sg_next(dst_sg);
	}
	return dst;
}

static struct sg_table *eva_gem_get_sg_table(struct drm_gem_object *obj)
{
	struct eva_gem_obj *gobj = to_eva_gem(obj);

	// if (gobj->imported)
	// 	return ERR_PTR(-EINVAL);
	return eva_gem_dup_sg_table(gobj->sgt);
}

static int eva_gem_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	struct eva_gem_obj *gobj = to_eva_gem(obj);
	struct scatterlist *sg;
	unsigned long offset = 0;
	int i, ret;

	if (gobj->imported)
		return -EINVAL;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	for_each_sgtable_sg(gobj->sgt, sg, i) {
		ret = remap_pfn_range(vma, vma->vm_start + offset,
				       page_to_pfn(sg_page(sg)), sg->length,
				       vma->vm_page_prot);
		if (ret)
			return ret;
		offset += sg->length;
	}
	return 0;
}

/*
 * eva_gem_vmap()/eva_gem_vunmap() - kernel vmap for internal-alloc objects.
 *
 * Exported for direct use by cvp_smem.c (no drm_gem_vmap() dma_resv_lock
 * needed there — see D3); also the implementation behind the .vmap/.vunmap
 * funcs-table callbacks below.
 */
int eva_gem_vmap(struct eva_gem_obj *gobj, struct iosys_map *map)
{
	void *vaddr;

	if (gobj->imported) {
		dprintk(CVP_ERR, "%s: imported obj can not be vmapped", __func__);
		return -EINVAL;
	}
	vaddr = vmap(gobj->pages, gobj->num_pages, VM_MAP,
		     pgprot_writecombine(PAGE_KERNEL));
	if (!vaddr) {
		dprintk(CVP_ERR, "%s: vmap fail", __func__);
		return -ENOMEM;
	}
	iosys_map_set_vaddr(map, vaddr);
	return 0;
}

void eva_gem_vunmap(struct eva_gem_obj *gobj, struct iosys_map *map)
{
	if (gobj->imported)
		return;

	if (map->vaddr && !map->is_iomem)
		vunmap(map->vaddr);
	else
		dprintk(CVP_ERR, "%s: vunmap fail", __func__);
}

static int eva_gem_object_vmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	dprintk(CVP_WARN, "%s: invoked", __func__);
	return eva_gem_vmap(to_eva_gem(obj), map);
}

static void eva_gem_object_vunmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	eva_gem_vunmap(to_eva_gem(obj), map);
}

/*
 * eva_gem_map_iova()/eva_gem_unmap_iova() - IOMMU map/unmap for
 * internal-alloc objects, operating on a dup of gobj->sgt (see D4/R11).
 * Result is written directly into gobj->smem.
 */
// TODO: include the imported gem obj logic into scope
int eva_gem_map_iova(struct eva_gem_obj *gobj,
		      struct msm_cvp_platform_resources *res)
{
	struct msm_cvp_smem *mem = gobj->smem;
	struct cvp_dma_mapping_info *mapping_info = &mem->mapping_info;
	struct context_bank_info *cb;
	struct sg_table *dup;
	dma_addr_t iova;
	unsigned long attrs = DMA_ATTR_SKIP_CPU_SYNC;
	int rc;

	if (!is_iommu_present(res)) {
		dprintk(CVP_MEM, "iommu not present, use phys mem addr\n");
		return 0;
	}

	if (gobj->imported){
		rc = msm_cvp_map_smem(gobj->inst, mem, "eva gem prime import iova mapping");
		if (rc) {
			dprintk(CVP_ERR, "%s: imported buf map failed", __func__);
			return rc;
		}
	}
	else{
		cb = msm_cvp_smem_get_context_bank(res, mem->flags);
		if (!cb) {
			dprintk(CVP_ERR, "%s: Failed to get context bank device\n", __func__);
			return -EIO;
		}

		dup = eva_gem_dup_sg_table(gobj->sgt);
		if (IS_ERR(dup))
			return PTR_ERR(dup);
	
		rc = dma_map_sgtable(cb->dev, dup, DMA_BIDIRECTIONAL, attrs);
		if (rc) {
			dprintk(CVP_ERR, "Failed to map sgtable: %d\n", rc);
			sg_free_table(dup);
			kfree(dup);
			return rc;
		}
		if (!dup->sgl) {
			dprintk(CVP_ERR, "sgl is NULL\n");
			dma_unmap_sgtable(cb->dev, dup, DMA_BIDIRECTIONAL, attrs);
			sg_free_table(dup);
			kfree(dup);
			return -ENOMEM;
		}
	
		iova = dup->sgl->dma_address;
		mem->device_addr = (u32)iova;
		if ((dma_addr_t)mem->device_addr != iova) {
			dprintk(CVP_ERR, "iova truncated: %pad\n", &iova);
			dma_unmap_sgtable(cb->dev, dup, DMA_BIDIRECTIONAL, attrs);
			sg_free_table(dup);
			kfree(dup);
			return -EINVAL;
		}
		mapping_info->dev = cb->dev;
		mapping_info->domain = cb->domain;
		mapping_info->table = dup;
		mapping_info->attach = NULL;
		mapping_info->buf = NULL;
		mapping_info->cb_info = (void *)cb;
	}

	return 0;
}

int eva_gem_unmap_iova(struct eva_gem_obj *gobj)
{
	struct msm_cvp_smem *smem = gobj->smem;
	struct cvp_dma_mapping_info *mapping_info = &smem->mapping_info;
	int rc = 0;
	if (gobj->imported && smem && smem->device_addr){
		rc = msm_cvp_unmap_smem(gobj->inst, smem, "eva gem free");
	}
	else {
		if (!mapping_info->table || !mapping_info->dev)
			return rc;
		dma_unmap_sgtable(mapping_info->dev, mapping_info->table,
				   DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
		/* release the dup allocated by eva_gem_map_iova(); gobj->sgt (master) untouched */
		sg_free_table(mapping_info->table);
		kfree(mapping_info->table);
		memset(mapping_info, 0, sizeof(*mapping_info));
		gobj->smem->device_addr = 0;
	}
	return rc;
}

/*
 * eva_gem_create_internal() - construct-only entry point (see D1). Does not
 * export a dma_buf.
 */
struct eva_gem_obj *eva_gem_create_internal(struct msm_cvp_platform_resources *res,
					     size_t size)
{
	struct msm_cvp_core *core = eva_core_from_res(res);
	struct eva_gem_obj *gobj;
	int rc;

	size = PAGE_ALIGN(size);
	gobj = kzalloc(sizeof(*gobj), GFP_KERNEL);
	if (!gobj)
		return ERR_PTR(-ENOMEM);

	rc = eva_gem_alloc_pages_sgt(gobj, size);
	if (rc) {
		kfree(gobj);
		return ERR_PTR(rc);
	}
	// Because internal buf's lifecycle is session， and drm_gem_private
	drm_gem_private_object_init(&core->drm_dev, &gobj->base, size);
	gobj->base.funcs = &eva_gem_object_funcs;
	gobj->imported = false;

	return gobj;
}

/* eva_gem_export_dma_buf() - SFR-only lazy export (see D9) */
struct dma_buf *eva_gem_export_dma_buf(struct eva_gem_obj *gobj)
{
	return drm_gem_prime_export(&gobj->base, O_RDWR);
}

struct drm_gem_object *eva_gem_import_smem(struct msm_cvp_inst *inst, struct drm_device *dev,
			struct cvp_buf_type *buf, enum eva_gem_type gem_type, u32 pkt_type)
{
	struct eva_gem_obj *gem_obj;
	struct msm_cvp_smem *smem;
	struct dma_buf *dma_buf;
	bool found_smem = true;
	int rc;

	dma_buf = msm_cvp_smem_get_dma_buf(buf->fd);
	// inst = core->current_import_inst;
	if (!inst) {
		dprintk(CVP_ERR, "%s: no import inst context\n", __func__);
		return ERR_PTR(-ENODEV);
	}
	/* Check own-buffer: if this dma-buf was exported by us, reuse. */
	if (drm_gem_is_prime_exported_dma_buf(dev, dma_buf)) {
		struct drm_gem_object *existing = dma_buf->priv;

		if (existing && existing->dev == dev &&
		    !to_eva_gem(existing)->imported) {
			drm_gem_object_get(existing);
			return existing;
		}
	}

	if (gem_type == EVA_GEM_FRAME) {
		// 1. find one in dma-cache/persistbuf/frame, will automaticly add refcount
		smem = msm_cvp_session_find_smem(inst, dma_buf, pkt_type);
		// 2. else create one
		if (!smem) {
			gem_obj = kzalloc(sizeof(*gem_obj), GFP_KERNEL);
			if (!gem_obj)
				return ERR_PTR(-ENOMEM);
			drm_gem_private_object_init(dev, &gem_obj->base, PAGE_ALIGN(dma_buf->size));
			gem_obj->base.funcs = &eva_gem_object_funcs;
			gem_obj->inst = inst;
			gem_obj->imported = true;
			gem_obj->type = gem_type;
			
			smem = cvp_kmem_cache_zalloc(&cvp_driver->smem_cache, GFP_KERNEL);
			if (!smem)
				return ERR_PTR(-ENOMEM);
			
			smem->dma_buf = dma_buf;
			smem->cached = false;
			smem->pkt_type = pkt_type;
			smem->fd = buf->fd;
			smem->gem = &gem_obj->base;
			gem_obj->smem = smem;

			// rc = msm_cvp_map_smem(inst, smem, "eva gem prime import");
			// map iova will inc the refcount
			rc = eva_gem_map_iova(gem_obj, &inst->core->resources);
			if (rc) {
				dprintk(CVP_ERR, "%s: iova map failed %d\n", __func__, rc);
				goto err_free_gem;
			}
			if (!IS_CVP_BUF_VALID(buf, smem)) {
				dprintk(CVP_ERR,
					"%s: invalid buf %d %d fd %d dma 0x%llx %s %d type %#x\n",
					__func__, buf->offset, buf->size, buf->fd,
					dma_buf, dma_buf->name, dma_buf->size, pkt_type);
				goto err_free_smem;
			}
			// 2.1 add to dma-cache
			rc = msm_cvp_session_add_smem(inst, smem);
			if (rc && rc != -ENOMEM) {
				dprintk(CVP_ERR, "%s: add to dma-cache failed", __func__);
				goto err_free_smem;
			}

		} else {
			return smem->gem;
		}

		if (!IS_CVP_BUF_VALID(buf, smem)) {
			dprintk(CVP_ERR, "%s: invalid offset %d or size %d found\n",
				__func__, buf->offset, buf->size);
			if (found_smem) {
				mutex_lock(&inst->dma_cache.lock);
				atomic_dec(&smem->refcount);
				mutex_unlock(&inst->dma_cache.lock);
				return NULL;
			}
			rc = -EINVAL;
			goto err_free_smem;
		}
	}

	return &gem_obj->base;

err_free_smem:
	rc = eva_gem_unmap_iova(gem_obj);
	if (rc)
		dprintk(CVP_ERR, "%s: Fail to unmap smem 0x%x, error %d\n",
			__func__, smem, rc);
	else {
		msm_cvp_smem_put_dma_buf(dma_buf);
		cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
		smem = NULL;
		return ERR_PTR(-EINVAL);
	}
err_free_gem:
	msm_cvp_smem_put_dma_buf(dma_buf);
	cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
	smem = NULL;
	kfree(gem_obj);
	return ERR_PTR(-EINVAL);
}

/*
 * eva_gem_free_object() - free an EVA GEM object
 *
 * Called by the DRM core when the last reference to a GEM object is dropped.
 * drm_gem_release fires before .postclose, so inst is still alive and can be
 * used to decrement smem_count correctly.
 */
void eva_gem_free_object(struct drm_gem_object *obj)
{
	struct eva_gem_obj *gem_obj = to_eva_gem(obj);
	struct msm_cvp_smem *smem = gem_obj->smem;

	
	if (gem_obj->imported) {
		dprintk(CVP_WARN, "%s: eva_gem_free_object is invoked for imported fd %d", __func__, smem->fd);
		struct msm_cvp_smem *smem = gem_obj->smem;
		/* follow logic will be proceed with dma API, so we need to handle them in a condition*/
		if (smem) {
			if (smem->device_addr)
				msm_cvp_unmap_smem(gem_obj->inst, smem, "eva gem free");
			if (smem->dma_buf)
				msm_cvp_smem_put_dma_buf(smem->dma_buf);
			smem->gem = NULL;
			cvp_kmem_cache_free(&cvp_driver->smem_cache, smem);
		}
	} else {
		/*
		 * gobj->sgt is the master table and was never passed to
		 * dma_map_sgtable() directly — the dup used
		 * for IOMMU mapping was already released by
		 * eva_gem_unmap_iova() before this ref hit zero.
		 */
		eva_gem_free_pages_sgt(gem_obj);
	}

	drm_gem_object_release(obj);
	kfree(gem_obj);
}

/*
 * eva_gem_get_from_fd() - import a dma-buf fd and return the GEM object
 *
 * Used by Path 6/10 to convert the fd in the HFI packet into a GEM object
 * reference.  Caller holds the GEM ref; must call eva_gem_put when done.
 */
int eva_gem_get_imported(struct msm_cvp_inst *inst, struct drm_file *file_priv,
			struct cvp_buf_type *buf, enum eva_gem_type gem_type, u32 pkt_type, struct drm_gem_object **obj_out)
{
	struct drm_device *dev;
	struct drm_gem_object *obj;

	if (!inst || !file_priv || buf->fd < 0 || !obj_out)
		return -EINVAL;

	dev = &inst->core->drm_dev;
	*obj_out = NULL;
	obj = eva_gem_import_smem(inst, dev, buf, gem_type, pkt_type);
	if (IS_ERR(obj)) {
		return PTR_ERR(obj);
	}
	*obj_out = obj;
	return 0;
}

/*
 * eva_gem_smem() - get the msm_cvp_smem from a GEM object
 */
struct msm_cvp_smem *eva_gem_smem(struct drm_gem_object *obj)
{
	if (!obj)
		return NULL;
	return to_eva_gem(obj)->smem;
}

/*
 * eva_gem_iova() - get the device address from a GEM object
 */
u32 eva_gem_iova(struct drm_gem_object *obj)
{
	struct msm_cvp_smem *smem = eva_gem_smem(obj);

	return smem ? smem->device_addr : 0;
}

/*
 * eva_gem_put() - drop a GEM object reference obtained via eva_gem_get_from_fd
 */
void eva_gem_put(struct drm_gem_object *obj)
{
	struct eva_gem_obj *gobj;
	struct msm_cvp_inst *inst;
	if (!obj)
		return;
	
	gobj = to_eva_gem(obj);
	inst = gobj->inst;

	if (gobj->imported && gobj->type == EVA_GEM_FRAME) {
		mutex_lock(&inst->dma_cache.lock);
		if (atomic_dec_and_test(&gobj->smem->refcount)) {
			gobj->smem->buf_idx |= 0x10000000;
		}
		mutex_unlock(&inst->dma_cache.lock);
		return;
	} else {
		drm_gem_object_put(obj);
	}
}

void eva_gem_get(struct drm_gem_object *obj)
{
	struct eva_gem_obj *gobj;
	struct msm_cvp_inst *inst;
	if (!obj)
		return;
	
	gobj = to_eva_gem(obj);
	inst = gobj->inst;

	if (gobj->imported && gobj->type == EVA_GEM_FRAME) {
		mutex_lock(&inst->dma_cache.lock);
		atomic_inc(&gobj->smem->refcount);
		mutex_unlock(&inst->dma_cache.lock);
		return;
	} else {
		drm_gem_object_get(obj);
	}
}

#if (KERNEL_VERSION(6, 13, 0) <= LINUX_VERSION_CODE)
MODULE_IMPORT_NS("DMA_BUF");
#else
MODULE_IMPORT_NS(DMA_BUF);
#endif