#include <tach.h>

static int run(const char* path, char* const argv[]) {
    pid_t child = fork();
    if (child < 0) return -1;
    if (child == 0) { execve(path, argv, (char* const*)0); _exit(127); }
    int status = 0;
    return waitpid(child, &status, 0) < 0 ? -1 : status;
}

int main(void) {
    puts_fd(1, "tach init: PID 1 online\n");
    char* ls_argv[] = {(char*)"ls", (char*)"/bin", (char*)0};
    char* cat_argv[] = {(char*)"cat", (char*)"/etc/motd", (char*)0};
    if (run("/bin/ls", ls_argv) != 0 || run("/bin/cat", cat_argv) != 0)
        puts_fd(2, "init: external-command probe failed\n");
    for (;;) {
        pid_t child = fork();
        if (child < 0) {
            puts_fd(2, "init: fork failed\n");
            return 1;
        }
        if (child == 0) {
            char* argv[] = {(char*)"sh", (char*)0};
            execve("/bin/sh", argv, (char* const*)0);
            puts_fd(2, "init: cannot execute /bin/sh\n");
            _exit(127);
        }
        int status = 0;
        (void)waitpid(child, &status, 0);
        puts_fd(1, "init: shell exited; restarting\n");
    }
}
