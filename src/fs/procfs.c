/*
 * fs/procfs.c - /proc: arquivos de texto gerados sob demanda
 *
 * Alimenta os comandos do shell de userspace (ps, uname -a, syscalls,
 * cat /proc/...), que antes liam estruturas do kernel diretamente.
 *
 * Um read em offset 0 regenera o texto num snapshot; reads seguintes
 * (offset > 0) fatiam esse mesmo snapshot, entao um arquivo lido em varios
 * pedacos (sys_read le de 256 em 256 bytes) fica consistente.
 */
#include <kernel/errno.h>
#include <kernel/printf.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/types.h>
#include <kernel/version.h>
#include <fs/procfs.h>
#include <mm/pmm.h>
#include <proc/process.h>
#include <proc/syscall.h>

#define PROCFS_FILE_COUNT 6
#define PROCFS_BUFFER_SIZE 2048

static spinlock_t g_procfs_lock = SPINLOCK_INIT;
static char* g_out;
static size_t g_out_length;

static void procfs_putc(char c) {
    if (g_out_length + 1 < PROCFS_BUFFER_SIZE) g_out[g_out_length++] = c;
}

#define pf(...) kprintf_to(procfs_putc, __VA_ARGS__)

static const char* procfs_arch(void) {
#if defined(__x86_64__)
    return "x86_64";
#elif defined(__i386__)
    return "i686";
#elif defined(__aarch64__)
    return "aarch64";
#elif defined(__arm__)
    return "arm32";
#elif defined(__riscv) && __riscv_xlen == 64
    return "riscv64";
#elif defined(__riscv)
    return "riscv32";
#else
    return "unknown";
#endif
}

static void gen_version(void) {
    pf("tach %s %s (freestanding C kernel)\n", TACH_VERSION_STRING,
       procfs_arch());
}

static void gen_uname(void) {
    pf("tach %s %s userspace\n", TACH_VERSION_STRING, procfs_arch());
}

static void gen_meminfo(void) {
    pf("MemTotal:\t%lu kB\nMemFree:\t%lu kB\n",
       (unsigned long)(pmm_get_total_pages() * 4),
       (unsigned long)(pmm_get_free_pages() * 4));
}

static void gen_self_status(void) {
    struct process* proc = process_get_current();
    if (!proc) return;
    pf("Name:\t%s\nPid:\t%u\nPPid:\t%u\nState:\t%s\nUid:\t%u\nGid:\t%u\n",
       proc->name, (unsigned)proc->pid, (unsigned)proc->ppid,
       process_state_name(proc->state), (unsigned)proc->uid,
       (unsigned)proc->gid);
}

static void gen_ps(void) {
    struct process* current = process_get_current();
    pf(" PID  PPID STATE     TICKS NAME\n");
    for (size_t i = 0; i < process_count(); i++) {
        struct process* proc = process_at(i);
        if (!proc) continue;
        pf("%c%4u %5u ", proc == current ? '*' : ' ', (unsigned)proc->pid,
           (unsigned)proc->ppid);
        pf("%s", process_state_name(proc->state));
        size_t state_len = strlen(process_state_name(proc->state));
        while (state_len++ < 9) procfs_putc(' ');
        pf("%5lu %s\n", proc->runtime_ticks, proc->name);
    }
}

static void gen_syscalls(void) {
    for (int i = 0; i < SYSCALL_TABLE_SIZE; i++) {
        const char* name = syscall_name(i);
        if (name) pf("%3d  %s\n", i, name);
    }
}

static const struct {
    const char* path;
    void (*generate)(void);
} g_table[PROCFS_FILE_COUNT] = {
    {"/proc/version", gen_version},
    {"/proc/uname", gen_uname},
    {"/proc/meminfo", gen_meminfo},
    {"/proc/self/status", gen_self_status},
    {"/proc/ps", gen_ps},
    {"/proc/syscalls", gen_syscalls},
};

/* Um unico snapshot compartilhado (protegido por g_procfs_lock). Mantemos o
 * .bss do kernel pequeno de proposito: a imagem do kernel nao pode avancar
 * sobre 0x400000, onde o userspace e carregado (ver user/user.ld). */
static char g_data[PROCFS_BUFFER_SIZE];
static size_t g_length;
static size_t g_owner = (size_t)-1; /* arquivo dono do snapshot atual */
static struct vnode g_vnodes[PROCFS_FILE_COUNT];

static int procfs_open(struct vnode* vn, int flags) {
    (void)vn;
    return (flags & O_ACCMODE) == O_RDONLY ? 0 : -EACCES;
}

static int procfs_close(struct vnode* vn) {
    (void)vn;
    return 0;
}

static ssize_t procfs_read(struct vnode* vn, void* buf, size_t count,
                           int64_t offset) {
    if (!vn || !buf || offset < 0) return -EFAULT;
    if (vn < g_vnodes || vn >= g_vnodes + PROCFS_FILE_COUNT) return -EBADF;
    size_t index = (size_t)(vn - g_vnodes);

    spinlock_lock(&g_procfs_lock);
    if (offset == 0 || g_owner != index) {
        g_out = g_data;
        g_out_length = 0;
        g_table[index].generate();
        g_data[g_out_length] = '\0';
        g_length = g_out_length;
        g_owner = index;
    }
    ssize_t result = 0;
    if ((size_t)offset < g_length) {
        size_t available = g_length - (size_t)offset;
        if (count > available) count = available;
        memcpy(buf, g_data + (size_t)offset, count);
        result = (ssize_t)count;
    }
    spinlock_unlock(&g_procfs_lock);
    return result;
}

static struct vnode_ops g_procfs_ops = {
    .open = procfs_open, .close = procfs_close,
    .read = procfs_read, .write = NULL, .readdir = NULL
};

void procfs_init(void) {
    for (size_t i = 0; i < PROCFS_FILE_COUNT; i++) {
        struct vnode* vn = &g_vnodes[i];
        memset(vn, 0, sizeof(*vn));
        strncpy(vn->path, g_table[i].path, sizeof(vn->path) - 1);
        char* base = vn->path;
        for (char* cursor = vn->path; *cursor; cursor++)
            if (*cursor == '/') base = cursor + 1;
        vn->name = base;
        vn->type = VNODE_FILE;
        vn->parent = vfs_root();
        vn->ops = &g_procfs_ops;
        vn->refcount = 1;
    }
}

struct vnode* procfs_lookup(const char* path) {
    if (!path) return NULL;
    for (size_t i = 0; i < PROCFS_FILE_COUNT; i++)
        if (strcmp(path, g_table[i].path) == 0) return &g_vnodes[i];
    return NULL;
}

size_t procfs_count(void) { return PROCFS_FILE_COUNT; }

const char* procfs_path(size_t index) {
    return index < PROCFS_FILE_COUNT ? g_table[index].path : NULL;
}
