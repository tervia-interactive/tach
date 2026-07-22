#ifndef TERM_TTY_H
#define TERM_TTY_H

#include <kernel/types.h>

#define TTY_BUFFER_SIZE 4096

typedef enum {
    TTY_MODE_RAW,
    TTY_MODE_COOKED,
    TTY_MODE_CBREAK
} tty_mode_t;

typedef struct {
    tty_mode_t mode;
    uint8_t echo;
    uint8_t *buffer;
    size_t buffer_pos;
    size_t buffer_size;
} tty_t;

int tty_init(tty_t *tty);
int tty_read(tty_t *tty, char *buf, size_t count);
int tty_write(tty_t *tty, const char *buf, size_t count);
void tty_set_mode(tty_t *tty, tty_mode_t mode);
void tty_putchar(tty_t *tty, char c);

#endif /* TERM_TTY_H */
