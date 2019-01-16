/*
 * Terasic DE1-SoC apds driver
 *
 * Copyright (C) 2018 Daniel Giritzer <daniel@giritzer.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "apds"

#define NUM_REGS 12
#define CHAR_DEVICE_SIZE (NUM_REGS * 4)


struct altera_apds {
	void *regs;
	char buffer[CHAR_DEVICE_SIZE];
	int size;
	struct miscdevice misc;
};

/*
 * @brief This function gets executed on fread.
 */
static int apds_read(struct file *filep, char *buf, size_t count,
			 loff_t *offp)
{
	struct altera_apds *apds = container_of(filep->private_data,
					   struct altera_apds, misc);

	if ((*offp < 0) || (*offp >= CHAR_DEVICE_SIZE))
		return 0;

	if ((*offp + count) > CHAR_DEVICE_SIZE)
		count = CHAR_DEVICE_SIZE - *offp;

	if (count > 0) {
        memcpy_fromio(apds->buffer, apds->regs, CHAR_DEVICE_SIZE);
		count = count - copy_to_user(buf,
					     apds->buffer + *offp,
					     count);

		*offp += count;
	}
	return count;
}

/*
 * @brief This function gets executed on fwrite.
 */
/*static int apds_write(struct file *filep, const char *buf,
			  size_t count, loff_t *offp)
{
	struct altera_apds *apds = container_of(filep->private_data,
					   struct altera_apds, misc);

	if ((*offp < 0) || (*offp >= CHAR_DEVICE_SIZE))
		return -EINVAL;

	if ((*offp + count) > CHAR_DEVICE_SIZE)
		count = CHAR_DEVICE_SIZE - *offp;

	if (count > 0) {
		count = count - copy_from_user(apds->buffer + *offp,
					       buf,
					       count);
	}

    memcpy_fromio(apds->buffer, apds->regs, CHAR_DEVICE_SIZE);

	*offp += count;
	return count;
} */

static const struct file_operations apds_fops = {
	.owner = THIS_MODULE,
	.read = apds_read,
	//.write = apds_write
};

static int apds_probe(struct platform_device *pdev)
{
	struct altera_apds *apds;
	struct resource *io;
	int retval;

	apds = devm_kzalloc(&pdev->dev, sizeof(*apds), GFP_KERNEL);
	if (apds == NULL)
		return -ENOMEM;
	platform_set_drvdata(pdev, apds);

	io = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	apds->regs = devm_ioremap_resource(&pdev->dev, io);
	if (IS_ERR(apds->regs))
		return PTR_ERR(apds->regs);
	apds->size = io->end - io->start + 1;

	apds->misc.name = DRIVER_NAME;
	apds->misc.minor = MISC_DYNAMIC_MINOR;
	apds->misc.fops = &apds_fops;
	apds->misc.parent = &pdev->dev;
	retval = misc_register(&apds->misc);
	if (retval) {
		dev_err(&pdev->dev, "Register misc device failed!\n");
		return retval;
	}

	dev_info(&pdev->dev, "apds driver loaded!");

	return 0;
}

static int apds_remove(struct platform_device *pdev)
{
	struct altera_apds *apds = platform_get_drvdata(pdev);

	misc_deregister(&apds->misc);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id apds_of_match[] = {
	{ .compatible = "ssl,apds9301-1.0", },
	{ },
};
MODULE_DEVICE_TABLE(of, apds_of_match);

static struct platform_driver apds_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(apds_of_match),
	},
	.probe		= apds_probe,
	.remove		= apds_remove,
};

module_platform_driver(apds_driver);

MODULE_AUTHOR("Giritzer");
MODULE_DESCRIPTION("Altera/Terasic apds driver");
MODULE_LICENSE("GPL v2");
