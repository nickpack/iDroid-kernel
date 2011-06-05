/*
 * pxa2xx-i2s.c  --  ALSA Soc Audio Layer
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         lrg@slimlogic.co.uk
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>

#ifdef CONFIG_IPHONE_3G
#	include "../codecs/wm8991.h"
#endif

#include <mach/iphone-clock.h>
#include <mach/hardware.h>
#include <mach/iphone-dma.h>

#include "iphone-audio.h"

#define I2S0_CLOCK 0x27
#define I2S1_CLOCK 0x2A

#define I2S0 IO_ADDRESS(0x3CA00000)
#define I2S1 IO_ADDRESS(0x3CD00000)

#define I2S_CLKCON 0
#define I2S_TXCON 0x4
#define I2S_TXCOM 0x8
#define I2S_RXCON 0x30
#define I2S_RXCOM 0x34
#define I2S_STATUS 0x3C

#ifdef CONFIG_IPODTOUCH_1G
#define WM_I2S I2S1
#define DMA_WM_I2S_TX IPHONE_DMA_I2S1_TX
#define DMA_WM_I2S_RX IPHONE_DMA_I2S1_RX
#else
#define WM_I2S I2S0
#define DMA_WM_I2S_TX IPHONE_DMA_I2S0_TX
#define DMA_WM_I2S_RX IPHONE_DMA_I2S0_RX
#define BB_I2S I2S1
#define DMA_BB_I2S_TX IPHONE_DMA_I2S1_TX
#define DMA_BB_I2S_RX IPHONE_DMA_I2S1_RX
#endif

struct iphone_i2s_dma_params dma_playback_wm = {
	.dma_target	= DMA_WM_I2S_TX,
	.i2sController	= WM_I2S,
};

struct iphone_i2s_dma_params dma_recording_wm = {
	.dma_target = DMA_WM_I2S_RX,
	.i2sController	= WM_I2S,
};

#ifndef CONFIG_IPODTOUCH_1G
struct iphone_i2s_dma_params dma_playback_bb = {
	.dma_target	= DMA_BB_I2S_TX,
	.i2sController	= BB_I2S,
};

struct iphone_i2s_dma_params dma_recording_bb = {
	.dma_target = DMA_BB_I2S_RX,
	.i2sController	= BB_I2S,
};
#endif

static int iphone_i2s_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *socdai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai_link *dai = rtd->dai;

	pr_debug("ENTER iphone_i2s_startup\n");

	/* prepopulate DMA data so that anyone can figure out which DMA controller is needed */
#ifdef CONFIG_IPODTOUCH_1G
	dai->cpu_dai->playback.dma_data = &dma_playback_wm;
#else
	dai->cpu_dai->playback.dma_data = (socdai->id == 0) ? &dma_playback_wm : &dma_playback_bb;
#endif
	return 0;
}

static int iphone_i2s_set_dai_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{
	pr_debug("ENTER iphone_i2s_set_dai_fmt %u\n", fmt);
	return 0;
}

static int iphone_i2s_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	pr_debug("ENTER iphone_i2s_set_dai_sysclk %d %u %d\n", clk_id, freq, dir);
	return 0;
}

static int iphone_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *socdai)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai_link *dai = rtd->dai;
	struct iphone_i2s_dma_params* dma_params;

	pr_debug("ENTER iphone_i2s_hw_params\n");

#ifdef CONFIG_IPHONE_3G
	snd_soc_dai_set_pll(socdai, 0, 0, 0x785fc);
#endif

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
#ifdef CONFIG_IPODTOUCH_1G
		dma_params = &dma_playback_wm;
#else
		dma_params = (socdai->id == 0) ? &dma_playback_wm : &dma_playback_bb;
#endif
		dai->cpu_dai->playback.dma_data = dma_params;
		writel( (1 << 24) |  /* undocumented */
			(1 << 20) |  /* undocumented */
			(0 << 16) |  /* burst length */
			(0 << 15) |  /* 0 = falling edge */
			(0 << 13) |  /* 0 = basic I2S format */
			(0 << 12) |  /* 0 = MSB first */
			(0 << 11) |  /* 0 = left channel for low polarity */
			(3 << 8) |   /* MCLK divider */
			(0 << 5) |   /* 0 = 16-bit */
			(0 << 3) |   /* bit clock per frame */
			(1 << 0),
			dma_params->i2sController + I2S_TXCON);    /* channel index */

		writel((1 << 0), dma_params->i2sController + I2S_CLKCON);

		writel(	(0 << 3) |   /* 1 = transmit mode on */
			(1 << 2) |   /* 1 = I2S interface enable */
			(1 << 1) |   /* 1 = DMA request enable */
			(0 << 0),
			dma_params->i2sController + I2S_TXCOM);    /* 0 = LRCK on */
	} else
	{
#ifdef CONFIG_IPODTOUCH_1G
		dma_params = &dma_recording_wm;
#else
		dma_params = (socdai->id == 0) ? &dma_recording_wm : &dma_recording_bb;
#endif
		dai->cpu_dai->playback.dma_data = dma_params;
		writel(
			(0 << 12) |  /* 0 = falling edge */
			(0 << 10) |  /* 0 = basic I2S format */
			(0 << 9) |  /* 0 = MSB first */
			(0 << 8) |  /* 0 = left channel for low polarity */
			(3 << 5) |   /* MCLK divider */
			(0 << 2) |   /* 0 = 16-bit */
			(0 << 0),   /* bit clock per frame */
			dma_params->i2sController + I2S_RXCON);    /* channel index */

		writel((1 << 0), dma_params->i2sController + I2S_CLKCON);

		writel(	(0 << 3) |   /* 1 = transmit mode on */
			(1 << 2) |   /* 1 = I2S interface enable */
			(1 << 1) |   /* 1 = DMA request enable */
			(0 << 0),
			dma_params->i2sController + I2S_RXCOM);    /* 0 = LRCK on */
	}

	return ret;
}

static int iphone_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			      struct snd_soc_dai *dai)
{
	int ret = 0;

	pr_debug("ENTER iphone_i2s_trigger\n");

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void iphone_i2s_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *socdai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai_link *dai = rtd->dai;
	struct iphone_i2s_dma_params* dma_params = dai->cpu_dai->playback.dma_data;

	writel((1 << 0), dma_params->i2sController + I2S_CLKCON);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		writel(1, dma_params->i2sController + I2S_TXCOM);
	} else {
		writel(1, dma_params->i2sController + I2S_RXCOM);
}
}

static struct snd_soc_dai_ops iphone_i2s_dai_ops = {
	.startup	= iphone_i2s_startup,
	.shutdown	= iphone_i2s_shutdown,
	.trigger	= iphone_i2s_trigger,
	.hw_params	= iphone_i2s_hw_params,
	.set_fmt	= iphone_i2s_set_dai_fmt,
	.set_sysclk	= iphone_i2s_set_dai_sysclk,
};

struct snd_soc_dai iphone_i2s_wm8758_dai = {
	.name = "iphone-i2s-wm8758",
	.id = 0,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_44100,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_44100,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = &iphone_i2s_dai_ops,
	.symmetric_rates = 1,
};

struct snd_soc_dai iphone_i2s_bb_dai = {
	.name = "iphone-i2s-baseband",
	.id = 1,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_44100,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_44100,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = &iphone_i2s_dai_ops,
	.symmetric_rates = 1,
};

static int __init iphone_i2s_init(void)
{
	int ret;
	iphone_clock_gate_switch(I2S0_CLOCK, 1);
	iphone_clock_gate_switch(I2S1_CLOCK, 1);
	ret = snd_soc_register_dai(&iphone_i2s_wm8758_dai);
	if(ret)
		return ret;

	ret = snd_soc_register_dai(&iphone_i2s_bb_dai);
	if(ret)
	{
		snd_soc_unregister_dai(&iphone_i2s_wm8758_dai);
		return ret;
	}

	return 0;
}

static void __exit iphone_i2s_exit(void)
{
	snd_soc_unregister_dai(&iphone_i2s_bb_dai);
	snd_soc_unregister_dai(&iphone_i2s_wm8758_dai);
	iphone_clock_gate_switch(I2S0_CLOCK, 0);
	iphone_clock_gate_switch(I2S1_CLOCK, 0);
}

module_init(iphone_i2s_init);
module_exit(iphone_i2s_exit);

/* Module information */
MODULE_AUTHOR("Yiduo Wang");
MODULE_DESCRIPTION("Apple iPhone I2S SoC Interface");
MODULE_LICENSE("GPL");
