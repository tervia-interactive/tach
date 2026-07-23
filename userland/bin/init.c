/**
 * @file init.c
 * @brief Initial user-space process for tach
 * 
 * This is the first userspace program that runs after kernel boot.
 * It initializes the system and starts the shell.
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#include "tach.h"

extern _Noreturn void shell_main(void);

static void print_banner(void) {
    printf("\n");
    printf("  _____      __\n");
    printf(" / ___/___  / /_  ____ _\n");
    printf(" \\__ \\/ _ \\/ __ \\/ __ `/\n");
    printf("___/ /  __/ /_/ / /_/ /\n");
    printf("/____/\\___/_.___/\\__,_/   \n");
    printf("\n");
    printf("tach OS v0.1 - Multiarch Unix-like OS\n");
    printf("\n");
}

static void system_init(void) {
    /* Initialize system components */
    printf("[init] System initialization...\n");
    
    /* TODO: Mount filesystems */
    /* TODO: Start daemons */
    /* TODO: Set up environment */
    
    printf("[init] Initialization complete.\n");
}

_Noreturn void init_main(void) {
    print_banner();
    system_init();
    
    printf("\n[init] Starting shell...\n\n");
    
    /* Start the interactive shell */
    shell_main();
    
    /* Should never reach here */
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    init_main();
    
    return 0;
}
