#include <tach.h>
int main(void) {
    if (sync() < 0) { puts_fd(2, "sync: failed\n"); return 1; }
    return 0;
}
