/*
 * arch/arm/mach-apple_iphone/spi.c - SPI functionality for the iPhone
 *
 * Copyright (C) 2008 Yiduo Wang
 *
 * Portions Copyright (C) 2010 Ricky Taylor
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

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/timer.h>

#include <mach/hardware.h>
#include <mach/iphone-clock.h>
#include <mach/gpio.h>
#include <mach/iphone-spi.h>

#define GET_BITS(x, start, length)	((((u32)(x)) << (32 - ((start) + (length)))) >> (32 - (length)))
#define spi_dbg(m, args...)			dev_dbg(&(m)->spi_dev->dev, args)
#define spi_err(m, args...)			dev_err(&(m)->spi_dev->dev, args)

// Device
#define CHIPID IO_ADDRESS(0x3E500000)

// Registers
#define SPICLOCKTYPE 0x4

// Values
#define GET_SPICLOCKTYPE(x) GET_BITS(x, 24, 4)

typedef enum SPIClockSource {
	PCLK = 0,
	NCLK = 1
} SPIClockSource;

typedef int (*iphone_cs_callback)(struct spi_device *_dev, unsigned int _active);

struct iphone_spi_master {
	struct spi_master *spi_dev;
	uint32_t *io_base;
	iphone_cs_callback cs_callback;

	int irq;
	int clockgate;

	SPIClockSource clock_source;

	struct list_head queue;
	struct spi_message *message;
	struct spi_transfer *transfer;

	const volatile u8* txBuffer;
	volatile int txCurrentLen;
	volatile int txTotalLen;
	volatile u8* rxBuffer;
	volatile int rxCurrentLen;
	volatile int rxTotalLen;
	volatile bool txDone;
	volatile bool rxDone;
	
	spinlock_t lock;
	struct timer_list timeout;
};

static inline struct iphone_spi_master *iphone_spi_master_get(struct spi_master *_master)
{
	return _master ? (struct iphone_spi_master*)spi_master_get_devdata(_master) : NULL;
}

static int iphone_spi_chip_clocktype(void)
{
	return GET_SPICLOCKTYPE(readl(CHIPID + SPICLOCKTYPE));
}

static int iphone_spi_gpio_cs(struct spi_device *_dev, unsigned int _active)
{
	if(_active > 0)
		_active = 1;

	if((_dev->mode & SPI_CS_HIGH) == 0)
		_active = 1-_active;

	dev_dbg(&_dev->dev, "chip-select setting GPIO 0x%04x to %d.\n", (uint32_t)_dev->controller_data, _active);

	iphone_gpio_pin_output((uint32_t)_dev->controller_data, _active);

	return 0;
}

static int iphone_spi_mrdy_cs(struct spi_device *_dev, unsigned int _active)
{
	int mrdy = ((uint32_t)_dev->controller_data) >> 16;
	int srdy = ((uint32_t)_dev->controller_data) & 0xFFFF;

	dev_dbg(&_dev->dev, "setting MRDY GPIO to %d.\n", _active);

	switch(_active)
	{
		case 1: // Rx
			{
				int i = 0;
				while(iphone_gpio_pin_state(srdy) == 0)
				{
					msleep(1);

					i++;

					if(i >= 2000)
					{
						dev_err(&_dev->dev, "SPI device has no data.\n");
						return 0;
					}
				}
			}
			break;

		case 2: // Tx
			{
				int i = 0;
				iphone_gpio_pin_output(mrdy, 1);
				while(iphone_gpio_pin_state(srdy) == 0)
				{
					msleep(1);

					i++;

					if(i >= 2000)
					{
						dev_err(&_dev->dev, "SPI device never became ready.\n");
						iphone_gpio_pin_output(mrdy, 0);
						return -EIO;
					}
				}
			}
			break;

		default: // Disable
			{
				iphone_gpio_pin_output(mrdy, 0);
			}
			break;
	}

	return 0;
}

static int iphone_spi_transfer_start(struct iphone_spi_master *_master, struct spi_transfer *_tx);
static void iphone_spi_transfer_complete(struct iphone_spi_master *_master);
static int iphone_spi_message_start(struct iphone_spi_master *_master, struct spi_message *_msg);
static void iphone_spi_message_complete(struct iphone_spi_master *_master);

static int iphone_spi_transfer_start(struct iphone_spi_master *_master, struct spi_transfer *_transfer)
{
	int cs_mode = (_transfer->tx_buf) ? 2 : 1;
	int speed = (_transfer->speed_hz <= 0) ? _master->message->spi->max_speed_hz : _transfer->speed_hz;
	unsigned long flags;
	
	if(speed <= 0)
		spi_err(_master, "invalid speed %d.\n", speed);

	spi_dbg(_master, "starting transfer %p.\n", _transfer);

	_master->transfer = _transfer;

	if(_master->cs_callback)
		_master->cs_callback(_master->message->spi, cs_mode);
	
	if(_transfer->delay_usecs > 0)
	{
		spi_dbg(_master, "sleeping for %d ms.\n", (_transfer->delay_usecs/1000));
		msleep(_transfer->delay_usecs/1000);
	}
	
	spin_lock_irqsave(&_master->lock, flags);
	{
		u32 clockFrequency = (_master->clock_source == PCLK) ? FREQUENCY_PERIPHERAL : FREQUENCY_FIXED;
		u32 divider;
		u32 options;

		writel(0, IPHONE_SPI_CONTROL(_master->io_base));

		if(iphone_spi_chip_clocktype() != 0)
		{
			divider = clockFrequency / speed;
			if(divider < 2)
				divider = 2;
		}
		else
			divider = clockFrequency / (speed * 2 - 1);

		if(divider > IPHONE_SPI_MAX_DIVIDER)
		{
			spi_err(_master, "divider too high, unable to send (%d).\n", divider);
		}
		else
		{
			struct spi_transfer *tx = _master->transfer;
			uint32_t *io_base = _master->io_base;
			uint32_t *status = IPHONE_SPI_STATUS(io_base);

			//spi_dbg(_master, "clock %d (%d) = %d / %d.\n", (clockFrequency/divider), speed, clockFrequency, divider);
			
			writel(divider, IPHONE_SPI_CLKDIV(io_base));

			options = ((_master->message->spi->mode & 0x3) << 1)
					| (0x3 << 3)	// Master
					| (1 << 5)		// Interrupt Mode
					| (1 << 8)		// Data Ready interrupt
					| (_master->clock_source << IPHONE_SPI_CLOCK_SHIFT);

			if(tx->rx_buf && !tx->tx_buf)
				options |= 1;

			writel(options, IPHONE_SPI_SETUP(io_base));
			writel(0, IPHONE_SPI_PIN(io_base));
			writel(1, IPHONE_SPI_CONTROL(io_base));

			writel(readl(IPHONE_SPI_CONTROL(io_base)) | (1 << 2), IPHONE_SPI_CONTROL(io_base));
			writel(readl(IPHONE_SPI_CONTROL(io_base)) | (1 << 3), IPHONE_SPI_CONTROL(io_base));
			
			//spi_dbg(_master, "starting transfer %p, rx=%p, tx=%p, options=0x%08x, txc=%d, rxc=%d.\n",
			//		_transfer, tx->rx_buf, tx->tx_buf, options, IPHONE_SPI_TX_BUFFER_USED(status), IPHONE_SPI_RX_BUFFER_USED(status));

			if(tx->tx_buf)
			{
				_master->txBuffer = tx->tx_buf;

				if(tx->len > IPHONE_SPI_MAX_TX_BUFFER)
					_master->txCurrentLen = IPHONE_SPI_MAX_TX_BUFFER;
				else
					_master->txCurrentLen = tx->len;

				_master->txTotalLen = tx->len;
				_master->txDone = false;
			}
			else
			{
				_master->txBuffer = NULL;
				_master->txDone = true;
			}

			if(tx->rx_buf)
			{
				_master->rxBuffer = tx->rx_buf;
				_master->rxCurrentLen = 0;
				_master->rxTotalLen = tx->len;
				_master->rxDone = false;
			}
			else
			{
				_master->rxBuffer = NULL;
				_master->rxDone = true;
			}

			if(tx->rx_buf)
				writel(_master->rxTotalLen, IPHONE_SPI_RXAMT(io_base));
			else
				writel(0, IPHONE_SPI_RXAMT(io_base));

			if(tx->tx_buf)
			{
				int i;
				uint8_t *buff = (uint8_t*)tx->tx_buf;
				for(i = 0; i < _master->txCurrentLen; i++)
				{
					writel(buff[i], IPHONE_SPI_TXDATA(io_base));
				}
			}

			// Go!
			writel(1, IPHONE_SPI_CONTROL(io_base));

			spi_dbg(_master, "transfer set.\n");

			mod_timer(&_master->timeout, jiffies + msecs_to_jiffies(IPHONE_SPI_TIMEOUT_MSECS));
		}
	}
	spin_unlock_irqrestore(&_master->lock, flags);

	return 0;
}

static void iphone_spi_transfer_timeout(unsigned long _data)
{
	struct iphone_spi_master *master = (struct iphone_spi_master*)_data;

	if(!master->message)
	{
		spi_err(master, "timeout called when not processing a message.");
		return;
	}

	spi_err(master, "timeout.\n");

	master->message->status = -EIO;

	iphone_spi_transfer_complete(master);
}

static void iphone_spi_transfer_complete(struct iphone_spi_master *_master)
{
	unsigned long flags;

	spi_dbg(_master, "completed transfer %p.\n", _master->transfer);
	udelay(50);

	del_timer(&_master->timeout);

	spin_lock_irqsave(&_master->lock, flags);

	spi_dbg(_master, "turning off IRQ.\n");
	writel(0, IPHONE_SPI_CONTROL(_master->io_base));

	if(_master->cs_callback)
		_master->cs_callback(_master->message->spi, 0);

	spin_unlock_irqrestore(&_master->lock, flags);

	if(_master->message->status == 0 && _master->transfer->transfer_list.next != &_master->message->transfers) // If this isn't the last transfer
	{
		int ret = iphone_spi_transfer_start(_master, container_of(_master->transfer->transfer_list.next, struct spi_transfer, transfer_list));
		if(ret < 0)
			spi_err(_master, "failed to start next transfer (%p).\n", _master->transfer->transfer_list.next);
	}
	else
	{
		_master->transfer = NULL;
		iphone_spi_message_complete(_master);
	}
}

static int iphone_spi_message_start(struct iphone_spi_master *_master, struct spi_message *_msg)
{
	_master->message = _msg;
	_msg->status = 0;

	spi_dbg(_master, "starting message %p.\n", _msg);

	if(list_empty(&_msg->transfers))
	{
		iphone_spi_message_complete(_master);
		return 0;
	}

	return iphone_spi_transfer_start(_master, list_first_entry(&_msg->transfers, struct spi_transfer, transfer_list));
}

static void iphone_spi_message_complete(struct iphone_spi_master *_master)
{
	spi_dbg(_master, "completed message %p with result %d.\n", _master->message, _master->message->status);

	list_del(&_master->message->queue);

	if(_master->message->complete)
		_master->message->complete(_master->message->context);

	if(list_empty(&_master->queue))
	{
		_master->message = NULL;
		return;
	}

	iphone_spi_message_start(_master, list_first_entry(&_master->queue, struct spi_message, queue));
}

static int iphone_spi_transfer(struct spi_device *_dev, struct spi_message *_msg)
{
	struct iphone_spi_master *master = iphone_spi_master_get(_dev->master);
	int start_now = list_empty(&master->queue);

	spi_dbg(master, "msg (%p) added to queue.\n", _msg);

	INIT_LIST_HEAD(&_msg->queue);
	list_add_tail(&master->queue, &_msg->queue);

	if(start_now)
		return iphone_spi_message_start(master, _msg);

	return 0;
}

static int iphone_spi_setup(struct spi_device *_dev)
{
	struct iphone_spi_master *master = iphone_spi_master_get(_dev->master);
	
	spi_dbg(master, "setup %s.\n", dev_name(&_dev->dev));

	return 0;
}

static void iphone_spi_cleanup(struct spi_device *_dev)
{
	struct iphone_spi_master *master = iphone_spi_master_get(_dev->master);

	spi_dbg(master, "cleanup %s.\n", dev_name(&_dev->dev));
}

static irqreturn_t iphone_spi_irq(int _irq, void* _tkn)
{
	struct iphone_spi_master *master = (struct iphone_spi_master *)_tkn;
	uint32_t *io_base = master->io_base;
	uint32_t status;
	unsigned long flags;
	int i;
	spi_dbg(master, "irq fired.\n");

	spin_lock_irqsave(&master->lock, flags);
	
	status = readl(IPHONE_SPI_STATUS(io_base));
	writel(status, IPHONE_SPI_STATUS(io_base));

	while(status & 3)
	{
		// take care of tx
		if(master->txBuffer != NULL)
		{
			int toTX = master->txTotalLen - master->txCurrentLen;
			int canTX = IPHONE_SPI_MAX_TX_BUFFER - IPHONE_SPI_TX_BUFFER_USED(status);

			if(toTX > canTX)
				toTX = canTX;

			if(toTX > 0)
			{
				for(i = 0; i < toTX; i++)
					writel(master->txBuffer[master->txCurrentLen + i], IPHONE_SPI_TXDATA(io_base));

				master->txCurrentLen += toTX;
			}
			
			if(master->txCurrentLen >= master->txTotalLen)
			{
				master->txDone = true;
				master->txBuffer = NULL;

				if(!master->rxBuffer)
					iphone_spi_transfer_complete(master);
			}
		}

		// take care of rx
		if(master->rxBuffer != NULL)
		{
			int toRX = master->rxTotalLen - master->rxCurrentLen;
			int canRX = IPHONE_SPI_RX_BUFFER_USED(status);

			if(toRX > canRX)
				toRX = canRX;

			if(toRX > 0)
			{
				for(i = 0; i < toRX; i++)
					master->rxBuffer[master->rxCurrentLen + i] = readl(IPHONE_SPI_RXDATA(io_base));

				master->rxCurrentLen += toRX;
			}

			if(master->rxCurrentLen >= master->rxTotalLen)
			{
				master->rxDone = true;
				master->rxBuffer = NULL;

				iphone_spi_transfer_complete(master);
			}
		}
	
		status = readl(IPHONE_SPI_STATUS(io_base));
		writel(status, IPHONE_SPI_STATUS(io_base));
	}
	
	spin_unlock_irqrestore(&master->lock, flags);

	spi_dbg(master, "irq exit.\n");

	return IRQ_HANDLED;
}

static struct iphone_spi_master iphone_spi0 = {
	.io_base = IPHONE_SPI0_REGBASE,
	.cs_callback = &iphone_spi_gpio_cs,
	.clock_source = NCLK,
	.irq = IPHONE_SPI0_IRQ,
	.clockgate = IPHONE_SPI0_CLOCKGATE,
};

static struct iphone_spi_master iphone_spi1 = {
	.io_base = IPHONE_SPI1_REGBASE,
	.cs_callback = &iphone_spi_gpio_cs,
	.clock_source = NCLK,
	.irq = IPHONE_SPI1_IRQ,
	.clockgate = IPHONE_SPI1_CLOCKGATE,
};

static struct iphone_spi_master iphone_spi2 = {
	.io_base = IPHONE_SPI2_REGBASE,
#ifdef CONFIG_IPHONE_3G
	.cs_callback = &iphone_spi_mrdy_cs,
#else
	.cs_callback = &iphone_spi_gpio_cs,
#endif
	.clock_source = NCLK,
	.irq = IPHONE_SPI2_IRQ,
	.clockgate = IPHONE_SPI2_CLOCKGATE,
};

struct iphone_spi_master *iphone_spi_masters[] = {
	&iphone_spi0,
	&iphone_spi1,
	&iphone_spi2,
};

static int __init iphone_spi_probe(struct platform_device *_pdev)
{
	int i;
	int ret = 0;

	for(i = 0; i < ARRAY_SIZE(iphone_spi_masters); i++)
	{
		struct iphone_spi_master *master = iphone_spi_masters[i];
		struct spi_master *spi_master = spi_alloc_master(&_pdev->dev, 0);
		spi_master_set_devdata(spi_master, master);
		master->spi_dev = spi_master;

		spi_master->bus_num = i;
		spi_master->num_chipselect = 64;
		spi_master->dma_alignment = 0;
		spi_master->mode_bits = SPI_CPOL | SPI_CPHA;
		spi_master->flags = 0;
		
		spi_master->setup = &iphone_spi_setup;
		spi_master->cleanup = &iphone_spi_cleanup;
		spi_master->transfer = &iphone_spi_transfer;

		INIT_LIST_HEAD(&master->queue);
		spin_lock_init(&master->lock);
		setup_timer(&master->timeout, iphone_spi_transfer_timeout, (unsigned long)master);
		writel(0, IPHONE_SPI_CONTROL(master->io_base));

		ret = request_irq(master->irq, iphone_spi_irq, IRQF_DISABLED, dev_name(&_pdev->dev), master);
		if(ret)
		{
			spi_err(master, "failed to register SPI IRQ %d.\n", master->irq);
		}
		else
		{
			if(master->clockgate)
				iphone_clock_gate_switch(master->clockgate, 1);

			ret = spi_register_master(master->spi_dev);
			if(ret)
				spi_err(master, "failed to register SPI master.\n");
		}
	}

	return ret;
}

static int __init iphone_spi_remove(struct platform_device *_pdev)
{
	int i;
	for(i = 0; i < ARRAY_SIZE(iphone_spi_masters); i++)
	{
		struct iphone_spi_master *master = iphone_spi_masters[i];

		del_timer(&master->timeout);

		writel(0, IPHONE_SPI_CONTROL(master->io_base));
		
		if(master->clockgate)
			iphone_clock_gate_switch(master->clockgate, 0);

		free_irq(master->irq, master);

		if(master->message)
		{
			master->message->status = -EIO;
			if(master->message->complete)
				master->message->complete(master->message->context);
		}

		spi_unregister_master(master->spi_dev);

	}

	return 0;
}

// TODO: Power management --Ricky26
#define iphone_spi_suspend NULL
#define iphone_spi_resume NULL

static struct platform_driver iphone_spi_driver = {
	.probe = iphone_spi_probe,
	.remove = iphone_spi_remove,
	.suspend = iphone_spi_suspend, /* optional but recommended */
	.resume = iphone_spi_resume,   /* optional but recommended */
	.driver = {
		.owner = THIS_MODULE,
		.name = "iphone-spi",
	},
};

static struct platform_device iphone_spi_device = {
	.name = "iphone-spi",
	.id = -1,
};

static int __init iphone_spi_init(void)
{
	int ret;

	ret = platform_driver_register(&iphone_spi_driver);

	if (!ret) {
		ret = platform_device_register(&iphone_spi_device);

		if (ret != 0) {
			platform_driver_unregister(&iphone_spi_driver);
		}
	}

	return ret;
}
module_init(iphone_spi_init);

static void __exit iphone_spi_exit(void)
{
	platform_device_unregister(&iphone_spi_device);
	platform_driver_unregister(&iphone_spi_driver);
}
module_exit(iphone_spi_exit);
