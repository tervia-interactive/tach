/* tach - Read-only USTAR initrd importer. */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <fs/initrd.h>
#include <fs/vfs.h>

struct tar_header {
    char name[100], mode[8], uid[8], gid[8], size[12], mtime[12];
    char checksum[8], type, linkname[100], magic[6], version[2];
    char owner[32], group[32], major[8], minor[8], prefix[155], padding[12];
} __attribute__((packed));

static size_t parse_octal(const char* text, size_t length) {
    size_t value = 0;
    for (size_t i = 0; i < length; i++) {
        if (text[i] == '\0' || text[i] == ' ') break;
        if (text[i] < '0' || text[i] > '7') return 0;
        value = (value << 3) + (size_t)(text[i] - '0');
    }
    return value;
}

static bool zero_block(const uint8_t* block) {
    for (size_t i = 0; i < 512; i++) if (block[i]) return false;
    return true;
}

static size_t bounded_length(const char* text, size_t limit) {
    size_t length = 0;
    while (length < limit && text[length]) length++;
    return length;
}

int initrd_parse(void* address, size_t archive_size) {
    if (!address || archive_size < 512) return -EINVAL;
    uint8_t* cursor = (uint8_t*)address;
    size_t imported = 0;
    for (size_t scanned = 0; scanned + 512 <= archive_size;) {
        if (zero_block(cursor)) break;
        struct tar_header* header = (struct tar_header*)cursor;
        if (memcmp(header->magic, "ustar", 5) != 0) return -ENOEXEC;
        size_t size = parse_octal(header->size, sizeof(header->size));
        char path[128];
        memset(path, 0, sizeof(path));
        size_t position = 0;
        path[position++] = '/';
        if (header->prefix[0]) {
            size_t prefix = bounded_length(header->prefix, sizeof(header->prefix));
            if (position + prefix + 1 >= sizeof(path)) return -ENAMETOOLONG;
            memcpy(path + position, header->prefix, prefix);
            position += prefix;
            path[position++] = '/';
        }
        size_t name = bounded_length(header->name, sizeof(header->name));
        if (position + name >= sizeof(path)) return -ENAMETOOLONG;
        memcpy(path + position, header->name, name);
        if ((header->type == '0' || header->type == '\0') && size) {
            int result = vfs_register_memfile(path, cursor + 512, size);
            if (result < 0) return result;
            imported++;
        }
        size_t advance = 512 + ((size + 511) & ~(size_t)511);
        if (advance < size || advance > archive_size - scanned) {
            return -ENOEXEC;
        }
        cursor += advance;
        scanned += advance;
    }
    return (int)imported;
}
