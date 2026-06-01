/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/dma-buf.h>
#include <linux/iosys-map.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include "msm_cvp_dma_buf.h"
#include "msm_cvp_debug.h"
#define MAX_BUFFER_SIZE (67108864)
#define MSM_CVP_FLAG_CACHE (1<<10) // TODO: need to be defined
struct msm_cvp_dma_buf_attachment {
	struct sg_table *table;
	struct list_head list;
	struct device *dev;
	bool mapped;
};
/**
 * struct msm_cvp_cb_buf_context - eva dma buffer manager context
 * 
 * @dev:      device context
 * @dev_lock: lock protecting the tree of nodes
 */
struct msm_cvp_cb_buf_context {
	struct device *dev;
	struct mutex dev_lock;
};
/**
 * struct msm_cvp_cb_buffer - metadata for a buffer
 *
 * @flags:     buffer specific flags
 * @size:      size of the buffer
 * @b_lock:    protects the msm_cvp_cb_buffer fields
 * @vaddr:     the kernel mapping if kmap_cnt is not zero
 * @kmap_cnt:  number of times the buffer is mapped to the kernel
 * @sg_table:  the sg table for the buffer
 * @pages:     Array of pointer to the allocated pages
 * @num_pages: The number of the allocated pages
 * @list:  list of entry attachments
 */
struct msm_cvp_cb_buffer {
	unsigned long flags;
	size_t size;
	struct mutex b_lock;
	void *vaddr;
	int kmap_cnt;
	struct sg_table sg_table;
	struct page **pages;
	size_t num_pages;
	struct list_head attachments;
};
static struct msm_cvp_cb_buf_context *idev;
static int _msm_cvp_buf_sgt_alloc(struct msm_cvp_cb_buffer *buf)
{
	struct sg_table *sgt = &buf->sg_table;
	int i, rc = 0;
	buf->num_pages = DIV_ROUND_UP(buf->size, PAGE_SIZE);
	buf->pages = kcalloc(buf->num_pages, sizeof(*buf->pages), GFP_KERNEL);
	if (!buf->pages){
		dprintk(CVP_ERR, "Cannot allocate pages");
		return -ENOMEM;
	}
	for ( i = 0; i < buf->num_pages; ++i) {
		buf->pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
		if (!buf->pages[i]) {
			dprintk(CVP_ERR, "Cannot allocate pages[%d]", i);
			rc = -ENOMEM;
			goto err_free_pages;
		}
	}
	rc = sg_alloc_table_from_pages(sgt, buf->pages,buf->num_pages, 0, 
		buf->size, GFP_KERNEL);
		
	if (rc) {
		dprintk(CVP_ERR, "Cannot allocate sg table, rc:%d", rc);
		goto err_free_pages;
	}
		
	// TODO: Need be verified because we should not do map during allocation.
	// sgt->nents = dma_map_sg(idev->dev, sgt->sgl, sgt->orig_nents, DMA_BIDIRECTIONAL);
	// if (!sgt->nents) {
	// 	dprintk(CVP_ERR, "DMA mapping failed");
	// 	rc = -ENOMEM;
	// 	goto err_free_table;
	// }
	return rc;
	
// err_free_table:
// 	sg_free_table(sgt);
err_free_pages:
	for ( i = 0; i < buf->num_pages; ++i) {
		if (buf->pages[i])
			__free_page(buf->pages[i]);
	}
	kfree(buf->pages);
	buf->pages = NULL;
	buf->num_pages = 0;
	return rc;
}
static void _msm_cvp_buf_sgt_free(struct msm_cvp_cb_buffer *buf)
{
	int i;
	// TODO: Need be verified because we should not do unmap because we dont do map when allocation.
	// dma_unmap_sg(idev->dev, buf->sg_table.sgl,
	// 				buf->sg_table.orig_nents, DMA_BIDIRECTIONAL);
	sg_free_table(&buf->sg_table);
	for( i = 0; i < buf->num_pages && buf->pages[i]; ++i) {
		if (buf->pages[i])
			__free_page(buf->pages[i]);
	}
	
	kfree(buf->pages);
	buf->pages = NULL;
	buf->num_pages = 0;
}
static int _msm_cvp_buf_alloc(struct msm_cvp_cb_buffer *buf)
{
	int rc = 0;
	rc = _msm_cvp_buf_sgt_alloc(buf);
	if (rc)
		dprintk(CVP_ERR, "Cannot allocate buf scl table");
	
	return rc;
}
static void _msm_cvp_buf_free(struct msm_cvp_cb_buffer *buf)
{
	_msm_cvp_buf_sgt_free(buf);
}
static struct msm_cvp_cb_buffer *_msm_cvp_buf_create(unsigned long len, unsigned long flags)
{
	struct msm_cvp_cb_buffer *buf;
	int rc = 0;
	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);
	buf->size = len;
	buf->flags = flags;
	rc = _msm_cvp_buf_alloc(buf);
	if (rc)
		goto err_buf_free;
	
	INIT_LIST_HEAD(&buf->attachments);
	mutex_init(&buf->b_lock);
	return buf;
err_buf_free:
	kfree(buf);
	return ERR_PTR(rc);
}
static void _msm_cvp_buf_destroy(struct msm_cvp_cb_buffer *buf)
{
	if (buf->kmap_cnt > 0) {
		pr_err_once("Likely missing a call to unmap\n");
		vunmap(buf->vaddr);
		buf->vaddr = NULL;
	}
	mutex_destroy(&buf->b_lock);
	_msm_cvp_buf_free(buf);
	kfree(buf);
}
static struct sg_table *_msm_cvp_dma_dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	struct scatterlist *sg, *new_sg;
	int ret, i;
	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}
	
	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}
	return new_table;
}
static void *_msm_cvp_dma_kmap_get(struct msm_cvp_cb_buffer *buffer)
{
    pgprot_t pgprot;
    if (buffer->kmap_cnt++)
        return buffer->vaddr;
    
    if (buffer->flags & MSM_CVP_FLAG_CACHE)
        pgprot = PAGE_KERNEL;
    else
        pgprot = pgprot_writecombine(PAGE_KERNEL);
    
    buffer->vaddr = vmap(buffer->pages, buffer->num_pages, VM_MAP, pgprot);
    if (!buffer->vaddr) {
        goto err_kmap_dec;
    }
    dprintk(CVP_DBG, "Buffer vmap, map_cnt: %d", buffer->kmap_cnt);
    return buffer->vaddr;
err_kmap_dec:
    buffer->kmap_cnt--;
    return ERR_PTR(-ENOMEM);
}
static void _msm_cvp_dma_kmap_put(struct msm_cvp_cb_buffer *buffer)
{
    if (buffer->kmap_cnt && --buffer->kmap_cnt)
        return;
    
    vunmap(buffer->vaddr);
    buffer->vaddr = NULL;
    dprintk(CVP_DBG, "Buffer vunmap, map_cnt: %d", buffer->kmap_cnt);
}
static int _op_dma_buf_attach(struct dma_buf *dmabuf, 
							  struct dma_buf_attachment *attachment)
{
	struct msm_cvp_cb_buffer *buffer = dmabuf->priv;
	struct msm_cvp_dma_buf_attachment *new_att;
	struct sg_table *sgt;
	struct sg_table *table;
	int rc = 0;
	new_att = kzalloc(sizeof(*new_att), GFP_KERNEL);
	if (!new_att)
		return -ENOMEM;
	table = _msm_cvp_dma_dup_sg_table(&buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(new_att);
		return -ENOMEM;
	}
	new_att->table = table;
	new_att->dev = attachment->dev;
	INIT_LIST_HEAD(&new_att->list);
	new_att->mapped = false;
	attachment->priv = new_att;
	mutex_lock(&buffer->b_lock);
	list_add(&new_att->list, &buffer->attachments);
	mutex_unlock(&buffer->b_lock);
	return 0;
}
static void _op_dma_buf_detach(struct dma_buf *dmabuf,
							   struct dma_buf_attachment *attachment)
{
	struct msm_cvp_cb_buffer *buffer = dmabuf->priv;
	struct msm_cvp_dma_buf_attachment *a = attachment->priv;
	mutex_lock(&buffer->b_lock);
	list_del(&a->list);
	mutex_unlock(&buffer->b_lock);
	
	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}
static struct sg_table *_op_dma_buf_map(struct dma_buf_attachment *attachment,
										enum dma_data_direction direction)
{
	struct msm_cvp_dma_buf_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	struct msm_cvp_cb_buffer *buffer = attachment->dmabuf->priv;
	int map_attrs = DMA_ATTR_SKIP_CPU_SYNC;
	// TODO: checking why mutex lock in here? For multiple device access one dev?
	mutex_lock(&buffer->b_lock);
	table->nents = dma_map_sg_attrs(attachment->dev, table->sgl,
									table->orig_nents, direction,
									map_attrs);
	if (!table->nents) {
		mutex_unlock(&buffer->b_lock);
		return ERR_PTR(-ENOMEM);
	}
    a->mapped = true;
	mutex_unlock(&buffer->b_lock);
	return table;
}
static void _op_dma_buf_unmap(struct dma_buf_attachment *attachment,
							  struct sg_table *table,
							  enum dma_data_direction direction)
{
	int map_attrs = DMA_ATTR_SKIP_CPU_SYNC;
	struct msm_cvp_cb_buffer *buffer = attachment->dmabuf->priv;
    struct msm_cvp_dma_buf_attachment *a = attachment->priv;
	mutex_lock(&buffer->b_lock);
    a->mapped = false;
	dma_unmap_sg_attrs(attachment->dev, table->sgl, table->nents, 
					   direction, map_attrs);
	mutex_unlock(&buffer->b_lock);
}
static int _op_dma_buf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct msm_cvp_cb_buffer *buffer = dmabuf->priv;
	struct sg_table *table = &buffer->sg_table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	int i, ret = 0;
	// TODO: we need to define a macro MSM_CVP_FLAG_CACHE
	if (!(buffer->flags & MSM_CVP_FLAG_CACHE))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	
	vma->vm_private_data = buffer;
	mutex_lock(&buffer->b_lock);
	for_each_sg(table->sgl, sg, table->orig_nents, i) {
		struct page *page = sg_page(sg);
		unsigned long remainder = vma->vm_end - addr;
		unsigned long len = sg->length;
		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg->length - offset;
			offset = 0;
		}
		len = min(len, remainder);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
							  vma->vm_page_prot);
		if (ret)
			goto err_vm_close;
		
		addr += len;
		if (addr >= vma->vm_end)
			break;
	}
	mutex_unlock(&buffer->b_lock);
	return ret;
err_vm_close:
	mutex_unlock(&buffer->b_lock);
	dprintk(CVP_ERR, "failure mapping buffer to userspace");
	return ret;
}
static void _op_dma_buf_release(struct dma_buf *dmabuf)
{
	struct msm_cvp_cb_buffer *buffer = dmabuf->priv;
	_msm_cvp_buf_destroy(buffer);
	kfree(dmabuf->exp_name);
}
static int _op_dma_buf_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	struct msm_cvp_cb_buffer *buffer = dmabuf->priv;
	void *vaddr = ERR_PTR(-EINVAL);
    mutex_lock(&buffer->b_lock);
    vaddr = _msm_cvp_dma_kmap_get(buffer);
    mutex_unlock(&buffer->b_lock);
    if (IS_ERR(vaddr))
        return PTR_ERR(vaddr);
    
    iosys_map_set_vaddr(map, vaddr);
    return 0;
}
static void _op_dma_buf_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
    struct msm_cvp_cb_buffer *buffer = dmabuf->priv;
    mutex_lock(&buffer->b_lock);
    _msm_cvp_dma_kmap_put(buffer);
    mutex_unlock(&buffer->b_lock);
}
static int _op_dma_buf_beg_cpu_access(struct dma_buf *dmabuf,
				                      enum dma_data_direction direction)
{
    struct msm_cvp_cb_buffer *buffer = dmabuf->priv;
    struct msm_cvp_dma_buf_attachment *a;
    mutex_lock(&buffer->b_lock);
    if (!(buffer->flags & MSM_CVP_FLAG_CACHE)) {
		mutex_unlock(&buffer->b_lock);
        return 0;
	}
    
    if (buffer->kmap_cnt)
        invalidate_kernel_vmap_range(buffer->vaddr, buffer->size);
    
    // find all attachments of device
    list_for_each_entry(a, &buffer->attachments, list) {
        if (!a->mapped)
            continue;
        dma_sync_sgtable_for_cpu(a->dev, a->table, direction);
    }
    mutex_unlock(&buffer->b_lock);
	return 0;
}
static int _op_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
                                      enum dma_data_direction direction) 
{
    struct msm_cvp_cb_buffer *buffer = dmabuf->priv;
    struct msm_cvp_dma_buf_attachment *a;
    mutex_lock(&buffer->b_lock);
	if(!(buffer->flags & MSM_CVP_FLAG_CACHE)) {
		mutex_unlock(&buffer->b_lock);
		return 0;
	}
    if (buffer->kmap_cnt)
        flush_kernel_vmap_range(buffer->vaddr, buffer->size);
    
    list_for_each_entry(a, &buffer->attachments, list) {
        if (!a->mapped)
            continue;
        dma_sync_sgtable_for_device(a->dev, a->table, direction);
    }
    mutex_unlock(&buffer->b_lock);
    return 0;
}
static const struct dma_buf_ops msm_cvp_dma_buf_ops = {
	.map_dma_buf = _op_dma_buf_map,
	.unmap_dma_buf = _op_dma_buf_unmap,
	.mmap = _op_dma_buf_mmap,
	.release = _op_dma_buf_release,
	.attach = _op_dma_buf_attach,
	.detach = _op_dma_buf_detach,
	.begin_cpu_access = _op_dma_buf_beg_cpu_access,
	.end_cpu_access = _op_dma_buf_end_cpu_access,
	.vmap = _op_dma_buf_vmap,
	.vunmap = _op_dma_buf_vunmap
};
struct dma_buf *msm_cvp_alloc_dma_buffer(size_t len, unsigned int flags)
{
	struct msm_cvp_cb_buffer *buffer = NULL;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dma_buf;
	
	len = PAGE_ALIGN(len);
	if (!len) {
		dprintk(CVP_ERR, "Invalid buffer size: %zu", len);
		return ERR_PTR(-EINVAL);
	}
	if (len > MAX_BUFFER_SIZE) {
		dprintk(CVP_ERR, "Buffer size is over limit: %zu", len);
		return ERR_PTR(-EINVAL);
	}
	buffer = _msm_cvp_buf_create(len, flags);
	if (IS_ERR_OR_NULL(buffer)) {
		dprintk(CVP_ERR, "Cannot create cvp buffer");
		return ERR_CAST(buffer);
	}
	exp_info.ops = &msm_cvp_dma_buf_ops;
	exp_info.size = buffer->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;
	exp_info.exp_name = kasprintf(GFP_KERNEL, "%s", "MSM_CVP");
	if (!exp_info.exp_name) {
		dprintk(CVP_ERR, "Cannot allocate memory for exp_name");
		_msm_cvp_buf_destroy(buffer);
		// kfree(exp_info.exp_name);
		return ERR_PTR(-ENOMEM);
	}
	dma_buf = dma_buf_export(&exp_info);
	if (IS_ERR(dma_buf)) {
		dprintk(CVP_ERR, "Cannot export msm cvp dma buffer");
		_msm_cvp_buf_destroy(buffer);
		kfree(exp_info.exp_name);
		return ERR_CAST(dma_buf);
	}
	return dma_buf;	
}
void msm_cvp_free_dma_buffer(struct dma_buf *dmabuf)
{
	if (!dmabuf) {
		dprintk(CVP_ERR, "Invalid argument(s)");
		return;
	}
	dma_buf_put(dmabuf);
}