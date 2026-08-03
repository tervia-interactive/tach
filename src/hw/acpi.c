#include <kernel/string.h>
#include <hw/acpi.h>

#define ACPI_MAX_CPUS 256
#define MADT_LOCAL_APIC_ENABLED 1u

static struct acpi_rsdp* g_rsdp;
static struct acpi_sdt_header* g_rsdt;
static struct acpi_sdt_header* g_xsdt;
static uint32_t g_apic_ids[ACPI_MAX_CPUS];
static size_t g_cpu_count;

static bool checksum_ok(const void* address, size_t length) {
    const uint8_t* bytes = (const uint8_t*)address;
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) sum = (uint8_t)(sum + bytes[i]);
    return sum == 0;
}

static struct acpi_rsdp* scan_rsdp(uintptr_t start, uintptr_t end) {
    start = (start + 15u) & ~(uintptr_t)15u;
    for (uintptr_t address = start; address + 20 <= end; address += 16) {
        struct acpi_rsdp* rsdp = (struct acpi_rsdp*)address;
        if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0 ||
            !checksum_ok(rsdp, 20)) continue;
        if (rsdp->revision >= 2 && (rsdp->length < sizeof(*rsdp) ||
            !checksum_ok(rsdp, rsdp->length))) continue;
        return rsdp;
    }
    return NULL;
}

static void parse_madt(void) {
    struct acpi_madt* madt = (struct acpi_madt*)acpi_find_table("APIC");
    if (!madt || madt->header.length < sizeof(*madt)) return;
    uintptr_t cursor = (uintptr_t)madt + sizeof(*madt);
    uintptr_t end = (uintptr_t)madt + madt->header.length;
    while (cursor + sizeof(struct acpi_madt_entry) <= end) {
        struct acpi_madt_entry* header = (struct acpi_madt_entry*)cursor;
        if (header->length < sizeof(*header) || cursor + header->length > end)
            break;
        if (header->type == ACPI_MADT_TYPE_LOCAL_APIC && header->length >= 8) {
            const uint8_t* entry = (const uint8_t*)header;
            uint32_t flags;
            memcpy(&flags, entry + 4, sizeof(flags));
            if ((flags & MADT_LOCAL_APIC_ENABLED) && g_cpu_count < ACPI_MAX_CPUS)
                g_apic_ids[g_cpu_count++] = entry[3];
        }
        cursor += header->length;
    }
}

void acpi_init(void) {
    g_rsdp = NULL; g_rsdt = NULL; g_xsdt = NULL; g_cpu_count = 0;
#if defined(__x86_64__) || defined(__i386__)
    uint16_t ebda_segment = 0;
    memcpy(&ebda_segment, (const void*)(uintptr_t)0x40e,
           sizeof(ebda_segment));
    uintptr_t ebda = (uintptr_t)ebda_segment << 4;
    if (ebda) g_rsdp = scan_rsdp(ebda, ebda + 1024);
    if (!g_rsdp) g_rsdp = scan_rsdp(0xe0000, 0x100000);
    if (!g_rsdp) return;
    if (g_rsdp->rsdt_address)
        g_rsdt = (struct acpi_sdt_header*)(uintptr_t)g_rsdp->rsdt_address;
    if (g_rsdp->revision >= 2 && g_rsdp->xsdt_address)
        g_xsdt = (struct acpi_sdt_header*)(uintptr_t)g_rsdp->xsdt_address;
    parse_madt();
#endif
}

struct acpi_sdt_header* acpi_find_table(const char* signature) {
    if (!signature) return NULL;
    struct acpi_sdt_header* roots[2] = {g_xsdt, g_rsdt};
    for (size_t root_index = 0; root_index < 2; root_index++) {
        struct acpi_sdt_header* root = roots[root_index];
        if (!root || root->length < sizeof(*root) ||
            !checksum_ok(root, root->length)) continue;
        size_t width = root == g_xsdt ? 8 : 4;
        size_t count = (root->length - sizeof(*root)) / width;
        const uint8_t* entries = (const uint8_t*)root + sizeof(*root);
        for (size_t i = 0; i < count; i++) {
            uint64_t address = 0;
            memcpy(&address, entries + i * width, width);
            struct acpi_sdt_header* table =
                (struct acpi_sdt_header*)(uintptr_t)address;
            if (table && table->length >= sizeof(*table) &&
                memcmp(table->signature, signature, 4) == 0 &&
                checksum_ok(table, table->length)) return table;
        }
    }
    return NULL;
}

void* acpi_get_rsdp(void) { return g_rsdp; }
size_t acpi_cpu_count(void) { return g_cpu_count; }
uint32_t acpi_cpu_apic_id(size_t index) {
    return index < g_cpu_count ? g_apic_ids[index] : 0;
}
