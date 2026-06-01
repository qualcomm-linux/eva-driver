// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/iommu.h>
#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
#include <linux/firmware/qcom/qcom_scm.h>
#else
#include <linux/qcom_scm.h>
#endif
#include "msm_cvp_debug.h"
#include "cvp_comm_def.h"
#include "cvp_core_hfi.h"
#include "cvp_hfi.h"
#include <linux/of_address.h>
#include <linux/firmware.h>
#include <linux/soc/qcom/mdt_loader.h>
#include "cvp_dump.h"

#define MAX_FIRMWARE_NAME_SIZE 128

#ifdef CVP_KVM_ENABLED
static int __load_fw_to_memory(struct iris_hfi_device *device,
		const char *fw_name)
#else
static int __load_fw_to_memory(struct platform_device *pdev,
		const char *fw_name)
#endif
{
	int rc = 0;
	const struct firmware *firmware = NULL;
	char firmware_name[MAX_FIRMWARE_NAME_SIZE] = {0};
	struct device_node *node = NULL;
#ifdef CVP_KVM_ENABLED
	struct qcom_scm_pas_context *ctx;
	struct platform_device *pdev = device->res->pdev;
	struct device *dev = &pdev->dev;
#endif
	struct resource res = {0};
	phys_addr_t phys = 0;
	size_t res_size = 0;
	ssize_t fw_size = 0;
	void *virt = NULL;
	int pas_id = 0;

	if (!fw_name || !(*fw_name) || !pdev) {
		dprintk(CVP_ERR, "%s: Invalid inputs\n", __func__);
		return -EINVAL;
	}
	if (strlen(fw_name) >= MAX_FIRMWARE_NAME_SIZE - 4) {
		dprintk(CVP_ERR, "%s: Invalid fw name\n", __func__);
		return -EINVAL;
	}
	scnprintf(firmware_name, ARRAY_SIZE(firmware_name), "%s.mbn", fw_name);

	rc = of_property_read_u32(pdev->dev.of_node, "pas-id", &pas_id);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: error %d while reading DT for \"pas-id\"\n",
				__func__, rc);
		return rc;
	}

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!node) {
		dprintk(CVP_ERR,
			"%s: DT error getting \"memory-region\" property\n",
				__func__);
		return -EINVAL;
	}

	rc = of_address_to_resource(node, 0, &res);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: error %d getting \"memory-region\" resource\n",
				__func__, rc);
		return rc;
	}
	phys = res.start;
	res_size = (size_t)resource_size(&res);

#ifdef CVP_KVM_ENABLED
	dev = device->resources.fw.dev ? : &pdev->dev;

	ctx = devm_qcom_scm_pas_context_alloc(dev, pas_id, phys, res_size);
	if (!ctx)
		return -ENOMEM;
	
	ctx->use_tzmem = device->resources.fw.dev;
	
	rc = request_firmware(&firmware, firmware_name, dev);
#else
	rc = request_firmware(&firmware, firmware_name, &pdev->dev);
#endif

	if (rc) {
		dprintk(CVP_ERR, "%s: error %d requesting \"%s\"\n",
				__func__, rc, firmware_name);
		return rc;
	}

	fw_size = qcom_mdt_get_size(firmware);
	if (fw_size < 0 || res_size < (size_t)fw_size) {
		rc = -EINVAL;
		dprintk(CVP_ERR,
			"%s: Corrupted fw image. Alloc size: %lu, fw size: %ld",
				__func__, res_size, fw_size);
		goto err_release_fw;
	}

#ifndef CVP_KVM_ENABLED
	// We don't need virt if KVM enabled because the qcom_mdt_pas_load updated.
	virt = memremap(phys, res_size, MEMREMAP_WC);
	if (!virt) {
		rc = -ENOMEM;
		dprintk(CVP_ERR, "%s: unable to remap firmware memory\n",
				__func__);
		goto err_release_fw;
	}
	rc = qcom_mdt_load(&pdev->dev, firmware, firmware_name,
			pas_id, virt, phys, res_size, NULL);
	
	if (rc) {
		dprintk(CVP_ERR, "%s: error %d loading \"%s\"\n",
				__func__, rc, firmware_name);
		goto err_mem_unmap;
	}
	rc = qcom_scm_pas_auth_and_reset(pas_id);
	if (rc) {
		dprintk(CVP_ERR, "%s: error %d authenticating \"%s\"\n",
				__func__, rc, firmware_name);
		goto err_mem_unmap;
	}
	// for the md_eva_dump, it can't be used based on KVM 
	// because the qcom_mdt_pas_load will do the vritual adress mapping internally
	rc = md_eva_dump("evafwdata", (uintptr_t)virt, phys, EVAFW_IMAGE_SIZE);
	if (rc) {
		dprintk(CVP_ERR, "%s: error %d in dumping \"%s\"\n",
				__func__, rc, firmware_name);
	}
	memunmap(virt);
#else
	rc = qcom_mdt_pas_load(ctx, firmware, firmware_name, NULL);
	qcom_scm_pas_metadata_release(ctx);
	if (rc) {
		dprintk(CVP_ERR, "%s: error %d loading \"%s\"\n",
				__func__, rc, firmware_name);
		goto err_release_fw;
	}
	if (device->resources.fw.iommu_domain) {
		rc = iommu_map(device->resources.fw.iommu_domain, 0, phys, res_size,
				IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV, GFP_KERNEL);
		if (rc) {
			dprintk(CVP_ERR, "%s: error %d mapping fw iommu \"%s\"\n",
				__func__, rc, firmware_name);
			goto err_release_fw;
		}
	}
	rc = qcom_scm_pas_prepare_and_auth_reset(ctx);
	if (rc) {
		dprintk(CVP_ERR, "%s: error %d authenticating \"%s\"\n",
				__func__, rc, firmware_name);
		goto err_iommu_unmap;
	}
	device->resources.fw.ctx = ctx;
#endif

	release_firmware(firmware);
	dprintk(CVP_CORE, "%s: firmware \"%s\" loaded successfully\n",
			__func__, firmware_name);
	return pas_id;

#ifdef CVP_KVM_ENABLED
err_iommu_unmap:
	if (device->resources.fw.iommu_domain)
		iommu_unmap(device->resources.fw.iommu_domain, 0, res_size);
#else
err_mem_unmap:
	if (virt)
		memunmap(virt);
#endif
err_release_fw:
	if (firmware)
		release_firmware(firmware);
	return rc;
}

int load_cvp_fw_impl(struct iris_hfi_device *device)
{
	int rc = 0;

	if (!device->resources.fw.cookie) {
		device->resources.fw.cookie =
#ifdef CVP_KVM_ENABLED
			__load_fw_to_memory(device,
			device->res->fw_name);
#else
			__load_fw_to_memory(device->res->pdev,
			device->res->fw_name);
#endif
		if (device->resources.fw.cookie <= 0) {
			dprintk(CVP_ERR, "Failed to download firmware\n");
			device->resources.fw.cookie = 0;
			rc = -ENOMEM;
		}
	}
	return rc;
}

int unload_cvp_fw_impl(struct iris_hfi_device *device)
{
	qcom_scm_pas_shutdown(device->resources.fw.cookie);
#ifdef CVP_KVM_ENABLED
	if (device->resources.fw.iommu_domain && device->resources.fw.ctx)
		iommu_unmap(device->resources.fw.iommu_domain, 0, device->resources.fw.ctx->mem_size);
	// use the devm API to allocate ctx, so shoule be fine in here without release
	device->resources.fw.ctx = NULL;
#endif
	device->resources.fw.cookie = 0;
	return 0;
}

#ifdef CVP_KVM_ENABLED
int init_cvp_fw(struct iris_hfi_device *device)
{
	struct platform_device_info info;
	struct iommu_domain *iommu_dom;
	struct platform_device *pdev;
	struct iris_resources *res = &device->resources;
	struct device_node *np;
	int ret;

	np = of_get_child_by_name(device->res->pdev->dev.of_node, "cvp-firmware");
	if (!np)
		return 0;

	memset(&info, 0, sizeof(info));
	info.fwnode   = &np->fwnode;
	info.parent   = &device->res->pdev->dev;
	info.name     = np->name;
	info.dma_mask = DMA_BIT_MASK(32);

	pdev = platform_device_register_full(&info);
	if (IS_ERR(pdev)) {
		of_node_put(np);
		return PTR_ERR(pdev);
	}

	pdev->dev.of_node = np;
	
	ret = of_dma_configure(&pdev->dev, np, true);
	if (ret)
		goto err_unregister;

	res->fw.dev = &pdev->dev;
	iommu_dom = iommu_get_domain_for_dev(res->fw.dev);
	if (!iommu_dom) {
		ret = -EINVAL;
		goto err_unset_fw_dev;
	}

	ret = iommu_attach_device(iommu_dom, res->fw.dev);
	if (ret)
		goto err_unset_fw_dev;

	res->fw.iommu_domain = iommu_dom;

	of_node_put(np);
	return 0;	

err_unset_fw_dev:
	res->fw.dev = NULL;
err_unregister:
	platform_device_unregister(pdev);
	of_node_put(np);
	return ret;	
}

void uninit_cvp_fw(struct iris_hfi_device *device)
{
	struct iris_resources *res = &device->resources;

	if (!res->fw.dev)
		return;
	
	if (res->fw.iommu_domain) {
		iommu_detach_device(res->fw.iommu_domain, res->fw.dev);
		res->fw.iommu_domain = NULL;
	}

	platform_device_unregister(to_platform_device(res->fw.dev));
	res->fw.dev = NULL;
}
#endif
