/**
 * @file shell.c
 * @brief Unix-like command shell for tach
 * 
 * A minimal POSIX-like shell providing basic command execution.
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#include "tach.h"

#define MAX_CMD_LEN 256
#define MAX_ARGS 16
#define PROMPT "$ "

/* Built-in commands */
static int cmd_help(void);
static int cmd_clear(void);
static int cmd_echo(int argc, char **argv);
static int cmd_exit(int argc, char **argv);

struct builtin_cmd {
    const char *name;
    int (*func)(int argc, char **argv);
    const char *description;
};

static struct builtin_cmd builtins[] = {
    {"help", cmd_help, "Show this help message"},
    {"clear", cmd_clear, "Clear the screen"},
    {"echo", cmd_echo, "Print arguments to stdout"},
    {"exit", cmd_exit, "Exit the shell"},
    {NULL, NULL, NULL}
};

static void print_prompt(void) {
    printf("%s", PROMPT);
}

static void read_line(char *buf, size_t size) {
    size_t i = 0;
    int c;
    
    while (i < size - 1) {
        c = getchar();
        if (c == EOF || c == '\n' || c == '\r') {
            break;
        }
        
        if (c == '\b' || c == 127) {  /* Backspace or DEL */
            if (i > 0) {
                i--;
                printf("\b \b");
            }
            continue;
        }
        
        buf[i++] = (char)c;
        putchar(c);
    }
    
    buf[i] = '\0';
    putchar('\n');
}

static int parse_args(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;
    
    while (*p && argc < max_args - 1) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        
        if (*p == '\0') {
            break;
        }
        
        argv[argc++] = p;
        
        /* Find end of word */
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }
        
        if (*p) {
            *p++ = '\0';
        }
    }
    
    argv[argc] = NULL;
    return argc;
}

static int cmd_help(void) {
    printf("tach shell - built-in commands:\n\n");
    
    for (int i = 0; builtins[i].name != NULL; i++) {
        printf("  %-10s %s\n", builtins[i].name, builtins[i].description);
    }
    
    return 0;
}

static int cmd_clear(void) {
    /* ANSI escape sequence to clear screen */
    printf("\033[2J\033[H");
    return 0;
}

static int cmd_echo(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        if (i > 0) {
            printf(" ");
        }
        printf("%s", argv[i]);
    }
    printf("\n");
    return 0;
}

static int cmd_exit(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    int status = 0;
    if (argc > 1) {
        status = atoi(argv[1]);
    }
    
    exit(status);
    return 0;
}

static int execute_command(int argc, char **argv) {
    if (argc == 0) {
        return 0;
    }
    
    /* Check for built-in commands */
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(argv[0], builtins[i].name) == 0) {
            return builtins[i].func(argc - 1, argv + 1);
        }
    }
    
    /* External command - not implemented yet */
    printf("Command not found: %s\n", argv[0]);
    return 1;
}

_Noreturn void shell_main(void) {
    char cmd_buf[MAX_CMD_LEN];
    char *argv[MAX_ARGS];
    int argc;
    
    printf("\n");
    printf("tach shell v0.1\n");
    printf("Type 'help' for available commands.\n\n");
    
    while (1) {
        print_prompt();
        read_line(cmd_buf, sizeof(cmd_buf));
        
        argc = parse_args(cmd_buf, argv, MAX_ARGS);
        execute_command(argc, argv);
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    shell_main();
    
    return 0;
}
