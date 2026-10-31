#include <tach.h>
#include <tachc.h>

/* sync: grava no disco tudo que esta em cache. */
int main(void) {
    if (sync() < 0) {
        eprintf("sync: failed\n");
        return 1;
    }
    return 0;
}
