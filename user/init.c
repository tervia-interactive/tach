#include <tach.h>

int main(void) {
    puts_fd(1, "tach init: PID 1 online\n");
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
