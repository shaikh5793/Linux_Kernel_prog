/*
 * Device Tree MMIO Demo Driver
 * 
 * Demonstrates accessing memory-mapped I/O regions from device tree:
 * - Parsing 'reg' property with of_address_to_resource()
 * - Using platform_get_resource() for resource access
 * - Mapping memory with devm_ioremap_resource()
 * - Reading from mapped registers
 * - Handling multiple memory regions
 *
 * Copyright (C) 2024 TechVeda
 * Author: Raghu Bharadwaj <raghu@techveda.org>
 *
 * License: Dual MIT/GPL
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/ioport.h>

#define DRIVER_NAME "dt-mmio"
#define MAX_REGIONS 4

/* Private data structure */
struct dt_mmio_priv {
	struct device *dev;
	void __iomem *io_base[MAX_REGIONS];
	struct resource *res[MAX_REGIONS];
	int num_regions;
};

static void dt_mmio_read_registers(struct dt_mmio_priv *priv)
{
	int i;
	u32 val;

	for (i = 0; i < priv->num_regions; i++) {
		if (!priv->io_base[i])
			continue;

		/* Read first 4 registers from each region */
		dev_info(priv->dev, "Region %d at 0x%08llx (size 0x%llx):\n",
			 i, (u64)priv->res[i]->start,
			 (u64)resource_size(priv->res[i]));

		/* Safely read only the first register to demonstrate */
		val = ioread32(priv->io_base[i]);
		dev_info(priv->dev, "  Offset 0x00: 0x%08x\n", val);

		if (resource_size(priv->res[i]) >= 8) {
			val = ioread32(priv->io_base[i] + 0x04);
			dev_info(priv->dev, "  Offset 0x04: 0x%08x\n", val);
		}
	}
}

static int dt_mmio_parse_resources(struct platform_device *pdev,
				    struct dt_mmio_priv *priv)
{
	struct device *dev = &pdev->dev;
	int i;

	/* Get number of memory regions from device tree */
	for (i = 0; i < MAX_REGIONS; i++) {
		priv->res[i] = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!priv->res[i])
			break;

		dev_info(dev, "Found memory region %d: 0x%08llx - 0x%08llx\n",
			 i, (u64)priv->res[i]->start,
			 (u64)priv->res[i]->end);

		/* Request and map the region */
		priv->io_base[i] = devm_ioremap_resource(dev, priv->res[i]);
		if (IS_ERR(priv->io_base[i])) {
			dev_err(dev, "Failed to map region %d: %ld\n",
				i, PTR_ERR(priv->io_base[i]));
			return PTR_ERR(priv->io_base[i]);
		}

		dev_info(dev, "Mapped region %d to virtual address %p\n",
			 i, priv->io_base[i]);
	}

	priv->num_regions = i;

	if (priv->num_regions == 0) {
		dev_err(dev, "No memory regions found in device tree\n");
		return -ENODEV;
	}

	dev_info(dev, "Successfully mapped %d memory region(s)\n",
		 priv->num_regions);

	return 0;
}

static int dt_mmio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dt_mmio_priv *priv;
	int ret;

	dev_info(dev, "Probing %s driver\n", DRIVER_NAME);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	platform_set_drvdata(pdev, priv);

	/* Parse and map memory resources from device tree */
	ret = dt_mmio_parse_resources(pdev, priv);
	if (ret) {
		dev_err(dev, "Failed to parse resources: %d\n", ret);
		return ret;
	}

	/* Read and display register values */
	dt_mmio_read_registers(priv);

	dev_info(dev, "Driver probed successfully\n");
	return 0;
}

static void dt_mmio_remove(struct platform_device *pdev)
{
	struct dt_mmio_priv *priv = platform_get_drvdata(pdev);

	dev_info(priv->dev, "Removing %s driver\n", DRIVER_NAME);

	/* Memory regions are automatically unmapped by devm_* */
}

static const struct of_device_id dt_mmio_of_match[] = {
	{ .compatible = "techveda,dt-mmio", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, dt_mmio_of_match);

static struct platform_driver dt_mmio_driver = {
	.probe = dt_mmio_probe,
	.remove = dt_mmio_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = dt_mmio_of_match,
	},
};

module_platform_driver(dt_mmio_driver);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Device Tree MMIO Demo Driver");
MODULE_VERSION("1.0");
