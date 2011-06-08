/*
 * lis331dl.c - ST LIS331DL  Accelerometer Driver
 *
 * Copyright (C) 2009 Intel Corp
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon-vid.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>


MODULE_AUTHOR("Kalhan Trisal <kalhan.trisal at intel.com");
MODULE_DESCRIPTION("STMicroelectronics LIS331DL Accelerometer Driver");
MODULE_LICENSE("GPL v2");

#define ACCEL_DATA_RATE_100HZ 0
#define ACCEL_DATA_RATE_400HZ 1
#define ACCEL_POWER_MODE_DOWN 0
#define ACCEL_POWER_MODE_ACTIVE 1

/* internal return values */

struct lis331dl_data {
	struct device *hwmon_dev;
	struct mutex update_lock;
};

static ssize_t rate_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret_val, val;

	val = i2c_smbus_read_byte_data(client, 0x20);
	ret_val = (val & 0x80); /* 1= 400HZ 0= 100HZ */
	if (ret_val == 0x80)
		ret_val = ACCEL_DATA_RATE_400HZ;
	return sprintf(buf, "%d\n", ret_val);

}

static ssize_t state_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret_val, val;

	val = i2c_smbus_read_byte_data(client, 0x20);
	ret_val = (val & 0x40);
	if (ret_val == 0x40)
		ret_val = ACCEL_POWER_MODE_ACTIVE;
	return sprintf(buf, "%d\n", ret_val);
}

static ssize_t xyz_pos_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int x, y, z;
	struct i2c_client *client = to_i2c_client(dev);

	x = i2c_smbus_read_byte_data(client, 0x29);
	y = i2c_smbus_read_byte_data(client, 0x2B);
	z = i2c_smbus_read_byte_data(client, 0x2D);
	return sprintf(buf, "%d, %d, %d \n", x, y, z);
}

static ssize_t rate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lis331dl_data *data = i2c_get_clientdata(client);
	unsigned int ret_val, set_val;
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	ret_val = i2c_smbus_read_byte_data(client, 0x20);

	mutex_lock(&data->update_lock);
	if (val == ACCEL_DATA_RATE_100HZ)
		set_val = (ret_val & 0x7F); /* setting the 8th bit to 0 */
	else if (val == ACCEL_DATA_RATE_400HZ)
		set_val = (ret_val | (1 << 7));
	else
		goto invarg;

	i2c_smbus_write_byte_data(client, 0x20, set_val);
	mutex_unlock(&data->update_lock);
	return count;
invarg:
	mutex_unlock(&data->update_lock);
	return -EINVAL;
}

static ssize_t state_store(struct device *dev,
		struct device_attribute *attr, const  char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lis331dl_data *data = i2c_get_clientdata(client);
	unsigned int ret_val, set_val;
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&data->update_lock);
	ret_val = i2c_smbus_read_byte_data(client, 0x20);
	if (val == ACCEL_POWER_MODE_DOWN)
		set_val = ret_val & 0xBF; /* if value id 0 */
	else if (val == ACCEL_POWER_MODE_ACTIVE)
		set_val = (ret_val | (1<<6)); /* if value is 1 */
	else
		goto invarg;

	i2c_smbus_write_byte_data(client, 0x20, set_val);
	mutex_unlock(&data->update_lock);
	return count;
invarg:
	mutex_unlock(&data->update_lock);
	return -EINVAL;
}

static DEVICE_ATTR(data_rate, S_IRUGO | S_IWUSR, rate_show, rate_store);
static DEVICE_ATTR(power_state, S_IRUGO | S_IWUSR, state_show, state_store);
static DEVICE_ATTR(position, S_IRUGO, xyz_pos_show, NULL);

static struct attribute *lis331dl_attr[] = {
	&dev_attr_data_rate.attr,
	&dev_attr_power_state.attr,
	&dev_attr_position.attr,
	NULL
};

static struct attribute_group lis331dl_gr = {
	.attrs = lis331dl_attr
};

static void accel_set_default_config(struct i2c_client *client)
{
	/* Device is configured in
	* 100Hz output data rate 7 bit 0
	* active mode   6 bit 1
	* x,y,z axix enable   0,1,2 bits 1*/
	i2c_smbus_write_byte_data(client, 0x20, 0x47);
}

static int  lis331dl_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	int res;
	struct lis331dl_data *data;

	data = kzalloc(sizeof(struct lis331dl_data), GFP_KERNEL);
	if (data == NULL) {
		printk(KERN_WARNING "lis331dl: Memory alloc failed\n");
		return -ENOMEM;
	}
	mutex_init(&data->update_lock);
	i2c_set_clientdata(client, data);

	res = sysfs_create_group(&client->dev.kobj, &lis331dl_gr);
	if (res) {
		printk(KERN_WARNING "lis331dl: Sysfs group creation failed\n");
		goto acclero_error1;
	}
	data->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(data->hwmon_dev)) {
		res = PTR_ERR(data->hwmon_dev);
		data->hwmon_dev = NULL;
		sysfs_remove_group(&client->dev.kobj, &lis331dl_gr);
		printk(KERN_WARNING "lis331dl: Unable to register hwmon device\n");
		goto acclero_error1;
	}
	accel_set_default_config(client);

	dev_info(&client->dev, "%s lis331dl: Accelerometer found", client->name);
	return res;

acclero_error1:
	i2c_set_clientdata(client, NULL);
	kfree(data);
	return res;
}

static int lis331dl_remove(struct i2c_client *client)
{
	struct lis331dl_data *data = i2c_get_clientdata(client);

	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, &lis331dl_gr);
	kfree(data);
	return 0;
}

static struct i2c_device_id lis331dl_id[] = {
	{ "lis331dl", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, lis331dl_id);

static struct i2c_driver lis331dl_driver = {
	.driver = {
		.name = "lis331dl",
	},
	.probe = lis331dl_probe,
	.remove = lis331dl_remove,
	.id_table = lis331dl_id,
};

static int __init sensor_lis331dl_init(void)
{
	return i2c_add_driver(&lis331dl_driver);
}

static void  __exit sensor_lis331dl_exit(void)
{
	i2c_del_driver(&lis331dl_driver);
}

module_init(sensor_lis331dl_init);
module_exit(sensor_lis331dl_exit);

