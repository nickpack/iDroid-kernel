#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "iphone-audio.h"

#ifdef CONFIG_IPHONE_3G
#include "../codecs/wm8991.h"
#endif

#ifndef CONFIG_IPHONE_3G
static int iphone_soc_to_wm8758_init(struct snd_soc_codec *codec)
{
	pr_debug("ENTER iphone_soc_to_wm8758_init\n");
	return 0;
}

static int iphone_soc_to_bb_init(struct snd_soc_codec *codec)
{
	pr_debug("ENTER iphone_soc_to_bb_init\n");
	return 0;
}
#endif

#ifdef CONFIG_IPHONE_3G
static int iphone_wm8991_init(struct snd_soc_codec *codec)
{
	printk("WM8991 initialising...\n");
	return 0;
}

static int iphone_wm8991_link_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8991_MCLK_DIV, WM8991_MCLK_DIV_2);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8991_BCLK_DIV, WM8991_BCLK_DIV_8);
	if (ret < 0)
		return ret;

	// this forces N = 7, K = 0x85FC for wm8991.
	ret = snd_soc_dai_set_pll(codec_dai, 0, 0, 0x0785FC);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops iphone_wm8991_link_ops =
{
	.hw_params = iphone_wm8991_link_hw_params,
};
#endif

static struct snd_soc_dai_link iphone_dai_links[] = {
#ifndef CONFIG_IPHONE_3G
	{
		.name           = "WM8758",
		.stream_name    = "WM8758",
		.cpu_dai        = &iphone_i2s_wm8758_dai,
		.codec_dai      = &iphone_wm8758_dai,
		.init           = iphone_soc_to_wm8758_init,
	},
#ifndef CONFIG_IPODTOUCH_1G
	{
		.name           = "Baseband",
		.stream_name    = "Baseband",
		.cpu_dai        = &iphone_i2s_bb_dai,
		.codec_dai      = &iphone_bb_dai,
		.init           = iphone_soc_to_bb_init,
	}
#endif
#else
	{
		.name			= "WM8991",
		.stream_name	= "WM8991",
		.cpu_dai		= &iphone_i2s_wm8758_dai, // This is bad, jah?
		.codec_dai		= &wm8991_dai,
		.init			= iphone_wm8991_init,
		.ops			= &iphone_wm8991_link_ops,
	},
#endif
};

static struct snd_soc_card iphone_snd_soc_card = {
	.name           = "iPhoneSound",
	.platform       = &snd_iphone_soc_platform,
	.dai_link       = iphone_dai_links,
	.num_links      = ARRAY_SIZE(iphone_dai_links),
};

//#ifdef CONFIG_IPHONE_3G
#if 0
static struct wm8990_setup_data wm8991_i2c_setup = {
	.i2c_bus = 0,
	.i2c_address = 0x36,
};
#endif

static struct snd_soc_device iphone_snd_soc_device = {
	.card           = &iphone_snd_soc_card,

#ifndef CONFIG_IPHONE_3G
	.codec_dev      = &soc_codec_dev_wm8758,
#else
	.codec_dev		= &soc_codec_dev_wm8991,
	//.codec_data		= &wm8991_i2c_setup,
#endif
};

static struct platform_device *snd_dev;

static int __init iphone_sound_init(void)
{
	int ret = 0;

	snd_dev = platform_device_alloc("soc-audio", -1);
	if (!snd_dev) {
		printk("failed to alloc soc-audio device\n");
		ret = -ENOMEM;
		return ret;
	}

	platform_set_drvdata(snd_dev, &iphone_snd_soc_device);
	iphone_snd_soc_device.dev = &snd_dev->dev;

	ret = platform_device_add(snd_dev);
	if (ret) {

		printk("failed to add soc-audio dev\n");
		platform_device_put(snd_dev);
	}

	return ret;
}

static void __exit iphone_sound_exit(void)
{
	platform_device_unregister(snd_dev);
}

module_init(iphone_sound_init);
module_exit(iphone_sound_exit);

MODULE_DESCRIPTION("iPhone SoC sound driver");
MODULE_AUTHOR("Yiduo Wang");
MODULE_LICENSE("GPL");
