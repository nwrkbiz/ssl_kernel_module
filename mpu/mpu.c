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
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/signal.h>
#include <linux/sched.h> 
#include <asm/siginfo.h>	

#define DRIVER_NAME "mpu"
#define PID_OFFSET 13
#define TOLERANCE_CONFIG_OFFSET 24
#define TOLERANCE_CONFIG_SIZE PID_OFFSET - 1 

#define SIG_TEST 44	

#define NUM_REGS 45
#define CHAR_DEVICE_SIZE ((NUM_REGS) * 4)

struct altera_mpu {
	void *regs;
	char buffer[CHAR_DEVICE_SIZE];
	char data_buffer[CHAR_DEVICE_SIZE];
	int size;
	int pid;
	int irq_num;
	struct mutex mutex_lock;
	struct miscdevice misc;
};

/*
 * @brief Lock character device if opened once.
 * @return EBUSY if device is already opened.
 */
static int lock_misc(struct inode *inode, struct file *filep)
{
	struct altera_mpu *mpu = container_of(filep->private_data,
				       struct altera_mpu, misc);

	if (!mutex_trylock(&mpu->mutex_lock))
		return -EBUSY;

	return 0;
}

/*
 * @brief Unlock character device after closing it.
 */
static int release_misc(struct inode *inode, struct file *filep)
{
	struct altera_mpu *mpu = container_of(filep->private_data,
				       struct altera_mpu, misc);

	mutex_unlock(&mpu->mutex_lock);
	return 0;
}


/*
 * @brief IRQ handler function.
 */
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct altera_mpu *mpu = dev_id;
	struct siginfo info;
   	struct task_struct *t;

        t = pid_task(find_vpid(mpu->pid), PIDTYPE_PID);
	if(t == NULL)
		return IRQ_HANDLED;

	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIG_TEST;
	info.si_code = SI_QUEUE;
	info.si_int = 1234;  

	/* Tell userspace that IRQ occured */
	send_sig_info(SIG_TEST, &info, t);

	return IRQ_HANDLED;
}

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
static int mpu_write(struct file *filep, const char *buf,
			  size_t count, loff_t *offp)
{
	int i = 0;
	int result = 0;
        char values_to_write[TOLERANCE_CONFIG_SIZE+1];
	char tmp[CHAR_DEVICE_SIZE];
	struct altera_mpu *mpu = container_of(filep->private_data,
					   struct altera_mpu, misc);

	if ((*offp < 0) || (*offp >= CHAR_DEVICE_SIZE))
		return -EINVAL;

	if ((*offp + count) > CHAR_DEVICE_SIZE)
		count = CHAR_DEVICE_SIZE - *offp;

	if (count > 0) {
		count = count - copy_from_user(tmp + *offp,
					       buf,
					       count);
	}

        // value to write
        for (i = TOLERANCE_CONFIG_SIZE; i < TOLERANCE_CONFIG_SIZE; i++)
        {
	   if(tmp[i] != ' ')
	   {
		mpu->data_buffer[i] = tmp[i];
		values_to_write[i-TOLERANCE_CONFIG_SIZE] = mpu->data_buffer[i];
           }
        }

	result = kstrtoint(&mpu->data_buffer[PID_OFFSET], 10, &mpu->pid);

        iowrite32((u32)values_to_write, mpu->regs + TOLERANCE_CONFIG_OFFSET);
	*offp += count;
	return count;
}

static const struct file_operations mpu_fops = {
	.owner = THIS_MODULE,
	.read = mpu_read,
	.write = mpu_write,
	.open = lock_misc,
	.release = release_misc
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

	mpu->irq_num = irq_of_parse_and_map(pdev->dev.of_node, 0);

	retval = devm_request_irq(&pdev->dev, mpu->irq_num, irq_handler,
				  IRQF_SHARED,
				  DRIVER_NAME, mpu);

	if (retval) {
		dev_err(&pdev->dev, "Register misc device failed!\n");
		return retval;
	}

	mutex_init(&mpu->mutex_lock);
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
