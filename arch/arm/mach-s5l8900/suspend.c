#include <mach/pmu.h>
#include <linux/suspend.h>
#include <linux/earlysuspend.h>

static suspend_state_t iphone_suspend_state = PM_SUSPEND_ON;

static int iphone_suspend_valid(suspend_state_t _state)
{
	return suspend_valid_only_mem(_state);
}

static int iphone_suspend_begin(suspend_state_t _state)
{
	iphone_suspend_state = _state;
	return 0;
}

static void iphone_suspend_end(void)
{
	iphone_suspend_state = PM_SUSPEND_ON;
}

static int iphone_suspend_prepare(void)
{
	return 0;
}

static int iphone_suspend_enter(suspend_state_t _state)
{
	if(_state == PM_SUSPEND_MEM)
	{
#	if POWER_PCF50633
		pcf50633_suspend();
#	endif
	}

	return 0;
}

static void iphone_suspend_finish(void)
{
}

static struct platform_suspend_ops iphone_suspend_ops = {
	.valid = &iphone_suspend_valid,
	.begin = &iphone_suspend_begin,
	.end = &iphone_suspend_end,
	.prepare = &iphone_suspend_prepare,
	.enter = &iphone_suspend_enter,
	.finish = &iphone_suspend_finish,
};

void iphone_init_suspend(void)
{
	suspend_set_ops(&iphone_suspend_ops);
}
