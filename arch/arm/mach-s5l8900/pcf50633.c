/*
 * pcf50633 - PCF50633 Platform-side data and code.
 *
 * Copyright 2010 Ricky Taylor
 *
 * This file is part of iDroid. An android distribution for Apple products.
 * For more information, please visit http://www.idroidproject.org/.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/power_supply.h>

#include <mach/pmu.h>

struct platform_device iphone_backlight = {
	.name           = "pcf50633-backlight",
	.id             = -1,
	.num_resources  = 0,
};

static struct power_supply iphone_battery;
static struct pcf50633 *pcf50633;
static struct {
	struct delayed_work monitor_work;

	int voltage;
	int level;
} iphone_battery_info;

static void iphone_battery_update_status(struct pcf50633 *pcf, void *unused, int res)
{
	const int interval = msecs_to_jiffies(60 * 1000);

	int voltage = (res*6000)/1023;
	int level = ((voltage - 3500) * 100) / (4200 - 3500);
	if(level < 0)
		level = 0;
	if(level > 100)
		level = 100;
	
	dev_info(pcf50633->dev, "V:%d, L:%d\n", voltage, level);

	iphone_battery_info.voltage = voltage;
	iphone_battery_info.level = level;

	power_supply_changed(&iphone_battery);

	if(unused)
		schedule_delayed_work(&iphone_battery_info.monitor_work, interval);
}

static void iphone_battery_work(struct work_struct* work)
{
	const int interval = msecs_to_jiffies(60 * 1000);

	if(platform_get_drvdata(pcf50633->adc_pdev) == NULL || pcf50633_adc_async_read(pcf50633, PCF50633_ADCC1_MUX_BATSNS_RES, PCF50633_ADCC1_AVERAGE_16, &iphone_battery_update_status, (void*)1) < 0)
	{
		dev_err(pcf50633->dev, "failed to get battery level\n");
		schedule_delayed_work(&iphone_battery_info.monitor_work, interval);
	}
}

static int iphone_battery_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int charging = 0;
	int status = 0;

	if(pcf50633 && platform_get_drvdata(pcf50633->mbc_pdev))
	{
		status = pcf50633_mbc_get_status(pcf50633);
		charging = (status & (PCF50633_MBC_USB_ONLINE | PCF50633_MBC_ADAPTER_ONLINE)) != 0;
	}
	else
		dev_err(pcf50633->dev, "failed to contact mbc for charging state.\n");

	dev_info(pcf50633->dev, "status: %d, charging: %d.\n", status, charging);

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			if(charging)
			{
				if(iphone_battery_info.level == 100)
				       	val->intval = POWER_SUPPLY_STATUS_FULL;
				else
				       	val->intval = POWER_SUPPLY_STATUS_CHARGING;
			}
			else
			{
			    val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			/* TODO: we might need to set POWER_SUPPLY_HEALTH_OVERHEAT if we figure out the battery temperature stuff */
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = (iphone_battery_info.voltage > 0) ? 1: 0;
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			val->intval = iphone_battery_info.level;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val->intval = iphone_battery_info.voltage * 1000;
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static enum power_supply_property iphone_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static char* iphone_batteries[] = {
	"battery",
};

static struct power_supply iphone_battery = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = iphone_battery_properties,
	.num_properties = ARRAY_SIZE(iphone_battery_properties),
	.get_property = iphone_battery_get_property,
};

static void pcf50633_event_callback(struct pcf50633 *_pcf, int _i)
{
	if(platform_get_drvdata(pcf50633->adc_pdev) == NULL || pcf50633_adc_async_read(pcf50633, PCF50633_ADCC1_MUX_BATSNS_RES, PCF50633_ADCC1_AVERAGE_16, &iphone_battery_update_status, NULL) < 0)
	{
		dev_err(pcf50633->dev, "failed to get battery level\n");
	}

	power_supply_changed(&iphone_battery);
}

static void pcf50633_probe_done(struct pcf50633 *_pcf)
{
	int ret;

	pcf50633 = _pcf;

	iphone_battery_info.voltage = 0;
	iphone_battery_info.level = 50;

	ret = power_supply_register(_pcf->dev, &iphone_battery);
	if(ret)
		dev_err(_pcf->dev, "failed to register battery power supply!\n");

	INIT_DELAYED_WORK(&iphone_battery_info.monitor_work, iphone_battery_work);

	iphone_backlight.dev.platform_data = _pcf;
	if(platform_device_register(&iphone_backlight) < 0)
		dev_err(_pcf->dev, "failed to create backlight driver!\n");
	
	// Start the battery level watcher.
	iphone_battery_work(NULL);
}

struct pcf50633_platform_data pcf50633_pdata = {
	.resumers = {
		[0] =	PCF50633_INT1_USBINS |
			PCF50633_INT1_USBREM |
			PCF50633_INT1_ALARM,
		[1] =	PCF50633_INT2_ONKEYF,
		[2] =	PCF50633_INT3_ONKEY1S,
		[3] =	PCF50633_INT4_LOWSYS |
			PCF50633_INT4_LOWBAT |
			PCF50633_INT4_HIGHTMP,
	},

	.reg_init_data = {
		[PCF50633_REGULATOR_AUTO] = {
			.constraints = {
				.min_uV = 3300000,
				.max_uV = 3300000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.always_on = 1,
				.apply_uV = 1,
				.state_mem = {
					.enabled = 1,
				},
			},
		},
		[PCF50633_REGULATOR_DOWN1] = {
			.constraints = {
				.min_uV = 1300000,
				.max_uV = 1600000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.always_on = 1,
				.apply_uV = 1,
			},
		},
		[PCF50633_REGULATOR_DOWN2] = {
			.constraints = {
				.min_uV = 1800000,
				.max_uV = 1800000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
				.always_on = 1,
				.state_mem = {
					.enabled = 1,
				},
			},
		},
		[PCF50633_REGULATOR_HCLDO] = {
			.constraints = {
				.min_uV = 2000000,
				.max_uV = 3300000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
				.always_on = 1,
			},
		},
		[PCF50633_REGULATOR_LDO1] = {
			.constraints = {
				.min_uV = 3300000,
				.max_uV = 3300000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
				.state_mem = {
					.enabled = 0,
				},
			},
		},
		[PCF50633_REGULATOR_LDO2] = {
			.constraints = {
				.min_uV = 3300000,
				.max_uV = 3300000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
			},
		},
		[PCF50633_REGULATOR_LDO3] = {
			.constraints = {
				.min_uV = 3000000,
				.max_uV = 3000000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
			},
		},
		[PCF50633_REGULATOR_LDO4] = {
			.constraints = {
				.min_uV = 3200000,
				.max_uV = 3200000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
			},
		},
		[PCF50633_REGULATOR_LDO5] = {
			.constraints = {
				.min_uV = 3000000,
				.max_uV = 3000000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.apply_uV = 1,
				.state_mem = {
					.enabled = 1,
				},
			},
		},
		[PCF50633_REGULATOR_LDO6] = {
			.constraints = {
				.min_uV = 3000000,
				.max_uV = 3000000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
			},
		},
		[PCF50633_REGULATOR_MEMLDO] = {
			.constraints = {
				.min_uV = 1800000,
				.max_uV = 1800000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.state_mem = {
					.enabled = 1,
				},
			},
		},

	},

	.probe_done = &pcf50633_probe_done,
	.mbc_event_callback = &pcf50633_event_callback,

	.batteries = iphone_batteries,
	.num_batteries = ARRAY_SIZE(iphone_batteries),
};

void pcf50633_power_off(void)
{
	if(pcf50633)
	{
		pcf50633_reg_write(pcf50633, 0x0d, 0x1); // Only ONKEY can wake the device up.
		pcf50633_reg_write(pcf50633, 0x0f, 0x7); // Set the debounce for ONKEY to 2s.
		pcf50633_reg_write(pcf50633, 0x76, 0x80); // No idea what this does.
		pcf50633_reg_write(pcf50633, PCF50633_REG_OOCSHDWN, 2 | 1); // Yes, we're setting a reserved bit.

		printk("PCF50633 shutdown complete.\n");
	}
}

void pcf50633_suspend(void)
{
	if(pcf50633)
	{
		pcf50633_reg_write(pcf50633, 0x0d, 0xFFFFFFFF); // Volatile, allow anything to wake
		pcf50633_reg_write(pcf50633, 0x0f, 0x0); // No debounce
		pcf50633_reg_write(pcf50633, PCF50633_REG_OOCSHDWN, 2); // Fall asleep

		printk("PCF50633 suspend complete.\n");
	}
}
