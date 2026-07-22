#ifndef TERM_SHELL_H
#define TERM_SHELL_H

#include <kernel/types.h>

#define SHELL_MAX_CMD_LEN 256
#define SHELL_MAX_ARGS 16

typedef struct {
    char *cmd;
    int (*handler)(int argc, char **argv);
    const char *description;
} shell_command_t;

typedef struct {
    char buffer[SHELL_MAX_CMD_LEN];
    size_t buffer_pos;
    int running;
} shell_t;

int shell_init(shell_t *shell);
void shell_run(shell_t *shell);
void shell_register_command(const char *cmd, int (*handler)(int argc, char **argv), const char *desc);
void shell_print_prompt(shell_t *shell);

#endif /* TERM_SHELL_H */
