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
typedef struct tty {
    int mode;
    bool echo;
    char buffer[TTY_BUFFER_SIZE];
    size_t buf_pos;
    size_t buf_size;
    char history[TTY_HISTORY_SIZE][TTY_BUFFER_SIZE];
    size_t history_count;
    size_t history_head;
    int history_cursor;
    size_t cursor;                  /* edit position inside buffer[0..buf_pos] */
    bool complete_listed;           /* set by complete_fn after printing a list */
    void (*prompt_fn)(void);        /* reprints the prompt (Ctrl+L, completion list) */
    /* Tab completion: fills `out` with the text to insert at the cursor and
     * returns its length. When several candidates remain the hook prints the
     * list itself and returns the length of the common prefix to insert. */
    size_t (*complete_fn)(struct tty* tty, char* out, size_t out_size);
} tty_t;

/* Initialize TTY */
void tty_init(tty_t* tty);

/* Set TTY mode */
void tty_set_mode(tty_t* tty, int mode);

/* Process input character */
char tty_process_char(tty_t* tty, char c);

/* Insert text at the cursor (used by completion). */
void tty_insert_text(tty_t* tty, const char* text, size_t len);

/* Read line from TTY */
ssize_t tty_readline(tty_t* tty, char* buf, size_t size);

#endif /* _TERM_TTY_H */
