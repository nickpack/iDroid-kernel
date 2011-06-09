#define UART_UTRSTAT (*(volatile unsigned char*)(0x82500010))
#define UART_UTXH (*(volatile unsigned char*)(0x82500020))

static inline void putc(int c)
{
	while((UART_UTRSTAT & 4) == 0)
		barrier();

	UART_UTXH = c;
}

static inline void flush(void)
{
	while((UART_UTRSTAT & 4) == 0)
		barrier();
}

/*
 * nothing to do
 */
#define arch_decomp_setup()
#define arch_decomp_wdog()
