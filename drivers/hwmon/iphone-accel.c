/*
 * drivers/hwmon/iphone_accel.c - driver for iPhone/iPod Accelerometer
 *
 * Copyright (C) 2010 Patrick Wildt <webmaster@patrick-wildt.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * addons by Dario Russo, (turbominchiameister@gmail.com)
 * based on the work of Kalhan Trisal and Alan Cox
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <mach/accel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/input-polldev.h>
#include <linux/slab.h>

#define CTRL_REG1				0x20
#define CTRL_REG1_POWER_DOWN			(1<<6)

struct iphone_accel_info *accel_info;

struct iphone_accel_info {
	struct i2c_device *i2c_dev;
	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
	struct input_polled_dev *idev;
	struct mutex lock;
	struct work_struct work;
	unsigned int flags;
	unsigned int working;
	u_int8_t regs[0x40];
};

int accel_read_reg(int reg) {
	return i2c_smbus_read_byte_data(accel_info->i2c_client, reg);
}

void accel_write_reg(int reg, int data) {
	i2c_smbus_write_byte_data(accel_info->i2c_client, reg, data);
}

/* Sysfs methods */
static int iphone_accel_read(int *x, int *y, int *z) {
	*x = (signed char)accel_read_reg(ACCEL_OUTX);
	*y = (signed char)accel_read_reg(ACCEL_OUTY);
	*z = (signed char)accel_read_reg(ACCEL_OUTZ);

	return 0;
}

/* Sysfs Files */
static ssize_t power_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val;

	val = accel_read_reg(CTRL_REG1) & CTRL_REG1_POWER_DOWN;
	if (val == CTRL_REG1_POWER_DOWN)
		val = 1;
	return sprintf(buf, "%d\n", val);
}

static ssize_t power_mode_store(struct device *dev,
		struct device_attribute *attr, const  char *buf, size_t count)
{
	unsigned int ret_val;
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&accel_info->lock);

	ret_val = accel_read_reg( CTRL_REG1);
	ret_val &= 0xBF;

	switch(val) {
		case 1:
			ret_val |= CTRL_REG1_POWER_DOWN;
		case 0:
			accel_write_reg( CTRL_REG1, ret_val);
			break;
		default:
			mutex_unlock(&accel_info->lock);
			return -EINVAL;
	}
	mutex_unlock(&accel_info->lock);
	return count;
}


static DEVICE_ATTR(power_state, S_IRUGO | S_IWUSR, power_mode_show, power_mode_store);

static struct attribute *iphone_accel_attributes[] = {
	&dev_attr_power_state.attr,
	NULL,
};

static struct attribute_group iphone_accel_attribute_group = {
	.attrs = iphone_accel_attributes,
};


static void iphone_accel_input_poll(struct input_polled_dev *idev)
{
	int x,y,z;
	iphone_accel_read(&x,&y,&z);

#ifdef CONFIG_IPODTOUCH_1G
	input_report_abs(idev->input, ABS_X, -x);
#else
	input_report_abs(idev->input, ABS_X, x);
#endif

#if defined(CONFIG_IPHONE_3G) || defined(CONFIG_IPODTOUCH_1G)
	input_report_abs(idev->input, ABS_Y, y);
	input_report_abs(idev->input, ABS_Z, z); 
#else
	input_report_abs(idev->input, ABS_Y, -y);
	input_report_abs(idev->input, ABS_Z, -z);
#endif

}

static int __devinit iphone_accel_probe(struct i2c_client *i2c,
                            const struct i2c_device_id *device_id)
{
	int ret;
	int whoami;

	accel_info=kzalloc(sizeof(*accel_info), GFP_KERNEL);
	if (!accel_info)
		return -ENOMEM;

	mutex_init(&accel_info->lock);

	accel_info->idev=input_allocate_polled_device();
	accel_info->idev->poll = iphone_accel_input_poll;
	accel_info->idev->poll_interval = 50;
	accel_info->input_dev=accel_info->idev->input;
	accel_info->i2c_client=i2c;

	ret = sysfs_create_group(&accel_info->i2c_client->dev.kobj, &iphone_accel_attribute_group);
	if (ret)
		goto out;

	whoami = accel_read_reg(ACCEL_WHOAMI);
	if(whoami != ACCEL_WHOAMI_VALUE) {
		printk(KERN_INFO "iphone-accel: incorrect whoami value\n");
		goto out_sysfs;
	}

	accel_write_reg(ACCEL_CTRL_REG2, ACCEL_CTRL_REG2_BOOT);
	accel_write_reg(ACCEL_CTRL_REG1, ACCEL_CTRL_REG1_PD | ACCEL_CTRL_REG1_XEN | ACCEL_CTRL_REG1_YEN | ACCEL_CTRL_REG1_ZEN);

	accel_info->input_dev->name = "ST LIS331DL 3 axis-accelerometer";
	accel_info->input_dev->phys       = "iphone_accel/input0";
	accel_info->input_dev->id.bustype = BUS_I2C;
	accel_info->input_dev->id.vendor = 0;
	accel_info->input_dev->dev.parent = &i2c->dev;

	set_bit(EV_ABS, accel_info->input_dev->evbit);
	set_bit(ABS_X, accel_info->input_dev->absbit);
	set_bit(ABS_Y, accel_info->input_dev->absbit);
	set_bit(ABS_Z, accel_info->input_dev->absbit);

	ret = input_register_polled_device(accel_info->idev);
	if (ret)
		goto out_sysfs;

	printk( "iphone_accel: device successfully initialized.\n");
	return 0;

out_sysfs:
	sysfs_remove_group(&accel_info->i2c_client->dev.kobj, &iphone_accel_attribute_group);
out:
	printk(KERN_INFO "iphone_accel: error initializing\n");
	return -1;
}

static int __devexit iphone_accel_remove(struct i2c_client *client)
{
	sysfs_remove_group(&accel_info->i2c_client->dev.kobj, &iphone_accel_attribute_group);
	input_unregister_polled_device(accel_info->idev);
	input_free_polled_device(accel_info->idev);
	kfree(accel_info);

	return 0;
}

static const struct i2c_device_id iphone_accel_i2c_id[] = {
	{ "iphone-accel", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, iphone_accel_i2c_id);

static struct i2c_driver iphone_accel_driver = {
	.driver	= {
		.name = "iphone-accel",
		.owner = THIS_MODULE,
	},
	.probe = iphone_accel_probe,
	.remove = iphone_accel_remove,
	.id_table = iphone_accel_i2c_id,
};


/* Module stuff */

static int __init iphone_accel_init(void)
{
	int ret;

	ret = i2c_add_driver(&iphone_accel_driver);
	if (ret) {
		printk("iphone_accel: unable to register I2C driver: %d\n", ret);
		goto out;
	}

	printk(KERN_INFO "iphone_accel: driver successfully loaded.\n");
	return 0;

out:
	i2c_del_driver(&iphone_accel_driver);
	printk(KERN_WARNING "iphone_accel: driver init failed (ret=%d)!\n", ret);
	return ret;
}

static void __exit iphone_accel_exit(void)
{
	i2c_del_driver(&iphone_accel_driver);
	printk(KERN_INFO "iphone_accel: driver unloaded.\n");
}

module_init(iphone_accel_init);
module_exit(iphone_accel_exit);

MODULE_AUTHOR("Patrick Wildt <webmaster@patrick-wildt.de>");
MODULE_DESCRIPTION("iPhone Accelerometer Driver");
MODULE_LICENSE("GPL v2");
