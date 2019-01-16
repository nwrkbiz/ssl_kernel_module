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

#define DRIVER_NAME "sevensegment"

#define HEX_NUM 6
#define PWM_NUM 2
#define CHAR_DEVICE_SIZE (HEX_NUM + PWM_NUM)
#define PWM_OFFSET 4
#define ENABLE_OFFSET 8 //offset for sevenseg enable bit

struct altera_sevenseg {
	void *regs;
	char buffer[CHAR_DEVICE_SIZE];
	int size;
	struct miscdevice misc;
};

/*
 * @brief This function gets executed on fread.
 */
static int sevenseg_read(struct file *filep, char *buf, size_t count,
			 loff_t *offp)
{
	struct altera_sevenseg *sevenseg = container_of(filep->private_data,
					   struct altera_sevenseg, misc);

	if ((*offp < 0) || (*offp >= CHAR_DEVICE_SIZE))
		return 0;

	if ((*offp + count) > CHAR_DEVICE_SIZE)
		count = CHAR_DEVICE_SIZE - *offp;

	if (count > 0) {
		count = count - copy_to_user(buf,
					     sevenseg->buffer + *offp,
					     count);

		*offp += count;
	}
	return count;
}

/*
 * @brief This function gets executed on fwrite.
 */
static int sevenseg_write(struct file *filep, const char *buf,
			  size_t count, loff_t *offp)
{
	int i = 0;
    long value = 0;
    int result = 0;
    char values_to_write[HEX_NUM+1];
    char pwm_to_write[PWM_NUM+1];

    u32 enable_segments = 0;
	struct altera_sevenseg *sevenseg = container_of(filep->private_data,
					   struct altera_sevenseg, misc);

	if ((*offp < 0) || (*offp >= CHAR_DEVICE_SIZE))
		return -EINVAL;

	if ((*offp + count) > CHAR_DEVICE_SIZE)
		count = CHAR_DEVICE_SIZE - *offp;

	if (count > 0) {
		count = count - copy_from_user(sevenseg->buffer + *offp,
					       buf,
					       count);
	}

	// Enable segments with valid values
    iowrite32(0x00000000, sevenseg->regs + ENABLE_OFFSET);
	for (i = 0; i < HEX_NUM; i++)
        if((sevenseg->buffer[i] >= '0' && sevenseg->buffer[i] <= '9') ||
           (sevenseg->buffer[i] >= 'a' && sevenseg->buffer[i] <= 'z') ||
           (sevenseg->buffer[i] >= 'A' && sevenseg->buffer[i] <= 'Z')
           )
        {
            enable_segments |= (1UL << (HEX_NUM - i - 1));
            values_to_write[i] = sevenseg->buffer[i];
        }
        else
        {
            values_to_write[i] = '0';
        }
    iowrite32(enable_segments, sevenseg->regs + ENABLE_OFFSET);

    // write values to hex
    values_to_write[HEX_NUM] = '\0';
    result = kstrtol(values_to_write,16, &value);
    iowrite32((u32)value, sevenseg->regs);

    // write pwm value
    for (i = HEX_NUM; i < CHAR_DEVICE_SIZE; i++)
    {
        pwm_to_write[i-HEX_NUM] = sevenseg->buffer[i];
    }
    pwm_to_write[PWM_NUM] = '\0';
    result = kstrtol(pwm_to_write,16, &value);
    iowrite32((u32)value, sevenseg->regs + PWM_OFFSET);


	*offp += count;
	return count;
}

static const struct file_operations sevenseg_fops = {
	.owner = THIS_MODULE,
	.read = sevenseg_read,
	.write = sevenseg_write
};

static int sevenseg_probe(struct platform_device *pdev)
{
	struct altera_sevenseg *sevenseg;
	struct resource *io;
	int retval;

	sevenseg = devm_kzalloc(&pdev->dev, sizeof(*sevenseg), GFP_KERNEL);
	if (sevenseg == NULL)
		return -ENOMEM;
	platform_set_drvdata(pdev, sevenseg);

	io = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sevenseg->regs = devm_ioremap_resource(&pdev->dev, io);
	if (IS_ERR(sevenseg->regs))
		return PTR_ERR(sevenseg->regs);
	sevenseg->size = io->end - io->start + 1;

	sevenseg->misc.name = DRIVER_NAME;
	sevenseg->misc.minor = MISC_DYNAMIC_MINOR;
	sevenseg->misc.fops = &sevenseg_fops;
	sevenseg->misc.parent = &pdev->dev;
	retval = misc_register(&sevenseg->misc);
	if (retval) {
		dev_err(&pdev->dev, "Register misc device failed!\n");
		return retval;
	}

	dev_info(&pdev->dev, "Seveseg driver loaded!");

	return 0;
}

static int sevenseg_remove(struct platform_device *pdev)
{
	struct altera_sevenseg *sevenseg = platform_get_drvdata(pdev);

	misc_deregister(&sevenseg->misc);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id sevenseg_of_match[] = {
	{ .compatible = "hof,sevensegment-1.0", },
	{ },
};
MODULE_DEVICE_TABLE(of, sevenseg_of_match);

static struct platform_driver sevenseg_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sevenseg_of_match),
	},
	.probe		= sevenseg_probe,
	.remove		= sevenseg_remove,
};

module_platform_driver(sevenseg_driver);

MODULE_AUTHOR("Giritzer");
MODULE_DESCRIPTION("Altera/Terasic seven segment driver");
MODULE_LICENSE("GPL v2");
