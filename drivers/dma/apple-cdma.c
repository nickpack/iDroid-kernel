/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <plat/cdma.h>

typedef struct _cmda_segment
{
	u32 next;	// Should be a DMA address
	u32 flags;
	u32 data;			// Should be a DMA address
	u32 length;
	u32 iv[4];
} __attribute__((packed)) __attribute__((aligned(4))) cdma_segment_t;

struct cdma_segment_tag
{
	cdma_segment_t *seg;
	dma_addr_t addr;

	struct list_head list;
};

// These first 3 are 32-bit blocks.
#define CDMA_ENABLE(x)			(0x0 + ((x)*0x4))
#define CDMA_DISABLE(x)			(0x8 + ((x)*0x4))
#define CDMA_STATUS(x)			(0x10 + ((x)*0x4))

#define CDMA_CHAN(x)			(0x1000*((x)+1))
#define CDMA_CSTATUS(x)			(CDMA_CHAN(x) + 0x0)
#define CDMA_CCONFIG(x)			(CDMA_CHAN(x) + 0x4)
#define CDMA_CREG(x)			(CDMA_CHAN(x) + 0x8)
#define CDMA_CSIZE(x)			(CDMA_CHAN(x) + 0xC)
#define CDMA_CSEGPTR(x)			(CDMA_CHAN(x) + 0x14)

#define CDMA_AES(x)				(((x)+1)*0x1000)
#define CDMA_AES_CONFIG(x)		(CDMA_AES(x) + 0x0)
#define CDMA_AES_KEY(x, y)		(CDMA_AES(x) + 0x20 + ((y)*4))

#define CSTATUS_ACTIVE			(1 << 0)
#define CSTATUS_SETUP			(1 << 1)
#define CSTATUS_CONT			(1 << 3)
#define CSTATUS_TXRDY			((1 << 17) | (1 << 16))
#define CSTATUS_INTERR			(1 << 18)
#define CSTATUS_INTCLR			(1 << 19)
#define CSTATUS_SPURCIR			(1 << 20)
#define CSTATUS_AES_SHIFT		(8)
#define CSTATUS_AES_MASK		(0xFF) // This is a guess
#define CSTATUS_AES(x)			(((x) & CSTATUS_AES_MASK) << CSTATUS_AES_SHIFT)

#define CCONFIG_DIR				(1 << 1)
#define CCONFIG_BURST_SHIFT		(2)
#define CCONFIG_BURST_MASK		(0x3)
#define CCONFIG_BURST(x)		(((x) & CCONFIG_BURST_MASK) << CCONFIG_BURST_SHIFT)
#define CCONFIG_WORDSIZE_SHIFT	(4)
#define CCONFIG_WORDSIZE_MASK	(0x7)
#define CCONFIG_WORDSIZE(x)		(((x) & CCONFIG_WORDSIZE_MASK) << CCONFIG_WORDSIZE_SHIFT)
#define CCONFIG_PERI_SHIFT		(16)
#define CCONFIG_PERI_MASK		(0x3F)
#define CCONFIG_PERIPHERAL(x)	(((x) & CCONFIG_PERI_MASK) << CCONFIG_PERI_SHIFT)

#define FLAG_DATA				(1 << 0)
#define FLAG_ENABLE				(1 << 1)
#define FLAG_LAST				(1 << 8)
#define FLAG_AES				(1 << 16)
#define FLAG_AES_START			(1 << 17)

#define AES_DIRECTION			(1 << 16)
#define AES_ENABLED				(1 << 17)
#define AES_128					(0 << 18)
#define AES_192					(1 << 18)
#define AES_256					(2 << 18)
#define AES_KEY					(1 << 20)
#define AES_GID					(2 << 20)
#define AES_UID					(4 << 20)

#define CDMA_MAX_CHANNELS	37

struct cdma_channel_state
{
	unsigned active: 1;
	unsigned in_use: 1;

	struct scatterlist *sg;
	size_t sg_count;
	size_t sg_offset;

	size_t count;
	size_t done;
	int current_transfer;

	struct cdma_aes *aes;
	int aes_channel;

	struct list_head segments;
	struct completion completion;
};

struct cdma_state
{
	struct platform_device *dev;
	void *__iomem regs, *__iomem aes_regs;
	struct clk *clk;
	struct dma_pool *pool;
	int irq;
	int num_channels;

	u32 aes_bitmap;
	struct cdma_channel_state channels[CDMA_MAX_CHANNELS];
};

static struct cdma_state *cdma_state = NULL;
static DECLARE_COMPLETION(cdma_completion);

static cdma_segment_t *cdma_alloc_segment(struct cdma_state *_state, struct cdma_channel_state *_cstate, dma_addr_t *_addr)
{
	cdma_segment_t *ret = dma_pool_alloc(_state->pool, GFP_KERNEL, _addr);
	struct cdma_segment_tag *tag = kzalloc(sizeof(*tag), GFP_KERNEL);

	memset(ret, 0, sizeof(*ret));
	tag->seg = ret;
	tag->addr = *_addr;

	list_add_tail(&tag->list, &_cstate->segments);
	return ret;
}

static void cdma_free_segments(struct cdma_state *_state, struct cdma_channel_state *_cstate)
{
	struct list_head *pos;

	list_for_each(pos, &_cstate->segments)
	{
		struct cdma_segment_tag *tag = container_of(pos, struct cdma_segment_tag, list);
		dma_pool_free(_state->pool, tag->seg, tag->addr);
		kfree(tag);
	}

	INIT_LIST_HEAD(&_cstate->segments);
}

static inline void cdma_next_sg(struct cdma_channel_state *_cstate)
{
	_cstate->sg = sg_next(_cstate->sg);
	_cstate->sg_count--;
	_cstate->sg_offset = 0;
}

static int cdma_activate(struct cdma_state *_state, int _chan, int _enable)
{
	u32 status;
	int block = (_chan >> 5);
	u32 mask = 1 << ((_chan & 0x1f)+1);

	printk("%s: %d %d -> %d %d.\n", __func__, _chan, _enable, block, mask);

	status = readl(_state->regs + CDMA_STATUS(block));
	if((status & mask) && !_enable)
	{
		// disable
		writel(mask, _state->regs + CDMA_DISABLE(block));
	}
	else if(!(status & mask) && _enable)
	{
		// enable
		writel(mask, _state->regs + CDMA_ENABLE(block));
	}

	printk("%s: 0x%08x.\n", __func__, readl(_state->regs + CDMA_STATUS(block)));
	
	_state->channels[_chan].active = _enable;
	return status;
}

static int cdma_continue(struct cdma_state *_state, int _chan)
{
	struct cdma_channel_state *cstate = &_state->channels[_chan];
	dma_addr_t addr = 0;
	int segsleft = 32;
	int amt_done = 0;

	if(!cstate->count || !cstate->sg)
	{
		dev_err(&_state->dev->dev, "tried to transfer nothing.\n");
		return -EINVAL;
	}

	if(cstate->aes)
	{
		struct cdma_aes *aes = cstate->aes;
		cdma_segment_t *seg = cdma_alloc_segment(_state, cstate, &addr);

		while(segsleft > 0)
		{
			int aes_seg_size = aes->data_size;
			int aes_seg_offset = 0;

			seg->flags = FLAG_ENABLE;
			cstate->aes->gen_iv(cstate->aes->iv_param, cstate->current_transfer++, seg->iv);
			segsleft--;

			while(aes_seg_offset < aes_seg_size)
			{
				int len = cstate->sg->length - cstate->sg_offset;
				if(len > aes_seg_size)
					len = aes_seg_size;

				seg = cdma_alloc_segment(_state, cstate, &seg->next);
				if(!seg)
				{
					dev_err(&_state->dev->dev, "failed to allocate segment!\n");
					return -ENOMEM;
				}

				seg->data = sg_phys(cstate->sg) + cstate->sg_offset;
				seg->flags = FLAG_ENABLE | FLAG_DATA | FLAG_AES;
				if(!aes_seg_offset)
					seg->flags |= FLAG_AES_START;
				seg->length = len;
				segsleft--;

				amt_done += len;
					cstate->sg_offset += len;

				dev_info(&_state->dev->dev, "generated AES segment: (%p(%u) flags=0x%08x)\n",
						(void*)seg->data, seg->length, seg->flags);

				cdma_next_sg(cstate);

				if(!cstate->sg_count)
				{
					seg->flags |= FLAG_LAST;
					segsleft = 0;
					break;
				}

				aes_seg_offset += len;
			}

			// Empty last seg OR next seg.
			seg = cdma_alloc_segment(_state, cstate, &seg->next);
		}
	}
	else
	{
		cdma_segment_t *seg = cdma_alloc_segment(_state, cstate, &addr);

		while(segsleft > 0)
		{
			seg->flags = FLAG_ENABLE | FLAG_DATA;
			seg->length = cstate->sg->length;
			seg->data = sg_phys(cstate->sg);

			dev_info(&_state->dev->dev, "generated segment: (%p(%u) flags=0x%08x)\n",
					(void*)seg->data, seg->length, seg->flags);

			cdma_next_sg(cstate);

			if(!cstate->sg_count)
			{
				seg->flags |= FLAG_LAST;
				segsleft = 0;
			}

			// Empty last seg OR next seg
			seg = cdma_alloc_segment(_state, cstate, &seg->next);
		}
	}

	cstate->done += amt_done;
	writel(addr, _state->regs + CDMA_CSEGPTR(_chan));
	writel(0x1C0009 | ((cstate->aes_channel+1) << 8), _state->regs + CDMA_CSTATUS(_chan));

	printk("%s: %d 0x%08x.\n", __func__, _chan, readl(_state->regs + CDMA_CSTATUS(_chan)));
	return 0;
}

int cdma_begin(u32 _channel, cdma_dir_t _dir, struct scatterlist *_sg, size_t _sg_count, size_t _size, dma_addr_t _reg, size_t _burst, size_t _busw, u32 _pid)
{
	struct cdma_state *state;
	struct cdma_channel_state *cstate;
	u32 flags = 0;

	wait_for_completion(&cdma_completion);
	state = cdma_state; // TODO: somewhat hacky.
	cstate = &state->channels[_channel];

	if(_channel > state->num_channels)
	{
		dev_err(&state->dev->dev, "no such channel %d.\n", _channel);
		return -ENOENT;
	}

	switch(_burst)
	{
	case 1:
		flags |= (0 << 2);
		break;
	
	case 2:
		flags |= (1 << 2);
		break;

	case 4:
		flags |= (2 << 2);
		break;

	default:
		dev_err(&state->dev->dev, "invalid burst size %d.\n", _burst);
		return -EINVAL;
	}

	switch(_busw)
	{
	case 1:
		flags |= (0 << 4);
		break;

	case 2:
		flags |= (1 << 4);
		break;

	case 4:
		flags |= (2 << 4);
		break;

	case 8:
		flags |= (3 << 4);
		break;

	case 16:
		flags |= (4 << 4);
		break;

	case 32:
		flags |= (5 << 4);
		break;

	default:
		dev_err(&state->dev->dev, "invalid bus width %d.\n", _busw);
		return -EINVAL;
	}

	flags |= ((_pid & 0x3f) << 16) | ((_dir & 1) << 1);

	INIT_COMPLETION(cstate->completion);

	cstate->sg = _sg;
	cstate->sg_count = _sg_count;
	cstate->sg_offset = 0;

	cstate->count = _size;
	cstate->done = 0;
	cstate->current_transfer = 0;

	cdma_activate(state, _channel, 1);
	writel(2, state->regs + CDMA_CSTATUS(_channel));
	writel(flags, state->regs + CDMA_CCONFIG(_channel));
	writel(_reg, state->regs + CDMA_CREG(_channel));
	writel(_size, state->regs + CDMA_CSIZE(_channel));

	return cdma_continue(state, _channel);
}
EXPORT_SYMBOL_GPL(cdma_begin);

int cdma_cancel(u32 _channel)
{
	struct cdma_state *state;
	struct cdma_channel_state *cstate;

	wait_for_completion(&cdma_completion);
	state = cdma_state; // TODO: somewhat hacky.
	cstate = &state->channels[_channel];

	if(_channel > state->num_channels)
		return -ENOENT;

	cdma_activate(state, _channel, 1);

	if(((readl(state->regs + CDMA_CSTATUS(_channel)) >> 16) & 3) == 1)
	{
		int i;
		writel(2, state->regs + CDMA_STATUS(_channel));

		for(i = 0; (((readl(state->regs + CDMA_STATUS(_channel)) >> 16) & 3) == 1)
				&& i < 1000; i++)
		{
			udelay(10);
		}

		if(i == 1000)
		{
			dev_err(&state->dev->dev, "failed to cancel transaction\n");
			return -ETIMEDOUT;
		}
	}

	complete(&cstate->completion);
	cdma_aes(_channel, NULL);

	cdma_activate(state, _channel, 0);
	return 0;
}
EXPORT_SYMBOL_GPL(cdma_cancel);

int cdma_wait(u32 _channel)
{
	struct cdma_state *state;
	struct cdma_channel_state *cstate;

	wait_for_completion(&cdma_completion);
	state = cdma_state; // TODO: somewhat hacky.
	cstate = &state->channels[_channel];

	if(_channel > state->num_channels)
		return -ENOENT;

	wait_for_completion(&cstate->completion);
	return 0;
}
EXPORT_SYMBOL_GPL(cdma_wait);

int cdma_aes(u32 _channel, struct cdma_aes *_aes)
{
	struct cdma_state *state;
	struct cdma_channel_state *cstate;
	int status;
	u32 cfg = 0;
	u32 type, keytype;

	wait_for_completion(&cdma_completion);
	state = cdma_state; // TODO: somewhat hacky.
	cstate = &state->channels[_channel];

	if(_channel > state->num_channels)
		return -ENOENT;

	if(cstate->aes == _aes)
		return 0;

	if(cstate->aes && !_aes)
	{
		state->aes_bitmap &=~ (1 << cstate->aes_channel);
		cstate->aes_channel = -1;
	}

	cstate->aes = _aes;
	if(!_aes)
		return 0;

	if(cstate->aes_channel < 0)
	{
		// TODO: lock this?

		int i;
		for(i = 0; i < 8; i++)
		{
			if(state->aes_bitmap & (1 << i))
				continue;

			state->aes_bitmap |= (1 << i);
			cstate->aes_channel = i;
		}

		if(i == 8)
		{
			dev_err(&state->dev->dev, "no more AES channels!\n");
			return -ENOENT;
		}
	}

	cfg = AES_ENABLED | (_channel & 0xFF) << 8;

	if(!_aes->inverse)
		cfg |= AES_DIRECTION;

	type = _aes->type;
	keytype = (type >> 28) & 0xF;

	switch(keytype)
	{
	case 2:
		cfg |= AES_256;
		break;

	case 1:
		cfg |= AES_192;
		break;

	case 0:
		cfg |= AES_128;
		break;

	default:
		dev_err(&state->dev->dev, "invalid AES key type %d.\n", keytype);
		return -EINVAL;
	}

	status = cdma_activate(state, _channel, 1);

	switch(type & 0xFFF)
	{
	case CDMA_GID:
		cfg |= AES_GID;
		break;

	case CDMA_UID:
		cfg |= AES_UID;
		break;

	case 0:
		cfg |= AES_KEY;
		switch(keytype)
		{
		case 2: // AES-256
			writel(_aes->key[7], state->aes_regs + CDMA_AES_KEY(cstate->aes_channel, 7));
			writel(_aes->key[6], state->aes_regs + CDMA_AES_KEY(cstate->aes_channel, 6));

		case 1: // AES-192
			writel(_aes->key[5], state->aes_regs + CDMA_AES_KEY(cstate->aes_channel, 5));
			writel(_aes->key[4], state->aes_regs + CDMA_AES_KEY(cstate->aes_channel, 4));

		default: // AES-128
			writel(_aes->key[3], state->aes_regs + CDMA_AES_KEY(cstate->aes_channel, 3));
			writel(_aes->key[2], state->aes_regs + CDMA_AES_KEY(cstate->aes_channel, 2));
			writel(_aes->key[1], state->aes_regs + CDMA_AES_KEY(cstate->aes_channel, 1));
			writel(_aes->key[0], state->aes_regs + CDMA_AES_KEY(cstate->aes_channel, 0));
			break;
		}
		break;
	
	default:
		dev_err(&state->dev->dev, "invalid AES key 0x%012x.\n", type & 0xFFF);
		cdma_activate(state, _channel, status);
		return -EINVAL;
	}

	writel(cfg, state->aes_regs + CDMA_AES_CONFIG(cstate->aes_channel));
	cdma_activate(state, _channel, status);
	return 0;
}
EXPORT_SYMBOL_GPL(cdma_aes);

static irqreturn_t cdma_irq_handler(int _irq, void *_token)
{
	struct cdma_state *state = _token;
	int chan = _irq - state->irq;
	struct cdma_channel_state *cstate = &state->channels[chan];
	u32 sz;
	int res = 0;

	u32 status = readl(state->regs + CDMA_CSTATUS(chan));

	printk("%s!\n", __func__);

	if(status & CSTATUS_INTERR)
	{
		dev_err(&state->dev->dev, "channel %d: error interrupt.\n", chan);
		res = -EIO;
	}

	if(status & CSTATUS_SPURCIR)
		dev_err(&state->dev->dev, "channel %d: spurious CIR.\n", chan);

	writel(CSTATUS_INTCLR, state->regs + CDMA_CSTATUS(chan));

	sz = readl(state->regs + CDMA_CSIZE(chan));
	if(!res && (cstate->count < cstate->done || sz))
	{
		if(status & CSTATUS_TXRDY)
			panic("TODO: %s, incomplete transfers.\n", __func__);
		
		cdma_continue(state, chan);
	}
	else
	{
		complete(&cstate->completion);
		cdma_activate(state, chan, 0);
		cdma_free_segments(state, cstate);
	}

	return IRQ_HANDLED;
}

static int cdma_probe(struct platform_device *_dev)
{
	struct cdma_state *state;
	struct resource *res;
	int i, ret = 0;

	if(cdma_state)
	{
		dev_err(&_dev->dev, "CDMA controller already registered.\n");
		return -EINVAL;
	}

	state = kzalloc(sizeof(struct cdma_state), GFP_KERNEL);
	if(!state)
	{
		dev_err(&_dev->dev, "failed to allocate state.\n");
		return -ENOMEM;
	}

	res = platform_get_resource(_dev, IORESOURCE_MEM, 0);
	if(!res)
	{
		dev_err(&_dev->dev, "failed to get register block.\n");
		ret = -EINVAL;
		goto err_state;
	}

	state->regs = ioremap(res->start, resource_size(res));
	if(!state->regs)
	{
		dev_err(&_dev->dev, "failed to remap register block.\n");
		ret = -EIO;
		goto err_state;
	}
	
	res = platform_get_resource(_dev, IORESOURCE_MEM, 1);
	if(!res)
	{
		dev_err(&_dev->dev, "failed to get AES block.\n");
		ret = -EINVAL;
		goto err_regs;
	}

	state->aes_regs = ioremap(res->start, resource_size(res));
	if(!state->aes_regs)
	{
		dev_err(&_dev->dev, "failed to map AES block.\n");
		ret = -EINVAL;
		goto err_regs;
	}

	res = platform_get_resource(_dev, IORESOURCE_IRQ, 0);
	if(!res)
	{
		dev_err(&_dev->dev, "failed to get irq base.\n");
		ret = -EINVAL;
		goto err_aes;
	}

	state->clk = clk_get(&_dev->dev, "cdma");
	if(state->clk && !IS_ERR(state->clk))
	{
		clk_enable(state->clk);
		dev_info(&_dev->dev, "enabling clock.\n");
	}

	state->dev = _dev;
	state->irq = res->start;
	state->num_channels = resource_size(res);
	
	for(i = res->start; i <= res->end; i++)
	{
		ret = request_irq(i, cdma_irq_handler, IRQF_SHARED, "apple-cdma", state);
		if(ret < 0)
		{
			dev_err(&_dev->dev, "failed to request irq %d.\n", i);
			goto err_aes;
		}
	}

	state->pool = dma_pool_create("apple-cdma", &_dev->dev, sizeof(cdma_segment_t),
			sizeof(cdma_segment_t),	0);
	if(!state->pool)
	{
		dev_err(&_dev->dev, "failed to create DMA pool.\n");
		ret = -ENOMEM;
		goto err_irqs;
	}

	for(i = 0; i < state->num_channels; i++)
	{
		struct cdma_channel_state *cstate = &state->channels[i];
		init_completion(&cstate->completion);
		INIT_LIST_HEAD(&cstate->segments);
	}

	dev_info(&state->dev->dev, "driver started.\n");
	cdma_state = state;
	complete_all(&cdma_completion);
	platform_set_drvdata(_dev, state);
	goto exit;

err_irqs:
	for(i = res->start; i <= res->end; i++)
		free_irq(i, state);

err_aes:
	iounmap(state->aes_regs);

err_regs:
	iounmap(state->regs);

err_state:
	kfree(state);

exit:
	return ret;
}

static int cdma_remove(struct platform_device *_dev)
{
	struct cdma_state *state = platform_get_drvdata(_dev);
	if(!state)
		return 0;

	if(state->pool)
		dma_pool_destroy(state->pool);

	if(state->irq)
	{
		int i;
		for(i = 0; i < state->num_channels; i++)
			free_irq(state->irq + i, state);
	}

	if(state->regs)
		iounmap(state->regs);

	if(state->aes_regs)
		iounmap(state->aes_regs);

	if(state->clk && !IS_ERR(state->clk))
	{
		clk_disable(state->clk);
		clk_put(state->clk);
	}

	kfree(state);
	return 0;
}

#ifdef CONFIG_PM
static int cdma_suspend(struct platform_device *_dev, pm_message_t _state)
{
	return 0;
}

static int cdma_resume(struct platform_device *_dev)
{
	return 0;
}
#else
#define cdma_suspend	NULL
#define cdma_resume		NULL
#endif

static struct platform_driver cdma_driver = {
	.driver = {
		.name = "apple-cdma",
	},

	.probe = cdma_probe,
	.remove = cdma_remove,
	.suspend = cdma_suspend,
	.resume = cdma_resume,
};

static int __init cdma_init(void)
{
	return platform_driver_register(&cdma_driver);
}
module_init(cdma_init);

static void __exit cdma_exit(void)
{
	platform_driver_unregister(&cdma_driver);
}
module_exit(cdma_exit);

MODULE_DESCRIPTION("Apple CDMA");
MODULE_AUTHOR("Richard Ian Taylor");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:apple-cdma");
