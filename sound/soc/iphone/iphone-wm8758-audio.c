#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/i2c.h>

/*
 *
 * FIXME: This is an ugly as sin hack because technically we should implement a proper i2c driver and
 * use something based off the wm8978. We'd also have to implement the hw_params op on the dai_link and figure
 * out how to translate our magic numbers here into numbers that codec driver can use.
 *
 */

#define RESET       0x00
#define PWRMGMT1    0x01
#define PWRMGMT2    0x02
#define PWRMGMT3    0x03
#define AINTFCE     0x04
#define COMPAND     0x05
#define CLKGEN      0x06
#define SRATECTRL   0x07
#define GPIOCTL     0x08
#define JACKDETECT0 0x09
#define DACCTRL     0x0a
#define LDACVOL     0x0b
#define RDACVOL     0x0c
#define JACKDETECT1 0x0d
#define ADCCTL      0x0e
#define LADCVOL     0x0f
#define RADCVOL     0x10

#define EQ1         0x12
#define EQ2         0x13
#define EQ3         0x14
#define EQ4         0x15
#define EQ5         0x16
#define EQ_GAIN_MASK       0x001f
#define EQ_CUTOFF_MASK     0x0060
#define EQ_GAIN_VALUE(x)   (((-x) + 12) & 0x1f)
#define EQ_CUTOFF_VALUE(x) ((((x) - 1) & 0x03) << 5)

#define CLASSDCTL   0x17
#define DACLIMIT1   0x18
#define DACLIMIT2   0x19

#define NOTCH1      0x1b
#define NOTCH2      0x1c
#define NOTCH3      0x1d
#define NOTCH4      0x1e

#define ALCCTL1     0x20
#define ALCCTL2     0x21
#define ALCCTL3     0x22
#define NOISEGATE   0x23
#define PLLN        0x24
#define PLLK1       0x25
#define PLLK2       0x26
#define PLLK3       0x27

#define THREEDCTL   0x29
#define OUT4ADC     0x2a
#define BEEPCTRL    0x2b
#define INCTRL      0x2c
#define LINPGAGAIN  0x2d
#define RINPGAGAIN  0x2e
#define LADCBOOST   0x2f
#define RADCBOOST   0x30
#define OUTCTRL     0x31
#define LOUTMIX     0x32
#define ROUTMIX     0x33
#define LOUT1VOL    0x34
#define ROUT1VOL    0x35
#define LOUT2VOL    0x36
#define ROUT2VOL    0x37
#define OUT3MIX     0x38
#define OUT4MIX     0x39

#define BIASCTL     0x3d
#define WMREG_3E    0x3e

static const u16 wm8978_reg[0x3A] = {
	0x0000, 0x0000, 0x0000, 0x0000,	/* 0x00...0x03 */
	0x0050, 0x0000, 0x0140, 0x0000,	/* 0x04...0x07 */
	0x0000, 0x0000, 0x0000, 0x00ff,	/* 0x08...0x0b */
	0x00ff, 0x0000, 0x0100, 0x00ff,	/* 0x0c...0x0f */
	0x00ff, 0x0000, 0x012c, 0x002c,	/* 0x10...0x13 */
	0x002c, 0x002c, 0x002c, 0x0000,	/* 0x14...0x17 */
	0x0032, 0x0000, 0x0000, 0x0000,	/* 0x18...0x1b */
	0x0000, 0x0000, 0x0000, 0x0000,	/* 0x1c...0x1f */
	0x0038, 0x000b, 0x0032, 0x0000,	/* 0x20...0x23 */
	0x0008, 0x000c, 0x0093, 0x00e9,	/* 0x24...0x27 */
	0x0000, 0x0000, 0x0000, 0x0000,	/* 0x28...0x2b */
	0x0033, 0x0010, 0x0010, 0x0100,	/* 0x2c...0x2f */
	0x0100, 0x0002, 0x0001, 0x0001,	/* 0x30...0x33 */
	0x0039, 0x0039, 0x0039, 0x0039,	/* 0x34...0x37 */
	0x0001,	0x0001,			/* 0x38...0x3b */
};

static const char *wm8978_companding[] = {"Off", "NC", "u-law", "A-law"};
static const char *wm8978_eqmode[] = {"Capture", "Playback"};
static const char *wm8978_bw[] = {"Narrow", "Wide"};
static const char *wm8978_eq1[] = {"80Hz", "105Hz", "135Hz", "175Hz"};
static const char *wm8978_eq2[] = {"230Hz", "300Hz", "385Hz", "500Hz"};
static const char *wm8978_eq3[] = {"650Hz", "850Hz", "1.1kHz", "1.4kHz"};
static const char *wm8978_eq4[] = {"1.8kHz", "2.4kHz", "3.2kHz", "4.1kHz"};
static const char *wm8978_eq5[] = {"5.3kHz", "6.9kHz", "9kHz", "11.7kHz"};
static const char *wm8978_alc3[] = {"ALC", "Limiter"};
static const char *wm8978_alc1[] = {"Off", "Right", "Left", "Both"};

static const SOC_ENUM_SINGLE_DECL(adc_compand, COMPAND, 1,
				  wm8978_companding);
static const SOC_ENUM_SINGLE_DECL(dac_compand, COMPAND, 3,
				  wm8978_companding);
static const SOC_ENUM_SINGLE_DECL(eqmode, EQ1, 8, wm8978_eqmode);
static const SOC_ENUM_SINGLE_DECL(eq1, EQ1, 5, wm8978_eq1);
static const SOC_ENUM_SINGLE_DECL(eq2bw, EQ2, 8, wm8978_bw);
static const SOC_ENUM_SINGLE_DECL(eq2, EQ2, 5, wm8978_eq2);
static const SOC_ENUM_SINGLE_DECL(eq3bw, EQ3, 8, wm8978_bw);
static const SOC_ENUM_SINGLE_DECL(eq3, EQ3, 5, wm8978_eq3);
static const SOC_ENUM_SINGLE_DECL(eq4bw, EQ4, 8, wm8978_bw);
static const SOC_ENUM_SINGLE_DECL(eq4, EQ4, 5, wm8978_eq4);
static const SOC_ENUM_SINGLE_DECL(eq5, EQ5, 5, wm8978_eq5);
static const SOC_ENUM_SINGLE_DECL(alc3, ALCCTL3, 8, wm8978_alc3);
static const SOC_ENUM_SINGLE_DECL(alc1, ALCCTL1, 7, wm8978_alc1);

static const DECLARE_TLV_DB_SCALE(digital_tlv, -12750, 50, 1);
static const DECLARE_TLV_DB_SCALE(eq_tlv, -1200, 100, 0);
static const DECLARE_TLV_DB_SCALE(inpga_tlv, -1200, 75, 0);
static const DECLARE_TLV_DB_SCALE(spk_tlv, -5700, 100, 0);
static const DECLARE_TLV_DB_SCALE(boost_tlv, -1500, 300, 1);

static int bb_volume_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
static int bb_volume_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

static const struct snd_kcontrol_new wm8978_snd_controls[] = {

	SOC_SINGLE("Digital Loopback Switch",
		COMPAND, 0, 1, 0),

	SOC_ENUM("ADC Companding", adc_compand),
	SOC_ENUM("DAC Companding", dac_compand),

	SOC_DOUBLE("DAC Inversion Switch", DACCTRL, 0, 1, 1, 0),

	SOC_DOUBLE_R_TLV("PCM Volume",
		LDACVOL, RDACVOL,
		0, 255, 0, digital_tlv),

	SOC_SINGLE("High Pass Filter Switch", ADCCTL, 8, 1, 0),
	SOC_SINGLE("High Pass Cut Off", ADCCTL, 4, 7, 0),
	SOC_DOUBLE("ADC Inversion Switch", ADCCTL, 0, 1, 1, 0),

	SOC_DOUBLE_R_TLV("ADC Volume",
		LADCVOL, RADCVOL,
		0, 255, 0, digital_tlv),

	SOC_ENUM("Equaliser Function", eqmode),
	SOC_ENUM("EQ1 Cut Off", eq1),
	SOC_SINGLE_TLV("EQ1 Volume", EQ1,  0, 24, 1, eq_tlv),

	SOC_ENUM("Equaliser EQ2 Bandwith", eq2bw),
	SOC_ENUM("EQ2 Cut Off", eq2),
	SOC_SINGLE_TLV("EQ2 Volume", EQ2,  0, 24, 1, eq_tlv),

	SOC_ENUM("Equaliser EQ3 Bandwith", eq3bw),
	SOC_ENUM("EQ3 Cut Off", eq3),
	SOC_SINGLE_TLV("EQ3 Volume", EQ3,  0, 24, 1, eq_tlv),

	SOC_ENUM("Equaliser EQ4 Bandwith", eq4bw),
	SOC_ENUM("EQ4 Cut Off", eq4),
	SOC_SINGLE_TLV("EQ4 Volume", EQ4,  0, 24, 1, eq_tlv),

	SOC_ENUM("EQ5 Cut Off", eq5),
	SOC_SINGLE_TLV("EQ5 Volume", EQ5, 0, 24, 1, eq_tlv),

	SOC_SINGLE("DAC Playback Limiter Switch",
		DACLIMIT1, 8, 1, 0),
	SOC_SINGLE("DAC Playback Limiter Decay",
		DACLIMIT1, 4, 15, 0),
	SOC_SINGLE("DAC Playback Limiter Attack",
		DACLIMIT1, 0, 15, 0),

	SOC_SINGLE("DAC Playback Limiter Threshold",
		DACLIMIT2, 4, 7, 0),
	SOC_SINGLE("DAC Playback Limiter Boost",
		DACLIMIT2, 0, 15, 0),

	SOC_ENUM("ALC Enable Switch", alc1),
	SOC_SINGLE("ALC Capture Min Gain", ALCCTL1, 0, 7, 0),
	SOC_SINGLE("ALC Capture Max Gain", ALCCTL1, 3, 7, 0),

	SOC_SINGLE("ALC Capture Hold", ALCCTL2, 4, 7, 0),
	SOC_SINGLE("ALC Capture Target", ALCCTL2, 0, 15, 0),

	SOC_ENUM("ALC Capture Mode", alc3),
	SOC_SINGLE("ALC Capture Decay", ALCCTL3, 4, 15, 0),
	SOC_SINGLE("ALC Capture Attack", ALCCTL3, 0, 15, 0),

	SOC_SINGLE("ALC Capture Noise Gate Switch", NOISEGATE, 3, 1, 0),
	SOC_SINGLE("ALC Capture Noise Gate Threshold",
		NOISEGATE, 0, 7, 0),

	SOC_DOUBLE_R("Capture PGA ZC Switch",
		LINPGAGAIN, RINPGAGAIN,
		7, 1, 0),

	/* OUT1 - Headphones */
	SOC_DOUBLE_R("Headphone Playback ZC Switch",
		LOUT1VOL, ROUT1VOL, 7, 1, 0),

	SOC_DOUBLE_R_TLV("Headphone Playback Volume",
		LOUT1VOL, ROUT1VOL,
		0, 63, 0, spk_tlv),

	/* OUT2 */
	SOC_DOUBLE_R("OUT2 Playback ZC Switch",
		LOUT2VOL, ROUT2VOL, 7, 1, 0),

	SOC_DOUBLE_R_TLV("OUT2 Playback Volume",
		LOUT2VOL, ROUT2VOL,
		0, 63, 0, spk_tlv),

	/* OUT3/4 - Line Output */
	SOC_DOUBLE_R("Line Playback Switch",
		OUT3MIX, OUT4MIX, 6, 1, 1),

	/* Mixer #3: Boost (Input) mixer */
	SOC_DOUBLE_R("PGA Boost (+20dB)",
		LADCBOOST, RADCBOOST,
		8, 1, 0),
	SOC_DOUBLE_R_TLV("L2/R2 Boost Volume",
		LADCBOOST, RADCBOOST,
		4, 7, 0, boost_tlv),
	SOC_DOUBLE_R_TLV("Aux Boost Volume",
		LADCBOOST, RADCBOOST,
		0, 7, 0, boost_tlv),

	/* Input PGA volume */
	SOC_DOUBLE_R_TLV("Input PGA Volume",
		LINPGAGAIN, RINPGAGAIN,
		0, 63, 0, inpga_tlv),

	/* Headphone */
	SOC_DOUBLE_R("Headphone Switch",
		LOUT1VOL, ROUT1VOL, 6, 1, 1),

	/* OUT2 */
	SOC_DOUBLE_R("OUT2 Switch",
		LOUT2VOL, ROUT2VOL, 6, 1, 1),

	/* DAC / ADC oversampling */
	SOC_SINGLE("DAC 128x Oversampling Switch", DACCTRL, 8, 1, 0),
	SOC_SINGLE("ADC 128x Oversampling Switch", ADCCTL, 8, 1, 0),

	SOC_SINGLE_EXT("Speaker 1 Volume", 2, 0, 100, 0, bb_volume_get, bb_volume_set),
	SOC_SINGLE_EXT("Speaker 2 Volume", 0, 0, 100, 0, bb_volume_get, bb_volume_set),
};

/* Mixer #1: Output (OUT1, OUT2) Mixer: mix AUX, Input mixer output and DAC */
static const struct snd_kcontrol_new wm8978_left_out_mixer[] = {
	SOC_DAPM_SINGLE("Line Bypass Switch", LOUTMIX, 1, 1, 0),
	SOC_DAPM_SINGLE("Aux Playback Switch", LOUTMIX, 5, 1, 0),
	SOC_DAPM_SINGLE("PCM Playback Switch", LOUTMIX, 0, 1, 0),
};

static const struct snd_kcontrol_new wm8978_right_out_mixer[] = {
	SOC_DAPM_SINGLE("Line Bypass Switch", ROUTMIX, 1, 1, 0),
	SOC_DAPM_SINGLE("Aux Playback Switch", ROUTMIX, 5, 1, 0),
	SOC_DAPM_SINGLE("PCM Playback Switch", ROUTMIX, 0, 1, 0),
};

/* OUT3/OUT4 Mixer not implemented */

/* Mixer #2: Input PGA Mute */
static const struct snd_kcontrol_new wm8978_left_input_mixer[] = {
	SOC_DAPM_SINGLE("L2 Switch", INCTRL, 2, 1, 0),
	SOC_DAPM_SINGLE("MicN Switch", INCTRL, 1, 1, 0),
	SOC_DAPM_SINGLE("MicP Switch", INCTRL, 0, 1, 0),
};
static const struct snd_kcontrol_new wm8978_right_input_mixer[] = {
	SOC_DAPM_SINGLE("R2 Switch", INCTRL, 6, 1, 0),
	SOC_DAPM_SINGLE("MicN Switch", INCTRL, 5, 1, 0),
	SOC_DAPM_SINGLE("MicP Switch", INCTRL, 4, 1, 0),
};

static const struct snd_soc_dapm_widget wm8978_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("Left DAC", "Left HiFi Playback",
			 PWRMGMT3, 0, 0),
	SND_SOC_DAPM_DAC("Right DAC", "Right HiFi Playback",
			 PWRMGMT3, 1, 0),
	SND_SOC_DAPM_ADC("Left ADC", "Left HiFi Capture",
			 PWRMGMT2, 0, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right HiFi Capture",
			 PWRMGMT2, 1, 0),

	/* Mixer #1: OUT1,2 */
	SOC_MIXER_ARRAY("Left Output Mixer", PWRMGMT3,
			2, 0, wm8978_left_out_mixer),
	SOC_MIXER_ARRAY("Right Output Mixer", PWRMGMT3,
			3, 0, wm8978_right_out_mixer),

	SOC_MIXER_ARRAY("Left Input Mixer", PWRMGMT2,
			2, 0, wm8978_left_input_mixer),
	SOC_MIXER_ARRAY("Right Input Mixer", PWRMGMT2,
			3, 0, wm8978_right_input_mixer),

	SND_SOC_DAPM_PGA("Left Boost Mixer", PWRMGMT2,
			 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Boost Mixer", PWRMGMT2,
			 5, 0, NULL, 0),

	SND_SOC_DAPM_PGA("Left Capture PGA", LINPGAGAIN,
			 6, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Right Capture PGA", RINPGAGAIN,
			 6, 1, NULL, 0),

	SND_SOC_DAPM_PGA("Left Headphone Out", PWRMGMT2,
			 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Headphone Out", PWRMGMT2,
			 8, 0, NULL, 0),

	SND_SOC_DAPM_PGA("Left OUT2 Out", PWRMGMT3,
			 6, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right OUT2 Out", PWRMGMT3,
			 5, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("OUT4 VMID", PWRMGMT3,
			   8, 0, NULL, 0),

	SND_SOC_DAPM_MICBIAS("Mic Bias", PWRMGMT1, 4, 0),

	SND_SOC_DAPM_INPUT("LMICN"),
	SND_SOC_DAPM_INPUT("LMICP"),
	SND_SOC_DAPM_INPUT("RMICN"),
	SND_SOC_DAPM_INPUT("RMICP"),
	SND_SOC_DAPM_INPUT("LAUX"),
	SND_SOC_DAPM_INPUT("RAUX"),
	SND_SOC_DAPM_INPUT("L2"),
	SND_SOC_DAPM_INPUT("R2"),
	SND_SOC_DAPM_OUTPUT("LHP"),
	SND_SOC_DAPM_OUTPUT("RHP"),
	SND_SOC_DAPM_OUTPUT("LOUT2"),
	SND_SOC_DAPM_OUTPUT("ROUT2"),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* Output mixer */
	{"Right Output Mixer", "PCM Playback Switch", "Right DAC"},
	{"Right Output Mixer", "Aux Playback Switch", "RAUX"},
	{"Right Output Mixer", "Line Bypass Switch", "Right Boost Mixer"},

	{"Left Output Mixer", "PCM Playback Switch", "Left DAC"},
	{"Left Output Mixer", "Aux Playback Switch", "LAUX"},
	{"Left Output Mixer", "Line Bypass Switch", "Left Boost Mixer"},

	/* Outputs */
	{"Right Headphone Out", NULL, "Right Output Mixer"},
	{"RHP", NULL, "Right Headphone Out"},

	{"Left Headphone Out", NULL, "Left Output Mixer"},
	{"LHP", NULL, "Left Headphone Out"},

	{"Right OUT2 Out", NULL, "Right Output Mixer"},
	{"ROUT2", NULL, "Right OUT2 Out"},

	{"Left OUT2 Out", NULL, "Left Output Mixer"},
	{"LOUT2", NULL, "Left OUT2 Out"},

	/* Boost Mixer */
	{"Right ADC", NULL, "Right Boost Mixer"},

	{"Right Boost Mixer", NULL, "RAUX"},
	{"Right Boost Mixer", NULL, "Right Capture PGA"},
	{"Right Boost Mixer", NULL, "R2"},

	{"Left ADC", NULL, "Left Boost Mixer"},

	{"Left Boost Mixer", NULL, "LAUX"},
	{"Left Boost Mixer", NULL, "Left Capture PGA"},
	{"Left Boost Mixer", NULL, "L2"},

	/* Input PGA */
	{"Right Capture PGA", NULL, "Right Input Mixer"},
	{"Left Capture PGA", NULL, "Left Input Mixer"},

	{"Right Input Mixer", "R2 Switch", "R2"},
	{"Right Input Mixer", "MicN Switch", "RMICN"},
	{"Right Input Mixer", "MicP Switch", "RMICP"},

	{"Left Input Mixer", "L2 Switch", "L2"},
	{"Left Input Mixer", "MicN Switch", "LMICN"},
	{"Left Input Mixer", "MicP Switch", "LMICP"},
};

static const int update_reg[] = {
	LDACVOL,
	RDACVOL,
	LADCVOL,
	RADCVOL,
	LINPGAGAIN,
	RINPGAGAIN,
	LOUT1VOL,
	ROUT1VOL,
	LOUT1VOL,
	ROUT1VOL
};

static struct iphone_wm8758_priv {
	unsigned int sysclk;
	struct snd_soc_codec codec;
	struct snd_pcm_hw_constraint_list *sysclk_constraints;
	u16 reg_cache[0x40];
	u16 bb_volume_cache[10];
} priv;

static int bb_volume_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct iphone_wm8758_priv* priv = codec->dai->private_data; //(struct iphone_wm8758_priv*) codec->private_data;
	int val;

	if(ucontrol->value.integer.value[0] > mc->max)
		val = mc->max;
	else
		val = ucontrol->value.integer.value[0];

	priv->bb_volume_cache[mc->reg] = val;

	// TODO
	dev_info(codec->dev, "TODO: set baseband volume %d to %d\n", mc->reg, val);

	return 0;
}

static int bb_volume_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc = (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct iphone_wm8758_priv* priv = codec->dai->private_data; //(struct iphone_wm8758_priv*) codec->private_data;

	ucontrol->value.integer.value[0] = priv->bb_volume_cache[mc->reg];

	return 0;
}

static int wm8978_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, wm8978_dapm_widgets,
				  ARRAY_SIZE(wm8978_dapm_widgets));

	/* set up the WM8978 audio map */
	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	return 0;
}

static int iphone_wm8758_pcm_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_info(codec->dev, "ENTER iphone_wm8758_pcm_startup\n");
	return 0;
}


static int iphone_wm8758_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_info(codec->dev, "ENTER iphone_wm8758_pcm_hw_params\n");
	return 0;
}

static int iphone_wm8758_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;

	dev_info(codec->dev, "ENTER iphone_wm8758_set_dai_fmt %u\n", fmt);
	return 0;
}

static int iphone_wm8758_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;

	dev_info(codec->dev, "ENTER iphone_wm8758_set_dai_sysclk %d %u %d\n", clk_id, freq, dir);
	return 0;
}

static int iphone_wm8758_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_info(codec->dev, "ENTER iphone_wm8758_mute\n");

	if (mute)
		snd_soc_update_bits(codec, DACCTRL, 0x40, 0x40);
	else
		snd_soc_update_bits(codec, DACCTRL, 0x40, 0);

	return 0;
}

static struct snd_soc_dai_ops iphone_wm8758_dai_ops = {
	.startup = iphone_wm8758_pcm_startup,
	.hw_params = iphone_wm8758_pcm_hw_params,
	.set_fmt = iphone_wm8758_set_dai_fmt,
	.set_sysclk = iphone_wm8758_set_dai_sysclk,
	.digital_mute = iphone_wm8758_mute,
};

struct snd_soc_dai iphone_wm8758_dai = {
	.name = "wm8758",
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
	.ops = &iphone_wm8758_dai_ops,
	.symmetric_rates = 1,
};


static int iphone_wm8758_register(void)
{
	int ret;
	int i;

	struct snd_soc_codec *codec = &priv.codec;

	pr_debug("ENTER iphone_wm8758_audio_probe\n");

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	codec->name = "wm8758";
	codec->owner = THIS_MODULE;
	codec->dai = &iphone_wm8758_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = ARRAY_SIZE(priv.reg_cache);
	codec->reg_cache = &priv.reg_cache;

	iphone_wm8758_dai.private_data = &priv;
	iphone_wm8758_dai.dev = codec->dev;

	memcpy(codec->reg_cache, wm8978_reg, sizeof(wm8978_reg));

	ret = snd_soc_codec_set_cache_io(codec, 7, 9, SND_SOC_I2C);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		goto err;
	}

	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register codec: %d\n", ret);
		goto err;
	}

	ret = snd_soc_register_dai(&iphone_wm8758_dai);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to register DAI: %d\n", ret);
		snd_soc_unregister_codec(codec);
		goto err_codec;
	}

	priv.bb_volume_cache[2] = 100;
	priv.bb_volume_cache[0] = 68;

	snd_soc_write(codec, RESET,    0x1ff);    /* Reset */

	snd_soc_write(codec, LOUT1VOL, 0xc0);
	snd_soc_write(codec, ROUT1VOL, 0x1c0);
	snd_soc_write(codec, LOUT2VOL, 0xb9);
	snd_soc_write(codec, ROUT2VOL, 0x1b9);

	snd_soc_write(codec, BIASCTL,  0x100); /* BIASCUT = 1 */

	snd_soc_write(codec, PWRMGMT1, 0x2d);   /* BIASEN = 1, PLLEN = 1, BUFIOEN = 1, VMIDSEL = 1 */
	snd_soc_write(codec, PWRMGMT2, 0x180);
	snd_soc_write(codec, PWRMGMT3, 0x6f);

	snd_soc_write(codec, AINTFCE, 0x10);   /* 16-bit, I2S format */

	snd_soc_write(codec, COMPAND, 0x0);
	snd_soc_write(codec, CLKGEN, 0x14d);
	snd_soc_write(codec, SRATECTRL, 0x0);
	snd_soc_write(codec, GPIOCTL, 0x0);
	snd_soc_write(codec, JACKDETECT0, 0x0);

	snd_soc_write(codec, DACCTRL,  0x3);
	snd_soc_write(codec, LDACVOL,  0xff);
	snd_soc_write(codec, RDACVOL,  0x1ff);

	snd_soc_write(codec, JACKDETECT1, 0x0);

	snd_soc_write(codec, ADCCTL, 0x0);
	snd_soc_write(codec, LADCVOL, 0xff);
	snd_soc_write(codec, RADCVOL, 0xff);

	snd_soc_write(codec, EQ1, 0x12c);
	snd_soc_write(codec, EQ2, 0x2c);
	snd_soc_write(codec, EQ3, 0x2c);
	snd_soc_write(codec, EQ4, 0x2c);
	snd_soc_write(codec, EQ5, 0x2c);

	snd_soc_write(codec, DACLIMIT1, 0x32);
	snd_soc_write(codec, DACLIMIT2, 0x0);

	snd_soc_write(codec, NOTCH1, 0x0);
	snd_soc_write(codec, NOTCH2, 0x0);
	snd_soc_write(codec, NOTCH3, 0x0);
	snd_soc_write(codec, NOTCH4, 0x0);

	snd_soc_write(codec, PLLN, 0xa);

	snd_soc_write(codec, PLLK1, 0x1);
	snd_soc_write(codec, PLLK2, 0x1fd);
	snd_soc_write(codec, PLLK3, 0x1e8);

	snd_soc_write(codec, THREEDCTL, 0x0);
	snd_soc_write(codec, OUT4ADC, 0x0);
	snd_soc_write(codec, BEEPCTRL, 0x0);

	snd_soc_write(codec, INCTRL, 0x0);
	snd_soc_write(codec, LINPGAGAIN, 0x40);
	snd_soc_write(codec, RINPGAGAIN, 0x140);

	snd_soc_write(codec, LADCBOOST, 0x0);
	snd_soc_write(codec, RADCBOOST, 0x0);

	snd_soc_write(codec, OUTCTRL,  0x186);   /* Thermal shutdown, DACL2RMIX = 1, DACR2LMIX = 1, SPKBOOST = 1 */
	snd_soc_write(codec, LOUTMIX, 0x15);
	snd_soc_write(codec, ROUTMIX, 0x15);

	snd_soc_write(codec, OUT3MIX,  0x40);
	snd_soc_write(codec, OUT4MIX,  0x40);

	snd_soc_write(codec, WMREG_3E, 0x8c90);

	for (i = 0; i < ARRAY_SIZE(update_reg); i++)
		((u16 *)codec->reg_cache)[update_reg[i]] |= 0x100;

	dev_info(codec->dev, "DAI and codec registered\n");

	return 0;

err_codec:
	snd_soc_unregister_codec(codec);
err:
	return ret;
}

static int iphone_wm8758_unregister(void)
{
	struct snd_soc_codec *codec = &priv.codec;

	snd_soc_unregister_dai(&iphone_wm8758_dai);
	snd_soc_unregister_codec(codec);

	return 0;
}

static int soc_codec_dev_iphone_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_soc_codec *codec;
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	pr_debug("ENTER soc_codec_dev_iphone_probe\n");
	socdev->card->codec = &priv.codec;
	codec = &priv.codec;

	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		pr_debug(KERN_ERR "iphone-wm8758-audio: failed to create pcms\n");
		goto pcm_err;
	}

	snd_soc_add_controls(codec, wm8978_snd_controls,
			     ARRAY_SIZE(wm8978_snd_controls));
	wm8978_add_widgets(codec);

	/*ret = snd_soc_init_card(socdev);
	if (ret < 0) {
		printk(KERN_ERR "iphone-wm8758-audio: failed to register card\n");
		goto card_err;
	}*/

	return ret;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
	return ret;

pcm_err:
	return ret;
}

static int soc_codec_dev_iphone_remove(struct platform_device *pdev)
{
	pr_debug("ENTER soc_codec_dev_iphone_remove\n");
	return 0;
}

struct snd_soc_codec_device soc_codec_dev_wm8758 = {
	.probe          = soc_codec_dev_iphone_probe,
	.remove         = soc_codec_dev_iphone_remove,
};

static int wm8758_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct iphone_wm8758_priv *wm8758;
	struct snd_soc_codec *codec;

	wm8758 = &priv;

	codec = &wm8758->codec;

	i2c_set_clientdata(i2c, wm8758);
	codec->control_data = i2c;

	codec->dev = &i2c->dev;

	return iphone_wm8758_register();
}

static int wm8758_i2c_remove(struct i2c_client *client)
{
	struct wm8758_priv *wm8758 = i2c_get_clientdata(client);
	iphone_wm8758_unregister();
	return 0;
}

static const struct i2c_device_id wm8758_i2c_id[] = {
	{ "wm8758", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8758_i2c_id);

static struct i2c_driver wm8758_i2c_driver = {
	.driver = {
		.name = "wm8758",
		.owner = THIS_MODULE,
	},
	.probe = wm8758_i2c_probe,
	.remove = wm8758_i2c_remove,
	.id_table = wm8758_i2c_id,
};

static int __init wm8758_modinit(void)
{
	int ret;

	ret = i2c_add_driver(&wm8758_i2c_driver);
	if (ret != 0)
		pr_err("WM8758: Unable to register I2C driver: %d\n", ret);
	return ret;
}
module_init(wm8758_modinit);

static void __exit wm8758_exit(void)
{
	i2c_del_driver(&wm8758_i2c_driver);
}
module_exit(wm8758_exit);


MODULE_DESCRIPTION("ASoC WM8758 driver");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
