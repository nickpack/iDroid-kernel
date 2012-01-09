#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <mach/iphone-i2c.h>
#include <mach/gpio.h>
#include <mach/iphone-clock.h>
#include <mach/hardware.h>
#include <mach/i2c.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/sysdev.h>
#include <linux/io.h>

static const I2CInfo I2CInit[] = {
			{NULL, 0, I2CNoError, 0, 0, 0, I2CDone, 0, 0, 0, 0, 0, NULL, 0, NULL, I2C0_SCL_GPIO, I2C0_SDA_GPIO, I2C0 + IICCON, I2C0 + IICSTAT, I2C0 + IICADD, I2C0 + IICDS, I2C0 + IICLC,
				I2C0 + IICREG14, I2C0 + IICREG18, I2C0 + IICREG1C, I2C0 + IICREG20},
			{NULL, 0, I2CNoError, 0, 0, 0, I2CDone, 0, 0, 0, 0, 0, NULL, 0, NULL, I2C1_SCL_GPIO, I2C1_SDA_GPIO, I2C1 + IICCON, I2C1 + IICSTAT, I2C1 + IICADD, I2C1 + IICDS, I2C1 + IICLC,
				I2C1 + IICREG14, I2C1 + IICREG18, I2C1 + IICREG1C, I2C1 + IICREG20}
		};

static I2CInfo I2C[2];

static void init_i2c(I2CInfo* i2c, u32 frequency);
static I2CError iphone_i2c_readwrite(I2CInfo* i2c);
static void do_i2c(I2CInfo* i2c);

static struct i2c_adapter iphone_i2c0;
static struct i2c_adapter iphone_i2c1;

static int initialized = 0;

static int iphone_i2c_setup(void) {
	printk("iphone-i2c: inside setup...\n");
	memcpy(I2C, I2CInit, sizeof(I2CInfo) * 2);

	I2C[0].rw_sem = (struct mutex *)kzalloc(sizeof(struct mutex), GFP_KERNEL);
	I2C[1].rw_sem = (struct mutex *)kzalloc(sizeof(struct mutex), GFP_KERNEL);

	mutex_init(I2C[0].rw_sem);
	mutex_init(I2C[1].rw_sem);

	iphone_clock_gate_switch(I2C0_CLOCKGATE, 1);
	iphone_clock_gate_switch(I2C1_CLOCKGATE, 1);

	init_i2c(&I2C[0], FREQUENCY_PERIPHERAL / 100000);
	init_i2c(&I2C[1], FREQUENCY_PERIPHERAL / 100000);
	printk("iphone-i2c: done setup!\n");
	initialized = 1;
	return 0;
}

I2CError iphone_i2c_recv(int bus, int iicaddr, int send_stop, void* buffer, int len) {
	I2CError ret;
	mutex_lock(I2C[bus].rw_sem);
	I2C[bus].address = iicaddr;
	I2C[bus].is_write = 0;
	I2C[bus].send_stop = send_stop;
	I2C[bus].bufferLen = len;
	I2C[bus].buffer = (uint8_t*) buffer;
	ret = iphone_i2c_readwrite(&I2C[bus]);
	mutex_unlock(I2C[bus].rw_sem);
	return ret;
}

I2CError iphone_i2c_send(int bus, int iicaddr, int send_stop, const void* buffer, int len) {
	I2CError ret;
	mutex_lock(I2C[bus].rw_sem);
	I2C[bus].address = iicaddr;
	I2C[bus].is_write = 1;
	I2C[bus].send_stop = send_stop;
	I2C[bus].bufferLen = len;
	I2C[bus].buffer = (uint8_t*) buffer;
	ret = iphone_i2c_readwrite(&I2C[bus]);
	mutex_unlock(I2C[bus].rw_sem);
	return ret;
}

static void init_i2c(I2CInfo* i2c, u32 frequency) {
	int prescaler, divisorRequired;
	int i;
	printk("iphone-i2c: start initializing!\n");
	i2c->frequency = 2560000 / frequency;
	divisorRequired = 640000 / i2c->frequency;
	if(divisorRequired < 512) {
		// round up
		i2c->iiccon_settings = IICCON_INIT | IICCON_TXCLKSRC_FPCLK16;
		prescaler = ((divisorRequired + 0x1F) >> 5) - 1;
	} else {
		i2c->iiccon_settings = IICCON_INIT | IICCON_TXCLKSRC_FPCLK512;
		prescaler = ((divisorRequired + 0x1FF) >> 9) - 1;
	}

	if(prescaler == 0)
		prescaler = 1;

	i2c->iiccon_settings |= prescaler;

	iphone_gpio_custom_io(i2c->iic_sda_gpio, 0xE); // pull sda low?

	for(i = 0; i < 19; i++) {
		iphone_gpio_custom_io(i2c->iic_scl_gpio, ((i % 2) == 1) ? 0xE : 0x0);
		udelay(5);
	}

	iphone_gpio_custom_io(i2c->iic_scl_gpio, 0x2);	// generate stop condition?
	iphone_gpio_custom_io(i2c->iic_sda_gpio, 0x2);
	printk("iphone-i2c: init done!\n");
}

static I2CError iphone_i2c_readwrite(I2CInfo* i2c) {
	__raw_writel(i2c->iiccon_settings, i2c->register_IICCON);

	i2c->iiccon_settings |= IICCON_ACKGEN;
	i2c->state = I2CSetup;
	i2c->operation_result = 0;
	i2c->error_code = I2CNoError;

	__raw_writel(IICCON_INIT, i2c->register_20);

	do_i2c(i2c);

	if(i2c->error_code != I2CNoError)
		return i2c->error_code;

	while(1) {
		int hardware_status;
		do {
			hardware_status = __raw_readl(i2c->register_20);
			__raw_writel(hardware_status, i2c->register_20);
			i2c->operation_result &= ~hardware_status;
			udelay(5);
		} while(hardware_status != 0);

		if(i2c->state == I2CDone) {
			break;
		}

		do_i2c(i2c);
	}
	
	if(i2c->error_code != I2CNoError)
	{
		printk("I2C %s Command Failed!\n", i2c->is_write ? "Write" : "Read");
	}
	return i2c->error_code;
}

static void do_i2c(I2CInfo* i2c) {
	int proceed = 0;
	while(i2c->operation_result == 0 || proceed) {
		proceed = 0;
		switch(i2c->state) {
			case I2CSetup:
				__raw_writel(i2c->iiccon_settings | IICCON_ACKGEN, i2c->register_IICCON);
				i2c->operation_result = OPERATION_SEND;
				if(i2c->is_write) {
					__raw_writel(i2c->address & ~1, i2c->register_IICDS);
					i2c->current_iicstat =
						(IICSTAT_MODE_MASTERTX << IICSTAT_MODE_SHIFT)
						| (IICSTAT_STARTSTOPGEN_START << IICSTAT_STARTSTOPGEN_SHIFT)
						| (1 << IICSTAT_DATAOUTPUT_ENABLE_SHIFT);
					i2c->state = I2CTx;
				} else {
					__raw_writel(i2c->address | 1, i2c->register_IICDS);
					i2c->current_iicstat =
						(IICSTAT_MODE_MASTERRX << IICSTAT_MODE_SHIFT)
						| (IICSTAT_STARTSTOPGEN_START << IICSTAT_STARTSTOPGEN_SHIFT)
						| (1 << IICSTAT_DATAOUTPUT_ENABLE_SHIFT);
					i2c->state = I2CRxSetup;
				}
				__raw_writel(i2c->current_iicstat, i2c->register_IICSTAT);
				i2c->cursor = 0;
				break;
			case I2CTx:
				if((__raw_readl(i2c->register_IICSTAT) & IICSTAT_LASTRECEIVEDBIT) == 0) {
					if(i2c->cursor != i2c->bufferLen) {
						// need to send more from the register list
						i2c->operation_result = OPERATION_SEND;
						__raw_writel(i2c->buffer[i2c->cursor++], i2c->register_IICDS);
						__raw_writel(i2c->iiccon_settings | IICCON_INTPENDING | IICCON_ACKGEN, i2c->register_IICCON);
					} else {
						i2c->state = I2CFinish;
						proceed = 1;
					}
				} else {
					// ack not received
					printk("iphone-i2c: ack not received before byte %d\n", i2c->cursor);
					i2c->error_code = -1;
					i2c->state = I2CFinish;
					proceed = 1;
				}
				break;
			case I2CRx:
				i2c->buffer[i2c->cursor++] = __raw_readl(i2c->register_IICDS);
				// fall into I2CRxSetup
			case I2CRxSetup:
				if(i2c->cursor != 0 || (__raw_readl(i2c->register_IICSTAT) & IICSTAT_LASTRECEIVEDBIT) == 0) {
					if(i2c->cursor != i2c->bufferLen) {
						if((i2c->bufferLen - i2c->cursor) == 1) {
							// last byte
							i2c->iiccon_settings &= ~IICCON_ACKGEN;
						}
						i2c->operation_result = OPERATION_SEND;
						__raw_writel(i2c->iiccon_settings | IICCON_INTPENDING, i2c->register_IICCON);
						i2c->state = I2CRx;
					} else {
						i2c->state = I2CFinish;
					}
				} else {
					i2c->error_code = -1;
					i2c->state = I2CFinish;
					proceed = 1;
				}
				break;
			case I2CFinish:
				i2c->operation_result = OPERATION_CONDITIONCHANGE;
				/* Is this legal? */
				if(i2c->send_stop) {
					i2c->current_iicstat &= ~(IICSTAT_STARTSTOPGEN_MASK << IICSTAT_STARTSTOPGEN_SHIFT);
					if(i2c->is_write) {
						// turn off the tx bit in the mode
						i2c->current_iicstat &= ~(IICSTAT_MODE_SLAVETX << IICSTAT_MODE_SHIFT);
					}
				} else {
					i2c->current_iicstat = (IICSTAT_MODE_MASTERRX << IICSTAT_MODE_SHIFT) | (IICSTAT_STARTSTOPGEN_MASK << IICSTAT_STARTSTOPGEN_SHIFT);
				}

				__raw_writel(i2c->current_iicstat, i2c->register_IICSTAT);
				__raw_writel(i2c->iiccon_settings | IICCON_INTPENDING, i2c->register_IICCON);
				i2c->state =I2CDone;
				break;
			case I2CDone:
				// should not occur
				break;
		}
	}
}

static int iphone_i2c_xfer(struct i2c_adapter *adapter,
			      struct i2c_msg *msgs, int num)
{
	int ret = 0;
	int bus, send_stop, i;
	struct i2c_msg *msg;
	for(i=0; i<num; i++)
	{
		msg = &msgs[i];
		send_stop = (i == num-1) || (msgs[i+1].addr != msg->addr);
		bus = adapter->nr;

		if(msg->flags & I2C_M_RD)
			ret = iphone_i2c_recv(bus, msg->addr, send_stop, msg->buf, msg->len);
		else			
			ret = iphone_i2c_send(bus, msg->addr, send_stop, msg->buf, msg->len);

		if(ret != I2CNoError)
			return -EIO;
	}

	return num;
}

/*	TEMPORARY LEGACY SUPPORT	*/

I2CError iphone_i2c_rx(int bus, int iicaddr, const uint8_t* registers, int num_regs, void* buffer, int len)
{
         I2CError ret;
         struct i2c_msg xfer[2];
 
         /* Write register */
         xfer[0].addr = iicaddr;
         xfer[0].flags = 0;
         xfer[0].len = num_regs;
         xfer[0].buf = (u8 *)registers;

         xfer[1].addr = iicaddr;
         xfer[1].flags = I2C_M_RD;
         xfer[1].len = len;
         xfer[1].buf = (u8 *)buffer;
 
         ret = iphone_i2c_xfer(NULL, xfer, 2);
         return ret;
}

I2CError iphone_i2c_tx(int bus, int iicaddr, void* buffer, int len)
{
         I2CError ret;
         struct i2c_msg xfer[1];
 
         /* Write register */
         xfer[0].addr = iicaddr;
         xfer[0].flags = 0;
         xfer[0].len = len;
         xfer[0].buf = (u8 *)buffer;
 
         ret = iphone_i2c_xfer(NULL, xfer, 1);
         return ret;
}


static u32 iphone_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA;
}

static const struct i2c_algorithm iphone_i2c_algorithm = {
	.master_xfer		= iphone_i2c_xfer,
	.functionality		= iphone_i2c_func,
};

static struct i2c_adapter iphone_i2c0 = {
	.nr 		= 0,
	.owner		= THIS_MODULE,
	.class    	= 0, 
	.algo		= &iphone_i2c_algorithm,
};

static struct i2c_adapter iphone_i2c1 = {
	.nr 		= 1,
	.owner		= THIS_MODULE,
	.class    	= 0, 
	.algo		= &iphone_i2c_algorithm,
};

static int __init iphone_i2c_init(void)
{
	int ret = 0;

	iphone_i2c_setup();	
	printk("iphone-i2c: Initialising I2C busses!\n");

	ret = i2c_add_numbered_adapter(&iphone_i2c0);
	if(ret)
	{
		printk("iphone-i2c: Failed to add I2C Bus #0: %d\n", ret);
		return ret;
	}

	ret = i2c_add_numbered_adapter(&iphone_i2c1);
	if(ret)
	{
		printk("iphone-i2c: Failed to add I2C Bus #1: %d\n", ret);
		return ret;
	}

	return ret;
}

static void __exit iphone_i2c_exit(void)
{
	i2c_del_adapter(&iphone_i2c0);
	i2c_del_adapter(&iphone_i2c1);
}

module_init(iphone_i2c_init);
module_exit(iphone_i2c_exit);

static struct resource iphone_i2c_resources[] = {
	[0] = {
		.start  = I2C0,
		.end    = I2C0 + 0x1000 - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = I2C1,
		.end    = I2C1 + 0x1000 - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device iphone_i2c = {
	.name           = "iphone-i2c",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(iphone_i2c_resources),
	.resource       = iphone_i2c_resources,
};

MODULE_AUTHOR("Fredrik Gustafsson <frgustaf@kth.se>");
MODULE_DESCRIPTION("iPhone i2c adapter");
MODULE_LICENSE("GPL");

