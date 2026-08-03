#include <drivers/char/serial.h>
#include <boot/sbi.h>

static long g_pending = -1;
void serial_sbi_init(void) { g_pending = -1; }
void serial_sbi_putchar(char c) {
    if (c == '\n') (void)sbi_ecall(1, 0, '\r', 0, 0, 0, 0, 0);
    (void)sbi_ecall(1, 0, (long)(uint8_t)c, 0, 0, 0, 0, 0);
}
int serial_sbi_available(void) {
    if (g_pending >= 0) return 1;
    long value = sbi_ecall(2, 0, 0, 0, 0, 0, 0, 0).error;
    if (value >= 0) { g_pending = value; return 1; }
    return 0;
}
char serial_sbi_getchar(void) {
    while (!serial_sbi_available()) {}
    char value = (char)g_pending; g_pending = -1; return value;
}
