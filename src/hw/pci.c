#include <hw/pci.h>
int pci_init(void) { return 0; }
int pci_scan(void (*callback)(uint32_t)) {(void)callback; return 0;}
