#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <mach/time.h>
#include <asm/mach/time.h>

static void s5l8930_timer_set_mode(enum clock_event_mode mode,
			      struct clock_event_device *evt)
{
}

static int s5l8930_timer_set_next_event(unsigned long cycles,
				    struct clock_event_device *evt)
{
	writel(cycles, S5L_TIMER0_VAL);
	writel(S5L_TIMER_ENABLE, S5L_TIMER0_CTRL);
	return 0;
}

static irqreturn_t s5l8930_timer_interrupt(int irq, void* dev_id)
{
	struct clock_event_device *evt = dev_id;

	writel(S5L_TIMER_DISABLE, S5L_TIMER0_CTRL);
	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static cycle_t s5l8930_clock_read(struct clocksource *cs)
{
	register uint32_t hi = readl(S5L_CLOCK_HI);
	register uint32_t lo = readl(S5L_CLOCK_LO);
	register uint32_t tst = readl(S5L_CLOCK_HI);

	if(hi != tst)
	{
		hi = tst;
		lo = readl(S5L_CLOCK_LO);
	}

	return (((uint64_t)hi) << 32) | lo;
}

static struct clock_event_device clockevent =
{
	.name = "s5l8930-event-clock",
	.features = CLOCK_EVT_FEAT_ONESHOT,
	.rating = 200,
	.set_next_event = s5l8930_timer_set_next_event,
	.set_mode = s5l8930_timer_set_mode,
};

static struct clocksource clocksource =
{
	.name = "s5l8930-clock",
	.rating = 200,
	.read = s5l8930_clock_read,
	.mask = CLOCKSOURCE_MASK(64),
	.shift = 24,
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
};

static struct irqaction s5l8930_timer_irq = {
	.name		= "s5l8930-clock-irq",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= s5l8930_timer_interrupt,
	.dev_id		= &clockevent,
};

static void __init s5l8930_timer_init(void)
{
	int res;

	printk("s5l8930-timer: initializing\n");

	/*iowrite32(ioread32(__va(0xBF500004)) | 0x80000000, 0xBF500004);
	iowrite32(0xEF0000, 0xBF101218);
	iowrite32(0xFC020408, 0xBF10121C);
	iowrite32(0x27C0011, 0xBF101220);
	iowrite32(0x1830006, 0xBF101228);
	iowrite32(0x12F0006, 0xBF101230);
	iowrite32(0x20404, 0xBF101224);
	iowrite32(0x20404, 0xBF10122C);
	iowrite32(0x20409, 0xBF101234);
	iowrite32(0, 0xBF101200);
	iowrite32(0x3F, 0xBF101000);*/

	clockevents_calc_mult_shift(&clockevent, S5L_CLOCK_HZ, 4);
	clockevent.max_delta_ns = clockevent_delta2ns(0xF0000000, &clockevent);
	clockevent.min_delta_ns = clockevent_delta2ns(4, &clockevent);
	clockevent.cpumask = cpumask_of(0);

	clocksource.mult = clocksource_hz2mult(S5L_CLOCK_HZ, clocksource.shift);

	res = clocksource_register(&clocksource);
	if(res)
	{
		printk("s5l8930-timer: failed to register clock source\n");
		return;
	}

	res = setup_irq(IRQ_TIMER0, &s5l8930_timer_irq);
	if(res)
	{
		printk("s5l8930-timer: failed to setup irq\n");
		return;
	}

	clockevents_register_device(&clockevent);

	printk("s5l8930-timer: finished initialization: (cycles * %u) >> %u = ns and (cycles << %u) / %u = ns\n", clocksource.mult, clocksource.shift,
			clockevent.shift, clockevent.mult);
}

struct sys_timer s5l8930_timer = {
	.init		= s5l8930_timer_init,
};

