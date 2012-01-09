#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clockchips.h>

#include <asm/system.h>
#include <asm/leds.h>
#include <asm/mach-types.h>

#include <asm/irq.h>
#include <asm/mach/time.h>
#include <mach/hardware.h>
#include <mach/iphone-clock.h>

// Constants
#define EventTimer 4
#define TicksPerSec 12000000

// Devices

#define TIMER IO_ADDRESS(0x3E200000)

// Registers

#define TIMER_0 0x0
#define TIMER_1 0x20
#define TIMER_2 0x40
#define TIMER_3 0x60
#define TIMER_4 0xA0
#define TIMER_5 0xC0
#define TIMER_6 0xE0
#define TIMER_CONFIG 0
#define TIMER_STATE 0x4
#define TIMER_COUNT_BUFFER 0x8
#define TIMER_COUNT_BUFFER2 0xC
#define TIMER_PRESCALER 0x10
#define TIMER_UNKNOWN3 0x14
#define TIMER_TICKSHIGH 0x80
#define TIMER_TICKSLOW 0x84
#define TIMER_UNKREG0 0x88
#define TIMER_UNKREG1 0x8C
#define TIMER_UNKREG2 0x90
#define TIMER_UNKREG3 0x94
#define TIMER_UNKREG4 0x98
#define TIMER_IRQSTAT 0x10000
#define TIMER_IRQLATCH 0xF8

// Timer
#define NUM_TIMERS 7
#define TIMER_CLOCKGATE 0x25
#define TIMER_IRQ 0x7
#define TIMER_STATE_START 1
#define TIMER_STATE_STOP 0
#define TIMER_STATE_MANUALUPDATE 2
#define TIMER_UNKREG0_RESET1 0xA
#define TIMER_UNKREG0_RESET2 0x18010
#define TIMER_UNKREG1_RESET 0xFFFFFFFF
#define TIMER_UNKREG2_RESET 0xFFFFFFFF
#define TIMER_UNKREG3_RESET 0xFFFFFFFF
#define TIMER_UNKREG4_RESET 0xFFFFFFFF
#define TIMER_DIVIDER1 4
#define TIMER_DIVIDER2 0
#define TIMER_DIVIDER4 1
#define TIMER_DIVIDER16 2
#define TIMER_DIVIDER64 3
#define TIMER_SPECIALTIMER_BIT0 0x1000000
#define TIMER_SPECIALTIMER_BIT1 0x2000000

#define TIMER_Separator 4

typedef void (*TimerHandler)(void);

typedef struct TimerRegisters {
	u32	config;
	u32	state;
	u32	count_buffer;
	u32	count_buffer2;
	u32	prescaler;
	u32	cur_count;
} TimerRegisters;

typedef struct TimerInfo {
	int	option6;
	u32	divider;
	u32	unknown1;
	TimerHandler	handler1;
	TimerHandler	handler2;
	TimerHandler	handler3;
} TimerInfo;

const TimerRegisters HWTimers[] = {
		{	TIMER + TIMER_0 + TIMER_CONFIG, TIMER + TIMER_0 + TIMER_STATE, TIMER + TIMER_0 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_0 + TIMER_COUNT_BUFFER2, TIMER + TIMER_0 + TIMER_PRESCALER, TIMER + TIMER_0 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_1 + TIMER_CONFIG, TIMER + TIMER_1 + TIMER_STATE, TIMER + TIMER_1 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_1 + TIMER_COUNT_BUFFER2, TIMER + TIMER_1 + TIMER_PRESCALER, TIMER + TIMER_1 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_2 + TIMER_CONFIG, TIMER + TIMER_2 + TIMER_STATE, TIMER + TIMER_2 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_2 + TIMER_COUNT_BUFFER2, TIMER + TIMER_2 + TIMER_PRESCALER, TIMER + TIMER_2 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_3 + TIMER_CONFIG, TIMER + TIMER_3 + TIMER_STATE, TIMER + TIMER_3 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_3 + TIMER_COUNT_BUFFER2, TIMER + TIMER_3 + TIMER_PRESCALER, TIMER + TIMER_3 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_4 + TIMER_CONFIG, TIMER + TIMER_4 + TIMER_STATE, TIMER + TIMER_4 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_4 + TIMER_COUNT_BUFFER2, TIMER + TIMER_4 + TIMER_PRESCALER, TIMER + TIMER_4 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_5 + TIMER_CONFIG, TIMER + TIMER_5 + TIMER_STATE, TIMER + TIMER_5 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_5 + TIMER_COUNT_BUFFER2, TIMER + TIMER_5 + TIMER_PRESCALER, TIMER + TIMER_5 + TIMER_UNKNOWN3 },
		{	TIMER + TIMER_6 + TIMER_CONFIG, TIMER + TIMER_6 + TIMER_STATE, TIMER + TIMER_6 + TIMER_COUNT_BUFFER,
			TIMER + TIMER_6 + TIMER_COUNT_BUFFER2, TIMER + TIMER_6 + TIMER_PRESCALER, TIMER + TIMER_6 + TIMER_UNKNOWN3 }
	};

TimerInfo Timers[7];

static void timer_init_rtc(void)
{
	__raw_writel(TIMER_UNKREG0_RESET1, TIMER + TIMER_UNKREG0);
	__raw_writel(TIMER_UNKREG2_RESET, TIMER + TIMER_UNKREG2);
	__raw_writel(TIMER_UNKREG1_RESET, TIMER + TIMER_UNKREG1);
	__raw_writel(TIMER_UNKREG4_RESET, TIMER + TIMER_UNKREG4);
	__raw_writel(TIMER_UNKREG3_RESET, TIMER + TIMER_UNKREG3);
	__raw_writel(TIMER_UNKREG0_RESET2, TIMER + TIMER_UNKREG0);
}

int timer_on_off(int timer_id, int on_off) {
	if(timer_id < NUM_TIMERS) {
		if(on_off == 1) {
			__raw_writel(TIMER_STATE_START, HWTimers[timer_id].state);
		} else {
			__raw_writel(TIMER_STATE_STOP, HWTimers[timer_id].state);
		}

		return 0;
	} else if(timer_id == NUM_TIMERS) {
		if(on_off == 1) {
			// clear bits 0, 1, 2, 3
			__raw_writel(__raw_readl(TIMER + TIMER_UNKREG0) & ~(0xF), TIMER + TIMER_UNKREG0);
		} else {
			// set bits 1, 3
			__raw_writel(__raw_readl(TIMER + TIMER_UNKREG0) | 0xA, TIMER + TIMER_UNKREG0);
		}
		return 0;
	} else {
		/* invalid timer id */
		return -1;
	}
}

static int timer_stop_all(void)
{
	int i;
	for(i = 0; i < NUM_TIMERS; i++) {
		timer_on_off(i, 0);
	}
	timer_on_off(NUM_TIMERS, 0);

	return 0;
}

static int timer_setup_clk(int timer_id, int type, int divider, u32 unknown1) {
	if(type == 2) {
		Timers[timer_id].option6 = 0;
		Timers[timer_id].divider = 6;
	} else {
		if(type == 1) {
			Timers[timer_id].option6 = 1;
		} else {
			Timers[timer_id].option6 = 0;
		}

		/* translate divider into divider code */
		switch(divider) {
			case 1:
				Timers[timer_id].divider = TIMER_DIVIDER1;
				break;
			case 2:
				Timers[timer_id].divider = TIMER_DIVIDER2;
				break;
			case 4:
				Timers[timer_id].divider = TIMER_DIVIDER4;
				break;
			case 16:
				Timers[timer_id].divider = TIMER_DIVIDER16;
				break;
			case 64:
				Timers[timer_id].divider = TIMER_DIVIDER64;
				break;
			default:
				/* invalid divider */
				return -1;
		}
	}

	Timers[timer_id].unknown1 = unknown1;

	return 0;
}

int timer_init(int timer_id, u32 interval, u32 interval2, u32 prescaler, u32 z, int option24, int option28, int option11, int option5, int interrupts) {
	u32 config;

	if(timer_id >= NUM_TIMERS || timer_id < 0) {
		return -1;
	}

	/* need to turn it off, since we're messing with the settings */
	timer_on_off(timer_id, 0);

	if(interrupts)
		config = 0x7000; /* set bits 12, 13, 14 */
	else
		config = 0;

	/* these two options are only supported on timers 4, 5, 6 */
	if(timer_id >= TIMER_Separator) {
		config |= (option24 ? (1 << 24) : 0) | (option28 ? 1 << 28: 0);
	}

	/* set the rest of the options */
	config |= (Timers[timer_id].divider << 8)
			| (z << 3)
			| (option5 ? (1 << 5) : 0)
			| (Timers[timer_id].option6 ? (1 << 6) : 0)
			| (option11 ? (1 << 11) : 0);

	__raw_writel(config, HWTimers[timer_id].config);
	__raw_writel(interval, HWTimers[timer_id].count_buffer);
	__raw_writel(interval2, HWTimers[timer_id].count_buffer2);
	__raw_writel(prescaler, HWTimers[timer_id].prescaler);

	// apply the settings
	__raw_writel(TIMER_STATE_MANUALUPDATE, HWTimers[timer_id].state);

	return 0;
}

static void iphone_timer_get_rtc_ticks(u64* ticks) {
	register u32 ticksHigh;
	register u32 ticksLow;
	register u32 ticksHigh2;

	/* try to get a good read where the lower bits remain the same after reading the higher bits */
	do {
		ticksHigh = __raw_readl(TIMER + TIMER_TICKSHIGH);
		ticksLow = __raw_readl(TIMER + TIMER_TICKSLOW);
		ticksHigh2 = __raw_readl(TIMER + TIMER_TICKSHIGH);
	} while(ticksHigh != ticksHigh2);

	*ticks = (((u64)ticksHigh) << 32) | ticksLow;
}

u64 iphone_microtime(void) {
        u64 ticks;

        iphone_timer_get_rtc_ticks(&ticks);
	// FIXME: Unreliable for large tick values
        return ((u32)(ticks >> 2))/3;
}

int iphone_has_elapsed(u64 startTime, u64 elapsedTime) {
	if((iphone_microtime() - startTime) >= elapsedTime)
		return 1;
	else
		return 0;
}

static void callTimerHandler(int timer_id, uint32_t flags) {
	if((flags & (1 << 2)) != 0) {
		if(Timers[timer_id].handler1)
			Timers[timer_id].handler1();
	}

	if((flags & (1 << 1)) != 0) {
		if(Timers[timer_id].handler3)
			Timers[timer_id].handler3();
	}

	if((flags & (1 << 0)) != 0) {
		if(Timers[timer_id].handler2)
			Timers[timer_id].handler2();
	}
}

static irqreturn_t iphone_timer_interrupt(int irq, void* dev_id) {
	int i;
	/* this function does not implement incrementing a counter at dword_18022B28 like Apple's */
	uint32_t stat = __raw_readl(TIMER + TIMER_IRQSTAT);

	/* signal timer is being handled */
	volatile register uint32_t discard = __raw_readl(TIMER + TIMER_IRQLATCH); discard --;

	if(stat & TIMER_SPECIALTIMER_BIT0) {
		__raw_writel(__raw_readl(TIMER + TIMER_UNKREG0) | TIMER_SPECIALTIMER_BIT0, TIMER + TIMER_UNKREG0);
	}

	if(stat & TIMER_SPECIALTIMER_BIT1) {
		__raw_writel(__raw_readl(TIMER + TIMER_UNKREG0) | TIMER_SPECIALTIMER_BIT1, TIMER + TIMER_UNKREG0);
	}

	for(i = TIMER_Separator; i < NUM_TIMERS; i++) {
		callTimerHandler(i, stat >> (8 * (NUM_TIMERS - i - 1)));
	}

	/* signal timer has been handled */
	__raw_writel(stat, TIMER + TIMER_IRQLATCH);

	return IRQ_HANDLED;
}

static void timer_fired(void);

static void iphone_timer_setup(void)
{
	/* stop/cleanup any existing timers */
	timer_stop_all();

	/* do some voodoo */
	timer_init_rtc();

	Timers[EventTimer].handler2 = timer_fired;
}

static void iphone_timer_set_mode(enum clock_event_mode mode,
			      struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_RESUME:
	case CLOCK_EVT_MODE_PERIODIC:
		timer_on_off(EventTimer, 1);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		timer_on_off(EventTimer, 0);
		break;
	}
}

static int iphone_timer_set_next_event(unsigned long cycles,
				    struct clock_event_device *evt)
{
	timer_init(EventTimer, cycles, 0, 0, 0, 0, 0, 0, 0, 1);
	return 0;
}

static cycle_t iphone_timer_read(struct clocksource *cs)
{
	u64 ticks;
	iphone_timer_get_rtc_ticks(&ticks);
	return ticks;
}

struct clock_event_device clockevent =
{
	.name = "iphone_timer",
	.features = CLOCK_EVT_FEAT_PERIODIC,
	.rating = 200,
	.set_next_event = iphone_timer_set_next_event,
	.set_mode = iphone_timer_set_mode,
};

struct clocksource clocksource =
{
	.name = "iphone_timer",
	.rating = 200,
	.read = iphone_timer_read,
	.mask = CLOCKSOURCE_MASK(64),
	.shift = 24,
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
};

static struct irqaction iphone_timer_irq = {
	.name		= "iPhone Timer Tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= iphone_timer_interrupt,
};

static void timer_fired(void)
{
	struct clock_event_device *evt = &clockevent;
	evt->event_handler(evt);
}

static void __init iphone_timer_init(void)
{
	int i;
	int res;

	printk("iphone-timer: initializing\n");

	for(i = 0; i < NUM_TIMERS; i++) {
		timer_setup_clk(i, 1, 2, 0);
	}

	iphone_timer_setup();

	clockevents_calc_mult_shift(&clockevent, FREQUENCY_BASE, 4);
	clockevent.max_delta_ns = clockevent_delta2ns(0xF0000000, &clockevent);
	clockevent.min_delta_ns = clockevent_delta2ns(4, &clockevent);
	clockevent.cpumask = cpumask_of(0);

	clocksource.mult = clocksource_hz2mult(FREQUENCY_BASE, clocksource.shift);

	res = clocksource_register(&clocksource);
	if(res)
	{
		printk("iphone-timer: failed to register clock source\n");
		return;
	}

	res = setup_irq(TIMER_IRQ, &iphone_timer_irq);
	if(res)
	{
		printk("iphone-timer: failed to setup irq\n");
		return;
	}

	clockevents_register_device(&clockevent);

	printk("iphone-timer: finished initialization: (cycles * %u) >> %u = ns and (cycles << %u) / %u = ns\n", clocksource.mult, clocksource.shift,
			clockevent.shift, clockevent.mult);
}

struct sys_timer iphone_timer = {
	.init		= iphone_timer_init,
};

