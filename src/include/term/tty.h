/* tach - TTY Header */
#ifndef _TERM_TTY_H
#define _TERM_TTY_H

#include <kernel/types.h>
#include <stdint.h>

/* TTY modes */
#define TTY_MODE_RAW  0
#define TTY_MODE_COOKED 1

/* TTY structure */
typedef struct {
    int mode;
    bool echo;
    char* buffer;
    size_t buf_pos;
    size_t buf_size;
} tty_t;

/* Initialize TTY */
void tty_init(tty_t* tty);

/* Set TTY mode */
void tty_set_mode(tty_t* tty, int mode);

/* Process input character */
char tty_process_char(tty_t* tty, char c);

/* Read line from TTY */
ssize_t tty_readline(tty_t* tty, char* buf, size_t size);

#endif /* _TERM_TTY_H */
