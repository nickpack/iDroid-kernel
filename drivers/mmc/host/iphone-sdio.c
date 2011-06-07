#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <mach/iphone-clock.h>
#include <linux/workqueue.h>
#include <mach/gpio.h>
#include <linux/slab.h>
#include <asm/mach-types.h>

#define SDIO_PA 0x38D00000
#define SDIO_CLOCKGATE 0xB
#define SDIO_INT 0x2A

// These are required on ipt1g
#define SDIO_GPIO_POWER 0x1701
#define SDIO_GPIO_DEVICE_RESET 0x607

#define SDIO_CTRL       0x0
#define SDIO_DCTRL      0x4
#define SDIO_CMD        0x8
#define SDIO_ARGU       0xC
#define SDIO_STATE      0x10
#define SDIO_STAC       0x14
#define SDIO_DSTA       0x18
#define SDIO_FSTA       0x1C
#define SDIO_RESP0      0x20
#define SDIO_RESP1      0x24
#define SDIO_RESP2      0x28
#define SDIO_RESP3      0x2C
#define SDIO_CLKDIV     0x30
#define SDIO_CSR        0x34
#define SDIO_IRQ        0x38
#define SDIO_IRQMASK    0x3C
#define SDIO_BADDR      0x44
#define SDIO_BLKLEN     0x48
#define SDIO_NUMBLK     0x4C
#define SDIO_REMBLK     0x50

#define sdio_set_reg(reg, x) writel((x), sdio->regs + (reg))
#define sdio_get_reg(reg) readl(sdio->regs + (reg))

struct iphone_sdio
{
	struct device*		dev;
	struct resource*	regs_res;
	void __iomem*		regs;
	struct mmc_host*	mmc;
	struct mmc_request*	mrq;
	bool			irq_enabled;
	int			errors;
	spinlock_t		lock;
};

static struct iphone_sdio* sdio_driver;

static void sdio_workqueue_handler(struct work_struct* work);
static void sdio_reset_handler(struct work_struct* work);

DECLARE_WORK(sdio_workqueue, &sdio_workqueue_handler);
DECLARE_DELAYED_WORK(sdio_reset_workqueue, &sdio_reset_handler);
static struct workqueue_struct* sdio_wq;

static inline void sdio_check_for_irq(struct iphone_sdio* sdio)
{
	unsigned long flags;
	bool gotIRQ = false;

	spin_lock_irqsave(&sdio->lock, flags);
	if(sdio->irq_enabled)
	{
		u32 irqstat = sdio_get_reg(SDIO_IRQ);

		//printk("irqstat: %x\n", irqstat);
		if(irqstat & 2)
		{
			sdio_set_reg(SDIO_IRQ, 2);
			gotIRQ = true;
			//printk("got sdio IRQ!\n");
		}
	}
	spin_unlock_irqrestore(&sdio->lock, flags);

	if(gotIRQ)
			mmc_signal_sdio_irq(sdio->mmc);
		}

static irqreturn_t sdio_irq(int irq, void *pw)
{
	struct iphone_sdio* sdio = pw;

	//printk("in sdio irq\n");
	sdio_check_for_irq(sdio);
	//printk("out of sdio irq\n");

	return IRQ_HANDLED;
}

int sdio_wait_for_ready(struct iphone_sdio* sdio, int timeout)
{
	// wait for CMD_STATE to be CMD_IDLE, and DAT_STATE to be DAT_IDLE
	u64 startTime = iphone_microtime();
	while(sdio_get_reg(SDIO_STATE) != 0)
	{
		if(iphone_has_elapsed(startTime, timeout * 1000)) {
			return -ETIMEDOUT;
		}
		yield();
	}

	return 0;

}

static int sdio_wait_for_cmd_ready(struct iphone_sdio* sdio, int timeout)
{
	// wait for the command to be ready to transmit
	u64 startTime = iphone_microtime();
	while((sdio_get_reg(SDIO_DSTA) & 1) == 0)
	{
		if(iphone_has_elapsed(startTime, timeout * 1000)) {
			return -ETIMEDOUT;
		}
		yield();
	}

	return 0;
}

static int sdio_execute_command(struct iphone_sdio* sdio, int timeout)
{
	u32 status;
	u64 startTime;

	// set the execute bit
	sdio_set_reg(SDIO_CMD, sdio_get_reg(SDIO_CMD) | (1 << 31));

	// wait for the command to at least get transmitted
	startTime = iphone_microtime();
	while(((sdio_get_reg(SDIO_DSTA) >> 2) & 1) == 0)
	{
		if(iphone_has_elapsed(startTime, timeout * 1000)) {
			return -ETIMEDOUT;
		}
		yield();
	}

	// wait for the command status to get back to idle
	// the reason command sent status is checked first is to avoid race conditions
	// where the state hasn't yet transitioned from idle
	while(((sdio_get_reg(SDIO_STATE) >> 4) & 7) != 0)
	{
		yield();
		// No check here because device has its own timeout status
	}

	status = sdio_get_reg(SDIO_DSTA);
	return ((u32)(status >> 15));
}


static void sdio_clear_state(struct iphone_sdio* sdio)
{
	// clear current status
	sdio_set_reg(SDIO_STAC, sdio_get_reg(SDIO_DSTA));
}

static int sdio_reset(struct iphone_sdio* sdio)
{
	int ret;

	if(machine_is_ipod_touch_1g())
	{
		iphone_gpio_pin_output(SDIO_GPIO_POWER, 0);
		msleep(5);
		iphone_gpio_pin_output(SDIO_GPIO_POWER, 1);
		msleep(10);

		iphone_gpio_pin_output(SDIO_GPIO_DEVICE_RESET, 1);
		msleep(5);
		iphone_gpio_pin_output(SDIO_GPIO_DEVICE_RESET, 0);
		msleep(10);
	}

	ret = sdio_wait_for_ready(sdio, 100);
	if(ret)
		return ret;

	sdio_clear_state(sdio);

	return 0;
}

static int sdio_init(struct iphone_sdio* sdio)
{
	iphone_clock_gate_switch(SDIO_CLOCKGATE, 1);

	// SDCLK = PCLK/128 ~= 400 KHz
	sdio_set_reg(SDIO_CLKDIV, 1 << 7);

	// Reset FIFO
	sdio_set_reg(SDIO_DCTRL, 0x3);
	sdio_set_reg(SDIO_DCTRL, 0x0);

	// Enable SDIO, with 1-bit card bus.
	sdio_set_reg(SDIO_CTRL, 0x1);

	// Clear status
	sdio_set_reg(SDIO_STAC, 0xFFFFFFFF);

	// Clear IRQ
	sdio_set_reg(SDIO_IRQ, 0xFFFFFFFF);

	if(sdio_reset(sdio) != 0)
	{
		dev_err(sdio->dev, "error resetting card\n");
		return -1;
	}

	// Enable checking for SDIO irqs
	sdio_set_reg(SDIO_CSR, 2);

	// Enable xfer done IRQ
	sdio_set_reg(SDIO_IRQMASK, 0);

	return 0;
}

static inline u32 setup_cmd(struct mmc_command* cmd)
{
	u32 x = cmd->opcode;

	if(cmd->flags & MMC_CMD_AC)
	{
		x |= 2 << 6;
	} else if(cmd->flags & MMC_CMD_ADTC)
	{
		x |= 3 << 6;
		if(likely(cmd->data != NULL))
		{
			if(cmd->data->flags & MMC_DATA_READ)
				x |= 0 << 8;
			else if(cmd->data->flags & MMC_DATA_WRITE)
				x |= 1 << 8;
		}
	} else if(cmd->flags & MMC_CMD_BC)
	{
		x |= 0 << 6;
	} else if(cmd->flags & MMC_CMD_BCR)
	{
		x |= 1 << 6;
	}

	if(mmc_resp_type(cmd) == MMC_RSP_NONE)
		x |= 0 << 16;
	else if(mmc_resp_type(cmd) == MMC_RSP_R1)
		x |= 1 << 16;
	else if(mmc_resp_type(cmd) == MMC_RSP_R1B)
		x |= 1 << 16;
	else if(mmc_resp_type(cmd) == MMC_RSP_R2)
		x |= 2 << 16;
/*	else if(mmc_resp_type(cmd) == MMC_RSP_R3)
		x |= 3 << 16;*/
	else if(mmc_resp_type(cmd) == MMC_RSP_R4)
		x |= 4 << 16;
	else if(mmc_resp_type(cmd) == MMC_RSP_R5)
		x |= 5 << 16;
	else if(mmc_resp_type(cmd) == MMC_RSP_R6)
		x |= 6 << 16;

	if(cmd->flags & MMC_RSP_136)
		x |= 1 << 20;

	return x;
}

static void sdio_reinit(struct iphone_sdio* sdio)
{
	sdio->errors = -1;
	mmc_detect_change(sdio->mmc, 0);
	schedule_delayed_work(&sdio_reset_workqueue, msecs_to_jiffies(500));
}

static void sdio_workqueue_handler(struct work_struct* work)
{
	int ret;
	struct iphone_sdio* sdio = sdio_driver;
	struct mmc_request* mrq = sdio->mrq;
	struct mmc_command* cmd;
	int status;

	if(unlikely(sdio->mrq == NULL))
	{
		dev_warn(sdio->dev, "sdio_workqueue_handler called without any work!\n");
		return;
	}

	cmd = mrq->cmd;

	//dev_info(sdio->dev, "executing command %d, arg %08x\n", cmd->opcode, cmd->arg);

	sdio_set_reg(SDIO_IRQMASK, sdio_get_reg(SDIO_IRQMASK) & ~2);

	sdio_clear_state(sdio);

	ret = sdio_wait_for_ready(sdio, 100);
	if(ret)
	{
		dev_info(sdio->dev, "problem waiting for ready");
		goto sdio_error;
	}

	if(cmd->data)
	{
		int i;
		struct scatterlist *sg;
		int segs = 0;

		//dev_info(sdio->dev, "setting up data command %d, arg %08x, blklen = %d, numblk = %d\n", cmd->opcode, cmd->arg, cmd->data->blksz, cmd->data->blocks);

		if(cmd->data->flags & MMC_DATA_READ)
			segs = dma_map_sg(sdio->dev, cmd->data->sg, cmd->data->sg_len, DMA_FROM_DEVICE);
		else if(cmd->data->flags & MMC_DATA_WRITE)
			segs = dma_map_sg(sdio->dev, cmd->data->sg, cmd->data->sg_len, DMA_TO_DEVICE);

		if(segs != 1)
		{
			// undo whatever we did
			if(cmd->data->flags & MMC_DATA_READ)
				dma_unmap_sg(sdio->dev, cmd->data->sg, segs, DMA_FROM_DEVICE);
			else if(cmd->data->flags & MMC_DATA_WRITE)
				dma_unmap_sg(sdio->dev, cmd->data->sg, segs, DMA_TO_DEVICE);

			dev_err(sdio->dev, "scatterlist must have only one entry!\n");
			ret = -EINVAL;
			goto sdio_error;
		}

		for_each_sg(cmd->data->sg, sg, segs, i)
		{
			sdio_set_reg(SDIO_BADDR, sg_dma_address(sg));
		}

		sdio_set_reg(SDIO_BLKLEN, cmd->data->blksz);
		sdio_set_reg(SDIO_NUMBLK, cmd->data->blocks);

		if(cmd->data->blocks == 1)
			sdio_set_reg(SDIO_CTRL, sdio_get_reg(SDIO_CTRL) & ~0x8000);
		else
			sdio_set_reg(SDIO_CTRL, sdio_get_reg(SDIO_CTRL) | 0x8000);

		// reset the FIFOs
		sdio_set_reg(SDIO_DCTRL, 3);
		sdio_set_reg(SDIO_DCTRL, 0);

		// disable block transfer done IRQ since we will be polling
		//sdio_set_reg(SDIO_IRQMASK, sdio_get_reg(SDIO_IRQMASK) & ~3);
		//sdio_set_reg(SDIO_IRQMASK, sdio_get_reg(SDIO_IRQMASK) & ~1);
	}

	sdio_set_reg(SDIO_ARGU, cmd->arg);
	sdio_set_reg(SDIO_CMD, setup_cmd(cmd));
	//dev_info(sdio->dev, "SDIO_CMD = %08x\n", sdio_get_reg(SDIO_CMD));

	ret = sdio_wait_for_cmd_ready(sdio, 100);
	if(ret)
	{
		dev_info(sdio->dev, "problem waiting for cmd ready");
		goto sdio_error;
	}

	status = sdio_execute_command(sdio, 100);
	if(status < 0)
	{
		dev_info(sdio->dev, "problem with execute command");
		ret = status;
		goto sdio_error;
	}

	//dev_info(sdio->dev, "after execute status = %08x\n", sdio_get_reg(SDIO_DSTA));

	// check for timeout
	if(status & 1)
	{
		dev_info(sdio->dev, "controller signalled command timeout");
		ret = -ETIMEDOUT;
		goto sdio_error;
	}

	// check for response index and end bit errors
	if(status & 6)
	{
		dev_info(sdio->dev, "index or end bit error\n");
		ret = -EILSEQ;
		goto sdio_error;
	}

	// check for CRC error if necessary
	if((cmd->flags & MMC_RSP_CRC) && (status & 8))
	{
		dev_info(sdio->dev, "CRC error\n");
		ret = -EILSEQ;
		goto sdio_error;
	}

	// looks like the command is non-errone
	cmd->error = 0;

	// populate response if necessary
	if(cmd->flags & MMC_RSP_PRESENT)
	{
		cmd->resp[0] = sdio_get_reg(SDIO_RESP0);
		if(cmd->flags & MMC_RSP_136)
		{
			cmd->resp[1] = sdio_get_reg(SDIO_RESP1);
			cmd->resp[2] = sdio_get_reg(SDIO_RESP2);
			cmd->resp[3] = sdio_get_reg(SDIO_RESP3);
		}
	}

	if(cmd->data)
	{
		u64 startTime;

		sdio_clear_state(sdio);
		sdio_set_reg(SDIO_DCTRL, 0x10);

		startTime = iphone_microtime();
		while((sdio_get_reg(SDIO_IRQ) & 1) == 0)
		{
			if(iphone_has_elapsed(startTime, cmd->data->timeout_ns))
			{
				ret = -ETIMEDOUT;
				goto sdio_error;
			}
			yield();
		}

		// re-enable IRQs
		sdio_set_reg(SDIO_IRQ, 1);
		//sdio_set_reg(SDIO_IRQMASK, sdio_get_reg(SDIO_IRQMASK) | 3);
		//sdio_set_reg(SDIO_IRQMASK, sdio_get_reg(SDIO_IRQMASK) | 1);

		status = sdio_get_reg(SDIO_DSTA) >> 19;
		if(((cmd->data->flags & MMC_DATA_WRITE) && status != 2) || ((cmd->data->flags & MMC_DATA_READ) && status != 0))
		{
			dev_info(sdio->dev, "Data error for command %d, arg %08x, blklen = %d, numblk = %d, dsta=0x%08x\n", cmd->opcode, cmd->arg, cmd->data->blksz, cmd->data->blocks, sdio_get_reg(SDIO_DSTA));
			cmd->data->error = -EILSEQ;
			cmd->data->bytes_xfered = 0;
			sdio->mrq = NULL;
			mmc_request_done(sdio->mmc, mrq);
			return;
		}

		if(cmd->data->flags & MMC_DATA_READ)
			dma_unmap_sg(sdio->dev, cmd->data->sg, 1, DMA_FROM_DEVICE);
		else if(cmd->data->flags & MMC_DATA_WRITE)
			dma_unmap_sg(sdio->dev, cmd->data->sg, 1, DMA_TO_DEVICE);

		sdio_set_reg(SDIO_CTRL, sdio_get_reg(SDIO_CTRL) & ~0x8000);

		cmd->data->error = 0;
		cmd->data->bytes_xfered = cmd->data->blksz * cmd->data->blocks;

		//dev_info(sdio->dev, "data successful!\n");
	}

	sdio->mrq = NULL;
	mmc_request_done(sdio->mmc, mrq);

	if(sdio->irq_enabled)
	{
		sdio_set_reg(SDIO_IRQMASK, sdio_get_reg(SDIO_IRQMASK) | 2);
		sdio_check_for_irq(sdio);
	}

	//dev_info(sdio->dev, "command successful!\n");
	return;

sdio_error:
	dev_info(sdio->dev, "execution error %d for command %d, arg %08x\n", ret, cmd->opcode, cmd->arg);
	++sdio->errors;
	cmd->error = ret;
	if(cmd->data)
	{
		cmd->data->error = ret;
		cmd->data->bytes_xfered = 0;
	}

	sdio->mrq = NULL;
	mmc_request_done(sdio->mmc, mrq);

	if(sdio->errors > 3)
	{
		dev_err(sdio->dev, "too many errors -- signalling Instrument of God and notifying MMC core\n");
		sdio_reinit(sdio);
	}

}

static void sdio_reset_handler(struct work_struct* work)
{
	struct iphone_sdio* sdio = sdio_driver;
	dev_err(sdio->dev, "Instrument of God activated, resetting SDIO and notifying MMC core\n");
	sdio_reset(sdio);
	sdio->errors = 0;
	mmc_detect_change(sdio->mmc, 0);
}

static void iphone_sdio_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct iphone_sdio* sdio = mmc_priv(mmc);

	if(unlikely(sdio->errors == -1))
	{
		dev_err(sdio->dev, "got request while SDIO is in a dead state\n");
		mrq->cmd->error = -EINVAL;
		if(mrq->cmd->data)
			mrq->cmd->data->error = -EINVAL;

		mmc_request_done(mmc, mrq);
		return;
	}

	if(unlikely(sdio->mrq != NULL))
	{
		dev_err(sdio->dev, "got request before last request was finished!\n");
		mrq->cmd->error = -EINVAL;
		if(mrq->cmd->data)
			mrq->cmd->data->error = -EINVAL;

		mmc_request_done(mmc, mrq);
		return;
	}

	//dev_info(sdio->dev, "queueing request\n");

	sdio->mrq = mrq;
	queue_work(sdio_wq, &sdio_workqueue);
}

static void iphone_sdio_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct iphone_sdio* sdio = mmc_priv(mmc);
	int shift = 0;

	while((FREQUENCY_PERIPHERAL >> (shift + 1)) > ios->clock)
		++shift;

	if(shift > 7)
		shift = 7;

	sdio_set_reg(SDIO_CLKDIV, 1 << shift);

	if(ios->bus_width == MMC_BUS_WIDTH_1)
		sdio_set_reg(SDIO_CTRL, sdio_get_reg(SDIO_CTRL) & (~(3 << 2)));
	else if(ios->bus_width == MMC_BUS_WIDTH_4)
		sdio_set_reg(SDIO_CTRL, (sdio_get_reg(SDIO_CTRL) & (~(3 << 2))) | (1 << 2));
	else if(ios->bus_width == MMC_BUS_WIDTH_8)
		sdio_set_reg(SDIO_CTRL, (sdio_get_reg(SDIO_CTRL) & (~(3 << 2))) | (2 << 2));

	//dev_info(sdio->dev, "set_ios clock = %u (%d), width %d, ctrl = %08x, vdd = %d\n", ios->clock, shift, 1 << ios->bus_width, sdio_get_reg(SDIO_CTRL), ios->vdd);
}

static void iphone_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	unsigned long flags;
	struct iphone_sdio* sdio = mmc_priv(mmc);

	//dev_info(sdio->dev, "enable irq: %d\n", enable);

	if(enable)
	{
		spin_lock_irqsave(&sdio->lock, flags);
		sdio->irq_enabled = true;
		sdio_set_reg(SDIO_IRQMASK, sdio_get_reg(SDIO_IRQMASK) | 2);
		spin_unlock_irqrestore(&sdio->lock, flags);
		sdio_check_for_irq(sdio);
	} else
	{
		spin_lock_irqsave(&sdio->lock, flags);
		sdio->irq_enabled = false;
		sdio_set_reg(SDIO_IRQMASK, sdio_get_reg(SDIO_IRQMASK) & ~2);
		spin_unlock_irqrestore(&sdio->lock, flags);
	}
}

static struct mmc_host_ops iphone_sdio_ops = {
	.request		= iphone_sdio_request,
	.set_ios		= iphone_sdio_set_ios,
	.enable_sdio_irq	= iphone_enable_sdio_irq,
};

ssize_t iphone_sdio_show_reset(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0\n");
}

ssize_t iphone_sdio_store_reset(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iphone_sdio* sdio = sdio_driver;
	int doReset = 0;
	sscanf(buf, "%d", &doReset);
	if(doReset)
	{
		dev_info(sdio->dev, "reinitializing SDIO card\n");
		sdio_reinit(sdio);
	}

	return strnlen(buf, PAGE_SIZE);
}


DEVICE_ATTR(reset, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, iphone_sdio_show_reset, iphone_sdio_store_reset);

static int __devinit iphone_sdio_probe(struct platform_device* pdev)
{
	int ret;
	struct resource* res;
	struct iphone_sdio* sdio;
	struct mmc_host* mmc;
	struct device* dev = &pdev->dev;

	sdio_wq = create_workqueue("iphone_sdio_worker");
	if(!sdio_wq)
	{
		dev_err(dev, "cannot get worker thread\n");
		return -ENOMEM;
	}

	mmc = mmc_alloc_host(sizeof(struct iphone_sdio), dev);
	if(!mmc)
	{
		dev_err(dev, "cannot get memory\n");
		return -ENOMEM;
	}

	sdio = mmc_priv(mmc);
	sdio_driver = sdio;

	sdio->irq_enabled = false;
	sdio->mrq = NULL;
	sdio->dev = dev;
	sdio->mmc = mmc;
	sdio->errors = 0;
	spin_lock_init(&sdio->lock);
	platform_set_drvdata(pdev, sdio);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(!res)
	{
		dev_err(dev, "cannot find register resource 0\n");
		ret = -EINVAL;
		goto err_mem;
	}

	sdio->regs_res = request_mem_region(res->start, resource_size(res), dev_name(dev));
	if(!sdio->regs_res)
	{
		dev_err(dev, "cannot reserve registers\n");
		ret = -ENOENT;
		goto err_mem;
	}

	sdio->regs = ioremap(res->start, resource_size(res));
	if(!sdio->regs)
	{
		dev_err(dev, "cannot map registers\n");
		ret = -ENXIO;
		goto err_regs_res;
	}

	ret = request_irq(SDIO_INT, sdio_irq, IRQF_DISABLED, dev_name(dev), sdio);
	if(ret < 0)
	{
		dev_err(dev, "cannot claim IRQ\n");
		goto err_regs;
	}

	if(sdio_init(sdio) < 0)
	{
		dev_err(dev, "error initializing SDIO\n");
		goto err_regs;
	}

	mmc->caps = MMC_CAP_4_BIT_DATA | MMC_CAP_NONREMOVABLE | MMC_CAP_SDIO_IRQ | MMC_CAP_SD_HIGHSPEED;
	mmc->ops = &iphone_sdio_ops;
	mmc->f_min = FREQUENCY_PERIPHERAL / 256;
	mmc->f_max = FREQUENCY_PERIPHERAL / 2;
	mmc->ocr_avail = ~0x7f;

	mmc->max_segs = 1;
	mmc->max_blk_size = 4095;
	mmc->max_blk_count = 4095;
	mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count;
	mmc->max_seg_size = mmc->max_req_size;

	ret = mmc_add_host(mmc);
	if (ret < 0)
		goto err_remove_host;

	device_create_file(dev, &dev_attr_reset);
	dev_info(dev, "SDIO host started with regs at 0x%p, interrupt %d\n", sdio->regs, SDIO_INT);

	return 0;

err_remove_host:
	mmc_remove_host(mmc);
	mmc_free_host(mmc);

err_regs:
	iounmap(sdio->regs);

err_regs_res:
	release_resource(sdio->regs_res);
	kfree(sdio->regs_res);

err_mem:
	kfree(sdio);
	return ret;
}

static int __devexit iphone_sdio_remove(struct platform_device* pdev)
{
	struct iphone_sdio* sdio = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_reset);

	flush_workqueue(sdio_wq);

	free_irq(SDIO_INT, sdio);
	iounmap(sdio->regs);

	release_resource(sdio->regs_res);
	kfree(sdio->regs_res);

	kfree(sdio);

	sdio_driver = NULL;

	return 0;
}

static struct resource iphone_sdio_resources[] = {
	[0] = {
		.start  = SDIO_PA,
		.end    = SDIO_PA + 0x1000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = SDIO_INT,
		.end    = SDIO_INT,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device iphone_sdio = {
	.name           = "iphone-sdio",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(iphone_sdio_resources),
	.resource       = iphone_sdio_resources,
};

static struct platform_driver iphone_sdio_driver = {
	.driver         = {
		.name   = "iphone-sdio",
		.owner  = THIS_MODULE,
	},
	.probe          = iphone_sdio_probe,
	.remove         = __devexit_p(iphone_sdio_remove),
	.suspend        = NULL,
	.resume         = NULL,
};

static int __init iphone_sdio_init(void)
{
	int ret;

	ret = platform_driver_register(&iphone_sdio_driver);
	if(!ret)
	{
		ret = platform_device_register(&iphone_sdio);
	}

	return ret;
}

static void __exit iphone_sdio_exit(void)
{
	platform_device_unregister(&iphone_sdio);
	platform_driver_unregister(&iphone_sdio_driver);
}


module_init(iphone_sdio_init);
module_exit(iphone_sdio_exit);
