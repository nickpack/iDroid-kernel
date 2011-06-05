#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "iphone-audio.h"

static int iphone_bb_pcm_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_info(codec->dev, "ENTER iphone_bb_pcm_startup\n");
	return 0;
}


static int iphone_bb_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_info(codec->dev, "ENTER iphone_bb_pcm_hw_params\n");
	return 0;
}

static int iphone_bb_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;

	dev_info(codec->dev, "ENTER iphone_bb_set_dai_fmt %u\n", fmt);
	return 0;
}

static int iphone_bb_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;

	dev_info(codec->dev, "ENTER iphone_bb_set_dai_sysclk %d %u %d\n", clk_id, freq, dir);
	return 0;
}

static int iphone_bb_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_info(codec->dev, "ENTER iphone_bb_mute\n");
	return 0;
}

static struct snd_soc_dai_ops iphone_bb_dai_ops = {
	.startup = iphone_bb_pcm_startup,
	.hw_params = iphone_bb_pcm_hw_params,
	.set_fmt = iphone_bb_set_dai_fmt,
	.set_sysclk = iphone_bb_set_dai_sysclk,
	.digital_mute = iphone_bb_mute,
};

struct snd_soc_dai iphone_bb_dai = {
	.name = "Baseband",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_44100,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_44100,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	 },
	.ops = &iphone_bb_dai_ops,
	.symmetric_rates = 1,
};

static int __init iphone_bb_audio_init(void)
{
	return snd_soc_register_dai(&iphone_bb_dai);
}

static void __exit iphone_bb_audio_exit(void)
{
	snd_soc_unregister_dai(&iphone_bb_dai);
}

module_init(iphone_bb_audio_init);
module_exit(iphone_bb_audio_exit);

MODULE_DESCRIPTION("iPhone baseband audio codec driver");
MODULE_AUTHOR("Yiduo Wang");
MODULE_LICENSE("GPL");
