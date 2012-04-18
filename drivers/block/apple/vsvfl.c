#include <linux/apple_flash.h>

int apple_vsvfl_detect(struct apple_vfl *_vfl)
{
	printk(KERN_ERR "apple-flash: VSVFL not implemented!\n");
	return -42;
}
