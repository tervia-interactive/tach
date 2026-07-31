/* tach - TTY Header */
#ifndef _TERM_TTY_H
#define _TERM_TTY_H

#include <kernel/types.h>
#include <stdint.h>

/* TTY modes */
#define TTY_MODE_RAW  0
#define TTY_MODE_COOKED 1
#define TTY_BUFFER_SIZE 256
#define TTY_HISTORY_SIZE 8

/* TTY structure */
typedef struct {
    int mode;
    bool echo;
    char buffer[TTY_BUFFER_SIZE];
    size_t buf_pos;
    size_t buf_size;
    char history[TTY_HISTORY_SIZE][TTY_BUFFER_SIZE];
    size_t history_count;
    size_t history_head;
    int history_cursor;
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
