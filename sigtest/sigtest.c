/*
 * Terasic DE1-SoC sigtest driver
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
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/signal.h>
#include <linux/sched.h> 
#include <asm/siginfo.h>	

#define DRIVER_NAME "sigtest"
#define BUFFER_SIZE 25
#define PID_OFFSET 0
#define SIG_TEST 44

static char buffer[BUFFER_SIZE];
static int pid;

/*
 * @brief This function gets executed on fread.
 */
static int sigtest_read(struct file *filep, char *buf, size_t count,
			 loff_t *offp)
{

	if ((*offp < 0) || (*offp >= BUFFER_SIZE))
		return 0;

	if ((*offp + count) > BUFFER_SIZE)
		count = BUFFER_SIZE - *offp;

	if (count > 0) {
		count = count - copy_to_user(buf,
					     buffer + *offp,
					     count);

		*offp += count;
	}
	return count;
}

/*
 * @brief This function gets executed on fwrite.
 */
static int sigtest_write(struct file *filep, const char *buf,
			  size_t count, loff_t *offp)
{
	int result = 0;
	struct siginfo info;
   	struct task_struct *t;

	printk("Write\n");

	if ((*offp < 0) || (*offp >= BUFFER_SIZE))
		return -EINVAL;

	if ((*offp + count) > BUFFER_SIZE)
		count = BUFFER_SIZE - *offp;

	if (count > 0) {
		count = count - copy_from_user(buffer + *offp,
					       buf,
					       count);
	}

	// set PID
	result = kstrtoint(&buffer[PID_OFFSET], 10, &pid);
	printk("PID: %d \n", pid);
	*offp += count;


	// catch invalid PID
        t = pid_task(find_vpid(pid), PIDTYPE_PID);
	if(t == NULL)
	{
		printk("Not process found PID: %d \n", pid);
		return count;
	}

	// register signal (this will be done in irq handler in mpu driver)
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIG_TEST;
	info.si_code = SI_QUEUE;
	info.si_int = 1234;  

	// Send signal to userspace application
	send_sig_info(SIG_TEST, &info, t);

	printk("Signal sent to PID: %d \n", pid);

	return count;
}

static const struct file_operations sigtest_fops = {
	.owner = THIS_MODULE,
	.read = sigtest_read,
	.write = sigtest_write
};

struct miscdevice sigtest = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DRIVER_NAME,
    .fops = &sigtest_fops,
};

static int __init misc_init(void)
{

	int retval;
	retval = misc_register(&sigtest);
	if (retval) {
		pr_err("Register misc device failed!\n");
		return retval;
	}

	pr_info("sigtest driver loaded!");

	return 0;
}

static void __exit misc_exit(void)
{
	misc_deregister(&sigtest);
}

module_init(misc_init)
module_exit(misc_exit)

MODULE_AUTHOR("Giritzer");
MODULE_DESCRIPTION("Altera/Terasic sigtest driver");
MODULE_LICENSE("GPL v2");
