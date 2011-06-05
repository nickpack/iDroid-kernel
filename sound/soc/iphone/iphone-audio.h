#ifndef IPHONE_AUDIO_H
#define IPHONE_AUDIO_H

#include <linux/dma-mapping.h>

struct iphone_i2s_dma_params
{
	dma_addr_t dma_target;
	u32 i2sController;
};

extern struct snd_soc_platform snd_iphone_soc_platform;
extern struct snd_soc_dai iphone_i2s_wm8758_dai;
extern struct snd_soc_dai iphone_i2s_bb_dai;
extern struct snd_soc_dai iphone_bb_dai;
extern struct snd_soc_dai iphone_wm8758_dai;
extern struct snd_soc_codec_device soc_codec_dev_wm8758;
#endif
