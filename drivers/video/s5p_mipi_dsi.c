/* linux/drivers/video/s5p_mipi_dsi.c
 *
 * Samsung SoC MIPI-DSIM driver.
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * InKi Dae, <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/notifier.h>

#include <plat/fb.h>
#include <plat/mipi_dsim.h>
#include <plat/regs-dsim.h>

#include "s5p_mipi_dsi_common.h"
#include "s5p_mipi_dsi_lowlevel.h" // for IRQ handler

#define MIPI_DSI_DEBUG

#ifdef MIPI_DSI_DEBUG
#define dbg_printk(x...) printk(x)
#else
#define dbg_printk(x...)
#endif

#define master_to_driver(a)	(a->dsim_lcd_drv)
#define master_to_device(a)	(a->dsim_lcd_dev)

struct mipi_dsim_ddi
{
	int bus_num;
	struct list_head list;
	struct mipi_dsim_device *master;
	struct mipi_dsim_lcd_device *device;
	struct mipi_dsim_lcd_driver *driver;
};

static LIST_HEAD(dsim_ddi_list);
static LIST_HEAD(dsim_driver_list);

static DEFINE_MUTEX(dsim_ddi_lock);
static DEFINE_MUTEX(dsim_driver_lock);

static struct s5p_platform_mipi_dsim *to_dsim_plat(struct platform_device *pdev)
{
	return (struct s5p_platform_mipi_dsim *)pdev->dev.platform_data;
}

static irqreturn_t s5p_mipi_dsi_interrupt_handler(int irq, void *dev_id)
{
	struct mipi_dsim_device *dsim = dev_id;
	u32 intsrc = readl(dsim->reg_base + S5P_DSIM_INTSRC);

	printk("%s\n", __func__);

	if(intsrc & INTSRC_LPDR_TOUT)
	{
		dsim->rx_result = -ETIMEDOUT;
		complete(&dsim->rx_completion);
		intsrc &=~ INTSRC_LPDR_TOUT;
	}

	if(intsrc & INTSRC_RX_ACK)
	{
		dsim->rx_result = 0;
		complete(&dsim->rx_completion);
		intsrc &=~ INTSRC_RX_ACK;
	}

	if(intsrc & INTSRC_RXDAT_DONE)
	{
		uint8_t *buff = dsim->rx_buffer;
		u32 rd, amt;
		int i;

		s5p_mipi_dsi_wait_for_rx_fifo(dsim);

		rd = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
		if((rd & 0x3F) != 26 && (rd & 0x3F) != 28)
			printk(KERN_ERR "mipi_dsim: Weird response! 0x%x\n", rd & 0x3F);

		amt = (rd >> 8) & 0xFFFF;
		if(amt < dsim->rx_size)
			dsim->rx_result = amt;
		else
			dsim->rx_result = dsim->rx_size;

		for(i = 0; i < amt; i++)
		{
			if(!(i%4))
			{
				s5p_mipi_dsi_wait_for_rx_fifo(dsim);
				rd = readl(dsim->reg_base + S5P_DSIM_RXFIFO);
			}

			if(buff && i < dsim->rx_size)
				buff[i] = rd;

			rd >>= 8;
		}

		dsim->rx_buffer = NULL;
		dsim->rx_size = 0;
		complete(&dsim->rx_completion);
		intsrc &=~ INTSRC_RXDAT_DONE;
	}

	if(intsrc)
		printk(KERN_WARNING "mipi_dsim: Unhandled interrupts 0x%x.\n", intsrc);

	writel(intsrc, dsim->reg_base + S5P_DSIM_INTSRC);
	return IRQ_HANDLED;
}

static int s5p_mipi_dsi_registration_done(struct mipi_dsim_device *dsim)
{
	int ret;
	struct mipi_dsim_lcd_device *lcd_dev = dsim->dsim_lcd_dev;
	struct mipi_dsim_lcd_driver *lcd_drv = dsim->dsim_lcd_drv;

	printk(KERN_INFO "%s!\n", __func__);

	lcd_dev->master = dsim;
	lcd_dev->dev.parent = dsim->dev;
	dev_set_name(&lcd_dev->dev, "%s", lcd_drv->name);

	ret = device_register(&lcd_dev->dev);
	if (ret < 0)
	{
		dev_err(dsim->dev,
			"can't register %s, status %d\n",
			dev_name(&lcd_dev->dev), ret);
		return ret;
	}

	/* enable MIPI-DSI PHY. */
	if (dsim->pd->phy_enable)
		dsim->pd->phy_enable(dsim, true);

	printk("Initializing DSIM.\n");

	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);

	printk("Something HS\n");

	s5p_mipi_dsi_set_hs_enable(dsim);

	printk("Setting display mode.\n");

	/* set display timing. */
	s5p_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);

	printk("Probing driver");

	/* initialize mipi-dsi client(lcd panel). */
	if(lcd_drv->probe)
		lcd_drv->probe(lcd_dev);

	printk(KERN_DEBUG "mipi-dsi driver(%s mode) has been probed.\n",
		(dsim->dsim_config->e_interface == DSIM_COMMAND) ?
			"CPU" : "RGB");
	return 0;
}

static struct mipi_dsim_lcd_driver *s5p_mipi_dsi_find_driver(const char *_nm)
{
	struct mipi_dsim_lcd_driver *drv, *driver = NULL;

	mutex_lock(&dsim_driver_lock);
	list_for_each_entry(drv, &dsim_driver_list, list)
	{
		if(!strcmp(drv->name, _nm))
		{
			driver = drv;
			break;
		}
	}
	mutex_unlock(&dsim_driver_lock);

	return driver;
}

int s5p_mipi_dsi_register_lcd_device(struct mipi_dsim_lcd_device *lcd_dev)
{
	struct mipi_dsim_ddi *dsim_ddi;
	struct mipi_dsim_device *dev = NULL;

	if (!lcd_dev) {
		printk(KERN_ERR "mipi_dsim_lcd_device is NULL.\n");
		return -EFAULT;
	}

	if (!lcd_dev->name) {
		printk(KERN_ERR "dsim_lcd_device name is NULL.\n");
		return -EFAULT;
	}

	printk(KERN_INFO "registering mipi-dsim lcd %s!\n", lcd_dev->name);
	
	mutex_lock(&dsim_ddi_lock);
	list_for_each_entry(dsim_ddi, &dsim_ddi_list, list) {
		if(dsim_ddi->bus_num >= 0
				&& dsim_ddi->bus_num != lcd_dev->bus_id)
			continue;

		if(dsim_ddi->device)
			continue;

		dsim_ddi->device = lcd_dev;
		dsim_ddi->driver = s5p_mipi_dsi_find_driver(lcd_dev->name);
		if(dsim_ddi->driver)
		{
			dev = dsim_ddi->master;
			dev->dsim_lcd_drv = dsim_ddi->driver;
			dev->dsim_lcd_dev = dsim_ddi->device;
			list_del(&dsim_ddi->list);
			kfree(dsim_ddi);
			break;
		}
	}
	mutex_unlock(&dsim_ddi_lock);

	if(dev)
		return s5p_mipi_dsi_registration_done(dsim_ddi->master);

	dsim_ddi = kzalloc(sizeof(struct mipi_dsim_ddi), GFP_KERNEL);
	if (!dsim_ddi) {
		printk(KERN_ERR "failed to allocate dsim_ddi object.\n");
		return -EFAULT;
	}

	dsim_ddi->bus_num = lcd_dev->bus_id;
	dsim_ddi->device = lcd_dev;
	dsim_ddi->driver = NULL;
	dsim_ddi->master = NULL;

	mutex_lock(&dsim_ddi_lock);
	list_add_tail(&dsim_ddi->list, &dsim_ddi_list);
	mutex_unlock(&dsim_ddi_lock);

	printk("mipi dsim: No master waiting for device.\n");
	return 0;
}
EXPORT_SYMBOL_GPL(s5p_mipi_dsi_register_lcd_device);

int s5p_mipi_dsi_register_lcd_driver(struct mipi_dsim_lcd_driver *lcd_drv)
{
	struct mipi_dsim_ddi *dsim_ddi;

	if (!lcd_drv) {
		printk(KERN_ERR "mipi_dsim_lcd_driver is NULL.\n");
		return -EFAULT;
	}

	if (!lcd_drv->name) {
		printk(KERN_ERR "dsim_lcd_driver name is NULL.\n");
		return -EFAULT;
	}

	mutex_lock(&dsim_driver_lock);
	list_add_tail(&lcd_drv->list, &dsim_driver_list);
	mutex_unlock(&dsim_driver_lock);

	mutex_lock(&dsim_ddi_lock);
	list_for_each_entry(dsim_ddi, &dsim_ddi_list, list) {
		if(!dsim_ddi->master || !dsim_ddi->device)
			continue;

		if(!strcmp(dsim_ddi->device->name, lcd_drv->name))
		{
			dsim_ddi->driver = lcd_drv;
			mutex_unlock(&dsim_ddi_lock);
			s5p_mipi_dsi_registration_done(dsim_ddi->master);
			return 0;
		}
	}
	mutex_unlock(&dsim_ddi_lock);

	printk(KERN_INFO "registered panel driver(%s) to mipi-dsi driver.\n",
		lcd_drv->name);
	return 0;
}
EXPORT_SYMBOL_GPL(s5p_mipi_dsi_register_lcd_driver);

void s5p_mipi_dsi_unregister_lcd_driver(struct mipi_dsim_lcd_driver *_driver)
{
	mutex_lock(&dsim_driver_lock);
	list_del(&_driver->list);
	mutex_unlock(&dsim_driver_lock);
}
EXPORT_SYMBOL_GPL(s5p_mipi_dsi_unregister_lcd_driver);

int s5p_mipi_dsi_bind_lcd_ddi(struct mipi_dsim_device *dsim)
{
	struct mipi_dsim_ddi *dsim_ddi;

	mutex_lock(&dsim_ddi_lock);

	list_for_each_entry(dsim_ddi, &dsim_ddi_list, list) {
		if(dsim_ddi->bus_num >= 0
				&& dsim_ddi->bus_num != dsim->id)
			continue;

		if(dsim_ddi->master)
			continue;

		dsim_ddi->master = dsim;
		dsim_ddi->driver = s5p_mipi_dsi_find_driver(dsim_ddi->device->name);
		if(dsim_ddi->driver)
		{
			dsim->dsim_lcd_drv = dsim_ddi->driver;
			dsim->dsim_lcd_dev = dsim_ddi->device;
			list_del(&dsim_ddi->list);
			kfree(dsim_ddi);
			break;
		}
	}

	mutex_unlock(&dsim_ddi_lock);

	if(dsim->dsim_lcd_dev)
		return s5p_mipi_dsi_registration_done(dsim);

	dsim_ddi = kzalloc(sizeof(struct mipi_dsim_ddi), GFP_KERNEL);
	if (!dsim_ddi) {
		printk(KERN_ERR "failed to allocate dsim_ddi object.\n");
		return -EFAULT;
	}

	dsim_ddi->bus_num = dsim->id;
	dsim_ddi->device = NULL;
	dsim_ddi->driver = NULL;
	dsim_ddi->master = dsim;

	mutex_lock(&dsim_ddi_lock);
	list_add_tail(&dsim_ddi->list, &dsim_ddi_list);
	mutex_unlock(&dsim_ddi_lock);
	return 0;
}

/* define MIPI-DSI Master operations. */
static struct mipi_dsim_master_ops master_ops = {
	.cmd_write			= s5p_mipi_dsi_wr_data,
	.cmd_read			= s5p_mipi_dsi_rd_data,
	.get_dsim_frame_done		= s5p_mipi_dsi_get_frame_done_status,
	.clear_dsim_frame_done		= s5p_mipi_dsi_clear_frame_done,
};

static int s5p_mipi_dsi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mipi_dsim_device *dsim;
	struct mipi_dsim_config *dsim_config;
	struct s5p_platform_mipi_dsim *dsim_pd;
	int ret = -1;

	dsim = kzalloc(sizeof(struct mipi_dsim_device), GFP_KERNEL);
	if (!dsim) {
		dev_err(&pdev->dev, "failed to allocate dsim object.\n");
		return -EFAULT;
	}

	printk("%s\n", __func__);

	dsim->pd = to_dsim_plat(pdev);
	dsim->dev = &pdev->dev;
	dsim->id = pdev->id;
	dsim->resume_complete = 0;
	init_completion(&dsim->rx_completion);

	/* get s5p_platform_mipi_dsim. */
	dsim_pd = (struct s5p_platform_mipi_dsim *)dsim->pd;
	if (dsim_pd == NULL) {
		dev_err(&pdev->dev, "failed to get platform data for dsim.\n");
		return -EFAULT;
	}
	/* get mipi_dsim_config. */
	dsim_config = dsim_pd->dsim_config;
	if (dsim_config == NULL) {
		dev_err(&pdev->dev, "failed to get dsim config data.\n");
		return -EFAULT;
	}

	dsim->dsim_config = dsim_config;
	dsim->master_ops = &master_ops;

	dsim->clock = clk_get(&pdev->dev, "dsim");
	if (IS_ERR(dsim->clock)) {
		dev_err(&pdev->dev, "failed to get dsim clock source\n");
		goto err_clock_get;
	}

	clk_enable(dsim->clock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get io memory region\n");
		ret = -EINVAL;
		goto err_platform_get;
	}

	res = request_mem_region(res->start, resource_size(res),
					dev_name(&pdev->dev));
	if (!res) {
		dev_err(&pdev->dev, "failed to request io memory region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	dsim->res = res;

	dsim->reg_base = ioremap(res->start, resource_size(res));
	if (!dsim->reg_base) {
		dev_err(&pdev->dev, "failed to remap io region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	/*
	 * in case of MIPI Video mode,
	 * frame done interrupt handler would be used.
	 */
	if (dsim_config->e_interface == DSIM_VIDEO) {
		dsim->irq = platform_get_irq(pdev, 0);
		if (request_irq(dsim->irq, s5p_mipi_dsi_interrupt_handler,
				IRQF_DISABLED, "mipi-dsi", dsim)) {
			dev_err(&pdev->dev, "request_irq failed.\n");
			goto err_request_irq;
		}
	}

	mutex_init(&dsim->lock);

	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(dsim, 1);
	else {
		dev_err(&pdev->dev, "mipi_power is NULL.\n");
		goto err_request_irq;
	}

	platform_set_drvdata(pdev, dsim);

	/* bind lcd ddi matched with panel name. */
	ret = s5p_mipi_dsi_bind_lcd_ddi(dsim);
	if(ret < 0)
	{
		dev_err(&pdev->dev, "failed to register master.\n");
		goto err_bind;
	}

	dev_info(&pdev->dev, "mipi dsim master registered.\n");
	return 0;

err_bind:
	dsim->pd->mipi_power(dsim, 0);

err_request_irq:
	release_resource(dsim->res);
	kfree(dsim->res);

	iounmap((void __iomem *) dsim->reg_base);

err_mem_region:
err_platform_get:
	clk_disable(dsim->clock);

err_clock_get:
	clk_put(dsim->clock);
	kfree(dsim);

	return ret;

}

static int __devexit s5p_mipi_dsi_remove(struct platform_device *pdev)
{
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);
	struct mipi_dsim_ddi *dsim_ddi = NULL;

	if (dsim->dsim_config->e_interface == DSIM_VIDEO)
		free_irq(dsim->irq, dsim);

	iounmap(dsim->reg_base);

	clk_disable(dsim->clock);
	clk_put(dsim->clock);

	release_resource(dsim->res);
	kfree(dsim->res);

	list_for_each_entry(dsim_ddi, &dsim_ddi_list, list) {
		if (dsim->id == dsim_ddi->bus_num) {
			list_del(&dsim_ddi->list);
			kfree(dsim_ddi);
			dsim_ddi = NULL;
			break;
		}
	}

	kfree(dsim);

	return 0;
}

#ifdef CONFIG_PM
static int s5p_mipi_dsi_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	dsim->resume_complete = 0;

	if (master_to_driver(dsim) && (master_to_driver(dsim))->suspend)
		(master_to_driver(dsim))->suspend(master_to_device(dsim), state);

	clk_disable(dsim->clock);

	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(pdev, 0);

	return 0;
}

static int s5p_mipi_dsi_resume(struct platform_device *pdev)
{
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);

	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(pdev, 1);

	clk_enable(dsim->clock);

	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);

	s5p_mipi_dsi_set_hs_enable(dsim);

	/* change cpu command transfer mode to hs. */
	s5p_mipi_dsi_set_data_transfer_mode(dsim, 0);

	if (master_to_driver(dsim) && (master_to_driver(dsim))->resume)
		(master_to_driver(dsim))->resume(master_to_device(dsim));

	s5p_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);

	/* change lcdc data transfer mode to hs. */
	s5p_mipi_dsi_set_data_transfer_mode(dsim, 1);

	dsim->resume_complete = 1;

	return 0;
}
#else
#define s5p_mipi_dsi_suspend NULL
#define s5p_mipi_dsi_resume NULL
#endif

static struct platform_driver s5p_mipi_dsi_driver = {
	.probe = s5p_mipi_dsi_probe,
	.remove = __devexit_p(s5p_mipi_dsi_remove),
	.suspend = s5p_mipi_dsi_suspend,
	.resume = s5p_mipi_dsi_resume,
	.driver = {
		   .name = "s5p-mipi-dsim",
		   .owner = THIS_MODULE,
	},
};

static int s5p_mipi_dsi_register(void)
{
	platform_driver_register(&s5p_mipi_dsi_driver);

	return 0;
}

static void s5p_mipi_dsi_unregister(void)
{
	platform_driver_unregister(&s5p_mipi_dsi_driver);
}

module_init(s5p_mipi_dsi_register);
module_exit(s5p_mipi_dsi_unregister);

MODULE_AUTHOR("InKi Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("Samusung SoC MIPI-DSI driver");
MODULE_LICENSE("GPL");
