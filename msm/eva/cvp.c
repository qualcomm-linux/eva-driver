// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.​
 */

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <drm/drm_accel.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_gem.h>
#include "msm_cvp_core.h"
#include "msm_cvp_common.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_internal.h"
#include "msm_cvp_res_parse.h"
#include "msm_cvp_resources.h"
#include "msm_cvp_buf.h"
#include "cvp_hfi_api.h"
#include "cvp_private.h"
#include "msm_cvp_clocks.h"
#include "msm_cvp.h"
#include "vm/cvp_vm.h"
#include "target/cvp_kaanapali_hal.h"
#include "target/cvp_pakala_hal.h"
#include "target/cvp_hawi_hal.h"
#include "cvp_comm_def.h"

#define CLASS_NAME              "cvp"
#define DRIVER_NAME             "eva"

struct msm_cvp_drv *cvp_driver;

static int eva_drm_open(struct drm_device *dev, struct drm_file *file)
{
	struct msm_cvp_inst *inst;

	dprintk(CVP_SESS, "%s\n", __func__);

	inst = msm_cvp_open(MSM_CVP_USER, current);
	if (!inst) {
		dprintk(CVP_ERR, "Failed to create cvp instance\n");
		return -ENOMEM;
	}
	file->driver_priv = inst;
	return 0;
}

static void eva_drm_postclose(struct drm_device *dev, struct drm_file *file)
{
	struct msm_cvp_inst *inst = file->driver_priv;

	if (inst)
		msm_cvp_close(inst);
	file->driver_priv = NULL;
}

static const struct drm_ioctl_desc eva_ioctls[] = {
	DRM_IOCTL_DEF_DRV(EVA_GET_SESSION_INFO,   eva_ioctl_get_session_info,  0),
	DRM_IOCTL_DEF_DRV(EVA_UPDATE_POWER,       eva_ioctl_update_power,      0),
	DRM_IOCTL_DEF_DRV(EVA_SEND_CMD_PKT,       eva_ioctl_send_cmd_pkt,      0),
	DRM_IOCTL_DEF_DRV(EVA_RECEIVE_MSG_PKT,    eva_ioctl_receive_msg_pkt,   0),
	DRM_IOCTL_DEF_DRV(EVA_SET_SYS_PROPERTY,   eva_ioctl_set_sysprop,       0),
	DRM_IOCTL_DEF_DRV(EVA_GET_SYS_PROPERTY,   eva_ioctl_get_sysprop,       0),
	DRM_IOCTL_DEF_DRV(EVA_SESSION_CONTROL,    eva_ioctl_session_ctrl,      0),
	DRM_IOCTL_DEF_DRV(EVA_FLUSH_ALL,          eva_ioctl_flush_all,         0),
	DRM_IOCTL_DEF_DRV(EVA_FLUSH_FRAME,        eva_ioctl_flush_frame,       0),
};

static __poll_t eva_poll(struct file *filp, struct poll_table_struct *p)
{
	__poll_t rc = 0;
	struct drm_file *drm_filp = filp->private_data;
	struct msm_cvp_inst *inst = drm_filp->driver_priv;
	unsigned long flags = 0;

	poll_wait(filp, &inst->event_handler.wq, p);

	spin_lock_irqsave(&inst->event_handler.lock, flags);
	if (inst->event_handler.event == EVA_EVENT)
		rc |= EPOLLPRI;
	if (inst->event_handler.event == CVP_DUMP_EVENT)
		rc |= EPOLLIN;
	inst->event_handler.event = CVP_NO_EVENT;
	spin_unlock_irqrestore(&inst->event_handler.lock, flags);

	return rc;
}

static const struct file_operations eva_accel_fops = {
	.owner          = THIS_MODULE,
	.open           = accel_open,
	.release        = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.compat_ioctl   = drm_compat_ioctl,
	.poll           = eva_poll,
	.llseek         = noop_llseek,
	.mmap           = drm_gem_mmap,
	.fop_flags      = FOP_UNSIGNED_OFFSET,
};

static const struct drm_driver eva_drm_driver = {
	.driver_features = DRIVER_COMPUTE_ACCEL | DRIVER_GEM,
	.open            = eva_drm_open,
	.postclose       = eva_drm_postclose,
	.ioctls          = eva_ioctls,
	.num_ioctls      = ARRAY_SIZE(eva_ioctls),
	.fops            = &eva_accel_fops,
	.name            = DRIVER_NAME,
	.desc            = "Qualcomm EVA Accel driver",
};

static int read_platform_resources(struct msm_cvp_core *core,
		struct platform_device *pdev)
{
	int rc = 0;

	if (!core || !pdev) {
		dprintk(CVP_ERR, "%s: Invalid params %pK %pK\n",
			__func__, core, pdev);
		return -EINVAL;
	}

	core->hfi_type = CVP_HFI_IRIS;
	core->resources.pdev = pdev;
	if (pdev->dev.of_node) {
		/* Target supports DT, parse from it */
		rc = cvp_read_platform_resources_from_drv_data(core);
		if(rc){
			return rc;
		}
		rc = cvp_read_platform_resources_from_dt(core);
	} else {
		dprintk(CVP_ERR, "pdev node is NULL\n");
		rc = -EINVAL;
	}
	return rc;
}

static int msm_cvp_initialize_core(struct platform_device *pdev,
				struct msm_cvp_core *core)
{
	int i = 0;
	int rc = 0;

	if (!core)
		return -EINVAL;
	rc = read_platform_resources(core, pdev);
	if (rc) {
		dprintk(CVP_ERR, "Failed to get platform resources\n");
		return rc;
	}

	INIT_LIST_HEAD(&core->instances);
	mutex_init(&core->lock);
	mutex_init(&core->clk_lock);
	mutex_init(&core->idr_lock);
	idr_init(&core->sess_idr);

	core->state = CVP_CORE_UNINIT;
	for (i = SYS_MSG_INDEX(SYS_MSG_START);
		i <= SYS_MSG_INDEX(SYS_MSG_END); i++) {
		init_completion(&core->completions[i]);
	}

	INIT_WORK(&core->ssr_work, msm_cvp_ssr_handler);
	INIT_WORK(&core->iova_cleanup_work, msm_cvp_iova_cleanup_handler);
	core->ssr_count = 0;

	return rc;
}

static ssize_t link_name_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct msm_cvp_core *core = dev_get_drvdata(dev);

	if (core)
		if (dev == core->dev)
			return snprintf(buf, PAGE_SIZE, "msm_cvp\n");
		else
			return 0;
	else
		return 0;
}

static DEVICE_ATTR_RO(link_name);

static ssize_t pwr_collapse_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long val = 0;
	int rc = 0;
	struct msm_cvp_core *core = NULL;

	rc = kstrtoul(buf, 0, &val);
	if (rc)
		return rc;
	else if (!val)
		return -EINVAL;

	core = cvp_driver->cvp_core;
	if (!core)
		return -EINVAL;
	core->resources.msm_cvp_pwr_collapse_delay = val;
	return count;
}

static ssize_t pwr_collapse_delay_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct msm_cvp_core *core = NULL;

	core = cvp_driver->cvp_core;
	if (!core)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%u\n",
		core->resources.msm_cvp_pwr_collapse_delay);
}

static DEVICE_ATTR_RW(pwr_collapse_delay);

static ssize_t sku_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d",
			cvp_driver->sku_version);
}

static DEVICE_ATTR_RO(sku_version);

static ssize_t boot_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int rc = 0, val = 0;
	static int booted;
	dprintk(CVP_INFO, "Processing boot sysfs command");
	rc = kstrtoint(buf, 0, &val);
	if (rc || val < 0) {
		dprintk(CVP_WARN,
			"Invalid boot value: %s\n", buf);
		return -EINVAL;
	}

	if (val == 1 && booted == 0) {
		struct msm_cvp_inst *inst;

		inst = msm_cvp_open(MSM_CVP_BOOT, current);
		if (!inst) {
			dprintk(CVP_ERR,
			"Failed to create cvp instance\n");
			return -ENOMEM;
		}
		rc = msm_cvp_close(inst);
		if (rc) {
			dprintk(CVP_ERR,
			"Failed to close cvp instance\n");
			return rc;
		}
	} else if (val == 2) {
#ifdef USE_PRESIL
		struct msm_cvp_inst *inst;

		inst = msm_cvp_open(MSM_CVP_USER, current);
		if (!inst) {
			dprintk(CVP_ERR,
			"Failed to create eva instance\n");
			return -ENOMEM;
		}
		rc = msm_cvp_session_create(inst);
		if (rc)
			dprintk(CVP_ERR, "Failed to create eva session\n");

		rc = msm_cvp_close(inst);
		if (rc) {
			dprintk(CVP_ERR,
			"Failed to close eva instance\n");
			return rc;
		}
#endif
	}
	booted = 1;
	return count;
}

static DEVICE_ATTR_WO(boot);

static struct attribute *msm_cvp_core_attrs[] = {
		&dev_attr_pwr_collapse_delay.attr,
		&dev_attr_sku_version.attr,
		&dev_attr_link_name.attr,
		&dev_attr_boot.attr,
		NULL
};

static struct attribute_group msm_cvp_core_attr_group = {
		.attrs = msm_cvp_core_attrs,
};

static const struct of_device_id msm_cvp_plat_match[] = {
	{.compatible = "qcom,kaanapali-eva"},
	{.compatible = "qcom,glymur-eva"},
	{}
};

static int msm_cvp_probe_bus(struct platform_device *pdev)
{
	return cvp_read_bus_resources(pdev);
}

static int msm_cvp_probe_ipclite_mappings(struct platform_device *pdev)
{
	return cvp_read_ipclite_mappings(pdev);
}

static int msm_probe_cvp_device(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_cvp_core *core;

	if (!cvp_driver) {
		dprintk(CVP_ERR, "Invalid cvp driver\n");
		return -EINVAL;
	}

	core = devm_drm_dev_alloc(&pdev->dev, &eva_drm_driver,
				  struct msm_cvp_core, drm_dev);
	if (IS_ERR(core)) {
		dprintk(CVP_ERR, "Failed to alloc DRM device: %ld\n",
			PTR_ERR(core));
		return PTR_ERR(core);
	}

	core->dev = core->drm_dev.dev;

	core->platform_data = cvp_get_drv_data(&pdev->dev);
	dev_set_drvdata(&pdev->dev, core);
	rc = msm_cvp_initialize_core(pdev, core);
	if (rc) {
		dprintk(CVP_ERR, "Failed to init core\n");
		goto err_core_init;
	}

	rc = sysfs_create_group(&core->drm_dev.dev->kobj, &msm_cvp_core_attr_group);
	if (rc) {
		dprintk(CVP_ERR, "Failed to create attributes\n");
		goto err_core_init;
	}

#ifdef CVP_GUNYAH_ENABLED
	/* VM manager shall be started before HFI init */
	vm_manager.vm_ops->vm_start(core);
#endif

	core->dev_ops = cvp_hfi_initialize(core->hfi_type,
				&core->resources, &cvp_handle_cmd_response);
	if (IS_ERR_OR_NULL(core->dev_ops)) {
		mutex_lock(&cvp_driver->lock);
		mutex_unlock(&cvp_driver->lock);

		rc = PTR_ERR(core->dev_ops) ?: -EBADHANDLE;
		if (rc != -EPROBE_DEFER)
			dprintk(CVP_ERR, "Failed to create HFI device\n");
		else
			dprintk(CVP_CORE, "msm_cvp: request probe defer\n");
		goto err_hfi_initialize;
	}

	mutex_lock(&cvp_driver->lock);
	cvp_driver->cvp_core = core;
	mutex_unlock(&cvp_driver->lock);

	cvp_driver->debugfs_root = core->drm_dev.debugfs_root;
	if (!cvp_driver->debugfs_root)
		dprintk(CVP_ERR, "Failed to create debugfs for msm_cvp\n");
	
	msm_cvp_debugfs_init_drv(cvp_driver->debugfs_root);
	core->debugfs_root = msm_cvp_debugfs_init_core(
		core, cvp_driver->debugfs_root);

	cvp_driver->sku_version = core->resources.sku_version;

	core->kmd_trace.kmd_debug_log.log = vmalloc(sizeof(struct cvp_debug_log));
	if (!core->kmd_trace.kmd_debug_log.log) {
		dprintk(CVP_ERR, "%s: cvp_debug_log memory allocation failed, size 0x%x\n",
				__func__, sizeof(struct cvp_debug_log));
		rc = -ENOMEM;
		goto fail_dbglog_alloc;
	} else {
		memset((void *)core->kmd_trace.kmd_debug_log.log, 0, sizeof(struct cvp_debug_log));
	}

	dprintk(CVP_CORE, "populating sub devices\n");
	/*
	 * Trigger probe for remaining sub-devices (e.g. qcom,msm-cvp,mem-cdsp).
	 * Context banks are created explicitly below via
	 * cvp_init_context_bank_devices(), not through this auto-probe path.
	 */
	rc = of_platform_populate(pdev->dev.of_node, msm_cvp_plat_match, NULL,
			&pdev->dev);
	if (rc) {
		dprintk(CVP_ERR, "Failed to trigger probe for sub-devices\n");
		goto err_fail_sub_device_probe;
	}
	atomic64_set(&core->kernel_trans_id, MAX_PKT_IDX);
    
	rc = msm_cvp_probe_bus(pdev);
	dprintk(CVP_INFO, "cvp %s bus prob return value is %d", dev_name(&pdev->dev), rc);
	if (rc) {
		dprintk(CVP_ERR, "Failed to probe bus resources\n");
		goto err_fail_sub_device_probe;
	}
    
	rc = cvp_init_context_bank_devices(pdev, core);
	if (rc) {
		dprintk(CVP_ERR, "Failed to init context bank devices\n");
		goto err_fail_sub_device_probe;
	}
	
#ifdef CVP_IPCLITE_MAPPING_ENABLED
	rc = msm_cvp_probe_ipclite_mappings(pdev);
	dprintk(CVP_INFO, "cvp %s ipclite mappings prob return value is %d", dev_name(&pdev->dev), rc);
	if (rc) {
		dprintk(CVP_ERR, "Failed to probe ipclite mappings resources\n");
		goto err_fail_sub_device_probe;
	}
#endif

	if (core->platform_data->hal_version == DEFAULT_HAL_VER) {
		dprintk(CVP_DBG, "%s: using default");
		set_pakala_hal_functions();
	} else if (core->platform_data->hal_version == KNP_HAL_VER) {
		dprintk(CVP_DBG, "%s: using knp");
		set_kaanapali_hal_functions();
	} else if (core->platform_data->hal_version == HAWI_HAL_VER) {
		dprintk(CVP_DBG, "%s: using hawi");
		set_hawi_hal_functions();
	} else {
		dprintk(CVP_ERR, "Invalid hal_version %d\n", core->platform_data->hal_version);
		rc = -EINVAL;
	}

	rc = drm_dev_register(&core->drm_dev, 0);
	if (rc) {
		dprintk(CVP_ERR, "drm_dev_register failed: %d\n", rc);
		goto err_drm_register;
	}

	return 0;

err_drm_register:
err_fail_sub_device_probe:
	vfree(core->kmd_trace.kmd_debug_log.log);
	core->kmd_trace.kmd_debug_log.log = NULL;
fail_dbglog_alloc:
	cvp_hfi_deinitialize(core->hfi_type, core->dev_ops);
	debugfs_remove_recursive(cvp_driver->debugfs_root);
	if (core->cb_devs)
		cvp_deinit_context_bank_devices(core);
err_hfi_initialize:
	sysfs_remove_group(&pdev->dev.kobj, &msm_cvp_core_attr_group);
err_core_init:
	dev_set_drvdata(&pdev->dev, NULL);
	return rc;
}

static int msm_cvp_probe_mem_cdsp(struct platform_device *pdev)
{
	return cvp_read_mem_cdsp_resources_from_dt(pdev);
}

static int msm_cvp_probe(struct platform_device *pdev)
{
	if (!msm_cvp_probe_allowed)
		return 0;
	/*
	 * Sub devices probe will be triggered by of_platform_populate() towards
	 * the end of the probe function after msm-cvp device probe is
	 * completed. Return immediately after completing sub-device probe.
	 */
	int ret = 0;
	if (of_match_device(msm_cvp_plat_match, &pdev->dev)) {
		ret = msm_probe_cvp_device(pdev);
		dprintk(CVP_INFO, "cvp %s cvp device prob return value is %d", dev_name(&pdev->dev), ret);
		return ret;
	}
	MSM_CVP_ERROR(1);
	return -EINVAL;
}

#if KERNEL_VERSION(6, 10, 0) <= LINUX_VERSION_CODE
static void msm_cvp_remove(struct platform_device *pdev)
#else
static int msm_cvp_remove(struct platform_device *pdev)
#endif
{
	int rc = 0;
	struct msm_cvp_core *core;

	if (!pdev) {
		dprintk(CVP_ERR, "%s invalid input %pK", __func__, pdev);
		rc = -EINVAL;
		goto exit;
	}

	if (of_match_device(msm_cvp_plat_match, &pdev->dev) )
		core = dev_get_drvdata(&pdev->dev);
	else
		core = dev_get_drvdata(pdev->dev.parent);

	if (!core) {
		dprintk(CVP_ERR, "%s invalid core", __func__);
		rc = -EINVAL;
		goto exit;
	}

	if (core->kmd_trace.kmd_debug_log.log)
		vfree(core->kmd_trace.kmd_debug_log.log);
	
	drm_dev_unregister(&core->drm_dev);
	cvp_hfi_deinitialize(core->hfi_type, core->dev_ops);
	msm_cvp_free_platform_resources(&core->resources);

	if (core->cb_devs)
		cvp_deinit_context_bank_devices(core);
	sysfs_remove_group(&pdev->dev.kobj, &msm_cvp_core_attr_group);
	dev_set_drvdata(&pdev->dev, NULL);
	idr_destroy(&core->sess_idr);
	mutex_destroy(&core->idr_lock);
	mutex_destroy(&core->lock);
	mutex_destroy(&core->clk_lock);
exit:
#if KERNEL_VERSION(6, 10, 0) > LINUX_VERSION_CODE
	return rc;
#else
	return;
#endif
}

static int msm_cvp_pm_suspend(struct device *dev)
{
	int rc = 0;
	struct msm_cvp_core *core;

	/*
	 * Bail out if
	 * - driver possibly not probed yet
	 * - not the main device. We don't support power management on
	 *   subdevices (e.g. context banks)
	 */
	if (!dev || !dev->driver ||
		!of_match_device(msm_cvp_plat_match, dev) )
		return 0;

	core = dev_get_drvdata(dev);
	if (!core) {
		dprintk(CVP_ERR, "%s invalid core\n", __func__);
		return -EINVAL;
	}

	rc = msm_cvp_suspend();
	if (rc == -ENOTSUPP)
		rc = 0;
	else if (rc)
		dprintk(CVP_WARN, "Failed to suspend: %d\n", rc);


	return rc;
}

static int msm_cvp_pm_resume(struct device *dev)
{
	dprintk(CVP_INFO, "%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops msm_cvp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(msm_cvp_pm_suspend, msm_cvp_pm_resume)
};

MODULE_DEVICE_TABLE(of, msm_cvp_plat_match);

static struct platform_driver msm_cvp_driver = {
	.probe = msm_cvp_probe,
	.remove = msm_cvp_remove,
	.driver = {
		.name = "msm_cvp",
		.of_match_table = msm_cvp_plat_match,
		.pm = &msm_cvp_pm_ops,
	},
};

static int __init msm_cvp_init(void)
{
	int rc = 0;

	cvp_driver = kzalloc(sizeof(*cvp_driver), GFP_KERNEL);
	if (!cvp_driver) {
		dprintk(CVP_ERR,
			"Failed to allocate memroy for msm_cvp_drv\n");
		return -ENOMEM;
	}

	mutex_init(&cvp_driver->lock);

	rc = platform_driver_register(&msm_cvp_driver);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to register platform driver\n");
		kfree(cvp_driver);
		cvp_driver = NULL;
		return rc;
	}

	cvp_driver->msg_cache.cache = KMEM_CACHE(cvp_session_msg, 0);
	cvp_driver->frame_cache.cache = KMEM_CACHE(msm_cvp_frame, 0);
	cvp_driver->buf_cache.cache = KMEM_CACHE(cvp_internal_buf, 0);
	cvp_driver->smem_cache.cache = KMEM_CACHE(msm_cvp_smem, 0);

	return rc;
}

static void __exit msm_cvp_exit(void)
{
	kmem_cache_destroy(cvp_driver->msg_cache.cache);
	kmem_cache_destroy(cvp_driver->frame_cache.cache);
	kmem_cache_destroy(cvp_driver->buf_cache.cache);
	kmem_cache_destroy(cvp_driver->smem_cache.cache);

	platform_driver_unregister(&msm_cvp_driver);
	debugfs_remove_recursive(cvp_driver->debugfs_root);
	mutex_destroy(&cvp_driver->lock);
	kfree(cvp_driver);
	cvp_driver = NULL;
}

module_init(msm_cvp_init);
module_exit(msm_cvp_exit);

MODULE_SOFTDEP("pre: msm-mmrm");
MODULE_SOFTDEP("pre: synx-driver");
MODULE_SOFTDEP("pre: frpc-adsprpc");
MODULE_LICENSE("GPL v2");
#if (KERNEL_VERSION(6, 13, 0) <= LINUX_VERSION_CODE)
MODULE_IMPORT_NS("DMA_BUF");
#else
MODULE_IMPORT_NS(DMA_BUF);
#endif
