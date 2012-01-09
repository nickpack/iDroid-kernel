/* drivers/misc/iphone3g-vibrator.c
 *
 * Author: Patrick Wildt <webmaster@patrick-wildt.de>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <../drivers/staging/android/timed_output.h>

#define TicksPerSec 12000000
#define VibratorTimer 5

extern int timer_init(int timer_id, u32 interval, u32 interval2, u32 prescaler, u32 z, int option24, int option28, int option11, int option5, int interrupts);
extern int timer_on_off(int timer_id, int on_off);
extern u64 iphone_microtime(void);

static u64 iphone_vibrator_end;
static int iphone_vibrator_stop;
static int iphone_vibrator_last_timeout;
static u32 iphone_vibrator_waiting;
static int iphone_queue_running;

void iphone_vibrator_enable(struct timed_output_dev *iphone_vibrator_dev, int timeout) {
	u32 count, countRun;
	int prescaler, id;

	id = ++iphone_vibrator_waiting;

	iphone_vibrator_stop = 1;

	while(iphone_queue_running) {
		if (id != iphone_vibrator_waiting)
			return;
		msleep(100);
	}

	if (id != iphone_vibrator_waiting)
		return;

	iphone_vibrator_waiting = 0;
	iphone_vibrator_stop = 0;
	iphone_queue_running = 1;

	if (timeout < 0) {
		timer_init(VibratorTimer, 0, 1, 0, 0, 0, 0, 0, 0, 0);
		timer_on_off(VibratorTimer, 1);
		goto out;
	}

	if (timeout < 1) {
		if (iphone_vibrator_last_timeout<0) {
			timer_init(VibratorTimer, 0, 1, 0, 0, 0, 0, 0, 1, 0);
			timer_on_off(VibratorTimer, 1);
		}
		timer_on_off(VibratorTimer, 0);
		goto out;
	}

	timeout *= 1000;

	iphone_vibrator_end = iphone_microtime() + timeout;
        count = timeout * (TicksPerSec/1000000);
        while(!iphone_vibrator_stop) {
                countRun = count;
                if (count > TicksPerSec)
                        countRun = TicksPerSec;
                prescaler = 1;
                while(countRun > 0xFFFF)
                {
                        countRun >>= 1;
                        prescaler <<= 1;
                }

		timer_init(VibratorTimer, 0, countRun, prescaler - 1, 0, 0, 0, 0, 1, 0);
                timer_on_off(VibratorTimer, 1);
                if (TicksPerSec > count)
			goto out;
                count -= TicksPerSec;
                msleep(1000);
        }

out:
	iphone_vibrator_last_timeout = timeout;
	iphone_vibrator_end = 0;
	iphone_queue_running = 0;
}

int iphone_vibrator_get_time(struct timed_output_dev *iphone_vibrator_dev) {
        if (iphone_vibrator_end == 0)
                return 0;
        if (iphone_vibrator_end > iphone_microtime())
                return 0;
        return((int)(iphone_vibrator_end - iphone_microtime()));
}

struct timed_output_dev iphone_vibrator_dev = {
        .name = "vibrator",
        .enable = iphone_vibrator_enable,
        .get_time = iphone_vibrator_get_time,
};

static int __init iphone_vibrator_init(void)
{
	return timed_output_dev_register(&iphone_vibrator_dev);
}

static void __exit iphone_vibrator_exit(void)
{
	timed_output_dev_unregister(&iphone_vibrator_dev);
}

module_init(iphone_vibrator_init);
module_exit(iphone_vibrator_exit);

MODULE_AUTHOR("Patrick Wildt <webmaster@patrick-wildt.de>");
MODULE_DESCRIPTION("iPhone 3G Vibrator Driver");
MODULE_LICENSE("GPL");
