#include <fs/fat32.h>
#include <kernel/errno.h>
#include <kernel/string.h>

#define FAT32_MAX_NODES 128
#define FAT32_EOC 0x0ffffff8u
#define FAT32_ATTR_DIRECTORY 0x10u
#define FAT32_ATTR_LFN 0x0fu
#define FAT32_ATTR_VOLUME 0x08u

struct fat_dir_entry {
    uint8_t name[11], attributes, nt_reserved, creation_tenths;
    uint16_t creation_time, creation_date, cluster_high, modify_time;
    uint16_t modify_date, cluster_low;
    uint32_t size;
} __attribute__((packed));

struct fat32_fs {
    struct block_device* device;
    uint32_t partition_lba, fat_lba, data_lba, fat_sectors;
    uint32_t total_sectors, cluster_count, root_cluster;
    uint8_t sectors_per_cluster, fat_count;
    struct vnode root;
};

struct fat32_node {
    bool used;
    struct vnode vnode;
    struct fat32_fs* fs;
    uint32_t cluster, entry_lba;
    uint16_t entry_offset;
    uint8_t attributes;
};

static struct fat32_fs g_filesystem;
static struct fat32_node g_nodes[FAT32_MAX_NODES];
static void fat_ops_assign(struct vnode* vnode);

static uint32_t div_u32(uint32_t value, uint32_t divisor, uint32_t* remainder) {
    if (!divisor) return 0;
    uint32_t quotient = 0, rem = 0;
    for (int bit = 31; bit >= 0; bit--) {
        rem = (rem << 1) | ((value >> bit) & 1u);
        if (rem >= divisor) { rem -= divisor; quotient |= 1u << bit; }
    }
    if (remainder) *remainder = rem;
    return quotient;
}

static uint32_t node_cluster(const struct fat_dir_entry* entry) {
    return ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
}
static void set_entry_cluster(struct fat_dir_entry* entry, uint32_t cluster) {
    entry->cluster_high = (uint16_t)(cluster >> 16);
    entry->cluster_low = (uint16_t)cluster;
}
static uint32_t cluster_lba(const struct fat32_fs* fs, uint32_t cluster) {
    return fs->data_lba + (cluster - 2u) * fs->sectors_per_cluster;
}
static bool valid_cluster(const struct fat32_fs* fs, uint32_t cluster) {
    return cluster >= 2 && cluster < fs->cluster_count + 2;
}

static int read_sector(struct fat32_fs* fs, uint32_t lba, void* buffer) {
    return block_read(fs->device, lba, 1, buffer);
}
static int write_sector(struct fat32_fs* fs, uint32_t lba,
                        const void* buffer) {
    return block_write(fs->device, lba, 1, buffer);
}

static int fat_get(struct fat32_fs* fs, uint32_t cluster, uint32_t* value) {
    uint8_t sector[512];
    uint32_t byte = cluster << 2;
    int result = read_sector(fs, fs->fat_lba + (byte >> 9), sector);
    if (result < 0) return result;
    uint32_t offset = byte & 511u;
    uint32_t entry;
    memcpy(&entry, sector + offset, sizeof(entry));
    *value = entry & 0x0fffffffu;
    return 0;
}
static int fat_set(struct fat32_fs* fs, uint32_t cluster, uint32_t value) {
    uint32_t byte = cluster << 2;
    uint32_t sector_offset = byte >> 9, offset = byte & 511u;
    for (uint8_t fat = 0; fat < fs->fat_count; fat++) {
        uint8_t sector[512];
        uint32_t lba = fs->fat_lba + fat * fs->fat_sectors + sector_offset;
        int result = read_sector(fs, lba, sector);
        if (result < 0) return result;
        uint32_t old;
        memcpy(&old, sector + offset, sizeof(old));
        value = (old & 0xf0000000u) | (value & 0x0fffffffu);
        memcpy(sector + offset, &value, sizeof(value));
        result = write_sector(fs, lba, sector);
        if (result < 0) return result;
    }
    return 0;
}

static int zero_cluster(struct fat32_fs* fs, uint32_t cluster) {
    uint8_t zero[512]; memset(zero, 0, sizeof(zero));
    uint32_t first = cluster_lba(fs, cluster);
    for (uint8_t i = 0; i < fs->sectors_per_cluster; i++) {
        int result = write_sector(fs, first + i, zero);
        if (result < 0) return result;
    }
    return 0;
}
static int allocate_cluster(struct fat32_fs* fs, uint32_t* result_cluster) {
    for (uint32_t cluster = 2; cluster < fs->cluster_count + 2; cluster++) {
        uint32_t value;
        int result = fat_get(fs, cluster, &value);
        if (result < 0) return result;
        if (value) continue;
        result = fat_set(fs, cluster, 0x0fffffffu);
        if (result < 0) return result;
        result = zero_cluster(fs, cluster);
        if (result < 0) { (void)fat_set(fs, cluster, 0); return result; }
        *result_cluster = cluster;
        return 0;
    }
    return -ENOSPC;
}
static void free_chain(struct fat32_fs* fs, uint32_t cluster) {
    for (uint32_t guard = 0; valid_cluster(fs, cluster) &&
         guard < fs->cluster_count; guard++) {
        uint32_t next;
        if (fat_get(fs, cluster, &next) < 0) return;
        (void)fat_set(fs, cluster, 0);
        if (next >= FAT32_EOC) return;
        cluster = next;
    }
}

static int make_short_name(const char* name, uint8_t output[11]) {
    if (!name || !*name || !strcmp(name, ".") || !strcmp(name, ".."))
        return -EINVAL;
    memset(output, ' ', 11);
    size_t base = 0, extension = 0;
    bool dot = false;
    for (const char* cursor = name; *cursor; cursor++) {
        unsigned char value = (unsigned char)*cursor;
        if (value == '.') { if (dot) return -ENAMETOOLONG; dot = true; continue; }
        if (value <= 0x20 || value == '/' || value == '\\' || value == ':' ||
            value == '*' || value == '?' || value == '"' || value == '<' ||
            value == '>' || value == '|') return -EINVAL;
        if (value >= 'a' && value <= 'z') value -= 'a' - 'A';
        if (!dot) { if (base >= 8) return -ENAMETOOLONG; output[base++] = value; }
        else { if (extension >= 3) return -ENAMETOOLONG; output[8 + extension++] = value; }
    }
    return base ? 0 : -EINVAL;
}
static void decode_short_name(const uint8_t input[11], char* output,
                              size_t capacity) {
    size_t position = 0, base = 8, ext = 3;
    while (base && input[base - 1] == ' ') base--;
    while (ext && input[8 + ext - 1] == ' ') ext--;
    for (size_t i = 0; i < base && position + 1 < capacity; i++) {
        char value = (char)input[i];
        if (value >= 'A' && value <= 'Z') value += 'a' - 'A';
        output[position++] = value;
    }
    if (ext && position + 1 < capacity) output[position++] = '.';
    for (size_t i = 0; i < ext && position + 1 < capacity; i++) {
        char value = (char)input[8 + i];
        if (value >= 'A' && value <= 'Z') value += 'a' - 'A';
        output[position++] = value;
    }
    output[position] = '\0';
}

typedef int (*entry_callback)(struct fat32_fs*, uint32_t, uint16_t,
                              struct fat_dir_entry*, void*);
static int walk_directory(struct fat32_fs* fs, uint32_t directory_cluster,
                          entry_callback callback, void* context) {
    uint8_t sector[512];
    uint32_t cluster = directory_cluster;
    for (uint32_t guard = 0; valid_cluster(fs, cluster) &&
         guard < fs->cluster_count; guard++) {
        uint32_t first = cluster_lba(fs, cluster);
        for (uint8_t s = 0; s < fs->sectors_per_cluster; s++) {
            int result = read_sector(fs, first + s, sector);
            if (result < 0) return result;
            for (uint16_t offset = 0; offset < 512;
                 offset += sizeof(struct fat_dir_entry)) {
                struct fat_dir_entry entry;
                memcpy(&entry, sector + offset, sizeof(entry));
                result = callback(fs, first + s, offset, &entry, context);
                if (result) return result;
                if (entry.name[0] == 0) return 0;
            }
        }
        uint32_t next;
        int result = fat_get(fs, cluster, &next);
        if (result < 0) return result;
        if (next >= FAT32_EOC) return 0;
        cluster = next;
    }
    return 0;
}

static struct fat32_node* allocate_node(struct fat32_fs* fs,
                                        const struct fat_dir_entry* entry,
                                        uint32_t lba, uint16_t offset,
                                        struct vnode* parent) {
    uint32_t cluster = node_cluster(entry);
    for (size_t i = 0; i < FAT32_MAX_NODES; i++) {
        if (g_nodes[i].used && g_nodes[i].fs == fs &&
            g_nodes[i].entry_lba == lba && g_nodes[i].entry_offset == offset)
            return &g_nodes[i];
    }
    for (size_t i = 0; i < FAT32_MAX_NODES; i++) {
        if (g_nodes[i].used) continue;
        struct fat32_node* node = &g_nodes[i];
        memset(node, 0, sizeof(*node)); node->used = true; node->fs = fs;
        node->cluster = cluster; node->entry_lba = lba;
        node->entry_offset = offset; node->attributes = entry->attributes;
        node->vnode.parent = parent; node->vnode.data = node;
        node->vnode.size = entry->size; node->vnode.refcount = 1;
        decode_short_name(entry->name, node->vnode.name_storage,
                          sizeof(node->vnode.name_storage));
        node->vnode.name = node->vnode.name_storage;
        node->vnode.type = (entry->attributes & FAT32_ATTR_DIRECTORY) ?
            VNODE_DIR : VNODE_FILE;
        return node;
    }
    return NULL;
}

struct lookup_context { const uint8_t* name; struct vnode* parent; struct fat32_node* found; };
static int lookup_callback(struct fat32_fs* fs, uint32_t lba, uint16_t offset,
                           struct fat_dir_entry* entry, void* opaque) {
    struct lookup_context* context = (struct lookup_context*)opaque;
    if (!entry->name[0] || entry->name[0] == 0xe5 ||
        (entry->attributes & (FAT32_ATTR_LFN | FAT32_ATTR_VOLUME))) return 0;
    if (memcmp(entry->name, context->name, 11)) return 0;
    context->found = allocate_node(fs, entry, lba, offset, context->parent);
    return 1;
}

static struct vnode* fat_lookup(struct vnode* directory, const char* name) {
    struct fat32_node* parent = (struct fat32_node*)directory->data;
    struct fat32_fs* fs = parent ? parent->fs : &g_filesystem;
    uint32_t cluster = parent ? parent->cluster : fs->root_cluster;
    uint8_t short_name[11];
    if (make_short_name(name, short_name) < 0) return NULL;
    struct lookup_context context = { short_name, directory, NULL };
    int result = walk_directory(fs, cluster, lookup_callback, &context);
    if (result < 0 || !context.found) return NULL;
    fat_ops_assign(&context.found->vnode);
    return &context.found->vnode;
}

struct free_context { bool found; uint32_t lba; uint16_t offset; };
static int free_callback(struct fat32_fs* fs, uint32_t lba, uint16_t offset,
                         struct fat_dir_entry* entry, void* opaque) {
    (void)fs;
    struct free_context* context = (struct free_context*)opaque;
    if (entry->name[0] == 0 || entry->name[0] == 0xe5) {
        context->found = true; context->lba = lba; context->offset = offset;
        return 1;
    }
    return 0;
}

static int write_entry(struct fat32_fs* fs, uint32_t lba, uint16_t offset,
                       const struct fat_dir_entry* entry) {
    uint8_t sector[512];
    int result = read_sector(fs, lba, sector);
    if (result < 0) return result;
    memcpy(sector + offset, entry, sizeof(*entry));
    return write_sector(fs, lba, sector);
}

static int find_free_entry(struct fat32_fs* fs, uint32_t directory_cluster,
                           struct free_context* context) {
    memset(context, 0, sizeof(*context));
    int result = walk_directory(fs, directory_cluster, free_callback, context);
    if (result < 0) return result;
    if (context->found) return 0;
    uint32_t cluster = directory_cluster;
    for (;;) {
        uint32_t next;
        result = fat_get(fs, cluster, &next);
        if (result < 0) return result;
        if (next >= FAT32_EOC) break;
        cluster = next;
    }
    uint32_t new_cluster;
    result = allocate_cluster(fs, &new_cluster);
    if (result < 0) return result;
    result = fat_set(fs, cluster, new_cluster);
    if (result < 0) { free_chain(fs, new_cluster); return result; }
    context->found = true; context->lba = cluster_lba(fs, new_cluster);
    context->offset = 0;
    return 0;
}

static struct vnode* fat_create(struct vnode* directory, const char* name,
                                int type) {
    if (fat_lookup(directory, name)) return NULL;
    struct fat32_node* parent = (struct fat32_node*)directory->data;
    struct fat32_fs* fs = parent ? parent->fs : &g_filesystem;
    uint32_t directory_cluster = parent ? parent->cluster : fs->root_cluster;
    struct fat_dir_entry entry; memset(&entry, 0, sizeof(entry));
    if (make_short_name(name, entry.name) < 0) return NULL;
    if (type == VNODE_DIR) {
        uint32_t cluster;
        if (allocate_cluster(fs, &cluster) < 0) return NULL;
        entry.attributes = FAT32_ATTR_DIRECTORY;
        set_entry_cluster(&entry, cluster);
        struct fat_dir_entry dots[2]; memset(dots, 0, sizeof(dots));
        memset(dots[0].name, ' ', 11); dots[0].name[0] = '.';
        dots[0].attributes = FAT32_ATTR_DIRECTORY; set_entry_cluster(&dots[0], cluster);
        memset(dots[1].name, ' ', 11); dots[1].name[0] = '.'; dots[1].name[1] = '.';
        dots[1].attributes = FAT32_ATTR_DIRECTORY;
        set_entry_cluster(&dots[1], directory_cluster);
        uint8_t sector[512]; memset(sector, 0, sizeof(sector));
        memcpy(sector, dots, sizeof(dots));
        if (write_sector(fs, cluster_lba(fs, cluster), sector) < 0) {
            free_chain(fs, cluster); return NULL;
        }
    }
    struct free_context free_entry;
    if (find_free_entry(fs, directory_cluster, &free_entry) < 0 ||
        write_entry(fs, free_entry.lba, free_entry.offset, &entry) < 0) {
        if (node_cluster(&entry)) free_chain(fs, node_cluster(&entry));
        return NULL;
    }
    struct fat32_node* node = allocate_node(fs, &entry, free_entry.lba,
                                             free_entry.offset, directory);
    if (!node) return NULL;
    fat_ops_assign(&node->vnode);
    return &node->vnode;
}

static int update_node_entry(struct fat32_node* node) {
    uint8_t sector[512];
    int result = read_sector(node->fs, node->entry_lba, sector);
    if (result < 0) return result;
    struct fat_dir_entry* entry =
        (struct fat_dir_entry*)(sector + node->entry_offset);
    set_entry_cluster(entry, node->cluster);
    entry->size = (uint32_t)node->vnode.size;
    return write_sector(node->fs, node->entry_lba, sector);
}

static int cluster_at(struct fat32_node* node, uint32_t index, bool create,
                      uint32_t* result_cluster) {
    if (!node->cluster) {
        if (!create) return -ENOENT;
        int result = allocate_cluster(node->fs, &node->cluster);
        if (result < 0) return result;
        result = update_node_entry(node);
        if (result < 0) return result;
    }
    uint32_t cluster = node->cluster;
    while (index--) {
        uint32_t next;
        int result = fat_get(node->fs, cluster, &next);
        if (result < 0) return result;
        if (next >= FAT32_EOC) {
            if (!create) return -ENOENT;
            result = allocate_cluster(node->fs, &next);
            if (result < 0) return result;
            result = fat_set(node->fs, cluster, next);
            if (result < 0) { free_chain(node->fs, next); return result; }
        }
        cluster = next;
    }
    *result_cluster = cluster;
    return 0;
}

static ssize_t fat_read(struct vnode* vnode, void* buffer, size_t count,
                        int64_t offset) {
    if (!vnode || !buffer || offset < 0 || vnode->type != VNODE_FILE)
        return -EINVAL;
    if ((size_t)offset >= vnode->size) return 0;
    if (count > vnode->size - (size_t)offset) count = vnode->size - (size_t)offset;
    struct fat32_node* node = (struct fat32_node*)vnode->data;
    uint32_t cluster_bytes = (uint32_t)node->fs->sectors_per_cluster << 9;
    size_t done = 0;
    while (done < count) {
        uint32_t position = (uint32_t)((size_t)offset + done), within = 0;
        uint32_t cluster_index = div_u32(position, cluster_bytes, &within);
        uint32_t cluster;
        if (cluster_at(node, cluster_index, false, &cluster) < 0) break;
        uint32_t sector_index = within >> 9, sector_offset = within & 511u;
        uint8_t sector[512];
        if (read_sector(node->fs, cluster_lba(node->fs, cluster) + sector_index,
                        sector) < 0) return done ? (ssize_t)done : -EIO;
        size_t chunk = count - done;
        if (chunk > 512u - sector_offset) chunk = 512u - sector_offset;
        memcpy((uint8_t*)buffer + done, sector + sector_offset, chunk);
        done += chunk;
    }
    return (ssize_t)done;
}

static ssize_t fat_write(struct vnode* vnode, const void* buffer, size_t count,
                         int64_t offset) {
    if (!vnode || !buffer || offset < 0 || vnode->type != VNODE_FILE)
        return -EINVAL;
    if ((uint64_t)offset + count > 0xffffffffu) return -EFBIG;
    struct fat32_node* node = (struct fat32_node*)vnode->data;
    uint32_t cluster_bytes = (uint32_t)node->fs->sectors_per_cluster << 9;
    size_t done = 0;
    while (done < count) {
        uint32_t position = (uint32_t)((size_t)offset + done), within = 0;
        uint32_t cluster_index = div_u32(position, cluster_bytes, &within);
        uint32_t cluster;
        int result = cluster_at(node, cluster_index, true, &cluster);
        if (result < 0) return done ? (ssize_t)done : result;
        uint32_t sector_index = within >> 9, sector_offset = within & 511u;
        uint8_t sector[512];
        uint32_t lba = cluster_lba(node->fs, cluster) + sector_index;
        if (sector_offset || count - done < 512) {
            if (read_sector(node->fs, lba, sector) < 0)
                return done ? (ssize_t)done : -EIO;
        }
        size_t chunk = count - done;
        if (chunk > 512u - sector_offset) chunk = 512u - sector_offset;
        memcpy(sector + sector_offset, (const uint8_t*)buffer + done, chunk);
        if (write_sector(node->fs, lba, sector) < 0)
            return done ? (ssize_t)done : -EIO;
        done += chunk;
    }
    size_t end = (size_t)offset + done;
    if (end > vnode->size) { vnode->size = end; (void)update_node_entry(node); }
    return (ssize_t)done;
}

static int fat_truncate(struct vnode* vnode, size_t size) {
    if (!vnode || vnode->type != VNODE_FILE) return -EINVAL;
    struct fat32_node* node = (struct fat32_node*)vnode->data;
    if (!size) {
        if (node->cluster) free_chain(node->fs, node->cluster);
        node->cluster = 0; vnode->size = 0;
        return update_node_entry(node);
    }
    if (size > vnode->size) {
        uint8_t zero[64]; memset(zero, 0, sizeof(zero));
        while (vnode->size < size) {
            size_t chunk = size - vnode->size;
            if (chunk > sizeof(zero)) chunk = sizeof(zero);
            ssize_t result = fat_write(vnode, zero, chunk, vnode->size);
            if (result < 0) return (int)result;
        }
    } else if (size < vnode->size) {
        uint32_t cluster_bytes =
            (uint32_t)node->fs->sectors_per_cluster << 9;
        uint32_t needed = div_u32((uint32_t)size + cluster_bytes - 1,
                                  cluster_bytes, NULL);
        uint32_t last;
        if (needed && cluster_at(node, needed - 1, false, &last) == 0) {
            uint32_t next;
            if (fat_get(node->fs, last, &next) == 0) {
                (void)fat_set(node->fs, last, 0x0fffffffu);
                if (valid_cluster(node->fs, next)) free_chain(node->fs, next);
            }
        }
        vnode->size = size;
    }
    return update_node_entry(node);
}

static int fat_open(struct vnode* vnode, int flags) {
    return (flags & O_TRUNC) ? fat_truncate(vnode, 0) : 0;
}
static int fat_close(struct vnode* vnode) { (void)vnode; return 0; }

struct readdir_context { int64_t wanted, current; struct dirent* output; };
static int readdir_callback(struct fat32_fs* fs, uint32_t lba, uint16_t offset,
                            struct fat_dir_entry* entry, void* opaque) {
    (void)fs; (void)lba; (void)offset;
    struct readdir_context* context = (struct readdir_context*)opaque;
    if (!entry->name[0] || entry->name[0] == 0xe5 ||
        (entry->attributes & (FAT32_ATTR_LFN | FAT32_ATTR_VOLUME))) return 0;
    if (context->current++ != context->wanted) return 0;
    memset(context->output, 0, sizeof(*context->output));
    context->output->d_ino = node_cluster(entry);
    context->output->d_off = (uint32_t)(context->wanted + 1);
    context->output->d_reclen = sizeof(*context->output);
    context->output->d_type = (entry->attributes & FAT32_ATTR_DIRECTORY) ?
        VNODE_DIR : VNODE_FILE;
    decode_short_name(entry->name, context->output->d_name,
                      sizeof(context->output->d_name));
    return 1;
}
static int fat_readdir(struct vnode* vnode, struct dirent* entry,
                       int64_t offset) {
    if (!vnode || !entry || offset < 0) return -EINVAL;
    struct fat32_node* node = (struct fat32_node*)vnode->data;
    struct fat32_fs* fs = node ? node->fs : &g_filesystem;
    uint32_t cluster = node ? node->cluster : fs->root_cluster;
    struct readdir_context context = { offset, 0, entry };
    int result = walk_directory(fs, cluster, readdir_callback, &context);
    return result < 0 ? result : result == 1 ? 1 : 0;
}

struct empty_context { bool nonempty; };
static int empty_callback(struct fat32_fs* fs, uint32_t lba, uint16_t offset,
                          struct fat_dir_entry* entry, void* opaque) {
    (void)fs; (void)lba; (void)offset;
    struct empty_context* context = (struct empty_context*)opaque;
    if (!entry->name[0] || entry->name[0] == 0xe5 ||
        (entry->attributes & (FAT32_ATTR_LFN | FAT32_ATTR_VOLUME))) return 0;
    if (entry->name[0] == '.') return 0;
    context->nonempty = true; return 1;
}
static int fat_unlink(struct vnode* directory, const char* name) {
    struct vnode* vnode = fat_lookup(directory, name);
    if (!vnode) return -ENOENT;
    struct fat32_node* node = (struct fat32_node*)vnode->data;
    if (vnode->type == VNODE_DIR) {
        struct empty_context context = { false };
        int result = walk_directory(node->fs, node->cluster,
                                    empty_callback, &context);
        if (result < 0) return result;
        if (context.nonempty) return -ENOTEMPTY;
    }
    uint8_t sector[512];
    int result = read_sector(node->fs, node->entry_lba, sector);
    if (result < 0) return result;
    sector[node->entry_offset] = 0xe5;
    result = write_sector(node->fs, node->entry_lba, sector);
    if (result < 0) return result;
    if (node->cluster) free_chain(node->fs, node->cluster);
    node->used = false;
    return 0;
}
static int fat_sync(struct vnode* vnode) {
    struct fat32_node* node = vnode ? (struct fat32_node*)vnode->data : NULL;
    return block_sync(node ? node->fs->device : g_filesystem.device);
}

static struct vnode_ops g_fat_ops = {
    .open = fat_open, .close = fat_close, .read = fat_read,
    .write = fat_write, .readdir = fat_readdir, .lookup = fat_lookup,
    .create = fat_create, .unlink = fat_unlink, .truncate = fat_truncate,
    .sync = fat_sync
};
static void fat_ops_assign(struct vnode* vnode) { vnode->ops = &g_fat_ops; }

int fat32_mount(struct block_device* device, struct vnode** root) {
    if (!device || !root || device->sector_size != 512) return -EINVAL;
    uint8_t sector[512];
    if (block_read(device, 0, 1, sector) < 0) return -EIO;
    uint32_t partition = 0;
    struct fat32_boot_sector* boot = (struct fat32_boot_sector*)sector;
    if (boot->bytes_per_sector != 512 || !boot->fat_size_32) {
        if (sector[510] != 0x55 || sector[511] != 0xaa) return -ENOEXEC;
        for (size_t i = 0; i < 4; i++) {
            uint8_t* entry = sector + 446 + i * 16;
            if (entry[4] != 0x0b && entry[4] != 0x0c) continue;
            memcpy(&partition, entry + 8, sizeof(partition)); break;
        }
        if (!partition || block_read(device, partition, 1, sector) < 0)
            return -ENOEXEC;
        boot = (struct fat32_boot_sector*)sector;
    }
    if (boot->bytes_per_sector != 512 || !boot->sectors_per_cluster ||
        !boot->reserved_sectors || !boot->num_fats || !boot->fat_size_32 ||
        boot->root_cluster < 2 || sector[510] != 0x55 || sector[511] != 0xaa)
        return -ENOEXEC;
    memset(&g_filesystem, 0, sizeof(g_filesystem));
    memset(g_nodes, 0, sizeof(g_nodes));
    struct fat32_fs* fs = &g_filesystem;
    fs->device = device; fs->partition_lba = partition;
    fs->fat_lba = partition + boot->reserved_sectors;
    fs->fat_sectors = boot->fat_size_32; fs->fat_count = boot->num_fats;
    fs->data_lba = fs->fat_lba + fs->fat_count * fs->fat_sectors;
    fs->total_sectors = boot->total_sectors_32 ? boot->total_sectors_32 :
        boot->total_sectors_16;
    fs->sectors_per_cluster = boot->sectors_per_cluster;
    fs->root_cluster = boot->root_cluster;
    uint32_t data_sectors = fs->total_sectors -
        (boot->reserved_sectors + fs->fat_count * fs->fat_sectors);
    fs->cluster_count = div_u32(data_sectors, fs->sectors_per_cluster, NULL);
    if (fs->cluster_count < 65525) return -ENOEXEC;
    fs->root.type = VNODE_DIR; fs->root.refcount = 1;
    strcpy(fs->root.name_storage, "disk"); fs->root.name = fs->root.name_storage;
    fs->root.ops = &g_fat_ops; fs->root.data = NULL;
    *root = &fs->root;
    return 0;
}
