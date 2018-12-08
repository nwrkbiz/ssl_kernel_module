/*
 * Terasic DE1-SoC mpu driver
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

#define DRIVER_NAME "mpu"

#define NUM_REGS 37
#define CHAR_DEVICE_SIZE (NUM_REGS * 4)


struct altera_mpu {
	void *regs;
	char buffer[CHAR_DEVICE_SIZE];
	int size;
	struct miscdevice misc;
};

/*
 * @brief This function gets executed on fread.
 */
static int mpu_read(struct file *filep, char *buf, size_t count,
			 loff_t *offp)
{
	struct altera_mpu *mpu = container_of(filep->private_data,
					   struct altera_mpu, misc);

	if ((*offp < 0) || (*offp >= CHAR_DEVICE_SIZE))
		return 0;

	if ((*offp + count) > CHAR_DEVICE_SIZE)
		count = CHAR_DEVICE_SIZE - *offp;

	if (count > 0) {
        memcpy_fromio(mpu->buffer, mpu->regs, CHAR_DEVICE_SIZE);
		count = count - copy_to_user(buf,
					     mpu->buffer + *offp,
					     count);

		*offp += count;
	}
	return count;
}

/*
 * @brief This function gets executed on fwrite.
 */
/*static int mpu_write(struct file *filep, const char *buf,
			  size_t count, loff_t *offp)
{
	struct altera_mpu *mpu = container_of(filep->private_data,
					   struct altera_mpu, misc);

	if ((*offp < 0) || (*offp >= CHAR_DEVICE_SIZE))
		return -EINVAL;

	if ((*offp + count) > CHAR_DEVICE_SIZE)
		count = CHAR_DEVICE_SIZE - *offp;

	if (count > 0) {
		count = count - copy_from_user(mpu->buffer + *offp,
					       buf,
					       count);
	}

    memcpy_fromio(mpu->buffer, mpu->regs, CHAR_DEVICE_SIZE);

	*offp += count;
	return count;
} */

static const struct file_operations mpu_fops = {
	.owner = THIS_MODULE,
	.read = mpu_read,
	//.write = mpu_write
};

static int mpu_probe(struct platform_device *pdev)
{
	struct altera_mpu *mpu;
	struct resource *io;
	int retval;

	mpu = devm_kzalloc(&pdev->dev, sizeof(*mpu), GFP_KERNEL);
	if (mpu == NULL)
		return -ENOMEM;
	platform_set_drvdata(pdev, mpu);

	io = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mpu->regs = devm_ioremap_resource(&pdev->dev, io);
	if (IS_ERR(mpu->regs))
		return PTR_ERR(mpu->regs);
	mpu->size = io->end - io->start + 1;

	mpu->misc.name = DRIVER_NAME;
	mpu->misc.minor = MISC_DYNAMIC_MINOR;
	mpu->misc.fops = &mpu_fops;
	mpu->misc.parent = &pdev->dev;
	retval = misc_register(&mpu->misc);
	if (retval) {
		dev_err(&pdev->dev, "Register misc device failed!\n");
		return retval;
	}

	dev_info(&pdev->dev, "mpu driver loaded!");

	return 0;
}

static int mpu_remove(struct platform_device *pdev)
{
	struct altera_mpu *mpu = platform_get_drvdata(pdev);

	misc_deregister(&mpu->misc);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id mpu_of_match[] = {
	{ .compatible = "ssl,mpu9250-1.0", },
	{ },
};
MODULE_DEVICE_TABLE(of, mpu_of_match);

static struct platform_driver mpu_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mpu_of_match),
	},
	.probe		= mpu_probe,
	.remove		= mpu_remove,
};

module_platform_driver(mpu_driver);

MODULE_AUTHOR("Giritzer");
MODULE_DESCRIPTION("Altera/Terasic mpu driver");
MODULE_LICENSE("GPL v2");
