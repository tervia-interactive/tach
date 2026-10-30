#include <tach.h>
#include <tachc.h>

/* free [-m]: memoria fisica a partir de /proc/meminfo (kB; -m em MiB). */

static char g_text[512];

static int find_value(const char* key, unsigned long* out) {
    const char* at = strstr(g_text, key);
    if (!at) return -1;
    at += strlen(key);
    char* end;
    *out = strtoul(at, &end, 10);
    return end == at ? -1 : 0;
}

int main(int argc, char** argv) {
    int megabytes = 0;
    int c;
    while ((c = getopt(argc, argv, "m")) != -1) {
        if (c == 'm') megabytes = 1;
        else {
            eprintf("usage: free [-m]\n");
            return 2;
        }
    }
    (void)argc;
    int fd = open("/proc/meminfo", O_RDONLY);
    if (fd < 0) {
        eprintf("free: /proc/meminfo unavailable\n");
        return 1;
    }
    size_t length = 0;
    for (;;) {
        ssize_t count = read(fd, g_text + length, sizeof(g_text) - 1 - length);
        if (count <= 0) break;
        length += (size_t)count;
    }
    (void)close(fd);
    g_text[length] = '\0';

    unsigned long total, free_kb;
    if (find_value("MemTotal:", &total) < 0 || find_value("MemFree:", &free_kb) < 0) {
        eprintf("free: cannot parse /proc/meminfo\n");
        return 1;
    }
    unsigned long used = total >= free_kb ? total - free_kb : 0;
    if (megabytes) {
        total /= 1024u; used /= 1024u; free_kb /= 1024u;
    }
    printf("%12s%12s%12s\n", "total", "used", "free");
    printf("Mem:%8lu%12lu%12lu\n", total, used, free_kb);
    return 0;
}
