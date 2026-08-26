// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.​
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

#define MAX_FIRMWARE_NAME_SIZE 128


static int __load_fw_to_memory(struct platform_device *pdev,
		const char *fw_name)
{
	int rc = 0;
	const struct firmware *firmware = NULL;
	char firmware_name[MAX_FIRMWARE_NAME_SIZE] = {0};
	struct device_node *node = NULL;
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

	pas_id = ((struct msm_cvp_platform_data *)
		cvp_get_drv_data(&pdev->dev))->pas_id;

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

	rc = request_firmware(&firmware, firmware_name, &pdev->dev);

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
	memunmap(virt);

	release_firmware(firmware);
	dprintk(CVP_CORE, "%s: firmware \"%s\" loaded successfully\n",
			__func__, firmware_name);
	return pas_id;


err_mem_unmap:
	if (virt)
		memunmap(virt);
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
			__load_fw_to_memory(device->res->pdev,
			device->res->fw_name);
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
	device->resources.fw.cookie = 0;
	return 0;
}
