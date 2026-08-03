#include <drivers/block/ahci.h>
#include <hw/pci.h>
#include <kernel/errno.h>
#include <kernel/klog.h>
#include <kernel/string.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define HBA_GHC_AE (1u << 31)
#define HBA_PxCMD_ST (1u << 0)
#define HBA_PxCMD_FRE (1u << 4)
#define HBA_PxCMD_FR (1u << 14)
#define HBA_PxCMD_CR (1u << 15)
#define HBA_PxIS_TFES (1u << 30)
#define SATA_SIG_ATA 0x00000101u
#define SATA_SIG_ATAPI 0xeb140101u
#define FIS_TYPE_REG_H2D 0x27
#define ATA_CMD_IDENTIFY 0xec
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_FLUSH_CACHE_EXT 0xea
#define AHCI_TIMEOUT 1000000u
#define AHCI_TABLE_BYTES 256u

struct hba_port {
    uint32_t clb, clbu, fb, fbu, is, ie, cmd, reserved0, tfd, sig;
    uint32_t ssts, sctl, serr, sact, ci, sntf, fbs, reserved1[11], vendor[4];
};

struct hba_memory {
    uint32_t cap, ghc, is, pi, vs, ccc_ctl, ccc_ports, em_loc, em_ctl;
    uint32_t cap2, bohc;
    uint8_t reserved[0xa0 - 0x2c];
    uint8_t vendor[0x100 - 0xa0];
    struct hba_port ports[32];
};

struct command_header {
    uint16_t flags;
    uint16_t prdt_length;
    volatile uint32_t transferred;
    uint32_t table_base, table_base_upper;
    uint32_t reserved[4];
} __attribute__((packed));

struct prdt_entry {
    uint32_t data_base, data_base_upper, reserved, byte_count;
} __attribute__((packed));

struct command_table {
    uint8_t cfis[64], acmd[16], reserved[48];
    struct prdt_entry prdt[1];
} __attribute__((packed));

struct fis_reg_h2d {
    uint8_t type, flags, command, feature_low;
    uint8_t lba0, lba1, lba2, device, lba3, lba4, lba5, feature_high;
    uint16_t count;
    uint8_t icc, control;
    uint8_t reserved[4];
} __attribute__((packed));

static ahci_hba_t g_hba;
static ahci_port_t g_ports[AHCI_MAX_PORTS];
static size_t g_port_count;

static int wait_clear(volatile uint32_t* register_address, uint32_t mask) {
    for (uint32_t timeout = 0; timeout < AHCI_TIMEOUT; timeout++)
        if (!(*register_address & mask)) return 0;
    return -ETIMEOUT;
}

static int stop_port(struct hba_port* port) {
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;
    return wait_clear(&port->cmd, HBA_PxCMD_FR | HBA_PxCMD_CR);
}
static int start_port(struct hba_port* port) {
    int result = wait_clear(&port->cmd, HBA_PxCMD_CR);
    if (result < 0) return result;
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
    return 0;
}

static int issue(ahci_port_t* state, uint8_t command, uint64_t lba,
                 uint16_t sectors, bool write, void* dma, size_t bytes) {
    struct hba_port* port = (struct hba_port*)state->registers;
    struct command_header* header = (struct command_header*)state->command_list;
    struct command_table* table = (struct command_table*)state->command_tables;
    memset(header, 0, sizeof(*header));
    memset(table, 0, AHCI_TABLE_BYTES);
    header->flags = 5u | (write ? (1u << 6) : 0);
    header->prdt_length = bytes ? 1 : 0;
    uintptr_t table_address = (uintptr_t)table;
    header->table_base = (uint32_t)table_address;
    header->table_base_upper = (uint32_t)((uint64_t)table_address >> 32);
    if (bytes) {
        uintptr_t data_address = (uintptr_t)dma;
        table->prdt[0].data_base = (uint32_t)data_address;
        table->prdt[0].data_base_upper =
            (uint32_t)((uint64_t)data_address >> 32);
        table->prdt[0].byte_count = (uint32_t)(bytes - 1) | (1u << 31);
    }
    struct fis_reg_h2d* fis = (struct fis_reg_h2d*)table->cfis;
    fis->type = FIS_TYPE_REG_H2D;
    fis->flags = 1u << 7;
    fis->command = command;
    fis->device = 1u << 6;
    fis->lba0 = (uint8_t)lba; fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16); fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32); fis->lba5 = (uint8_t)(lba >> 40);
    fis->count = sectors;
    port->is = 0xffffffffu;
    for (uint32_t timeout = 0; timeout < AHCI_TIMEOUT; timeout++) {
        if (!(port->tfd & (0x80u | 0x08u))) break;
        if (timeout + 1 == AHCI_TIMEOUT) return -EBUSY;
    }
    port->ci = 1u;
    for (uint32_t timeout = 0; timeout < AHCI_TIMEOUT; timeout++) {
        if (!(port->ci & 1u))
            return (port->is & HBA_PxIS_TFES) ? -EIO : 0;
        if (port->is & HBA_PxIS_TFES) return -EIO;
    }
    return -ETIMEOUT;
}

int ahci_read(ahci_port_t* state, uint64_t lba, uint32_t count, void* buffer) {
    if (!state || !buffer || !count) return -EINVAL;
    spinlock_lock(&state->lock);
    uint8_t* output = (uint8_t*)buffer;
    for (uint32_t i = 0; i < count; i++) {
        int result = issue(state, ATA_CMD_READ_DMA_EXT, lba + i, 1, false,
                           state->bounce, 512);
        if (result < 0) { spinlock_unlock(&state->lock); return result; }
        memcpy(output + (size_t)i * 512, state->bounce, 512);
    }
    spinlock_unlock(&state->lock);
    return 0;
}
int ahci_write(ahci_port_t* state, uint64_t lba, uint32_t count,
               const void* buffer) {
    if (!state || !buffer || !count) return -EINVAL;
    spinlock_lock(&state->lock);
    const uint8_t* input = (const uint8_t*)buffer;
    for (uint32_t i = 0; i < count; i++) {
        memcpy(state->bounce, input + (size_t)i * 512, 512);
        int result = issue(state, ATA_CMD_WRITE_DMA_EXT, lba + i, 1, true,
                           state->bounce, 512);
        if (result < 0) { spinlock_unlock(&state->lock); return result; }
    }
    spinlock_unlock(&state->lock);
    return 0;
}

static int block_read_ahci(struct block_device* device, uint64_t lba,
                           uint32_t count, void* buffer) {
    return ahci_read((ahci_port_t*)device->data, lba, count, buffer);
}
static int block_write_ahci(struct block_device* device, uint64_t lba,
                            uint32_t count, const void* buffer) {
    return ahci_write((ahci_port_t*)device->data, lba, count, buffer);
}
static int block_sync_ahci(struct block_device* device) {
    ahci_port_t* state = (ahci_port_t*)device->data;
    spinlock_lock(&state->lock);
    int result = issue(state, ATA_CMD_FLUSH_CACHE_EXT, 0, 0, false, NULL, 0);
    spinlock_unlock(&state->lock);
    return result;
}
static const struct block_device_ops g_block_ops = {
    .read = block_read_ahci, .write = block_write_ahci,
    .sync = block_sync_ahci
};

static uint64_t identify_capacity(ahci_port_t* state) {
    if (issue(state, ATA_CMD_IDENTIFY, 0, 0, false, state->bounce, 512) < 0)
        return 0;
    uint16_t* words = (uint16_t*)state->bounce;
    uint64_t sectors = (uint64_t)words[100] | ((uint64_t)words[101] << 16) |
        ((uint64_t)words[102] << 32) | ((uint64_t)words[103] << 48);
    if (!sectors) sectors = (uint64_t)words[60] | ((uint64_t)words[61] << 16);
    return sectors;
}

static int initialize_port(struct hba_memory* memory, uint8_t number) {
    struct hba_port* port = &memory->ports[number];
    uint32_t status = port->ssts;
    if ((status & 0x0fu) != 3u || ((status >> 8) & 0x0fu) != 1u ||
        port->sig != SATA_SIG_ATA) return -ENODEV;
    if (g_port_count >= AHCI_MAX_PORTS) return -ENOSPC;
    ahci_port_t* state = &g_ports[g_port_count];
    memset(state, 0, sizeof(*state));
    state->hba = &g_hba; state->port_num = number;
    state->type = port->sig == SATA_SIG_ATAPI ? AHCI_PORT_SATAPI : AHCI_PORT_SATA;
    state->present = true; state->registers = port;
    spinlock_init(&state->lock);
    state->command_list = pmm_alloc_page();
    state->received_fis = pmm_alloc_page();
    state->command_tables = pmm_alloc_page();
    state->bounce = pmm_alloc_page();
    if (!state->command_list || !state->received_fis ||
        !state->command_tables || !state->bounce) return -ENOMEM;
    memset(state->command_list, 0, PAGE_SIZE);
    memset(state->received_fis, 0, PAGE_SIZE);
    memset(state->command_tables, 0, PAGE_SIZE);
    if (stop_port(port) < 0) return -ETIMEOUT;
    uintptr_t clb = (uintptr_t)state->command_list;
    uintptr_t fb = (uintptr_t)state->received_fis;
    port->clb = (uint32_t)clb;
    port->clbu = (uint32_t)((uint64_t)clb >> 32);
    port->fb = (uint32_t)fb;
    port->fbu = (uint32_t)((uint64_t)fb >> 32);
    port->serr = 0xffffffffu; port->is = 0xffffffffu; port->ie = 0;
    if (start_port(port) < 0) return -ETIMEOUT;
    strcpy(state->block.name, "sda");
    state->block.name[2] = (char)('a' + g_port_count);
    state->block.name[3] = '\0';
    state->block.sector_size = 512;
    state->block.sector_count = identify_capacity(state);
    state->block.data = state; state->block.ops = &g_block_ops;
    int result = block_register(&state->block);
    if (result < 0) return result;
    g_port_count++;
    return 0;
}

int ahci_init(void) {
    g_port_count = 0;
#if !defined(__i386__) && !defined(__x86_64__)
    return -ENOTSUP;
#else
    struct pci_device* controller = NULL;
    for (size_t i = 0; i < pci_device_count(); i++) {
        struct pci_device* candidate = pci_device_at(i);
        if (candidate && candidate->class_code == PCI_CLASS_STORAGE &&
            candidate->subclass == PCI_CLASS_STORAGE_SATA &&
            candidate->prog_if == 1) { controller = candidate; break; }
    }
    if (!controller) return -ENODEV;
    uint32_t abar = controller->bar[5] & ~0x0fu;
    if (!abar) return -ENODEV;
    for (uintptr_t page = abar & ~(uintptr_t)(PAGE_SIZE - 1);
         page < (uintptr_t)abar + 0x2000; page += PAGE_SIZE)
        if (vmm_map(vmm_kernel_context(), (void*)page, page,
                    VMM_WRITABLE | VMM_NOCACHE) < 0) return -ENOMEM;
    uint32_t command = pci_read_config(controller->bus, controller->slot,
                                        controller->function, PCI_COMMAND);
    command |= 0x00000006u;
    pci_write_config(controller->bus, controller->slot,
                     controller->function, PCI_COMMAND, command);
    struct hba_memory* memory = (struct hba_memory*)(uintptr_t)abar;
    memory->ghc |= HBA_GHC_AE;
    memset(&g_hba, 0, sizeof(g_hba));
    g_hba.mmio_base = memory; g_hba.port_map = memory->pi;
    g_hba.num_ports = (uint8_t)((memory->cap & 0x1fu) + 1);
    memset(g_ports, 0, sizeof(g_ports));
    for (uint8_t i = 0; i < 32; i++)
        if (memory->pi & (1u << i)) (void)initialize_port(memory, i);
    if (!g_port_count) return -ENODEV;
    klog_info("ahci", "%u SATA disk(s) online", (unsigned)g_port_count);
    return (int)g_port_count;
#endif
}

size_t ahci_port_count(void) { return g_port_count; }
ahci_port_t* ahci_port_at(size_t index) {
    return index < g_port_count ? &g_ports[index] : NULL;
}
int ahci_read_sector(ahci_port_t* port, uint32_t lba, uint8_t* buffer) {
    return ahci_read(port, lba, 1, buffer);
}
int ahci_write_sector(ahci_port_t* port, uint32_t lba,
                      const uint8_t* buffer) {
    return ahci_write(port, lba, 1, buffer);
}
