/* tach Operating System - Shell Header */
#ifndef _TERM_SHELL_H
#define _TERM_SHELL_H

#include <kernel/types.h>

/* Shell structure */
typedef struct {
    const char* prompt;
    bool running;
} shell_t;

/* Initialize shell */
void shell_init(shell_t* sh, const char* prompt);

/* Run shell main loop */
void shell_run(shell_t* sh);

/* Execute command */
int shell_exec(const char* cmd, const char** argv);

#endif /* _TERM_SHELL_H */
