#include <plat/irq.h>
#include <asm/hardware/vic.h>

void s5l_init_vics(void __iomem **_bases, uint32_t _count)
{
	uint32_t i;
	for(i = 0; i < _count; i++)
		vic_init(_bases[i], i*32, 0xffffffff, 0);
}
