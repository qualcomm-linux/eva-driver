// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/devfreq.h>
#include "msm_cvp_common.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_clocks.h"
#include <linux/types.h>

int msm_cvp_set_fmax(struct msm_cvp_core *core)
{
	struct cvp_hfi_ops *ops_tbl;
	struct allowed_clock_rates_table *tbl = NULL;
	unsigned int tbl_size, max_rate;
	int rc;

	if (!core || !core->dev_ops) {
		dprintk(CVP_ERR, "%s Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}

	tbl = core->resources.allowed_clks_tbl;
	tbl_size = core->resources.allowed_clks_tbl_size;
	max_rate = tbl[tbl_size - 1].clock_rate;
	ops_tbl = core->dev_ops;
	rc = call_hfi_op(ops_tbl, scale_clocks,
		ops_tbl->hfi_device_data, max_rate);

	return rc;
}

int msm_cvp_set_clocks(struct msm_cvp_core *core)
{
	struct cvp_hfi_ops *ops_tbl;
	int rc;

	if (!core || !core->dev_ops) {
		dprintk(CVP_ERR, "%s Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}

	ops_tbl = core->dev_ops;
	rc = call_hfi_op(ops_tbl, scale_clocks,
		ops_tbl->hfi_device_data, core->curr_freq);
	return rc;
}



int msm_cvp_opp_set_rate(struct iris_hfi_device *device, u64 freq)
{
	struct dev_pm_opp *opp;
	unsigned long opp_freq = freq;
	int ret;

	device->clk_freq = freq;

	/*
	 * Use devfreq_recommended_opp to find the best OPP >= requested freq.
	 * dev_pm_opp_set_opp then sets both OPP clocks (core0 + eva0) and
	 * votes the RPMH power domain corners (via required-opps in DT).
	 */
	opp = devfreq_recommended_opp(&device->res->pdev->dev, &opp_freq, 0);
	if (IS_ERR(opp)) {
		dprintk(CVP_ERR, "%s: unable to find recommended OPP for freq %llu\n",
			__func__, freq);
		return PTR_ERR(opp);
	}
    
	/* dev_pm_opp_set_opp applies the selected OPP to the device. 
	* Internally calls clk_set_rate() for each clock in opp_clk_tbl 
	*/
	ret = dev_pm_opp_set_opp(&device->res->pdev->dev, opp);
	/* dev_pm_opp_put releases the reference to the OPP handle obtained from devfreq_recommended_opp */
	dev_pm_opp_put(opp);
	if (ret) {
		dprintk(CVP_ERR, "%s: failed to set OPP: %d\n", __func__, ret);
		return ret;
	} else {
		struct clock_info *cl;

		device->clk_freq = freq;

		/* Print actual clock rates for core0 and eva0 after OPP set */
		iris_hfi_for_each_clock(device, cl) {
			if (!strcmp(cl->name, "core0") ||
			    !strcmp(cl->name, "eva0"))
				dprintk(CVP_PWR,
					"%s: OPP set, %s = %lu Hz\n",
					__func__, cl->name,
					clk_get_rate(cl->clk));
		}
	}

	return ret;
}

int msm_cvp_scale_clocks(struct iris_hfi_device *device)
{
	int rc = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	u64 rate = 0;

	allowed_clks_tbl = device->res->allowed_clks_tbl;

	rate = device->clk_freq ? device->clk_freq :
		allowed_clks_tbl[0].clock_rate;

	dprintk(CVP_PWR, "%s: scale clock rate %d\n", __func__, rate);

	rc = msm_cvp_opp_set_rate(device, rate);
	return rc;
}

int msm_cvp_prepare_enable_clk(struct iris_hfi_device *device,
		const char *name)
{
	struct clock_info *cl = NULL;
	int rc = 0;

	if (!device) {
		dprintk(CVP_ERR, "Invalid params: %pK\n", device);
		return -EINVAL;
	}

	iris_hfi_for_each_clock(device, cl) {
		if (strcmp(cl->name, name))
                        continue;
		/*
		* For the clocks we control, set the rate prior to preparing
		* them.  Since we don't really have a load at this point,
		* scale it to the lowest frequency possible
		*/
		if (!cl->clk) {
			dprintk(CVP_PWR, "%s %s already enabled by framework",
				__func__, cl->name);
			return 0;
		}

		/*
		 * For OPP-managed clocks (eva0), dev_pm_opp_set_opp handles
		 * both eva0 and core0 together, and also votes mxc/mmcx corners.
		 * Call msm_cvp_opp_set_rate(0) when enabling eva0 to set
		 * minimum OPP before clock enable. eva0 is enabled first (in __power_on_controller),
		 * core0 second (in __power_on_core).
		 */
		if (!strcmp(cl->name, "eva0")) {
			msm_cvp_opp_set_rate(device, 0);
		}
		rc = clk_prepare_enable(cl->clk);
		if (rc) {
			dprintk(CVP_ERR, "Failed to enable clock %s\n",
				cl->name);
			return rc;
		}
		if (!__clk_is_enabled(cl->clk)) {
			dprintk(CVP_ERR, "%s: clock %s not enabled\n",
					__func__, cl->name);
			clk_disable_unprepare(cl->clk);
			return -EINVAL;
		}

		dprintk(CVP_PWR, "Clock: %s prepared and enabled\n",
				cl->name);
		return 0;
	}

	dprintk(CVP_ERR, "%s clock %s not found\n", __func__, name);
	return -EINVAL;
}

int msm_cvp_disable_unprepare_clk(struct iris_hfi_device *device,
		const char *name)
{
	struct clock_info *cl;

	if (!device) {
		dprintk(CVP_ERR, "Invalid params: %pK\n", device);
		return -EINVAL;
	}

	iris_hfi_for_each_clock_reverse(device, cl) {
		if (strcmp(cl->name, name))
			continue;
		if (!cl->clk) {
			dprintk(CVP_PWR, "%s %s always enabled by framework",
				__func__, cl->name);
			return 0;
		}
		clk_disable_unprepare(cl->clk);
		dprintk(CVP_PWR, "Clock: %s disable and unprepare\n",
			cl->name);

		/*
		 * msm_cvp_opp_set_rate sets both scalable clocks (eva0 + core0)
		 * atomically. core0 is disabled first (in __power_off_core),
		 * eva0 last (in __power_off_controller). Set OPP to 0 after
		 * disabling eva0 (last scalable clock). Calling for core0 is
		 * redundant since eva0 is still running when core0 is disabled.
		 */
		if (!strcmp(cl->name, "eva0")) {
			msm_cvp_opp_set_rate(device, 0);
		}
		return 0;
	}

	dprintk(CVP_ERR, "%s clock %s not found\n", __func__, name);
	return -EINVAL;
}

int msm_cvp_init_clocks(struct iris_hfi_device *device)
{
	int rc = 0;
	struct clock_info *cl = NULL;

	if (!device) {
		dprintk(CVP_ERR, "Invalid params: %pK\n", device);
		return -EINVAL;
	}

	iris_hfi_for_each_clock(device, cl) {

		dprintk(CVP_PWR, "%s: scalable? %d, count %d\n",
			cl->name, cl->has_scaling, cl->count);
	}

	iris_hfi_for_each_clock(device, cl) {
		if (!cl->clk) {
			cl->clk = clk_get(&device->res->pdev->dev, cl->name);
			if (IS_ERR(cl->clk)) {
				rc = PTR_ERR(cl->clk);
				dprintk(CVP_ERR,
					"Failed to get clock: %s, rc %d\n",
					cl->name, rc);
				cl->clk = NULL;
				goto err_clk_get;
			}
		}
	}
	device->clk_freq = 0;
	return 0;

err_clk_get:
	msm_cvp_deinit_clocks(device);
	return rc;
}

void msm_cvp_deinit_clocks(struct iris_hfi_device *device)
{
	struct clock_info *cl;

	device->clk_freq = 0;
	iris_hfi_for_each_clock_reverse(device, cl) {
		if (cl->clk) {
			clk_put(cl->clk);
			cl->clk = NULL;
		}
	}
}

int msm_cvp_set_bw(struct msm_cvp_core *core, struct bus_info *bus, unsigned long bw)
{
	struct cvp_hfi_ops *ops_tbl;
	int rc;

	if (!core || !core->dev_ops) {
		dprintk(CVP_ERR, "%s Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}

	ops_tbl = core->dev_ops;
	rc = call_hfi_op(ops_tbl, vote_bus, ops_tbl->hfi_device_data, bus, bw);
	return rc;

}

int cvp_set_bw(struct bus_info *bus, unsigned long bw)
{
	int rc = 0;

	if (!bus->client)
		return -EINVAL;
	dprintk(CVP_PWR, "bus->name = %s to bw = %u\n",
			bus->name, bw);

	rc = icc_set_bw(bus->client, bw, 0);
	if (rc)
		dprintk(CVP_ERR, "Failed voting bus %s to ab %u\n",
			bus->name, bw);

	return rc;
}

