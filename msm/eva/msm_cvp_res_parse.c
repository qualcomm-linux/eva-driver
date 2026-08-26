// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/iommu.h>
#include <linux/interconnect.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include "msm_cvp_debug.h"
#include "msm_cvp_resources.h"
#include "msm_cvp_res_parse.h"
#include "cvp_core_hfi.h"
#include <linux/device.h>

enum clock_properties {
	CLOCK_PROP_HAS_SCALING = 1 << 0,
	CLOCK_PROP_HAS_MEM_RETENTION    = 1 << 1,
};

#define PERF_GOV "performance"

static inline struct device *msm_iommu_get_ctx(const char *ctx_name)
{
	return NULL;
}

static size_t get_u32_array_num_elements(struct device_node *np,
					char *name)
{
	int len;
	size_t num_elements = 0;

	if (!of_get_property(np, name, &len)) {
		dprintk(CVP_ERR, "Failed to read %s from device tree\n",
			name);
		goto fail_read;
	}

	num_elements = len / sizeof(u32);
	if (num_elements <= 0) {
		dprintk(CVP_ERR, "%s not specified in device tree\n",
			name);
		goto fail_read;
	}
	return num_elements;

fail_read:
	return 0;
}

static inline void msm_cvp_free_allowed_clocks_table(
		struct msm_cvp_platform_resources *res)
{
	res->allowed_clks_tbl = NULL;
}

static inline void msm_cvp_free_reg_table(
			struct msm_cvp_platform_resources *res)
{
	res->reg_set.reg_tbl = NULL;
}

static inline void err_load_load_PD_table(
			struct msm_cvp_platform_resources *res)
{
	res->pd_set.pd_tbl = NULL;
}

static inline void msm_cvp_free_qdss_addr_table(
			struct msm_cvp_platform_resources *res)
{
	res->qdss_addr_set.addr_tbl = NULL;
}

static inline void msm_cvp_free_bus_vectors(
			struct msm_cvp_platform_resources *res)
{
	devm_kfree(&res->pdev->dev, res->bus_set.bus_tbl);
	res->bus_set.bus_tbl = NULL;
	res->bus_set.count = 0;
}

static inline void msm_cvp_free_pd_table(
			struct msm_cvp_platform_resources *res)
{
	int i = 0;

	for (i = 0; i < res->pd_set.count; ++i) {
		struct power_domain_info *pd_info =
			&res->pd_set.pd_tbl[i];

		pd_info->name = NULL;
		pd_info->pd_device = NULL;
	}

	res->pd_set.pd_tbl = NULL;
	res->pd_set.count = 0;
}

static inline void msm_cvp_free_clock_table(
			struct msm_cvp_platform_resources *res)
{
	res->clock_set.clock_tbl = NULL;
	res->clock_set.count = 0;
}

void msm_cvp_free_platform_resources(
			struct msm_cvp_platform_resources *res)
{
	msm_cvp_free_clock_table(res);
	err_load_load_PD_table(res);
	msm_cvp_free_allowed_clocks_table(res);
	msm_cvp_free_reg_table(res);
	msm_cvp_free_qdss_addr_table(res);
	msm_cvp_free_bus_vectors(res);
}

static int msm_cvp_load_ipcc_regs(struct msm_cvp_platform_resources *res, struct msm_cvp_platform_data *platform_data)
{
	int ret = 0;
   
    /*
     * IPCC register region is driver-owned in upstream.
     * DT property qcom,gcc-reg has been removed.
     */

    res->ipcc_reg_base = platform_data->ipcc_regs->base;
    res->ipcc_reg_size  = platform_data->ipcc_regs->size;

	dprintk(CVP_CORE,
		"ipcc reg_base = %x, reg_size = %x\n",
		res->ipcc_reg_base,
		res->ipcc_reg_size
	);

	return ret;
}

static int msm_cvp_load_regspace_mapping(struct msm_cvp_platform_resources *res,struct msm_cvp_platform_data *platform_data)
{
	int ret = 0;

    res->reg_mappings.hwmutex_iova     = platform_data->regspace_mappings->hwmutex.iova;
    res->reg_mappings.hwmutex_size     = platform_data->regspace_mappings->hwmutex.size;
    res->reg_mappings.hwmutex_phyaddr  = platform_data->regspace_mappings->hwmutex.phys;

    res->reg_mappings.aon_iova         = platform_data->regspace_mappings->aon.iova;
    res->reg_mappings.aon_size         = platform_data->regspace_mappings->aon.size;
    res->reg_mappings.aon_phyaddr      = platform_data->regspace_mappings->aon.phys;
    
    res->reg_mappings.timer_iova       = platform_data->regspace_mappings->aon_timer.iova;
    res->reg_mappings.timer_size       = platform_data->regspace_mappings->aon_timer.size;
    res->reg_mappings.timer_phyaddr    = platform_data->regspace_mappings->aon_timer.phys;

	dprintk(CVP_CORE,
	"reg mappings %#x %#x %#x %#x %#x %#X %#x %#x %#x %#x %#x %#x\n",
	res->reg_mappings.ipclite_iova, res->reg_mappings.ipclite_size,
	res->reg_mappings.ipclite_phyaddr, res->reg_mappings.hwmutex_iova,
	res->reg_mappings.hwmutex_size, res->reg_mappings.hwmutex_phyaddr,
	res->reg_mappings.aon_iova, res->reg_mappings.aon_size,
	res->reg_mappings.aon_phyaddr,  res->reg_mappings.timer_iova,
	res->reg_mappings.timer_size, res->reg_mappings.timer_phyaddr);

	return ret;
}

static int msm_cvp_load_gcc_regs(struct msm_cvp_platform_resources *res, struct msm_cvp_platform_data *platform_data)
{
    
    int ret = 0;
    /*
     * GCC register region is driver-owned in upstream.
     * DT property qcom,gcc-reg has been removed.
     */

    res->gcc_reg_base = platform_data->gcc_regs->base;
    res->gcc_reg_size   = platform_data->gcc_regs->size;

    dprintk(CVP_CORE,
        "GCC reg region: offset=0x%x size=0x%x\n",
        res->gcc_reg_base,
        res->gcc_reg_size);

    return ret;
}

static int msm_cvp_load_reg_table(struct msm_cvp_platform_resources *res, struct msm_cvp_platform_data *platform_data)
{
    struct platform_device *pdev = res->pdev;
    struct reg_set *reg_set ;
    int i, rc = 0;

	/*
		* qcom,reg-presets is an optional property.  It likely won't be
 		* present if we don't have any register settings to program
 	*/
    if (!platform_data->reg_presets->count) {
        dprintk(CVP_CORE, "qcom,reg-presets not found in driver configs\n");
        return 0;
    }
    reg_set = &res->reg_set;
    reg_set->count = platform_data->reg_presets->count;
	
    if (!reg_set->count) {
		dprintk(CVP_CORE, "no elements in reg set\n");
		return rc;
	}
	
    reg_set->reg_tbl = devm_kzalloc(&pdev->dev,
                    reg_set->count *
                    sizeof(*reg_set->reg_tbl),
                    GFP_KERNEL);

    if (!reg_set->reg_tbl) {
        dprintk(CVP_ERR, "%s: Failed to allocate reg preset table\n",
            __func__);      
		rc = -ENOMEM;
				goto err_free;
    }
 
    // Copy driver-owned preset values into runtime table.
    
    memcpy(reg_set->reg_tbl,
           platform_data->reg_presets->tbl,
           platform_data->reg_presets->count * sizeof(*reg_set->reg_tbl));

    for (i = 0; i < reg_set->count; i++) {
        dprintk(CVP_CORE,
            "reg preset: reg=0x%x value=0x%x\n",
            reg_set->reg_tbl[i].reg,
            reg_set->reg_tbl[i].value);
    }

    return rc;
	
err_free:
    
    msm_cvp_free_reg_table(res);
    return rc;

}
static int msm_cvp_load_qdss_table(struct msm_cvp_platform_resources *res)
{
	struct addr_set *qdss_addr_set;
	struct platform_device *pdev = res->pdev;
	int i;
	int rc = 0;

	if (!of_find_property(pdev->dev.of_node, "qcom,qdss-presets", NULL)) {
		/*
		 * qcom,qdss-presets is an optional property. It likely won't be
		 * present if we don't have any register settings to program
		 */
		dprintk(CVP_CORE, "qcom,qdss-presets not found\n");
		return rc;
	}

	qdss_addr_set = &res->qdss_addr_set;
	qdss_addr_set->count = get_u32_array_num_elements(pdev->dev.of_node,
					"qcom,qdss-presets");
	qdss_addr_set->count /= sizeof(*qdss_addr_set->addr_tbl) / sizeof(u32);

	if (!qdss_addr_set->count) {
		dprintk(CVP_CORE, "no elements in qdss reg set\n");
		return rc;
	}

	qdss_addr_set->addr_tbl = devm_kzalloc(&pdev->dev,
			qdss_addr_set->count * sizeof(*qdss_addr_set->addr_tbl),
			GFP_KERNEL);
	if (!qdss_addr_set->addr_tbl) {
		dprintk(CVP_ERR, "%s Failed to alloc register table\n",
			__func__);
		rc = -ENOMEM;
		goto err_qdss_addr_tbl;
	}

	rc = of_property_read_u32_array(pdev->dev.of_node, "qcom,qdss-presets",
		(u32 *)qdss_addr_set->addr_tbl, qdss_addr_set->count * 2);
	if (rc) {
		dprintk(CVP_ERR, "Failed to read qdss address table\n");
		msm_cvp_free_qdss_addr_table(res);
		rc = -EINVAL;
		goto err_qdss_addr_tbl;
	}

	for (i = 0; i < qdss_addr_set->count; i++) {
		dprintk(CVP_CORE, "qdss addr = %x, value = %x\n",
				qdss_addr_set->addr_tbl[i].start,
				qdss_addr_set->addr_tbl[i].size);
	}
err_qdss_addr_tbl:
	return rc;
}

static int msm_cvp_load_subcache_info(struct msm_cvp_platform_resources *res, struct msm_cvp_platform_data *platform_data)
{
    int rc = 0;
    int c;
    struct platform_device *pdev = res->pdev;
    struct subcache_set *subcaches = &res->subcache_set;
    int num_subcaches;

    num_subcaches = platform_data->subcache_desc->num_slices;

    subcaches->subcache_tbl = devm_kzalloc(&pdev->dev,
        sizeof(*subcaches->subcache_tbl) * num_subcaches,
        GFP_KERNEL);
    if (!subcaches->subcache_tbl){
        dprintk(CVP_ERR,"Failed to allocate memory for subcache tbl\n");
		rc = -ENOMEM;
		goto err_load_subcache_table_fail;
	} 		

    subcaches->count = num_subcaches;

    for (c = 0; c < num_subcaches; c++)
        subcaches->subcache_tbl[c].name = platform_data->subcache_desc->cache_slice_names[c];

    res->sys_cache_present = true;

    dprintk(CVP_CORE, "Found %d CVP subcaches\n", num_subcaches);

    return 0;
err_load_subcache_table_fail:
	res->sys_cache_present = false;
	subcaches->count = 0;
	subcaches->subcache_tbl = NULL;

	return rc;
}

/**
 * msm_cvp_load_u32_table() - load dtsi table entries
 * @pdev: A pointer to the platform device.
 * @of_node:      A pointer to the device node.
 * @table_name:   A pointer to the dtsi table entry name.
 * @struct_size:  The size of the structure which is nothing but
 *                a single entry in the dtsi table.
 * @table:        A pointer to the table pointer which needs to be
 *                filled by the dtsi table entries.
 * @num_elements: Number of elements pointer which needs to be filled
 *                with the number of elements in the table.
 *
 * This is a generic implementation to load single or multiple array
 * table from dtsi. The array elements should be of size equal to u32.
 *
 * Return:        Return '0' for success else appropriate error value.
 */
int msm_cvp_load_u32_table(struct platform_device *pdev,
		struct device_node *of_node, char *table_name, int struct_size,
		u32 **table, u32 *num_elements)
{
	int rc = 0, num_elemts = 0;
	u32 *ptbl = NULL;

	if (!of_find_property(of_node, table_name, NULL)) {
		dprintk(CVP_CORE, "%s not found\n", table_name);
		return 0;
	}

	num_elemts = get_u32_array_num_elements(of_node, table_name);
	if (!num_elemts) {
		dprintk(CVP_ERR, "no elements in %s\n", table_name);
		return 0;
	}
	num_elemts /= struct_size / sizeof(u32);

	ptbl = devm_kzalloc(&pdev->dev, num_elemts * struct_size, GFP_KERNEL);
	if (!ptbl) {
		dprintk(CVP_ERR, "Failed to alloc table %s\n", table_name);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(of_node, table_name, ptbl,
			num_elemts * struct_size / sizeof(u32))) {
		dprintk(CVP_ERR, "Failed to read %s\n", table_name);
		return -EINVAL;
	}

	*table = ptbl;
	if (num_elements)
		*num_elements = num_elemts;

	return rc;
}
EXPORT_SYMBOL(msm_cvp_load_u32_table);

/* A comparator to compare loads (needed later on) */
static int cmp(const void *a, const void *b)
{
	return ((struct allowed_clock_rates_table *)a)->clock_rate -
		((struct allowed_clock_rates_table *)b)->clock_rate;
}

static int msm_cvp_load_allowed_clocks_table(
		struct msm_cvp_platform_resources *res, struct msm_cvp_platform_data *platform_data)
{
	int rc = 0;
	u32 i;
	struct platform_device *pdev = res->pdev;
	
    if (!platform_data->allowed_clk_rates->count) {
        dprintk(CVP_CORE, "allowed clock rates not found\n");
        /*
         * Preserve downstream semantics:
         * missing policy is NOT an error.
         */
        return 0;
    }
	
	res->allowed_clks_tbl = devm_kcalloc(&pdev->dev,
				platform_data->allowed_clk_rates->count,
				sizeof(*res->allowed_clks_tbl),
				GFP_KERNEL);
    if (!res->allowed_clks_tbl){
	    dprintk(CVP_ERR, "Failed to alloc table %s\n", __func__);
        return -ENOMEM;
	}

    for (i = 0; i < platform_data->allowed_clk_rates->count; i++)
        res->allowed_clks_tbl[i].clock_rate = platform_data->allowed_clk_rates->clk_rates[i];

    res->allowed_clks_tbl_size = platform_data->allowed_clk_rates->count;
	
	sort(res->allowed_clks_tbl, res->allowed_clks_tbl_size,
		 sizeof(*res->allowed_clks_tbl), cmp, NULL);

	return 0;
}

static int msm_cvp_populate_bus(struct device *dev,
                struct msm_cvp_platform_resources *res,struct msm_cvp_platform_data *platform_data)
{
    struct bus_set *buses = &res->bus_set;
    struct bus_info *bus;
    const struct cvp_bus_desc *desc;
    const char *name;
    int count, i;
    int rc = 0;

    count = of_property_count_strings(dev->of_node,
                      "interconnect-names");
    if (count <= 0) {
        dprintk(CVP_CORE, "No interconnects defined\n");
        return 0; /* Optional resource */
    }

    bus = devm_kcalloc(dev, count, sizeof(*bus), GFP_KERNEL);
    if (!bus) {
        dprintk(CVP_ERR, "%s: Failed to allocate bus table\n", __func__);
        rc = -ENOMEM;
        goto err_bus;
    }

    buses->bus_tbl = bus;
    buses->count = count;

    for (i = 0; i < count; i++) {
        rc = of_property_read_string_index(dev->of_node,
                           "interconnect-names",
                           i, &name);
        if (rc) {
            dprintk(CVP_ERR,
                "Failed to read interconnect-names[%d]\n", i);
            goto err_bus;
        }

        bus[i].name = name;
        bus[i].dev = dev;

        desc = &platform_data->bus_descs[i];  // Order of entries in bus_descs should be same as interconnects in DT
        if (!desc) {
            dprintk(CVP_WARN,
                "No bus policy found for %s\n", name);
            continue; /* non-fatal */
        }

		if(!desc->governor){
           bus[i].governor = PERF_GOV;
		}else{
			bus[i].governor = desc->governor;
		}
        if (!strcmp(bus[i].governor, PERF_GOV))
		    bus[i].is_prfm_gov_used = true;
        
		bus[i].range[0] = desc->min_bw; /* min */
	    bus[i].range[1] = desc->max_bw; /* max */

        dprintk(CVP_CORE,
            "Attached ICC path %s (%u-%u KBps)\n",
            name, bus[i].range[0], bus[i].range[1]);
    }

    return 0;

err_bus:
    /* Make state explicit on failure, like original code */
    buses->bus_tbl = NULL;
    buses->count = 0;

    return rc;
}


static int __init_power_domains(struct msm_cvp_platform_resources *res, struct msm_cvp_platform_data *platform_data)
{
	const char **pd_names = devm_kzalloc(&res->pdev->dev,
                                      sizeof(char *) * (res->pd_set.count + 1),
                                      GFP_KERNEL);
	int i, ret = 0;
    struct dev_pm_domain_list *opp_pmdomain_tbl = NULL;
	for (i = 0; i < res->pd_set.count; i++) {
		pd_names[i] = res->pd_set.pd_tbl[i].name;
	}

	/*
	 * Attach hardware GDSCs with PD_FLAG_NO_DEV_LINK.
	 * Driver retains full manual control via __enable/__disable_power_domain().
	 */
	struct dev_pm_domain_attach_data eva_pd_data = {
		.pd_names = pd_names,
		.num_pd_names = res->pd_set.count,
		.pd_flags = PD_FLAG_NO_DEV_LINK
	};
	ret = devm_pm_domain_attach_list(&res->pdev->dev, &eva_pd_data, &res->pd_set.pm_domain_list);
	if (ret < 0){
		dprintk(CVP_ERR, "%s: Failed to attach power domain", __func__);
		return ret;
	}
    dprintk(CVP_CORE, "Successfully attached %d hardware GDSC domain(s)\n", res->pd_set.count);
	
	for (i = 0; i < res->pd_set.count; i++) {
		res->pd_set.pd_tbl[i].pd_device = res->pd_set.pm_domain_list->pd_devs[i];
	}
    
	/*
	 * Attach OPP voltage rail domains (mxc, mmcx) with
	 * PD_FLAG_DEV_LINK_ON | PD_FLAG_REQUIRED_OPP. 
	 * PD_FLAG_DEV_LINK_ON — this is what powers on at probe and kept alive as long as the device link is active
	 * PD_FLAG_REQUIRED_OPP — this only registers for OPP voting (setting a voltage)
	 * The OPP framework automatically votes the correct voltage corner
	 * when dev_pm_opp_set_opp() is called.
	 */
	if (platform_data->opp_pd_tbl && platform_data->opp_pd_tbl_size > 0) {
		struct dev_pm_domain_attach_data opp_pd_data = {
			.pd_names     = platform_data->opp_pd_tbl,
			.num_pd_names = platform_data->opp_pd_tbl_size,
			.pd_flags     = PD_FLAG_DEV_LINK_ON | PD_FLAG_REQUIRED_OPP,
		};

		ret = devm_pm_domain_attach_list(&res->pdev->dev, &opp_pd_data, &opp_pmdomain_tbl);
		if (ret < 0) {
			dprintk(CVP_ERR, "Failed to attach OPP voltage rail domains: %d\n", ret);
			return ret;
		}
		dprintk(CVP_CORE, "Successfully attached %u OPP voltage rail domain(s) (mxc, mmcx)\n",
			platform_data->opp_pd_tbl_size);
	}
	
	/*
	 * Configure OPP clocks (core0 + eva0 scaled together).
	 */
	struct dev_pm_opp_config opp_clk_config = {
		.clk_names   = platform_data->opp_clk_tbl,
		.config_clks = dev_pm_opp_config_clks_simple,
	};
	
	ret = devm_pm_opp_set_config(&res->pdev->dev, &opp_clk_config);
	if (ret) {
		dprintk(CVP_ERR, "Failed to set OPP clock config: %d\n", ret);
		return ret;
	}
	dprintk(CVP_CORE, "Successfully set OPP clock config for core0 and eva0");

	// Add opp table
	ret = devm_pm_opp_of_add_table(&res->pdev->dev);
	if (ret) {
		if (ret == -ENODEV) {
			dprintk(CVP_WARN, "No OPP table in device tree\n");
			ret = 0;
		} else {
			dprintk(CVP_ERR, "Failed to add OPP table: %d\n", ret);
			return ret;
		}
	}

	dprintk(CVP_CORE, "Successfully attach opp table");
	
	devm_kfree(&res->pdev->dev, pd_names);
	return 0;
}

static int msm_cvp_load_PD_table(
		struct msm_cvp_platform_resources *res,struct msm_cvp_platform_data *platform_data)
{
	int rc = 0;
	struct platform_device *pdev = res->pdev;
	struct power_domain_set *pd_set = &res->pd_set;
	struct device_node *dt_of_node = NULL;

	pd_set->count = 0;
	pd_set->pd_tbl = NULL;

	dt_of_node = pdev->dev.of_node;

	/*
	 * Use only the hardware GDSC count from platform data, NOT the full
	 * DT power-domain-names count. The DT now also contains OPP voltage
	 * rail domains (mxc, mmcx) which are handled separately via
	 * opp_pd_tbl and must NOT be stored in pd_set.
	 */
	pd_set->count = platform_data->power_domains->pd_count;
	if (pd_set->count <= 0) {
		dprintk(CVP_ERR,
			"Can't parse power domain, count %d\n", pd_set->count);
		rc = -EINVAL;
		goto err_pd_tbl_alloc;
	} else {
		// const char *pd_names[pd_set->count];
		int *gdsc_has_hw_pc = NULL;
		int i = 0;

		pd_set->pd_tbl = devm_kzalloc(&pdev->dev,
			sizeof(*pd_set->pd_tbl) *
			pd_set->count, GFP_KERNEL);

		if (!pd_set->pd_tbl) {
			rc = -ENOMEM;
			dprintk(CVP_ERR,
				"Failed to alloc memory for power domain table\n");
			goto err_pd_tbl_alloc;
		}

		gdsc_has_hw_pc = devm_kzalloc(&pdev->dev, pd_set->count *
				sizeof(*gdsc_has_hw_pc), GFP_KERNEL);
		if (!gdsc_has_hw_pc) {
			dprintk(CVP_ERR, "No memory to read gdsc_has_hw_pc properties\n");
			rc = -ENOMEM;
			goto err_has_hw_pc_alloc;
		}

		for (i = 0; i < pd_set->count; i++)
			gdsc_has_hw_pc[i] = platform_data->power_domains->gdsc_has_hw_pc[i];

		/* Read only the first pd_set->count names (hardware GDSCs) from DT */
		for (i = 0; i < pd_set->count; i++) {
			struct power_domain_info *pd_info = &pd_set->pd_tbl[i];

			pd_info->has_hw_power_collapse = gdsc_has_hw_pc[i];

			rc = of_property_read_string_index(dt_of_node,
				"power-domain-names", i, &pd_info->name);
			if (rc) {
				dprintk(CVP_ERR, "Failed to read pd name: %d\n", rc);
				goto err_has_hw_pc_alloc;
			}
		}
		rc = __init_power_domains(res,platform_data);
		if (rc) {
			dprintk(CVP_ERR, "Failed to init power domain");
			goto err_has_hw_pc_alloc;
		}

		return 0;

err_has_hw_pc_alloc:
		msm_cvp_free_pd_table(res);
err_pd_tbl_alloc:
		return rc;
	}
}

static int msm_cvp_load_clock_table(
		struct msm_cvp_platform_resources *res,struct msm_cvp_platform_data *platform_data)
{
	int rc = 0, num_clocks = 0, c = 0;
	struct platform_device *pdev = res->pdev;
	struct clock_set *clocks = &res->clock_set;

	num_clocks = of_property_count_strings(pdev->dev.of_node,
				"clock-names");
	if (num_clocks <= 0) {
		dprintk(CVP_CORE, "No clocks found\n");
		clocks->count = 0;
		rc = 0;
		goto err_load_clk_table_fail;
	}

	if (num_clocks != platform_data->num_clock_ids) {
        dprintk(CVP_ERR,
                "Clock count mismatch: DT=%d pdata=%d\n",
                num_clocks, platform_data->num_clock_ids);
		dprintk(CVP_CORE, "Failed to get clock ids: %d\n", rc);
    }
    
	if (num_clocks != platform_data->clock_props->count_clkProps ) {
        dprintk(CVP_ERR, "Failed to get clock props/congifs\n");
        goto err_load_clk_prop_fail;
    }

	clocks->clock_tbl = devm_kzalloc(&pdev->dev, sizeof(*clocks->clock_tbl)
			* num_clocks, GFP_KERNEL);
	if (!clocks->clock_tbl) {
		dprintk(CVP_ERR, "Failed to allocate memory for clock tbl\n");
		rc = -ENOMEM;
		goto err_load_clk_prop_fail;
	}

	clocks->count = num_clocks;
	dprintk(CVP_CORE, "Found %d clocks (pdata num_clock_ids=%d, count_clkProps=%d)\n",
		num_clocks, platform_data->num_clock_ids,
		platform_data->clock_props->count_clkProps);

	for (c = 0; c < num_clocks; ++c) {
		struct clock_info *vc = &res->clock_set.clock_tbl[c];

		of_property_read_string_index(pdev->dev.of_node,
				"clock-names", c, &vc->name);

		if (platform_data->clock_props->clock_props[c] & CLOCK_PROP_HAS_SCALING) {
			vc->has_scaling = true;
		} else {
			vc->count = 0;
			vc->has_scaling = false;
		}

		if (platform_data->clock_props->clock_props[c] & CLOCK_PROP_HAS_MEM_RETENTION)
			vc->has_mem_retention = true;
		else
			vc->has_mem_retention = false;

		dprintk(CVP_CORE, "Found clock %s id %d: scale-able = %s\n",
			vc->name, vc->clk_id, vc->has_scaling ? "yes" : "no");
	}

	return 0;

err_load_clk_prop_fail:
err_load_clk_table_fail:
	return rc;
}

#define MAX_CLK_RESETS 5

static int msm_cvp_load_reset_table(
		struct msm_cvp_platform_resources *res, struct msm_cvp_platform_data *platform_data)
{
	struct platform_device *pdev = res->pdev;
	struct reset_set *rst = &res->reset_set;
	int num_clocks = 0, c = 0, ret = 0;

	num_clocks = of_property_count_strings(pdev->dev.of_node,
				"reset-names");
	if (num_clocks <= 0 || num_clocks > MAX_CLK_RESETS) {
		dprintk(CVP_ERR, "Num reset clocks out of range\n");
		rst->count = 0;
		return 0;
	}

	rst->reset_tbl = devm_kcalloc(&pdev->dev, num_clocks,
			sizeof(*rst->reset_tbl), GFP_KERNEL);
	if (!rst->reset_tbl)
		return -ENOMEM;

	rst->count = num_clocks;
	dprintk(CVP_CORE, "Found %d reset clocks\n", num_clocks);

    /*
    * Driver policy: reset-power-status moved to driver-owned table.
    */

	for (c = 0; c < num_clocks; ++c) {
		struct reset_info *rc = &res->reset_set.reset_tbl[c];

		of_property_read_string_index(pdev->dev.of_node,
				"reset-names", c, &rc->name);
		rc->required_stage = platform_data->reset_power_sets->pwr_stats[c];
	}

	return 0;
}

static int find_key_value(struct msm_cvp_platform_data *platform_data,
	const char *key)
{
	int i = 0;
	struct msm_cvp_common_data *common_data = platform_data->common_data;
	int size = platform_data->common_data_length;

	for (i = 0; i < size; i++) {
		if (!strcmp(common_data[i].key, key))
			return common_data[i].value;
	}
	return 0;
}

static int msm_cvp_setup_context_bank(struct msm_cvp_platform_resources *res,
		struct context_bank_info *cb, struct device *dev)
{
	int rc = 0;
	const struct bus_type *bus;

	if (!dev || !cb || !res) {
		dprintk(CVP_ERR,
			"%s: Invalid Input params\n", __func__);
		return -EINVAL;
	}
	cb->dev = dev;

	bus = cb->dev->bus;
	if (IS_ERR_OR_NULL(bus)) {
		dprintk(CVP_ERR, "%s - failed to get bus type\n", __func__);
		rc = PTR_ERR(bus) ?: -ENODEV;
		goto remove_cb;
	}

	/*
	 * configure device segment size and segment boundary to ensure
	 * iommu mapping returns one mapping (which is required for partial
	 * cache operations)
	 */
	if (!dev->dma_parms)
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
	
	// rc = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	// dprintk(CVP_DBG, "%s: set dma mark",__func__);
	// if (rc) {
	// 	dprintk(CVP_ERR, "%s:context bank set mask error",cb->name);
	// }

	dma_set_max_seg_size(dev, DMA_BIT_MASK(32));
	dma_set_seg_boundary(dev, DMA_BIT_MASK(64));

	return rc;

remove_cb:
	return rc;
}

static int msm_cvp_populate_context_bank(struct device *dev,
		struct msm_cvp_core *core, const char *name)
{
	int rc = 0, i, num;
	struct context_bank_info *cb = NULL;
	const struct cvp_iommu_context_bank *context_banks;

	if (!dev || !core || !name) {
		dprintk(CVP_ERR, "%s - invalid inputs\n", __func__);
		return -EINVAL;
	}

	cb = devm_kzalloc(dev, sizeof(*cb), GFP_KERNEL);
	if (!cb) {
		dprintk(CVP_ERR, "%s - Failed to allocate cb\n", __func__);
		return -ENOMEM;
	}

	cb->name = name;

	/* Driver-owned context-bank descriptors */
	context_banks = core->platform_data->cb_data;
	num = core->platform_data->cb_data_size;
	if (!context_banks || !num) {
		dprintk(CVP_ERR, "%s: no IOMMU clients defined\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i < num; i++) {
		const struct cvp_iommu_context_bank *desc = &context_banks[i];

		if (!strcmp(cb->name, desc->name)) {
			cb->is_secure = (desc->vmid != 0);
			cb->buffer_type = desc->buffer_type;
			cb->addr_range.start = desc->iova_start;
			cb->addr_range.size  = desc->iova_size;
			break;
		}
	}

	INIT_LIST_HEAD(&cb->list);
	list_add_tail(&cb->list, &core->resources.context_banks);

	dprintk(CVP_CORE, "%s: context bank has name %s\n", __func__, cb->name);
	if (!strcmp(cb->name, "cvp_camera")) {
		cb->is_secure = true;
		rc = msm_cvp_setup_context_bank(&core->resources, cb, dev);
		if (rc) {
			dprintk(CVP_ERR, "Cannot setup context bank %s %d\n",
					cb->name, rc);
			goto err_setup_cb;
		}

		return 0;
	}

	dprintk(CVP_CORE,
		"context bank %s address start = %x address size = %x buffer_type = %x\n",
		cb->name, cb->addr_range.start,
		cb->addr_range.size, cb->buffer_type);

	cb->domain = iommu_get_domain_for_dev(dev);
	if (IS_ERR_OR_NULL(cb->domain)) {
		dprintk(CVP_ERR, "Create domain failed\n");
		rc = -ENODEV;
		goto err_setup_cb;
	}

	rc = msm_cvp_setup_context_bank(&core->resources, cb, dev);
	if (rc) {
		dprintk(CVP_ERR, "Cannot setup context bank %d\n", rc);
		goto err_setup_cb;
	}

	rc = dma_coerce_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (rc) {
		dprintk(CVP_ERR, "%s:context bank set mask error", cb->name);
	}
	dma_set_max_seg_size(dev, DMA_BIT_MASK(32));
	dma_set_seg_boundary(dev, DMA_BIT_MASK(32));
	return 0;

err_setup_cb:
	list_del(&cb->list);
	return rc;
}

int cvp_read_platform_resources_from_drv_data(
		struct msm_cvp_core *core)
{
	struct msm_cvp_platform_data *platform_data;
	struct msm_cvp_platform_resources *res;
	int rc = 0, i;

	if (!core || !core->platform_data) {
		dprintk(CVP_ERR, "%s Invalid data\n", __func__);
		return -ENOENT;
	}
	platform_data = core->platform_data;
	res = &core->resources;

	res->sku_version = platform_data->sku_version;

	res->dsp_enabled = find_key_value(platform_data,
			"qcom,dsp-enabled");

	res->max_ssr_allowed = find_key_value(platform_data,
			"qcom,max-ssr-allowed");

	res->sw_power_collapsible = find_key_value(platform_data,
			"qcom,sw-power-collapse");

	res->debug_timeout = find_key_value(platform_data,
			"qcom,debug-timeout");

	res->pm_qos.latency_us = find_key_value(platform_data,
			"qcom,pm-qos-latency-us");
	res->pm_qos.silver_count = 0;
	for(i = 0; i < MAX_SILVER_CORE_NUM; i++) {
		if(topology_cluster_id(i) == 0)
			res->pm_qos.silver_count++;
		else
			break;
	}
	for (i = 0; i < res->pm_qos.silver_count; i++)
		res->pm_qos.silver_cores[i] = i;

	res->max_secure_inst_count = find_key_value(platform_data,
			"qcom,max-secure-instances");

	res->max_supported_inst_count = find_key_value(platform_data,
			"qcom,max-supported-instances");

	res->thermal_mitigable = find_key_value(platform_data,
			"qcom,enable-thermal-mitigation");
	res->msm_cvp_pwr_collapse_delay = find_key_value(platform_data,
			"qcom,power-collapse-delay");
	res->msm_cvp_hw_rsp_timeout = find_key_value(platform_data,
			"qcom,hw-resp-timeout");
	res->msm_cvp_dsp_rsp_timeout = find_key_value(platform_data,
			"qcom,dsp-resp-timeout");
	res->non_fatal_pagefaults = find_key_value(platform_data,
			"qcom,domain-attr-non-fatal-faults");

	/* Reading QOS NOC urgency low A and B register bit masks */
	res->qos_noc_urgency_low_a_bitmask = find_key_value(platform_data,
			"qcom,qos_noc_urgency_low_a_bitmask");
	res->qos_noc_urgency_low_b_bitmask = find_key_value(platform_data,
			"qcom,qos_noc_urgency_low_b_bitmask");
	res->rcg_vnoc_clk_en_low = find_key_value(platform_data,
			"qcom,rcg_vnoc_clk_en_low");
	res->core_noc_cx_pd_disable = find_key_value(platform_data,
			"qcom,core_noc_cx_pd_disable");
	res->gdsc_framework_type = find_key_value(platform_data,
			"CVP_GDSC_FRAMEWORK_TYPE");			

	res->vpu_ver = platform_data->vpu_ver;
	res->ubwc_config = platform_data->ubwc_config;
	res->fatal_ssr = false;

	rc = msm_cvp_load_gcc_regs(res,platform_data);
	if (rc)
        dprintk(CVP_ERR, "Failed to load gcc reg space mapping: %d\n", rc);
		
	rc = msm_cvp_load_ipcc_regs(res,platform_data);
	if (rc)
		dprintk(CVP_ERR, "Failed to load IPCC regs: %d\n", rc);	
	
	rc = msm_cvp_load_subcache_info(res,platform_data);
	if (rc)
		dprintk(CVP_WARN, "Failed to load subcache info: %d\n", rc);
	
	rc = msm_cvp_load_reg_table(res,platform_data);
	if (rc) {
		dprintk(CVP_ERR, "Failed to load reg table: %d\n", rc);
		return rc;
	}
	rc = msm_cvp_load_regspace_mapping(res,platform_data);
	
	rc = msm_cvp_load_allowed_clocks_table(res,platform_data);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to load allowed clocks table: %d\n", rc);
		return rc;
	}
	
	return rc;
}



int cvp_read_platform_resources_from_dt(
		struct msm_cvp_core *core)
{
	struct msm_cvp_platform_resources *res;
	struct platform_device *pdev;
	struct msm_cvp_platform_data *pdata;
	struct resource *kres = NULL;
	int rc = 0;
	uint32_t firmware_base = 0;

	pdata = core->platform_data;
	res = &core->resources;
	if (!res) {
		dprintk(CVP_ERR, "Resource not allocated\n");
		return -ENOENT;
	}

	pdev = res->pdev;
	if (!pdev->dev.of_node) {
		dprintk(CVP_ERR, "DT node not found\n");
		return -ENOENT;
	}

	INIT_LIST_HEAD(&res->context_banks);

	res->firmware_base = (phys_addr_t)firmware_base;

	kres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res->register_base = kres ? kres->start : -1;
	res->register_size = kres ? (kres->end + 1 - kres->start) : -1;

	res->irq = platform_get_irq(pdev, 0);

	dprintk(CVP_CORE, "%s: res->irq:%d \n",
		__func__, res->irq);

	//Parsing for WD interrupt
	res->irq_wd = platform_get_irq(pdev, 1);

	dprintk(CVP_CORE, "%s: res->irq_wd:%d \n",
		__func__, res->irq_wd);

	rc = msm_cvp_load_qdss_table(res);
	if (rc)
		dprintk(CVP_WARN, "Failed to load qdss reg table: %d\n", rc);


	rc = of_property_read_u32(pdev->dev.of_node, "soc_ver", &core->soc_version);
	if (rc) {
		dprintk(CVP_WARN,
			"%s: %d while reading DT for \"soc_ver\" default to 0x10000\n",
			__func__, rc);
		core->soc_version = 0x10000;
	}

	{
		const char *fw_name = NULL;
		size_t len;

		rc = of_property_read_string(pdev->dev.of_node,
				"firmware-name", &fw_name);
		if (rc || !fw_name || !*fw_name) {
			dprintk(CVP_ERR, "Failed to read \"firmware-name\" from DT: %d\n", rc);
			return rc ? rc : -EINVAL;
		}

		len = strlen(fw_name);
		if (len > 4 && !strcmp(fw_name + len - 4, ".mbn")) {
			char *name = devm_kstrdup(&pdev->dev, fw_name, GFP_KERNEL);

			if (!name)
				return -ENOMEM;
			name[len - 4] = '\0';
			fw_name = name;
		}
		res->fw_name = fw_name;
		dprintk(CVP_CORE, "Firmware filename from DT: %s\n", res->fw_name);
	}


	rc = msm_cvp_load_PD_table(res,pdata);
	if (rc) {
		dprintk(CVP_ERR, "Failed to load list of power domains %d\n", rc);
		goto err_load_load_PD_table;
	}

	rc = msm_cvp_load_clock_table(res,pdata);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to load clock table: %d\n", rc);
		goto err_load_clock_table;
	}

	rc = msm_cvp_load_reset_table(res,pdata);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to load reset table: %d\n", rc);
		goto err_load_reset_table;
	}

	res->use_non_secure_pil = of_property_read_bool(pdev->dev.of_node,
			"qcom,use-non-secure-pil");

	if (res->use_non_secure_pil || !is_iommu_present(res)) {
		of_property_read_u32(pdev->dev.of_node, "qcom,fw-bias",
				&firmware_base);
		res->firmware_base = (phys_addr_t)firmware_base;
		dprintk(CVP_CORE,
				"Using fw-bias : %pa", &res->firmware_base);
	}

	cvp_hfi_defs = pdata->cvp_hfi;
	cvp_hfi_msg_defs = pdata->cvp_hfi_msg;

return rc;

err_load_reset_table:
	msm_cvp_free_allowed_clocks_table(res);
	msm_cvp_free_clock_table(res);
err_load_clock_table:
err_load_load_PD_table:
	err_load_load_PD_table(res);
	msm_cvp_free_reg_table(res);
	return rc;
}


static struct device *cvp_create_cb_dev(struct device *parent, const char *name)
{
	struct device_node *child_of_node;
	struct platform_device_info plat_dev_info = {};
	struct platform_device *pdev;

	child_of_node = of_get_child_by_name(parent->of_node, name);
	if (!child_of_node)
		return NULL;

	plat_dev_info.fwnode = &child_of_node->fwnode;
	plat_dev_info.name = child_of_node->name;
	plat_dev_info.parent = parent;

	pdev = platform_device_register_full(&plat_dev_info);
	of_node_put(child_of_node);
	if (IS_ERR(pdev))
		return ERR_CAST(pdev);

	return &pdev->dev;
}

int cvp_init_context_bank_devices(struct platform_device *pdev,
		struct msm_cvp_core *core)
{
	const struct cvp_iommu_context_bank *context_banks;
	struct device *cb_dev;
	int rc = 0, i, num;

	if (!pdev || !core || !core->platform_data) {
		dprintk(CVP_ERR, "%s - invalid inputs\n", __func__);
		return -EINVAL;
	}

	context_banks = core->platform_data->cb_data;
	num = core->platform_data->cb_data_size;
	if (!context_banks || !num) {
		dprintk(CVP_ERR, "%s: no IOMMU clients defined\n", __func__);
		return -EINVAL;
	}

	core->cb_devs = kcalloc(num, sizeof(*core->cb_devs), GFP_KERNEL);
	if (!core->cb_devs) {
		dprintk(CVP_ERR, "%s - Failed to allocate cb_devs\n", __func__);
		return -ENOMEM;
	}
	core->num_cb_devs = 0;

	for (i = 0; i < num; i++) {
		cb_dev = cvp_create_cb_dev(&pdev->dev, context_banks[i].name);
		if (IS_ERR_OR_NULL(cb_dev)) {
			dprintk(CVP_ERR, "Failed to create cb dev %s\n",
					context_banks[i].name);
			rc = cb_dev ? PTR_ERR(cb_dev) : -ENODEV;
			goto err_create_cb_dev;
		}

		core->cb_devs[i] = cb_dev;
		core->num_cb_devs++;

		rc = msm_cvp_populate_context_bank(cb_dev, core,
				context_banks[i].name);
		if (rc) {
			dprintk(CVP_ERR, "Failed to populate cb dev %s\n",
					context_banks[i].name);
			goto err_create_cb_dev;
		}
	}

	return 0;

err_create_cb_dev:
	cvp_deinit_context_bank_devices(core);
	return rc;
}

void cvp_deinit_context_bank_devices(struct msm_cvp_core *core)
{
	u32 i;

	if (!core || !core->cb_devs)
		return;

	for (i = 0; i < core->num_cb_devs; i++) {
		if (core->cb_devs[i])
			platform_device_unregister(
				to_platform_device(core->cb_devs[i]));
	}

	kfree(core->cb_devs);
	core->cb_devs = NULL;
	core->num_cb_devs = 0;
}

int cvp_read_bus_resources(struct platform_device *pdev)
{
	struct msm_cvp_core *core;

    if (!pdev) {
        dprintk(CVP_ERR, "Invalid platform device\n");
        return -EINVAL;
    }

    core = dev_get_drvdata(&pdev->dev);
	if (!core) {
		dprintk(CVP_WARN, "No CVP core associated with device %s",dev_name(&pdev->dev));
		return -EINVAL;
	}

    return msm_cvp_populate_bus(&pdev->dev, &core->resources, core->platform_data);
}
