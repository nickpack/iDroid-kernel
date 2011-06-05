#include <linux/dma-mapping.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <mach/iphone-dma.h>
#include <mach/iphone-clock.h>
#include <mach/hardware.h>
#include <linux/slab.h>

#include "iphone-audio.h"

struct iphone_runtime_data
{
	spinlock_t lock;

	int dummy;
	int params;

	int controller;
	int channel;
	int i2s;

	dma_addr_t dma_start;
	unsigned int dma_periods;
	size_t dma_period_bytes;

	DMALinkedList* continueList;
	size_t continueListPeriodSize;
	size_t continueListSize;
	dma_addr_t continueListPhys;
};

static const struct snd_pcm_hardware iphone_pcm_hardware = {
	.info                   = SNDRV_PCM_INFO_INTERLEAVED |
                                  SNDRV_PCM_INFO_BLOCK_TRANSFER |
                                  SNDRV_PCM_INFO_MMAP |
                                  SNDRV_PCM_INFO_MMAP_VALID |
                                  SNDRV_PCM_INFO_PAUSE |
                                  SNDRV_PCM_INFO_RESUME,
	.formats                = SNDRV_PCM_FMTBIT_S16_LE,
	.channels_min           = 2,
	.channels_max           = 2,
	.buffer_bytes_max       = 128*1024,
	.period_bytes_min       = PAGE_SIZE,
	.period_bytes_max       = PAGE_SIZE*2,
	.periods_min            = 2,
	.periods_max            = 128,
	.fifo_size              = 32,
};

static int iphone_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct iphone_runtime_data *rtd;
	int ret;

	pr_debug("ENTER iphone_pcm_open\n");

	runtime->hw = iphone_pcm_hardware;

	/*
	 * For mysterious reasons (and despite what the manual says)
	 * playback samples are lost if the DMA count is not a multiple
	 * of the DMA burst size.  Let's add a rule to enforce that.
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);
	if (ret)
		goto out;

	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	if (ret)
		goto out;
	 */

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto out;

	ret = snd_soc_set_runtime_hwparams(substream, &iphone_pcm_hardware);
	if (ret < 0)
		goto out;

	ret = -ENOMEM;
	rtd = kzalloc(sizeof(*rtd), GFP_KERNEL);
	if (!rtd)
		goto out;

	rtd->controller = -1;
	rtd->channel = -1;

	spin_lock_init(&rtd->lock);

	runtime->private_data = rtd;
	return 0;

	kfree(rtd);
 out:
	return ret;
}

static int iphone_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct iphone_runtime_data *rtd = runtime->private_data;

	pr_debug("ENTER iphone_pcm_close\n");

	kfree(rtd);
	return 0;
}

static void iphone_pcm_period_handler(int controller, int channel, void* token)
{
	struct snd_pcm_substream *substream = token;

	pr_debug("ENTER iphone_pcm_period_handler\n");

	if (substream)
		snd_pcm_period_elapsed(substream);
}

static int iphone_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct iphone_runtime_data *prtd = substream->runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct iphone_i2s_dma_params *dma = rtd->dai->cpu_dai->playback.dma_data;
	dma_addr_t src;
	dma_addr_t dest;
	dma_addr_t orig_src;
	dma_addr_t orig_dest;
	dma_addr_t* memory;

	DMALinkedList* item;
	dma_addr_t itemPhys;
	DMALinkedList* last;

	int i;

	pr_debug("ENTER iphone_pcm_prepare\n");

	if (!prtd || prtd->controller == -1)
		return 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		iphone_dma_request(IPHONE_DMA_MEMORY, 2, 1, dma->dma_target, 2, 1, &prtd->controller, &prtd->channel);
		src = prtd->dma_start;
		dest = dma->dma_target;
		memory = &src;
	} else
	{
		iphone_dma_request(dma->dma_target, 2, 1, IPHONE_DMA_MEMORY, 2, 1, &prtd->controller, &prtd->channel);
		src = dma->dma_target;
		dest = prtd->dma_start;
		memory = &dest;
	}

	orig_src = src;
	orig_dest = dest;

	/* Create a circular continue list of periods, with each period being a link in the circular chain */
	item = prtd->continueList;
	itemPhys = prtd->continueListPhys;

	for(i = 0; i < prtd->dma_periods; ++i)
	{
		iphone_dma_create_continue_list(src, dest, prtd->dma_period_bytes, &prtd->controller, &prtd->channel,
				&item, &itemPhys, &prtd->continueListPeriodSize, &last);

		itemPhys += prtd->continueListPeriodSize;
		item = (DMALinkedList*)(((u32)(item)) + prtd->continueListPeriodSize);
		last->next = itemPhys;
		*memory += prtd->dma_period_bytes;
	}

	/* Close the loop */
	last->next = prtd->continueListPhys;

	return iphone_dma_prepare(orig_src, orig_dest, 0, prtd->continueList, &prtd->controller, &prtd->channel);
}

static int iphone_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct iphone_runtime_data *prtd = runtime->private_data;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct iphone_i2s_dma_params *dma = rtd->dai->cpu_dai->playback.dma_data;
	unsigned int period_bytes = params_period_bytes(params);
	unsigned int periods = params_periods(params);
	unsigned int totbytes = params_buffer_bytes(params);

	pr_debug("ENTER iphone_pcm_hw_params\n");

	if((periods * period_bytes) != totbytes)
	{
		printk(KERN_ERR "iphone-audio: periods must be integral\n");
		return -EINVAL;
	}

	if(prtd->controller == -1)
	{
		int controller = 0;
		int channel = 0;

		/* the details don't matter yet at this point, we're just trying to get a channel.. we need to get a controller that can send to the peripheral, though */
		/* but whether it's the source or the destination doesn't really matter at this point. */
		int ret = iphone_dma_request(dma->dma_target, 2, 1, IPHONE_DMA_MEMORY, 2, 1, &controller, &channel);
		if(ret != 0)
		{
			printk(KERN_ERR "iphone-audio: failed to get dma channel\n");
			return -EBUSY;
		}

		iphone_dma_set_done_handler(&controller, &channel, iphone_pcm_period_handler, substream);

		prtd->controller = controller;
		prtd->channel = channel;

		prtd->continueListPeriodSize = iphone_dma_continue_list_size(runtime->dma_addr, IPHONE_DMA_I2S0_TX, period_bytes, &prtd->controller, &prtd->channel);
		prtd->continueListSize = prtd->continueListPeriodSize * periods;
		prtd->continueList = dma_alloc_writecombine(buf->dev.dev, prtd->continueListSize, &prtd->continueListPhys, GFP_KERNEL);
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totbytes;

	spin_lock_irq(&prtd->lock);
	prtd->i2s = 1;
	prtd->dma_start = runtime->dma_addr;
	prtd->dma_periods = periods;
	prtd->dma_period_bytes = period_bytes;
	spin_unlock_irq(&prtd->lock);

	return 0;
}

static int iphone_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct iphone_runtime_data *prtd = substream->runtime->private_data;
	struct snd_dma_buffer *buf = &substream->dma_buffer;

	pr_debug("ENTER iphone_pcm_hw_free\n");

	snd_pcm_set_runtime_buffer(substream, NULL);

	if (prtd->controller != -1)
	{
		iphone_dma_pause(prtd->controller, prtd->channel);
		iphone_dma_finish(prtd->controller, prtd->channel, 0);
		prtd->controller = -1;
		prtd->channel = -1;
		dma_free_writecombine(buf->dev.dev, prtd->continueListSize, prtd->continueList, prtd->continueListPhys);

	}

	return 0;
}

static int iphone_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct iphone_runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_debug("ENTER iphone_pcm_trigger\n");

	spin_lock(&prtd->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		iphone_dma_resume(prtd->controller, prtd->channel);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		iphone_dma_pause(prtd->controller, prtd->channel);
		break;

	default:
		ret = -EINVAL;
	}

	spin_unlock(&prtd->lock);

	return ret;
}

static snd_pcm_uframes_t iphone_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct iphone_runtime_data *prtd = runtime->private_data;

	dma_addr_t ptr;
	dma_addr_t diff;
	snd_pcm_uframes_t x;

	spin_lock(&prtd->lock);
	ptr = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
			 iphone_dma_srcpos(prtd->controller, prtd->channel) : iphone_dma_dstpos(prtd->controller, prtd->channel);

	diff = ptr - runtime->dma_addr;
	spin_unlock(&prtd->lock);

	x = bytes_to_frames(runtime, diff);

	if (x == runtime->buffer_size)
		x = 0;

	return x;
}

static int iphone_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
}

static struct snd_pcm_ops iphone_pcm_ops = {
	.open		= iphone_pcm_open,
	.close		= iphone_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= iphone_pcm_hw_params,
	.hw_free	= iphone_pcm_hw_free,
	.prepare	= iphone_pcm_prepare,
	.trigger	= iphone_pcm_trigger,
	.pointer	= iphone_pcm_pointer,
	.mmap		= iphone_pcm_mmap,
};

static u64 iphone_pcm_dmamask = DMA_BIT_MASK(32);

static int iphone_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = iphone_pcm_hardware.buffer_bytes_max;
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	return 0;
}

static int iphone_soc_pcm_new(struct snd_card *card, struct snd_soc_dai *dai,
	struct snd_pcm *pcm)
{
	int ret = 0;

	pr_debug("ENTER iphone_soc_pcm_new\n");

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &iphone_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (dai->playback.channels_min) {
		ret = iphone_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (dai->capture.channels_min) {
		ret = iphone_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}

 out:
	return ret;
}

static void iphone_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	pr_debug("ENTER iphone_soc_pcm_free_dma_buffers\n");

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;
		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;
		dma_free_writecombine(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
		buf->area = NULL;
	}
}

struct snd_soc_platform snd_iphone_soc_platform = {
	.name		= "iphone-i2s-dma",
	.pcm_ops 	= &iphone_pcm_ops,
	.pcm_new	= iphone_soc_pcm_new,
	.pcm_free	= iphone_pcm_free_dma_buffers,
};
EXPORT_SYMBOL_GPL(snd_iphone_soc_platform);

static int __init snd_iphone_soc_platform_init(void)
{
	return snd_soc_register_platform(&snd_iphone_soc_platform);
}
module_init(snd_iphone_soc_platform_init);

static void __exit snd_iphone_soc_platform_exit(void)
{
	snd_soc_unregister_platform(&snd_iphone_soc_platform);
}
module_exit(snd_iphone_soc_platform_exit);

MODULE_AUTHOR("Yiduo Wang");
MODULE_DESCRIPTION("Apple iPhone PCM DMA module");
MODULE_LICENSE("GPL");
