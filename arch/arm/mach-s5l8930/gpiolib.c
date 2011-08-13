/**
 * Copyright (c) 2011 Richard Ian Taylor.
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/bitops.h>

#include <asm/mach-types.h>

#include <mach/map.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <plat/irqs.h>
#include <plat/gpio-core.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-cfg-helpers.h>

static DEFINE_SPINLOCK(gpio_lock);
static u16 s5l8930_gpio_cache[ARCH_NR_GPIOS];
static u16 *s5l8930_gpio_reset;

static u16 s5l8930_gpio_reset_ipad[ARCH_NR_GPIOS] = {
	0x210, 0x210, 0x390, 0x390, 0x210, 0x212, 0x1E, 0x612,
	0x1E, 0x612, 0x612, 0x210, 0x210, 0x390, 0x612, 0x613,
	0x1E, 0x210, 0x390, 0x612, 0x612, 0x290, 0x212, 0x613,
	0x612, 0x1E, 0x210, 0x212, 0x1E, 0x1E, 0x210, 0x1E,
	0x1E, 0x390, 0x212, 0x390, 0x390, 0x390, 0x390, 0x1E,
	0x1E, 0x1E, 0xA30, 0xA30, 0xA30, 0x613, 0xA30, 0xA30,
	0xA30, 0x612, 0xA30, 0xA30, 0xA30, 0x612, 0xA30, 0xA30,
	0xA30, 0xA30, 0xA30, 0xA30, 0x613, 0x613, 0xA30, 0xA30,
	0xA13, 0xA30, 0x213, 0x9E, 0x613, 0x41E, 0xA30, 0xA30,
	0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0xA50, 0xA50, 0xA30,
	0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0xA30,
	0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0xA30,
	0xA30, 0x212, 0x212, 0x212, 0x212, 0xA30, 0xA30, 0xA30,
	0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0xA30,
	0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0x212, 0xA30, 0x613,
	0x390, 0xA30, 0x612, 0xA12, 0x1E, 0xA30, 0xA30, 0xA30,
	0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0xA30, 0x1E, 0xA30,
	0xA30, 0xA30, 0xA30, 0x213, 0x212, 0x1E, 0x230, 0xA30,
	0xA30, 0xA30, 0xBB0, 0xBB0, 0xBB0, 0xBB0, 0xBB0, 0x212,
	0x212, 0x612, 0x212, 0x212, 0x212, 0x612, 0x612, 0x212,
	0x212, 0x612, 0x610, 0x612, 0x612, 0x210, 0x210, 0x212,
	0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
};

static u16 s5l8930_gpio_reset_others[ARCH_NR_GPIOS] = {
	0x210, 0x210, 0x390, 0x390, 0x210, 0x290, 0x213, 0x212,
	0x213, 0x212, 0x213, 0x290, 0x290, 0x390, 0x212, 0x1E,
	0x212, 0x212, 0x390, 0x212, 0x212, 0x290, 0x212, 0x390,
	0x210, 0x1E, 0x290, 0x212, 0x1E, 0x1E, 0x213, 0x212,
	0x390, 0x290, 0x212, 0x1E, 0x390, 0x390, 0x390, 0x1E,
	0x1E, 0x1E, 0x630, 0x630, 0x630, 0x213, 0x630, 0x630,
	0x630, 0x212, 0x630, 0x630, 0x630, 0x212, 0x630, 0x630,
	0x630, 0x630, 0x630, 0x630, 0x1E, 0x1E, 0x630, 0x630,
	0x613, 0x630, 0x630, 0x630, 0x630, 0x630, 0x230, 0x230,
	0x230, 0x1E, 0x1E, 0x230, 0x230, 0x250, 0x250, 0x230,
	0x230, 0xA30, 0xA30, 0xA30, 0xA30, 0xAB0, 0xAB0, 0xBB0,
	0xBB0, 0xAB0, 0xAB0, 0xAB0, 0xAB0, 0xAB0, 0xAB0, 0xAB0,
	0xAB0, 0x81E, 0x81E, 0x81E, 0x81E, 0xA30, 0xA30, 0xA30,
	0xA30, 0xAB0, 0xAB0, 0xBB0, 0xBB0, 0xAB0, 0xAB0, 0xAB0,
	0xAB0, 0xAB0, 0xAB0, 0xAB0, 0xAB0, 0x1E, 0xA30, 0x230,
	0x212, 0x230, 0x213, 0x212, 0x630, 0x630, 0x630, 0x630,
	0x630, 0x630, 0x630, 0x630, 0x630, 0x630, 0x630, 0x630,
	0x630, 0x630, 0x630, 0x1E, 0x212, 0x1E, 0x230, 0x630,
	0x630, 0xE30, 0xFB0, 0xFB0, 0xFB0, 0xFB0, 0xFB0, 0x81E,
	0x81E, 0x81E, 0x81E, 0x1E, 0x1E, 0x1E, 0x210, 0x210,
	0x210, 0x212, 0x210, 0x212, 0x1E, 0x210, 0x210, 0x212,
	0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
};

static void s5l8930_gpioic_mask(struct irq_data *_data)
{
	u32 block = EINT_OFFSET(_data->irq);
	u32 pin = IRQ_EINT_BIT(_data->irq);

	__raw_writel(1 << pin, VA_GPIO + S5L_GPIO_INTDIS(block));
}

static void s5l8930_gpioic_unmask(struct irq_data *_data)
{
	u32 block = EINT_OFFSET(_data->irq);
	u32 pin = IRQ_EINT_BIT(_data->irq);

	spin_lock(&gpio_lock);
	if((s5l8930_gpio_cache[_data->irq] & 0xc) == 4) // autoflip
		__raw_writel(1 << pin, VA_GPIO + S5L_GPIO_INTSTS(block));
	spin_unlock(&gpio_lock);

	__raw_writel(1 << pin, VA_GPIO + S5L_GPIO_INTEN(block));
}

static void s5l8930_gpioic_ack(struct irq_data *_data)
{
	u32 block = EINT_OFFSET(_data->irq);
	u32 pin = IRQ_EINT_BIT(_data->irq);

	__raw_writel(1 << pin, VA_GPIO + S5L_GPIO_INTSTS(block));
}

static void s5l8930_gpioic_maskack(struct irq_data *_data)
{
	u32 block = EINT_OFFSET(_data->irq);
	u32 pin = IRQ_EINT_BIT(_data->irq);

	__raw_writel(1 << pin, VA_GPIO + S5L_GPIO_INTDIS(block));
	__raw_writel(1 << pin, VA_GPIO + S5L_GPIO_INTSTS(block));
}

static int s5l8930_gpioic_set_type(struct irq_data *_data, unsigned int _type)
{
	u8 mode;
	u16 value;
	switch(_type)
	{
	case IRQ_TYPE_EDGE_BOTH:
	case IRQ_TYPE_EDGE_RISING:
		mode = 0x4;
		break;

	case IRQ_TYPE_EDGE_FALLING:
		mode = 0x6;
		break;

	case IRQ_TYPE_LEVEL_LOW:
		mode = 0x8;
		break;

	case IRQ_TYPE_LEVEL_HIGH:
		mode = 0xa;
		break;

	default:
		return EINVAL;
	}

	spin_lock(&gpio_lock);
	value = s5l8930_gpio_cache[_data->irq];
	value &=~ 0xE;
	value |= 0x200 | mode;
	s5l8930_gpio_cache[_data->irq] = value;
	__raw_writel(value, VA_GPIO + S5L_GPIO_PIN(_data->irq));
	spin_unlock(&gpio_lock);

	return 0;
}

static void s5l8930_gpioic_demux(unsigned int irq, struct irq_desc *desc)
{
	u32 gsts, sts, block, pin, gpio, done;

	for(;;)
	{
		gsts = __raw_readl(VA_GPIO + S5L_GPIO_INTBLK);
		if(!gsts)
			break;

		block = fls(gsts)-1;
		sts = __raw_readl(VA_GPIO + S5L_GPIO_INTSTS(block));
		if(sts)
		{
			pin = fls(sts)-1;
			gpio = (block*8) + pin;

			spin_lock(&gpio_lock);
			if((s5l8930_gpio_cache[gpio] & 0xC) == 4)
			{
				done = 1;
				__raw_writel(1 << pin, VA_GPIO + S5L_GPIO_INTSTS(block));
			}
			else
				done = 0;
			spin_unlock(&gpio_lock);

			generic_handle_irq(IRQ_EINT(gpio));

			if(!done)
				__raw_writel(1 << pin, VA_GPIO + S5L_GPIO_INTSTS(block));
		}
	}
}

static struct irq_chip s5l8930_gpioic = {
	.name = "s5l8930-gpioic",
	.irq_mask = s5l8930_gpioic_mask,
	.irq_unmask = s5l8930_gpioic_unmask,
	.irq_mask_ack = s5l8930_gpioic_maskack,
	.irq_ack = s5l8930_gpioic_ack,
	.irq_set_type = s5l8930_gpioic_set_type,
};

static void s5l8930_gpio_resume(struct s3c_gpio_chip *_chip)
{
	int i;
	for(i = 0; i < ARCH_NR_GPIOS; i++)
		__raw_writel(s5l8930_gpio_cache[i], VA_GPIO + S5L_GPIO_PIN(i));
}

static struct s3c_gpio_pm s5l8930_gpio_pm = {
	.resume = s5l8930_gpio_resume,
};

static s3c_gpio_pull_t s5l8930_gpio_get_pull(struct s3c_gpio_chip *_chip,
		unsigned _off)
{
	u16 cfg;

	spin_lock(&gpio_lock);
	cfg = s5l8930_gpio_cache[_off];
	spin_unlock(&gpio_lock);

	switch((cfg >> 7) & 3)
	{
	case 1:
		return S3C_GPIO_PULL_DOWN;

	case 3:
		return S3C_GPIO_PULL_UP;

	default:
		return S3C_GPIO_PULL_NONE;
	}
}

static int s5l8930_gpio_set_pull(struct s3c_gpio_chip *_chip,
		unsigned _off, s3c_gpio_pull_t _pull)
{
	u16 cfg, value;
	switch(_pull)
	{
	case S3C_GPIO_PULL_DOWN:
		cfg = 1;
		break;

	case S3C_GPIO_PULL_UP:
		cfg = 3;
		break;

	case S3C_GPIO_PULL_NONE:
		cfg = 0;
		break;

	default:
		return EINVAL;
	};

	spin_lock(&gpio_lock);
	value = s5l8930_gpio_cache[_off];
	cfg |= (value &~ (3 << 7));
	s5l8930_gpio_cache[_off] = cfg;

	__raw_writel(cfg, VA_GPIO + S5L_GPIO_PIN(_off));
	spin_unlock(&gpio_lock);

	return 0;
}

static unsigned s5l8930_gpio_get_config(struct s3c_gpio_chip *_chip,
		unsigned _off)
{
	u16 val;

	spin_lock(&gpio_lock);
	val = s5l8930_gpio_cache[_off];
	spin_unlock(&gpio_lock);

	return val;
}

static int s5l8930_gpio_configure(unsigned _idx, unsigned _cfg)
{
	u16 bitmask, value;
	BUG_ON(_idx >= ARCH_NR_GPIOS);

	switch(_cfg)
	{
	case 0: // use_as_input
		value = 0x210;
		bitmask = 0x27E;
		break;

	case 1: // use_as_output
		value = 0x212;
		bitmask = 0x27E;
		break;

	case 2: // clear_output
		value = 0x212;
		bitmask = 0x27F;
		break;

	case 3: // set_output
		value = 0x213;
		bitmask = 0x27F;
		break;

	case 4: // reset
		value = s5l8930_gpio_reset[_idx];
		bitmask = 0x3FF;
		break;

	case 5:
		value = 0x230;
		bitmask = 0x27E;
		value &= ~0x10;
		break;

	case 6:
		value = 0x250;
		bitmask = 0x27E;
		value &= ~0x10;
		break;

	case 7:
		value = 0x270;
		bitmask = 0x27E;
		value &= ~0x10;
		break;

	default:
		return EINVAL;
	}

	spin_lock(&gpio_lock);
	value = (s5l8930_gpio_cache[_idx] &~ bitmask)
		| (value & bitmask);
	s5l8930_gpio_cache[_idx] = value;
	__raw_writel(value, VA_GPIO + S5L_GPIO_PIN(_idx));
	spin_unlock(&gpio_lock);

	return 0;
}

static int s5l8930_gpio_set_config(struct s3c_gpio_chip *_chip,
		unsigned _off, unsigned _val)
{
	if(!(_val & S3C_GPIO_SPECIAL_MARK))
		return s5l8930_gpio_configure(_off, _val & 0xF);
	
	spin_lock(&gpio_lock);
	s5l8930_gpio_cache[_off] = _val;
	__raw_writel(_val, VA_GPIO + S5L_GPIO_PIN(_off));
	spin_unlock(&gpio_lock);
	return 0;
}

static struct s3c_gpio_cfg s5l8930_gpio_cfg = {
	.get_pull = s5l8930_gpio_get_pull,
	.set_pull = s5l8930_gpio_set_pull,
	
	.get_config = s5l8930_gpio_get_config,
	.set_config = s5l8930_gpio_set_config,
};

static int s5l8930_gpio_direction_input(struct gpio_chip *_chip,
		unsigned _off)
{
	return s5l8930_gpio_configure(_off, 0);
}

static int s5l8930_gpio_direction_output(struct gpio_chip *_chip,
		unsigned _off, int _val)
{
	return s5l8930_gpio_configure(_off, 2 + (_val&1));
}

static int s5l8930_gpio_get(struct gpio_chip *_chip,
		unsigned _off)
{
	return __raw_readl(VA_GPIO + S5L_GPIO_PIN(_off)) & 1;
}

static void s5l8930_gpio_set(struct gpio_chip *_chip,
		unsigned _off, int _val)
{
	u32 val;

	spin_lock(&gpio_lock);
	val = s5l8930_gpio_cache[_off] &~ 1;
	if(_val)
		val |= 1;

	s5l8930_gpio_cache[_off] = val;
	__raw_writel(val, VA_GPIO + S5L_GPIO_PIN(_off));
	spin_unlock(&gpio_lock);
}

static inline int s5l8930_gpio_to_irq(struct gpio_chip *_c, unsigned _x)
{
	return IRQ_EINT(_x);
}

// Whilst this is really many blocks of chips
// the register memory is contiguous, so
// it's simplier to pretend it's one big bank.
static struct s3c_gpio_chip s5l8930_gpio_chip = {
	.pm = &s5l8930_gpio_pm,
	.config = &s5l8930_gpio_cfg,

	.base = (void*)1, // silly samsung
	.chip = {
		.ngpio = ARCH_NR_GPIOS,
		.label = "GPIO",

		.direction_input = &s5l8930_gpio_direction_input,
		.direction_output = &s5l8930_gpio_direction_output,
		.get = &s5l8930_gpio_get,
		.set = &s5l8930_gpio_set,
		.to_irq = &s5l8930_gpio_to_irq,
	},
};

static void s5l8930_init_i2c(unsigned _scl, unsigned _sda)
{
	int i;

	s3c_gpio_cfgpin(_sda, S3C_GPIO_SFN(2));
	
	for(i = 0; i < 19; i++)
	{
		s3c_gpio_cfgpin(_scl, (i%2) ? 2 : 0);
		udelay(5);
	}

	s3c_gpio_cfgpin(_scl, 5);
	s3c_gpio_cfgpin(_sda, 5);
}

void s3c_i2c0_cfg_gpio(struct platform_device *dev)
{
	s5l8930_init_i2c(S5L8930_GPIO(0x904), S5L8930_GPIO(0x903));
}

void s3c_i2c1_cfg_gpio(struct platform_device *dev)
{
	s5l8930_init_i2c(S5L8930_GPIO(0x905), S5L8930_GPIO(0x906));
}

static __init int s5l8930_gpio_setup(void)
{
	int i;

	if(machine_is_ipad_1g())
		s5l8930_gpio_reset = s5l8930_gpio_reset_ipad;
	else
		s5l8930_gpio_reset = s5l8930_gpio_reset_others;

	for(i = 0; i < ARCH_NR_GPIOS; i++)
	{
		s5l8930_gpio_cache[i] = s5l8930_gpio_reset[i];
		__raw_writel(s5l8930_gpio_reset[i],
				VA_GPIO + S5L_GPIO_PIN(i));

		irq_set_chip(IRQ_EINT(i), &s5l8930_gpioic);
		set_irq_flags(IRQ_EINT(i), IRQF_VALID);
	}

	irq_set_chained_handler(IRQ_GPIO, s5l8930_gpioic_demux);
	s3c_gpiolib_add(&s5l8930_gpio_chip);

	return 0;
}
core_initcall(s5l8930_gpio_setup);
