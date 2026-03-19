/*
 * Device Tree Properties Demo Driver
 * 
 * Demonstrates reading various device tree property types:
 * - String properties (label, status, custom strings)
 * - Integer properties (u32 values)
 * - Boolean properties
 * - Array properties (u32 arrays)
 * - Phandle references
 *
 * Copyright (C) 2024 TechVeda
 * Author: Raghu Bharadwaj <raghu@techveda.org>
 *
 * License: Dual MIT/GPL
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#define DRIVER_NAME "dt-props"

/* Private data structure */
struct dt_props_priv {
	struct device *dev;
	char *label;
	char *custom_string;
	u32 clock_freq;
	u32 custom_value;
	bool enable_feature;
	u32 *array_values;
	int array_size;
	struct device_node *ref_node;
};

static void dt_props_print_info(struct dt_props_priv *priv)
{
	int i;

	dev_info(priv->dev, "=== Device Tree Properties Demo ===\n");
	
	if (priv->label)
		dev_info(priv->dev, "Label: %s\n", priv->label);
	
	if (priv->custom_string)
		dev_info(priv->dev, "Custom String: %s\n", priv->custom_string);
	
	dev_info(priv->dev, "Clock Frequency: %u Hz\n", priv->clock_freq);
	dev_info(priv->dev, "Custom Value: %u\n", priv->custom_value);
	dev_info(priv->dev, "Feature Enabled: %s\n", 
		 priv->enable_feature ? "yes" : "no");
	
	if (priv->array_values && priv->array_size > 0) {
		dev_info(priv->dev, "Array values (%d elements): ", priv->array_size);
		for (i = 0; i < priv->array_size; i++) {
			pr_cont("%u ", priv->array_values[i]);
		}
		pr_cont("\n");
	}
	
	if (priv->ref_node) {
		dev_info(priv->dev, "Referenced node: %s\n", 
			 priv->ref_node->full_name);
	}
	
	dev_info(priv->dev, "===================================\n");
}

static int dt_props_parse_dt(struct platform_device *pdev, 
			      struct dt_props_priv *priv)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	const char *str;
	int ret;

	if (!np) {
		dev_err(dev, "No device tree node found\n");
		return -ENODEV;
	}

	/* Read string properties */
	ret = of_property_read_string(np, "label", &str);
	if (ret == 0) {
		priv->label = devm_kstrdup(dev, str, GFP_KERNEL);
		if (!priv->label)
			return -ENOMEM;
	}

	ret = of_property_read_string(np, "techveda,custom-string", &str);
	if (ret == 0) {
		priv->custom_string = devm_kstrdup(dev, str, GFP_KERNEL);
		if (!priv->custom_string)
			return -ENOMEM;
	}

	/* Read integer properties */
	ret = of_property_read_u32(np, "clock-frequency", &priv->clock_freq);
	if (ret) {
		dev_warn(dev, "clock-frequency not found, using default\n");
		priv->clock_freq = 1000000; /* 1 MHz default */
	}

	ret = of_property_read_u32(np, "techveda,custom-value", &priv->custom_value);
	if (ret) {
		dev_warn(dev, "custom-value not found, using default\n");
		priv->custom_value = 42;
	}

	/* Read boolean property */
	priv->enable_feature = of_property_read_bool(np, "techveda,enable-feature");

	/* Read array property */
	priv->array_size = of_property_count_u32_elems(np, "techveda,array-values");
	if (priv->array_size > 0) {
		priv->array_values = devm_kmalloc_array(dev, priv->array_size,
							sizeof(u32), GFP_KERNEL);
		if (!priv->array_values)
			return -ENOMEM;

		ret = of_property_read_u32_array(np, "techveda,array-values",
						 priv->array_values, priv->array_size);
		if (ret) {
			dev_warn(dev, "Failed to read array values\n");
			priv->array_size = 0;
		}
	}

	/* Parse phandle reference */
	priv->ref_node = of_parse_phandle(np, "techveda,reference", 0);
	if (priv->ref_node) {
		dev_info(dev, "Found phandle reference\n");
	}

	/* Check if device is enabled */
	if (!of_device_is_available(np)) {
		dev_warn(dev, "Device is disabled in device tree\n");
	}

	return 0;
}

static int dt_props_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dt_props_priv *priv;
	int ret;

	dev_info(dev, "Probing %s driver\n", DRIVER_NAME);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	platform_set_drvdata(pdev, priv);

	/* Parse device tree properties */
	ret = dt_props_parse_dt(pdev, priv);
	if (ret) {
		dev_err(dev, "Failed to parse device tree: %d\n", ret);
		return ret;
	}

	/* Print all parsed properties */
	dt_props_print_info(priv);

	dev_info(dev, "Driver probed successfully\n");
	return 0;
}

static void dt_props_remove(struct platform_device *pdev)
{
	struct dt_props_priv *priv = platform_get_drvdata(pdev);

	dev_info(priv->dev, "Removing %s driver\n", DRIVER_NAME);

	/* Release phandle reference */
	if (priv->ref_node)
		of_node_put(priv->ref_node);
}

static const struct of_device_id dt_props_of_match[] = {
	{ .compatible = "techveda,dt-props", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, dt_props_of_match);

static struct platform_driver dt_props_driver = {
	.probe = dt_props_probe,
	.remove = dt_props_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = dt_props_of_match,
	},
};

module_platform_driver(dt_props_driver);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Device Tree Properties Demo Driver");
MODULE_VERSION("1.0");
