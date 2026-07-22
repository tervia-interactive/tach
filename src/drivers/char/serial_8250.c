#include <drivers/char/serial.h>
int serial_8250_init(uint16_t port) {(void)port; return 0;}
void serial_8250_putc(char c) {(void)c;}
