#include <kernel/port.h>
int port_create(void) { return 0; }
void port_destroy(int p) {(void)p;}
int port_send(int p, void *m) {(void)p; (void)m; return 0;}
int port_recv(int p, void *m) {(void)p; (void)m; return 0;}
