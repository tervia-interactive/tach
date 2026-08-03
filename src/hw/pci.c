#include <hw/pci.h>
#include <kernel/string.h>

#define PCI_MAX_DEVICES 64
static struct pci_device g_devices[PCI_MAX_DEVICES];
static size_t g_device_count;

#if defined(__i386__) || defined(__x86_64__)
static inline void out32(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" :: "a"(value), "Nd"(port));
}
static inline uint32_t in32(uint16_t port) {
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t function,
                         uint8_t offset) {
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) | ((uint32_t)function << 8) | (offset & 0xfcu);
    out32(PCI_CONFIG_ADDRESS, address);
    return in32(PCI_CONFIG_DATA);
}
void pci_write_config(uint8_t bus, uint8_t slot, uint8_t function,
                      uint8_t offset, uint32_t value) {
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) | ((uint32_t)function << 8) | (offset & 0xfcu);
    out32(PCI_CONFIG_ADDRESS, address);
    out32(PCI_CONFIG_DATA, value);
}
#else
uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t function,
                         uint8_t offset) {
    (void)bus; (void)slot; (void)function; (void)offset; return 0xffffffffu;
}
void pci_write_config(uint8_t bus, uint8_t slot, uint8_t function,
                      uint8_t offset, uint32_t value) {
    (void)bus; (void)slot; (void)function; (void)offset; (void)value;
}
#endif

static void record_device(uint8_t bus, uint8_t slot, uint8_t function) {
    if (g_device_count >= PCI_MAX_DEVICES) return;
    uint32_t id = pci_read_config(bus, slot, function, 0);
    if ((id & 0xffffu) == 0xffffu) return;
    struct pci_device* device = &g_devices[g_device_count++];
    memset(device, 0, sizeof(*device));
    device->vendor_id = (uint16_t)id;
    device->device_id = (uint16_t)(id >> 16);
    uint32_t command = pci_read_config(bus, slot, function, PCI_COMMAND);
    device->command = (uint16_t)command;
    device->status = (uint16_t)(command >> 16);
    uint32_t class_info = pci_read_config(bus, slot, function, 8);
    device->revision_id = (uint8_t)class_info;
    device->prog_if = (uint8_t)(class_info >> 8);
    device->subclass = (uint8_t)(class_info >> 16);
    device->class_code = (uint8_t)(class_info >> 24);
    device->header_type = (uint8_t)(pci_read_config(bus, slot, function, 12) >> 16);
    device->bus = bus; device->slot = slot; device->function = function;
    for (uint8_t bar = 0; bar < 6; bar++)
        device->bar[bar] = pci_read_config(bus, slot, function,
                                           (uint8_t)(PCI_BAR0 + bar * 4));
}

int pci_scan(void (*callback)(uint32_t encoded_bdf)) {
    g_device_count = 0;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read_config((uint8_t)bus, slot, 0, 0);
            if ((id & 0xffffu) == 0xffffu) continue;
            uint8_t functions =
                (pci_read_config((uint8_t)bus, slot, 0, 12) & 0x00800000u) ? 8 : 1;
            for (uint8_t function = 0; function < functions; function++) {
                id = pci_read_config((uint8_t)bus, slot, function, 0);
                if ((id & 0xffffu) == 0xffffu) continue;
                record_device((uint8_t)bus, slot, function);
                if (callback) callback(((uint32_t)bus << 16) |
                                       ((uint32_t)slot << 8) | function);
            }
        }
    }
    return (int)g_device_count;
}
void pci_init(void) { (void)pci_scan(NULL); }
void pci_enumerate_devices(void) { (void)pci_scan(NULL); }
size_t pci_device_count(void) { return g_device_count; }
struct pci_device* pci_device_at(size_t index) {
    return index < g_device_count ? &g_devices[index] : NULL;
}
struct pci_device* pci_find_device(uint16_t vendor, uint16_t device_id) {
    for (size_t i = 0; i < g_device_count; i++)
        if (g_devices[i].vendor_id == vendor &&
            g_devices[i].device_id == device_id) return &g_devices[i];
    return NULL;
}
