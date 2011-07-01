#include <plat/irq.h>
#include <asm/hardware/vic.h>

#include <linux/serial_core.h>
#include <mach/map.h>
#include <plat/regs-timer.h>
#include <plat/regs-serial.h>
#include <plat/cpu.h>
#include <plat/irq-vic-timer.h>
#include <plat/irq-uart.h>

static struct s3c_uart_irq uart_irqs[] = {
	[0] = {
		.regs		= VA_UART0,
		.base_irq	= IRQ_S5L_UART_BASE0,
		.parent_irq	= IRQ_UART0,
	},
	[1] = {
		.regs		= VA_UART1,
		.base_irq	= IRQ_S5L_UART_BASE1,
		.parent_irq	= IRQ_UART1,
	},
	[2] = {
		.regs		= VA_UART2,
		.base_irq	= IRQ_S5L_UART_BASE2,
		.parent_irq	= IRQ_UART2,
	},
};

void s5l_init_vics(void __iomem **_bases, uint32_t _count)
{
	uint32_t i;
	for(i = 0; i < _count; i++)
		vic_init(_bases[i], i*32, 0xffffffff, 0);

	s3c_init_uart_irqs(uart_irqs, ARRAY_SIZE(uart_irqs));
}
