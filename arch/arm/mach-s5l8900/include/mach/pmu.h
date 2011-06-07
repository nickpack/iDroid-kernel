#ifndef IPHONE_HW_PMU_H
#define IPHONE_HW_PMU_H

#include <linux/mfd/pcf50633/core.h>
#include <linux/mfd/pcf50633/pmic.h>
#include <linux/mfd/pcf50633/adc.h>
#include <linux/mfd/pcf50633/mbc.h>

extern struct pcf50633_platform_data pcf50633_pdata;

extern void pcf50633_power_off(void);
extern void pcf50633_suspend(void);

void iphone_init_suspend(void);

#endif

