/*
 * Terasic DE1-SoC ID seven segment driver
 *
 * Copyright (C) 2017 Daniel Giritzer <daniel@giritzer.eu>
 *                    Onur Polat <onur.polat@students.fh-hagenberg.at>
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

#define DRIVER_NAME "hdc"

#define NUM_REGS 12
#define CHAR_DEVICE_SIZE (NUM_REGS * 4)


struct altera_hdc {
	void *regs;
	char buffer[CHAR_DEVICE_SIZE];
	int size;
	struct miscdevice misc;
};

/*
 * @brief This function gets executed on fread.
 */
static int hdc_read(struct file *filep, char *buf, size_t count,
			 loff_t *offp)
{
	struct altera_hdc *hdc = container_of(filep->private_data,
					   struct altera_hdc, misc);

	if ((*offp < 0) || (*offp >= CHAR_DEVICE_SIZE))
		return 0;

	if ((*offp + count) > CHAR_DEVICE_SIZE)
		count = CHAR_DEVICE_SIZE - *offp;

	if (count > 0) {
        memcpy_fromio(hdc->buffer, hdc->regs, CHAR_DEVICE_SIZE);
		count = count - copy_to_user(buf,
					     hdc->buffer + *offp,
					     count);

		*offp += count;
	}
	return count;
}

/*
 * @brief This function gets executed on fwrite.
 */
/*static int hdc_write(struct file *filep, const char *buf,
			  size_t count, loff_t *offp)
{
	struct altera_hdc *hdc = container_of(filep->private_data,
					   struct altera_hdc, misc);

	if ((*offp < 0) || (*offp >= CHAR_DEVICE_SIZE))
		return -EINVAL;

	if ((*offp + count) > CHAR_DEVICE_SIZE)
		count = CHAR_DEVICE_SIZE - *offp;

	if (count > 0) {
		count = count - copy_from_user(hdc->buffer + *offp,
					       buf,
					       count);
	}

    memcpy_fromio(hdc->buffer, hdc->regs, CHAR_DEVICE_SIZE);

	*offp += count;
	return count;
} */

static const struct file_operations hdc_fops = {
	.owner = THIS_MODULE,
	.read = hdc_read,
	//.write = hdc_write
};

static int hdc_probe(struct platform_device *pdev)
{
	struct altera_hdc *hdc;
	struct resource *io;
	int retval;

	hdc = devm_kzalloc(&pdev->dev, sizeof(*hdc), GFP_KERNEL);
	if (hdc == NULL)
		return -ENOMEM;
	platform_set_drvdata(pdev, hdc);

	io = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hdc->regs = devm_ioremap_resource(&pdev->dev, io);
	if (IS_ERR(hdc->regs))
		return PTR_ERR(hdc->regs);
	hdc->size = io->end - io->start + 1;

	hdc->misc.name = DRIVER_NAME;
	hdc->misc.minor = MISC_DYNAMIC_MINOR;
	hdc->misc.fops = &hdc_fops;
	hdc->misc.parent = &pdev->dev;
	retval = misc_register(&hdc->misc);
	if (retval) {
		dev_err(&pdev->dev, "Register misc device failed!\n");
		return retval;
	}

	dev_info(&pdev->dev, "hdc driver loaded!");

	return 0;
}

static int hdc_remove(struct platform_device *pdev)
{
	struct altera_hdc *hdc = platform_get_drvdata(pdev);

	misc_deregister(&hdc->misc);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id hdc_of_match[] = {
	{ .compatible = "unknown,unknown-1.0", },
	{ },
};
MODULE_DEVICE_TABLE(of, hdc_of_match);

static struct platform_driver hdc_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(hdc_of_match),
	},
	.probe		= hdc_probe,
	.remove		= hdc_remove,
};

module_platform_driver(hdc_driver);

MODULE_AUTHOR("Giritzer");
MODULE_DESCRIPTION("Altera/Terasic hdc driver");
MODULE_LICENSE("GPL v2");
