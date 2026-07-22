#include <kernel/types.h>
#include <proc/process.h>
struct process* process_create(const char* name) {(void)name; return (struct process*)0;}
void process_exit(int code) {(void)code;}
