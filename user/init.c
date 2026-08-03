#include <tach.h>

static int run(const char* path, char* const argv[]) {
    pid_t child = fork();
    if (child < 0) return -1;
    if (child == 0) { execve(path, argv, (char* const*)0); _exit(127); }
    int status = 0;
    return waitpid(child, &status, 0) < 0 ? -1 : status;
}

static void probe_persistent_storage(void) {
    int directory = open("/disk", O_RDONLY);
    if (directory < 0) return;
    close(directory);

    int existing = open("/disk/tach.txt", O_RDONLY);
    if (existing >= 0) {
        close(existing);
        char* cat_argv[] = {(char*)"cat", (char*)"/disk/tach.txt", (char*)0};
        if (run("/bin/cat", cat_argv) == 0)
            puts_fd(1, "init: persistent storage survived reboot\n");
        return;
    }

    char* write_argv[] = {(char*)"write", (char*)"/disk/tach.txt",
                          (char*)"tach", (char*)"persistent",
                          (char*)"storage", (char*)"verified", (char*)0};
    char* sync_argv[] = {(char*)"sync", (char*)0};
    if (run("/bin/write", write_argv) == 0 &&
        run("/bin/sync", sync_argv) == 0)
        puts_fd(1, "init: persistent storage seeded\n");
}

int main(void) {
    puts_fd(1, "tach init: PID 1 online\n");
    char* ls_argv[] = {(char*)"ls", (char*)"/bin", (char*)0};
    char* cat_argv[] = {(char*)"cat", (char*)"/etc/motd", (char*)0};
    if (run("/bin/ls", ls_argv) != 0 || run("/bin/cat", cat_argv) != 0)
        puts_fd(2, "init: external-command probe failed\n");
    probe_persistent_storage();
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
