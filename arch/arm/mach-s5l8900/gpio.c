#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/sysdev.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio_keys.h>
#include <linux/switch.h>
#include <linux/platform_device.h>

#include <mach/iphone-clock.h>
#include <mach/gpio.h>
#include <linux/irq.h>
#include <asm/mach-types.h>

#define GET_BITS(x, start, length) ((((u32)(x)) << (32 - ((start) + (length)))) >> (32 - (length)))

#define GPIO_INTTYPE_MASK 0x1
#define GPIO_INTTYPE_EDGE 0x0
#define GPIO_INTTYPE_LEVEL GPIO_INTTYPE_MASK

#define GPIO_INTLEVEL_MASK 0x2
#define GPIO_INTLEVEL_LOW 0x0
#define GPIO_INTLEVEL_HIGH GPIO_INTLEVEL_MASK

#define GPIO_AUTOFLIP_MASK 0x4
#define GPIO_AUTOFLIP_NO 0x0
#define GPIO_AUTOFLIP_YES GPIO_AUTOFLIP_MASK

bool autoflip[IPHONE_NR_GPIO_IRQS];

static GPIORegisters* GPIORegs = (GPIORegisters*) GPIO;

static irqreturn_t gpio_handle_interrupt(int irq, void* pToken);
static void iphone_gpio_interrupt_enable(struct irq_data *irq);
static void iphone_gpio_interrupt_disable(struct irq_data *irq);
static void iphone_gpio_interrupt_ack(struct irq_data *irq);
static int iphone_gpio_interrupt_set_type(struct irq_data *irq, unsigned int flags);

static struct irq_chip iphone_gpio_irq_chip = {
	.name = "iphone_gpio",
	.irq_mask = iphone_gpio_interrupt_disable,
	.irq_unmask = iphone_gpio_interrupt_enable,
	.irq_ack = iphone_gpio_interrupt_ack,
	.irq_set_type = iphone_gpio_interrupt_set_type,
};

static struct gpio_keys_button iphone_gpio_keys_table[] = {
	{
		.gpio = GPIO_BUTTONS_HOME,
		.code = 229,
		.desc = "MENU",
		.active_low = 0,
		.wakeup = 0,
		.debounce_interval = 20,
	},
	{
		.gpio = GPIO_BUTTONS_HOLD,
		.code = 158,
		.desc = "BACK",
		.active_low = 0,
		.wakeup = 0,
		.debounce_interval = 20,
	},
	/* we'll map these to more useful keys until somebody comes up with a better solution */
    {
            .gpio = GPIO_BUTTONS_VOLUP,
            .code = 61,
            .desc = "CALL",
            .active_low = 1,
            .wakeup = 0,
            .debounce_interval = 20,
    },
    {
            .gpio = GPIO_BUTTONS_VOLDOWN,
            .code = 102,
            .desc = "CALL",
            .active_low = 1,
            .wakeup = 0,
            .debounce_interval = 20,
    },
/*	{
		.gpio = GPIO_BUTTONS_VOLUP,
		.code = 115,
		.desc = "VOLUME_UP",
		.active_low = 1,
		.wakeup = 0,
		.debounce_interval = 20,
	},
	{
		.gpio = GPIO_BUTTONS_VOLDOWN,
		.code = 114,
		.desc = "VOLUME_DOWN",
		.active_low = 1,
		.wakeup = 0,
		.debounce_interval = 20,
	},*/
};

static struct gpio_keys_platform_data iphone_gpio_keys_data = {
	.buttons = iphone_gpio_keys_table,
	.nbuttons = ARRAY_SIZE(iphone_gpio_keys_table),
};

static struct platform_device iphone_device_gpiokeys = {
	.name = "gpio-keys",
	.dev = {
		.platform_data = &iphone_gpio_keys_data,
	},
};

static struct gpio_switch_platform_data headset_switch_data = {
	.name = "h2w",
};

static struct platform_device headset_switch_device = {
	.name = "switch-gpio",
	.dev = {
		.platform_data = &headset_switch_data,
	}
}; 

static int iphone_gpio_setup(void) {
	int i;
	int ret;

	if(machine_is_ipod_touch_1g())
		iphone_gpio_keys_table[0].gpio = GPIO_BUTTONS_HOME_IPOD;

	for(i = 0; i < GPIO_NUMINTGROUPS; i++) {
		// writes to all the interrupt status register to acknowledge and discard any pending
		writel(GPIO_INTSTAT_RESET, GPIOIC + GPIO_INTSTAT + (i * 0x4));

		// disable all interrupts
		writel(GPIO_INTEN_RESET, GPIOIC + GPIO_INTEN + (i * 0x4));
	}

	for(i = 0; i < IPHONE_NR_GPIO_IRQS; i++)
	{
		autoflip[i] = false;
		irq_set_chip(IPHONE_GPIO_IRQS + i, &iphone_gpio_irq_chip);
		irq_set_handler(IPHONE_GPIO_IRQS + i, handle_level_irq);
		set_irq_flags(IPHONE_GPIO_IRQS + i, IRQF_VALID);
	}

        ret = request_irq(0x21, gpio_handle_interrupt, IRQF_DISABLED, "iphone_gpio", (void*) 0);
	if(ret)
		return ret;

        ret = request_irq(0x20, gpio_handle_interrupt, IRQF_DISABLED, "iphone_gpio", (void*) 1);
	if(ret)
		return ret;

        ret = request_irq(0x1f, gpio_handle_interrupt, IRQF_DISABLED, "iphone_gpio", (void*) 2);
	if(ret)
		return ret;

        ret = request_irq(0x03, gpio_handle_interrupt, IRQF_DISABLED, "iphone_gpio", (void*) 3);
	if(ret)
		return ret;

        ret = request_irq(0x02, gpio_handle_interrupt, IRQF_DISABLED, "iphone_gpio", (void*) 4);
	if(ret)
		return ret;

        ret = request_irq(0x01, gpio_handle_interrupt, IRQF_DISABLED, "iphone_gpio", (void*) 5);
	if(ret)
		return ret;

        ret = request_irq(0x00, gpio_handle_interrupt, IRQF_DISABLED, "iphone_gpio", (void*) 6);
	if(ret)
		return ret;

	iphone_clock_gate_switch(GPIO_CLOCKGATE, 1);

	platform_device_register(&iphone_device_gpiokeys);

	// TODO: This needs to end up in mach_x.c -- Ricky26
	{
		int has_switch = 0;

		if(machine_is_iphone_2g())
		{
			headset_switch_data.gpio = 0x1402;
			headset_switch_data.state_on = "0";
			headset_switch_data.state_off = "1";
			has_switch = 1;
		}

		if(machine_is_ipod_touch_1g())
		{
			headset_switch_data.gpio = 0x1205;
			headset_switch_data.state_on = "1";
			headset_switch_data.state_off = "0";
			has_switch = 1;
		}

		if(has_switch)
			platform_device_register(&headset_switch_device);
	}

	printk("iphone-gpio: GPIO input devices registered\n");
	return 0;
}

module_init(iphone_gpio_setup);

int iphone_gpio_pin_state(int port) {
	return ((GPIORegs[GET_BITS(port, 8, 5)].DAT & (1 << GET_BITS(port, 0, 3))) != 0);
}
EXPORT_SYMBOL(iphone_gpio_pin_state);

void iphone_gpio_custom_io(int port, int bits) {
	writel(((GET_BITS(port, 8, 5) & GPIO_IO_MAJMASK) << GPIO_IO_MAJSHIFT)
				| ((GET_BITS(port, 0, 3) & GPIO_IO_MINMASK) << GPIO_IO_MINSHIFT)
				| ((bits & GPIO_IO_UMASK) << GPIO_IO_USHIFT), GPIO + GPIO_IO);
}
EXPORT_SYMBOL(iphone_gpio_custom_io);

void iphone_gpio_pin_use_as_input(int port) {
	iphone_gpio_custom_io(port, 0);
}
EXPORT_SYMBOL(iphone_gpio_pin_use_as_input);

void iphone_gpio_pin_reset(int port) {
	iphone_gpio_custom_io(port, 0);
}
EXPORT_SYMBOL(iphone_gpio_pin_reset);

void iphone_gpio_pin_output(int port, int bit) {
	iphone_gpio_custom_io(port, 0xE | bit); // 0b111U, where U is the argument
}
EXPORT_SYMBOL(iphone_gpio_pin_output);

int iphone_gpio_detect_configuration(void) {
	static int hasDetected = 0;
	static int detectedConfig = 0;

	if(hasDetected) {
		return detectedConfig;
	}

	detectedConfig = (iphone_gpio_pin_state(GPIO_DETECT3) ? 1 : 0) | ((iphone_gpio_pin_state(GPIO_DETECT2) ? 1 : 0) << 1) | ((iphone_gpio_pin_state(GPIO_DETECT1) ? 1 : 0) << 2);
	hasDetected = 1;
	return detectedConfig;
}

static void iphone_gpio_interrupt_enable(struct irq_data *irq)
{
	int interrupt = irq->irq - IPHONE_GPIO_IRQS;
	int group = interrupt >> 5;
	int index = interrupt & 0x1f;

	writel(readl(GPIOIC + GPIO_INTEN + (0x4 * group)) | (1 << index), GPIOIC + GPIO_INTEN + (0x4 * group));
}

static void iphone_gpio_interrupt_disable(struct irq_data *irq)
{
	int interrupt = irq->irq - IPHONE_GPIO_IRQS;
	int group = interrupt >> 5;
	int index = interrupt & 0x1f;
	int mask = ~(1 << index);

	writel(readl(GPIOIC + GPIO_INTEN + (0x4 * group)) & mask, GPIOIC + GPIO_INTEN + (0x4 * group));
}

static void iphone_gpio_interrupt_ack(struct irq_data *irq)
{
	int interrupt = irq->irq - IPHONE_GPIO_IRQS;
	int group = interrupt >> 5;
	int index = interrupt & 0x1f;
	writel(1 << index, GPIOIC + GPIO_INTSTAT + (0x4 * group));
}


static int iphone_gpio_interrupt_set_type(struct irq_data *irq, unsigned int flags)
{
	int interrupt = irq->irq - IPHONE_GPIO_IRQS;
	int group = interrupt >> 5;
	int index = interrupt & 0x1f;
	int mask = ~(1 << index);
	int level;
	int type;

	if((flags & IRQ_TYPE_EDGE_FALLING) || (flags & IRQ_TYPE_EDGE_RISING))
	{
		type = 0;
		irq_set_handler(irq->irq, handle_edge_irq);
	}

	if((flags & IRQ_TYPE_LEVEL_HIGH) || (flags & IRQ_TYPE_LEVEL_HIGH))
	{
		type = 1;
		irq_set_handler(irq->irq, handle_level_irq);
	}

	if((flags & IRQ_TYPE_EDGE_FALLING) || (flags & IRQ_TYPE_LEVEL_LOW))
		level = 0;

	if((flags & IRQ_TYPE_EDGE_RISING) || (flags & IRQ_TYPE_LEVEL_HIGH))
		level = 1;

	if((flags & IRQ_TYPE_EDGE_FALLING) && (flags & IRQ_TYPE_EDGE_RISING))
		autoflip[interrupt] = true;
	else if((flags & IRQ_TYPE_LEVEL_LOW) && (flags & IRQ_TYPE_LEVEL_HIGH))
		autoflip[interrupt] = true;
	else
		autoflip[interrupt] = false;

	writel((readl(GPIOIC + GPIO_INTTYPE + (0x4 * group)) & mask) | ((type ? 1 : 0) << index), GPIOIC + GPIO_INTTYPE + (0x4 * group));

	writel((readl(GPIOIC + GPIO_INTLEVEL + (0x4 * group)) & mask) | ((level ? 1 : 0) << index), GPIOIC + GPIO_INTLEVEL + (0x4 * group));

	return 0;
}

static irqreturn_t gpio_handle_interrupt(int irq, void* pToken)
{
	u32 token = (u32) pToken;
	u32 statReg = GPIOIC + GPIO_INTSTAT + (0x4 * token);

	int stat;

	while((stat = readl(statReg)) != 0)
	{
		int i;
		for(i = 0; i < 32; ++i)
		{
			if(stat & 1)
			{
				int num = (token << 5) + i;

				if(autoflip[num])
					writel(readl(GPIOIC + GPIO_INTLEVEL + (0x4 * token)) ^ (1 << i), GPIOIC + GPIO_INTLEVEL + (0x4 * token));

				generic_handle_irq(IPHONE_GPIO_IRQS + num);
			}

			stat >>= 1;
		}
	}

	return IRQ_HANDLED;
}

int gpio_to_irq(unsigned gpio)
{
	if(gpio >= 0x1600 && gpio < 0x1700)
	{
		return IPHONE_GPIO_IRQS + (gpio - 0x1600 + 0x28);
	} else if(gpio == 0x1402)
	{
		return IPHONE_GPIO_IRQS + 0x3A;
	} else if(gpio == 0x1205)
	{
		return IPHONE_GPIO_IRQS + 0x4D;
	}

	return -1;
}
EXPORT_SYMBOL(gpio_to_irq);

int irq_to_gpio(unsigned irq)
{
	if(irq > IPHONE_GPIO_IRQS)
	{
		if(irq >= (IPHONE_GPIO_IRQS + 0x28))
			return (irq - IPHONE_GPIO_IRQS - 0x28) + 0x1600;
		else
			return -1;
	}

	return -1;
}
EXPORT_SYMBOL(irq_to_gpio);
