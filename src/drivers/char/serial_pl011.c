#include <drivers/char/serial.h>

#define UART_DR 0x000u
#define UART_FR 0x018u
#define UART_IBRD 0x024u
#define UART_FBRD 0x028u
#define UART_LCRH 0x02cu
#define UART_CR 0x030u
#define UART_IMSC 0x038u
#define UART_ICR 0x044u
#define UART_FR_RXFE (1u << 4)
#define UART_FR_TXFF (1u << 5)

static uintptr_t g_pl011_base;
static volatile uint32_t* reg(uint32_t offset) {
    return (volatile uint32_t*)(g_pl011_base + offset);
}
int serial_pl011_init(uintptr_t base) {
    if (!base) return -1;
    g_pl011_base = base;
    *reg(UART_CR) = 0;
    *reg(UART_IMSC) = 0;
    *reg(UART_ICR) = 0x7ff;
    *reg(UART_IBRD) = 13;
    *reg(UART_FBRD) = 1;
    *reg(UART_LCRH) = (3u << 5) | (1u << 4);
    *reg(UART_CR) = (1u << 0) | (1u << 8) | (1u << 9);
    return 0;
}
void serial_pl011_putchar(char c) {
    if (!g_pl011_base) return;
    if (c == '\n') serial_pl011_putchar('\r');
    while (*reg(UART_FR) & UART_FR_TXFF) {}
    *reg(UART_DR) = (uint32_t)(uint8_t)c;
}
int serial_pl011_available(void) {
    return g_pl011_base && !(*reg(UART_FR) & UART_FR_RXFE);
}
char serial_pl011_getchar(void) {
    while (!serial_pl011_available()) {}
    return (char)(*reg(UART_DR) & 0xffu);
}
