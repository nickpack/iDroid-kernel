#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <mach/time.h>
#include <asm/mach/time.h>

static void timer_disable()
{
	//printk("%s\n", __func__);
	writel(2, S5L_TIMER0_CTRL);
	writel(0, S5L_TIMER0_CTRL);
}

static void timer_setup(unsigned long cyc)
{
	//printk("%s: %d.\n", __func__, cyc);
	writel(0xFFFFFFFF, S5L_TIMER0_VAL);
	writel(3, S5L_TIMER0_CTRL);
	writel(1, S5L_TIMER0_CTRL);
	writel(cyc, S5L_TIMER0_VAL);
}

static void s5l8930_timer_set_mode(enum clock_event_mode mode,
			      struct clock_event_device *evt)
{
}

static int s5l8930_timer_set_next_event(unsigned long cycles,
				    struct clock_event_device *evt)
{
	timer_setup(cycles);
	return 0;
}

static irqreturn_t s5l8930_timer_interrupt(int irq, void* dev_id)
{
	struct clock_event_device *evt = dev_id;

	timer_disable();
	evt->event_handler(evt);
	return IRQ_HANDLED;
}

static cycle_t s5l8930_clock_read(struct clocksource *cs)
{
	register uint32_t hi = __raw_readl(S5L_CLOCK_HI);
	register uint32_t lo = __raw_readl(S5L_CLOCK_LO);
	register uint32_t tst = __raw_readl(S5L_CLOCK_HI);

	if(hi != tst)
	{
		hi = tst;
		lo = __raw_readl(S5L_CLOCK_LO);
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
	.rating = 250,
	.read = s5l8930_clock_read,
	.mask = CLOCKSOURCE_MASK(64),
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

	// clocksource
	
	res = clocksource_register_hz(&clocksource, S5L_CLOCK_HZ);
	if(res)
	{
		printk("s5l8930-timer: failed to register clock source\n");
		return;
	}

	// event

	clockevents_calc_mult_shift(&clockevent, S5L_CLOCK_HZ, 4);
	clockevent.max_delta_ns = clockevent_delta2ns(0xF0000000, &clockevent);
	clockevent.min_delta_ns = clockevent_delta2ns(1, &clockevent);
	clockevent.cpumask = cpumask_of(0);

	timer_disable();
	res = setup_irq(S5L_TIMER0_IRQ, &s5l8930_timer_irq);
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

