/* linux/drivers/spi/spi_s5l89xx.c
 *
 * Copyright (c) 2011 Richard Ian Taylor
 * Copyright (c) 2006 Ben Dooks
 * Copyright 2006-2009 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>

#include <mach/regs-spi.h>
#include <mach/spi.h>

/**
 * s5l89xx_spi_devstate - per device data
 * @hz: Last frequency calculated for @sppre field.
 * @mode: Last mode setting for the @spcon field.
 * @spcon: Value to write to the SPCON register.
 * @sppre: Value to write to the SPPRE register.
 */
struct s5l89xx_spi_devstate {
	unsigned int	hz;
	unsigned int	mode;
	u8		word_size;
	u8		word_shift;
	u32		spcon;
	u32		sppre;
};

struct s5l89xx_spi {
	/* bitbang has to be first */
	struct spi_bitbang	 bitbang;
	struct completion	 done;

	void __iomem		*regs;
	int			 irq;
	int			 len;
	int			 rx_done;
	int			 tx_done;

	void			(*set_cs)(struct s5l_spi_info *spi,
					  int cs, int pol);

	/* data buffers */
	const unsigned char	*tx;
	unsigned char		*rx;
	u8 word_shift;

	struct clk		*clk;
	struct resource		*ioarea;
	struct spi_master	*master;
	struct spi_device	*curdev;
	struct device		*dev;
	struct s5l_spi_info *pdata;
};


#define SPCON_DEFAULT (S3C2410_SPCON_MSTR | S3C2410_SPCON_SMOD_INT)
#define SPPIN_DEFAULT (S3C2400_SPPIN_nCS)

static inline struct s5l89xx_spi *to_hw(struct spi_device *sdev)
{
	return spi_master_get_devdata(sdev->master);
}

static void s5l89xx_spi_gpiocs(struct s5l_spi_info *spi, int cs, int pol)
{
	gpio_set_value(spi->pin_cs, pol);
}

static void s5l89xx_spi_chipsel(struct spi_device *spi, int value)
{
	struct s5l89xx_spi_devstate *cs = spi->controller_state;
	struct s5l89xx_spi *hw = to_hw(spi);
	unsigned int cspol = spi->mode & SPI_CS_HIGH ? 1 : 0;

	/* change the chipselect state and the state of the spi engine clock */

	switch (value) {
	case BITBANG_CS_INACTIVE:
		hw->set_cs(hw->pdata, spi->chip_select, cspol^1);
		writel(cs->spcon, hw->regs + S3C2410_SPCON);
		break;

	case BITBANG_CS_ACTIVE:
		writel(cs->spcon | S3C2410_SPCON_ENSCK,
		       hw->regs + S3C2410_SPCON);
		hw->set_cs(hw->pdata, spi->chip_select, cspol);
		break;
	}
}

static int s5l89xx_spi_update_state(struct spi_device *spi,
				    struct spi_transfer *t)
{
	struct s5l89xx_spi *hw = to_hw(spi);
	struct s5l89xx_spi_devstate *cs = spi->controller_state;
	unsigned int bpw;
	unsigned int hz;
	unsigned int div;
	unsigned long clk;
	u32 spcon = SPCON_DEFAULT | S3C2410_SPCON_ENSCK;

	bpw = t ? t->bits_per_word : spi->bits_per_word;
	hz  = t ? t->speed_hz : spi->max_speed_hz;

	if (!bpw)
		bpw = 8;

	if (!hz)
		hz = spi->max_speed_hz;

	writel(0, hw->regs + S5L_SPCTL);

	switch(bpw)
	{
	case 8:
		spcon |= S5L89XX_SPCON_1BYTE;
		cs->word_size = 8;
		cs->word_shift = 0;
		break;

	case 16:
		spcon |= S5L89XX_SPCON_2BYTE;
		cs->word_size = 16;
		cs->word_shift = 1;
		break;

	case 32:
		spcon |= S5L89XX_SPCON_4BYTE;
		cs->word_size = 32;
		cs->word_shift = 2;
		break;

	default:
		dev_err(&spi->dev, "invalid bits-per-word (%d)\n", bpw);
		return -EINVAL;
	};

	if(spi->mode & SPI_CPHA)
		spcon |= S3C2410_SPCON_CPHA_FMTB;

	if(spi->mode & SPI_CPOL)
		spcon |= S3C2410_SPCON_CPOL_HIGH;

	cs->mode = spi->mode;
	cs->spcon = spcon;

	if (cs->hz != hz) {
		clk = clk_get_rate(hw->clk);
		div = DIV_ROUND_UP(clk, hz * 2) - 1;

		if (div > 255)
			div = 255;

		dev_dbg(&spi->dev, "pre-scaler=%d (wanted %d, got %ld)\n",
			div, hz, clk / (2 * (div + 1)));

		cs->hz = hz;
		cs->sppre = div;
	}

	return 0;
}

static int s5l89xx_spi_setupxfer(struct spi_device *spi,
				 struct spi_transfer *t)
{
	struct s5l89xx_spi_devstate *cs = spi->controller_state;
	struct s5l89xx_spi *hw = to_hw(spi);
	int ret;

	ret = s5l89xx_spi_update_state(spi, t);
	if (!ret)
		writel(cs->sppre, hw->regs + S3C2410_SPPRE);

	return ret;
}

static int s5l89xx_spi_setup(struct spi_device *spi)
{
	struct s5l89xx_spi_devstate *cs = spi->controller_state;
	struct s5l89xx_spi *hw = to_hw(spi);
	int ret;

	/* allocate settings on the first call */
	if (!cs) {
		cs = kzalloc(sizeof(struct s5l89xx_spi_devstate), GFP_KERNEL);
		if (!cs) {
			dev_err(&spi->dev, "no memory for controller state\n");
			return -ENOMEM;
		}

		cs->spcon = SPCON_DEFAULT;
		cs->hz = -1;
		spi->controller_state = cs;
	}

	/* initialise the state from the device */
	ret = s5l89xx_spi_update_state(spi, NULL);
	if (ret)
		return ret;

	spin_lock(&hw->bitbang.lock);
	if (!hw->bitbang.busy) {
		hw->bitbang.chipselect(spi, BITBANG_CS_INACTIVE);
		/* need to ndelay for 0.5 clocktick ? */
	}
	spin_unlock(&hw->bitbang.lock);

	return 0;
}

static void s5l89xx_spi_cleanup(struct spi_device *spi)
{
	kfree(spi->controller_state);
}

static inline void hw_tx(struct s5l89xx_spi *hw, u32 spsta)
{
	int i;
	struct s5l89xx_spi_devstate *cs = hw->curdev->controller_state;
	uint32_t from = hw->tx_done;
	uint32_t count = S5L_SPI_FIFO_SIZE
		- S5L_SPSTA_TXFIFOCNT(spsta);
	uint32_t left = hw->len - hw->tx_done;
	if(count > left)
		count = left;

	hw->tx_done += count;
	switch(cs->word_shift)
	{
	case 0:
		{
			u8 *buf = (u8*)hw->tx;
			for(i = 0; i < count; i++)
				writeb(buf[from+i], hw->regs + S3C2410_SPTDAT);
		}
		break;

	case 1:
		{
			u16 *buf = (u16*)hw->tx;
			for(i = 0; i < count; i++)
				writew(buf[from+i], hw->regs + S3C2410_SPTDAT);
		}
		break;

	case 2:
		{
			u32 *buf = (u32*)hw->tx;
			for(i = 0; i < count; i++)
				writel(buf[from+i], hw->regs + S3C2410_SPTDAT);
		}
		break;
	};
}

static inline void hw_rx(struct s5l89xx_spi *hw, u32 spsta)
{
	int i;
	struct s5l89xx_spi_devstate *cs = hw->curdev->controller_state;
	uint32_t from = hw->rx_done;
	uint32_t count = S5L_SPSTA_RXFIFOCNT(spsta);
	uint32_t left = hw->len - hw->rx_done;
	if(count > left)
		count = left;

	hw->rx_done += count;
	switch(cs->word_shift)
	{
	case 0:
		{
			u8 *buf = (u8*)hw->rx;
			for(i = 0; i < count; i++)
				buf[from+i] = readb(hw->regs + S3C2410_SPRDAT);
		}
		break;

	case 1:
		{
			u16 *buf = (u16*)hw->rx;
			for(i = 0; i < count; i++)
				buf[from+i] = readw(hw->regs + S3C2410_SPRDAT);
		}
		break;

	case 2:
		{
			u32 *buf = (u32*)hw->rx;
			for(i = 0; i < count; i++)
				buf[from+i] = readl(hw->regs + S3C2410_SPRDAT);
		}
		break;
	};
}

static int s5l89xx_spi_txrx(struct spi_device *spi, struct spi_transfer *t)
{
	struct s5l89xx_spi *hw = to_hw(spi);
	struct s5l89xx_spi_devstate *cs = spi->controller_state;

	hw->tx = t->tx_buf;
	hw->rx = t->rx_buf;
	hw->len = t->len >> cs->word_shift;
	hw->tx_done = 0;
	hw->rx_done = 0;
	hw->curdev = spi;

	BUG_ON(hw->len == 0);

	// TODO: Should I round up len for word_size > 8? -- Ricky26

	init_completion(&hw->done);

	writel((1 << 2) | (1 << 3)
			| readl(hw->regs + S5L_SPCTL),
			hw->regs + S5L_SPCTL);

	cs->spcon |= S5L_SPCON_FIFO_EN;

	if(hw->tx)
	{
		u32 spsta = readl(hw->regs + S3C2410_SPSTA);

		cs->spcon |= S5L_SPCON_TXFIFO_EN;
		writel(hw->len, hw->regs + S5L_SPTXC);
		hw_tx(hw, spsta);
	}
	else
		writel(0, hw->regs + S5L_SPTXC);

	if(hw->rx)
	{
		cs->spcon |= S5L_SPCON_RXFIFO_EN;
		writel(hw->len, hw->regs + S5L_SPRXC);
	}
	else
		writel(0, hw->regs + S5L_SPRXC);

	if(hw->rx && !hw->tx)
		cs->spcon |= S3C2410_SPCON_TAGD;

	writel(cs->spcon, hw->regs + S3C2410_SPCON);
	writel(1, hw->regs + S5L_SPCTL);

	wait_for_completion(&hw->done);
	return hw->tx_done;
}

static irqreturn_t s5l89xx_spi_irq(int irq, void *dev)
{
	struct s5l89xx_spi *hw = dev;
	struct s5l89xx_spi_devstate *cs = hw->curdev->controller_state;
	unsigned int spsta = readl(hw->regs + S3C2410_SPSTA);

	if(spsta & S3C2410_SPSTA_DCOL)
	{
		dev_dbg(hw->dev, "data-collision\n");
		complete(&hw->done);
		goto irq_done;
	}

	if(spsta & S5L_SPSTA_TXREADY)
	{
		// Ready for TX
		hw_tx(hw, spsta);
		if(hw->tx_done >= hw->len)
		{
			cs->spcon &=~ S5L_SPCON_TXFIFO_EN;
			writel(cs->spcon, hw->regs + S3C2410_SPCON);
		}
	}

	if(spsta & S3C2410_SPSTA_READY)
	{
		// Ready for RX
		hw_rx(hw, spsta);
		if(hw->rx_done >= hw->len)
		{
			cs->spcon &=~ S5L_SPCON_RXFIFO_EN;
			writel(cs->spcon, hw->regs + S3C2410_SPCON);
		}
	}

	if(hw->tx_done >= hw->len
			&& hw->rx_done >= hw->len)
	{
		cs->spcon &=~ S5L_SPCON_RXFIFO_EN
			| S5L_SPCON_TXFIFO_EN
			| S5L_SPCON_FIFO_EN;
		writel(cs->spcon, hw->regs + S3C2410_SPCON);

		complete(&hw->done);
	}

	writel(spsta, hw->regs + S3C2410_SPSTA);

 irq_done:
	return IRQ_HANDLED;
}

static void s5l89xx_spi_initialsetup(struct s5l89xx_spi *hw)
{
	/* for the moment, permanently enable the clock */

	clk_enable(hw->clk);
	writel(0, hw->regs + S5L_SPCTL);

	/* program defaults into the registers */

	writel(0xff, hw->regs + S3C2410_SPPRE);
	writel(SPPIN_DEFAULT, hw->regs + S3C2410_SPPIN);
	writel(SPCON_DEFAULT, hw->regs + S3C2410_SPCON);

	if (hw->pdata) {
		if (hw->set_cs == s5l89xx_spi_gpiocs)
			gpio_direction_output(hw->pdata->pin_cs, 1);

		if (hw->pdata->gpio_setup)
			hw->pdata->gpio_setup(hw->pdata, 1);
	}
}

static int __init s5l89xx_spi_probe(struct platform_device *pdev)
{
	struct s5l_spi_info *pdata;
	struct s5l89xx_spi *hw;
	struct spi_master *master;
	struct resource *res;
	int err = 0;

	master = spi_alloc_master(&pdev->dev, sizeof(struct s5l89xx_spi));
	if (master == NULL) {
		dev_err(&pdev->dev, "No memory for spi_master\n");
		err = -ENOMEM;
		goto err_nomem;
	}

	hw = spi_master_get_devdata(master);
	memset(hw, 0, sizeof(struct s5l89xx_spi));

	hw->master = spi_master_get(master);
	hw->pdata = pdata = pdev->dev.platform_data;
	hw->dev = &pdev->dev;

	if (pdata == NULL) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		err = -ENOENT;
		goto err_no_pdata;
	}

	platform_set_drvdata(pdev, hw);
	init_completion(&hw->done);

	/* setup the master state. */

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;

	master->num_chipselect = hw->pdata->num_cs;
	master->bus_num = pdata->bus_num;

	/* setup the state for the bitbang driver */

	hw->bitbang.master         = hw->master;
	hw->bitbang.setup_transfer = s5l89xx_spi_setupxfer;
	hw->bitbang.chipselect     = s5l89xx_spi_chipsel;
	hw->bitbang.txrx_bufs      = s5l89xx_spi_txrx;

	hw->master->setup  = s5l89xx_spi_setup;
	hw->master->cleanup = s5l89xx_spi_cleanup;

	dev_dbg(hw->dev, "bitbang at %p\n", &hw->bitbang);

	/* find and map our resources */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "Cannot get IORESOURCE_MEM\n");
		err = -ENOENT;
		goto err_no_iores;
	}

	hw->ioarea = request_mem_region(res->start, resource_size(res),
					pdev->name);

	if (hw->ioarea == NULL) {
		dev_err(&pdev->dev, "Cannot reserve region\n");
		err = -ENXIO;
		goto err_no_iores;
	}

	hw->regs = ioremap(res->start, resource_size(res));
	if (hw->regs == NULL) {
		dev_err(&pdev->dev, "Cannot map IO\n");
		err = -ENXIO;
		goto err_no_iomap;
	}

	hw->irq = platform_get_irq(pdev, 0);
	if (hw->irq < 0) {
		dev_err(&pdev->dev, "No IRQ specified\n");
		err = -ENOENT;
		goto err_no_irq;
	}

	err = request_irq(hw->irq, s5l89xx_spi_irq, 0, pdev->name, hw);
	if (err) {
		dev_err(&pdev->dev, "Cannot claim IRQ\n");
		goto err_no_irq;
	}

	hw->clk = clk_get(&pdev->dev, "spi");
	if (IS_ERR(hw->clk)) {
		dev_err(&pdev->dev, "No clock for device\n");
		err = PTR_ERR(hw->clk);
		goto err_no_clk;
	}

	/* setup any gpio we can */

	if (!pdata->set_cs) {
		if (pdata->pin_cs < 0) {
			dev_err(&pdev->dev, "No chipselect pin\n");
			goto err_register;
		}

		err = gpio_request(pdata->pin_cs, dev_name(&pdev->dev));
		if (err) {
			dev_err(&pdev->dev, "Failed to get gpio for cs\n");
			goto err_register;
		}

		hw->set_cs = s5l89xx_spi_gpiocs;
		gpio_direction_output(pdata->pin_cs, 1);
	} else
		hw->set_cs = pdata->set_cs;

	s5l89xx_spi_initialsetup(hw);

	/* register our spi controller */

	err = spi_bitbang_start(&hw->bitbang);
	if (err) {
		dev_err(&pdev->dev, "Failed to register SPI master\n");
		goto err_register;
	}

	return 0;

 err_register:
	if (hw->set_cs == s5l89xx_spi_gpiocs)
		gpio_free(pdata->pin_cs);

	clk_disable(hw->clk);
	clk_put(hw->clk);

 err_no_clk:
	free_irq(hw->irq, hw);

 err_no_irq:
	iounmap(hw->regs);

 err_no_iomap:
	release_resource(hw->ioarea);
	kfree(hw->ioarea);

 err_no_iores:
 err_no_pdata:
	spi_master_put(hw->master);

 err_nomem:
	return err;
}

static int __exit s5l89xx_spi_remove(struct platform_device *dev)
{
	struct s5l89xx_spi *hw = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	spi_unregister_master(hw->master);

	clk_disable(hw->clk);
	clk_put(hw->clk);

	free_irq(hw->irq, hw);
	iounmap(hw->regs);

	if (hw->set_cs == s5l89xx_spi_gpiocs)
		gpio_free(hw->pdata->pin_cs);

	release_resource(hw->ioarea);
	kfree(hw->ioarea);

	spi_master_put(hw->master);
	return 0;
}


#ifdef CONFIG_PM

static int s5l89xx_spi_suspend(struct device *dev)
{
	struct s5l89xx_spi *hw = platform_get_drvdata(to_platform_device(dev));

	if (hw->pdata && hw->pdata->gpio_setup)
		hw->pdata->gpio_setup(hw->pdata, 0);

	clk_disable(hw->clk);
	return 0;
}

static int s5l89xx_spi_resume(struct device *dev)
{
	struct s5l89xx_spi *hw = platform_get_drvdata(to_platform_device(dev));

	s5l89xx_spi_initialsetup(hw);
	return 0;
}

static const struct dev_pm_ops s5l89xx_spi_pmops = {
	.suspend	= s5l89xx_spi_suspend,
	.resume		= s5l89xx_spi_resume,
};

#define S5L89XX_SPI_PMOPS &s5l89xx_spi_pmops
#else
#define S5L89XX_SPI_PMOPS NULL
#endif /* CONFIG_PM */

MODULE_ALIAS("platform:s5l89xx-spi");
static struct platform_driver s5l89xx_spi_driver = {
	.remove		= __exit_p(s5l89xx_spi_remove),
	.driver		= {
		.name	= "s5l89xx-spi",
		.owner	= THIS_MODULE,
		.pm	= S5L89XX_SPI_PMOPS,
	},
};

static int __init s5l89xx_spi_init(void)
{
        return platform_driver_probe(&s5l89xx_spi_driver, s5l89xx_spi_probe);
}

static void __exit s5l89xx_spi_exit(void)
{
        platform_driver_unregister(&s5l89xx_spi_driver);
}

module_init(s5l89xx_spi_init);
module_exit(s5l89xx_spi_exit);

MODULE_DESCRIPTION("S5L89XX SPI Driver");
MODULE_AUTHOR("Richard Ian Taylor");
MODULE_LICENSE("GPL");
