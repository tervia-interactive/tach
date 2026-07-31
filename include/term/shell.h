/* tach - Shell Header */
#ifndef _TERM_SHELL_H
#define _TERM_SHELL_H

#include <kernel/types.h>
#include <term/tty.h>

/* Shell structure */
typedef struct {
    const char* prompt;
    bool running;
    tty_t* tty;
    char cwd[128];
    int last_status;
} shell_t;

/* Initialize shell */
void shell_init(shell_t* sh, const char* prompt);

/* Run shell main loop */
void shell_run(shell_t* sh);

/* Execute command */
int shell_exec(const char* cmd, int argc, const char** argv);

#endif /* _TERM_SHELL_H */
