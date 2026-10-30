#include <tach.h>
#include <tachc.h>

/* ---------------------------------------------------------------- ctype */

int isdigit(int c) { return c >= '0' && c <= '9'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isalpha(int c) { return isupper(c) || islower(c); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v';
}
int isprint(int c) { return c >= 0x20 && c < 0x7f; }
int isxdigit(int c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
int toupper(int c) { return islower(c) ? c - 'a' + 'A' : c; }
int tolower(int c) { return isupper(c) ? c - 'A' + 'a' : c; }

/* --------------------------------------------------------------- strings */

size_t strspn(const char* text, const char* accept) {
    size_t count = 0;
    while (text[count] && strchr(accept, text[count])) count++;
    return count;
}

size_t strcspn(const char* text, const char* reject) {
    size_t count = 0;
    while (text[count] && !strchr(reject, text[count])) count++;
    return count;
}

char* strpbrk(const char* text, const char* accept) {
    for (; *text; text++)
        if (strchr(accept, *text)) return (char*)text;
    return (char*)0;
}

int strcasecmp(const char* left, const char* right) {
    for (;;) {
        int a = tolower((unsigned char)*left);
        int b = tolower((unsigned char)*right);
        if (a != b) return a - b;
        if (!a) return 0;
        left++;
        right++;
    }
}

int strncasecmp(const char* left, const char* right, size_t count) {
    while (count--) {
        int a = tolower((unsigned char)*left);
        int b = tolower((unsigned char)*right);
        if (a != b) return a - b;
        if (!a) return 0;
        left++;
        right++;
    }
    return 0;
}

size_t strlcpy(char* destination, const char* source, size_t size) {
    size_t length = strlen(source);
    if (size) {
        size_t copy = length < size - 1 ? length : size - 1;
        memcpy(destination, source, copy);
        destination[copy] = '\0';
    }
    return length;
}

const char* path_basename(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

const char* str_find(const char* haystack, const char* needle, int icase) {
    if (!icase) return strstr(haystack, needle);
    size_t length = strlen(needle);
    if (!length) return haystack;
    for (; *haystack; haystack++)
        if (strncasecmp(haystack, needle, length) == 0) return haystack;
    return (const char*)0;
}

/* ---------------------------------------------------------------- stdlib */

int abs(int value) { return value < 0 ? -value : value; }

unsigned long strtoul(const char* text, char** end, int base) {
    const char* cursor = text;
    while (isspace((unsigned char)*cursor)) cursor++;
    int negative = 0;
    if (*cursor == '+') cursor++;
    else if (*cursor == '-') { negative = 1; cursor++; }
    if ((base == 0 || base == 16) && cursor[0] == '0' &&
        (cursor[1] == 'x' || cursor[1] == 'X') && isxdigit((unsigned char)cursor[2])) {
        cursor += 2;
        base = 16;
    } else if (base == 0) {
        base = cursor[0] == '0' ? 8 : 10;
    }
    if (base < 2 || base > 36) {
        if (end) *end = (char*)text;
        return 0;
    }
    unsigned long value = 0;
    int any = 0;
    int overflow = 0;
    for (;; cursor++) {
        int c = (unsigned char)*cursor;
        int digit;
        if (isdigit(c)) digit = c - '0';
        else if (isalpha(c)) digit = tolower(c) - 'a' + 10;
        else break;
        if (digit >= base) break;
        any = 1;
        if (value > ((unsigned long)-1 - (unsigned long)digit) / (unsigned long)base)
            overflow = 1;
        else
            value = value * (unsigned long)base + (unsigned long)digit;
    }
    if (end) *end = (char*)(any ? cursor : text);
    if (overflow) return (unsigned long)-1;
    return negative ? 0UL - value : value;
}

long strtol(const char* text, char** end, int base) {
    const char* cursor = text;
    while (isspace((unsigned char)*cursor)) cursor++;
    int negative = 0;
    if (*cursor == '+') cursor++;
    else if (*cursor == '-') { negative = 1; cursor++; }
    char* stop = (char*)cursor;
    unsigned long magnitude = strtoul(cursor, &stop, base);
    if (end) *end = (stop == cursor) ? (char*)text : stop;
    const unsigned long limit = (unsigned long)-1 / 2;
    if (negative) {
        if (magnitude > limit + 1UL) return -(long)limit - 1;
        return (long)(0UL - magnitude);
    }
    if (magnitude > limit) return (long)limit;
    return (long)magnitude;
}

static void swap_bytes(unsigned char* a, unsigned char* b, size_t size) {
    while (size--) {
        unsigned char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

static void sift_down(unsigned char* base, size_t size, size_t start,
                      size_t count,
                      int (*compare)(const void*, const void*)) {
    size_t root = start;
    for (;;) {
        size_t child = 2 * root + 1;
        if (child >= count) return;
        if (child + 1 < count &&
            compare(base + child * size, base + (child + 1) * size) < 0)
            child++;
        if (compare(base + root * size, base + child * size) >= 0) return;
        swap_bytes(base + root * size, base + child * size, size);
        root = child;
    }
}

void qsort(void* base, size_t count, size_t size,
           int (*compare)(const void*, const void*)) {
    if (!base || count < 2 || !size) return;
    unsigned char* bytes = (unsigned char*)base;
    for (size_t i = count / 2; i-- > 0;) sift_down(bytes, size, i, count, compare);
    for (size_t end = count - 1; end > 0; end--) {
        swap_bytes(bytes, bytes + end * size, size);
        sift_down(bytes, size, 0, end, compare);
    }
}

/* ---------------------------------------------------------------- printf */

struct sink {
    char* buf;
    size_t cap;
    size_t len;
    size_t total;
    int fd; /* >= 0: descarrega em fd; < 0: grava em buf */
};

int write_all(int fd, const void* data, size_t count) {
    const char* cursor = (const char*)data;
    while (count) {
        ssize_t written = write(fd, cursor, count);
        if (written <= 0) return -1;
        cursor += written;
        count -= (size_t)written;
    }
    return 0;
}

static void sink_flush(struct sink* sink) {
    if (sink->fd >= 0 && sink->len) {
        (void)write_all(sink->fd, sink->buf, sink->len);
        sink->len = 0;
    }
}

static void sink_putc(struct sink* sink, char c) {
    sink->total++;
    if (sink->fd >= 0) {
        if (sink->len == sink->cap) sink_flush(sink);
        sink->buf[sink->len++] = c;
    } else if (sink->cap && sink->len + 1 < sink->cap) {
        sink->buf[sink->len++] = c;
    }
}

static void sink_pad(struct sink* sink, char c, long count) {
    while (count-- > 0) sink_putc(sink, c);
}

/* Divide *value por base (2..16) sem usar divisao de 64 bits; devolve o
 * resto. Valores de 32 bits usam a instrucao nativa; os maiores caem numa
 * divisao bit a bit (so vale para %llu com numeros enormes). */
static unsigned divmod_small(unsigned long long* value, unsigned base) {
    unsigned long long v = *value;
    if ((v >> 32) == 0) {
        unsigned a = (unsigned)v;
        *value = a / base;
        return a % base;
    }
    unsigned long long quotient = 0;
    unsigned long long remainder = 0;
    for (int bit = 63; bit >= 0; bit--) {
        remainder = (remainder << 1) | ((v >> bit) & 1u);
        if (remainder >= base) {
            remainder -= base;
            quotient |= 1ULL << bit;
        }
    }
    *value = quotient;
    return (unsigned)remainder;
}

struct spec {
    int minus, zero, plus, space, alt;
    long width;
    long precision; /* -1 = ausente */
    int length;     /* 0 int, 1 long, 2 long long, -1 short, -2 char */
};

static void format_unsigned(struct sink* sink, const struct spec* spec,
                            unsigned long long value, unsigned base,
                            int upper, int negative) {
    char digits[72];
    size_t count = 0;
    const char* alphabet = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int is_zero = (value == 0);
    while (value) digits[count++] = alphabet[divmod_small(&value, base)];
    if (spec->precision == 0 && is_zero) count = 0;
    else if (count == 0) digits[count++] = '0';

    const char* prefix = "";
    size_t prefix_length = 0;
    char sign[2] = {0, 0};
    if (negative) sign[0] = '-';
    else if (spec->plus) sign[0] = '+';
    else if (spec->space) sign[0] = ' ';
    if (spec->alt && !is_zero) {
        if (base == 16) { prefix = upper ? "0X" : "0x"; prefix_length = 2; }
        else if (base == 8 && (spec->precision <= (long)count)) {
            prefix = "0"; prefix_length = 1;
        }
    }
    long zeros = 0;
    if (spec->precision > (long)count) zeros = spec->precision - (long)count;
    long body = (long)count + zeros + (long)prefix_length + (sign[0] ? 1 : 0);
    long pad = spec->width > body ? spec->width - body : 0;
    int zero_fill = spec->zero && !spec->minus && spec->precision < 0;

    if (!spec->minus && !zero_fill) sink_pad(sink, ' ', pad);
    if (sign[0]) sink_putc(sink, sign[0]);
    for (size_t i = 0; i < prefix_length; i++) sink_putc(sink, prefix[i]);
    if (zero_fill) sink_pad(sink, '0', pad);
    sink_pad(sink, '0', zeros);
    while (count) sink_putc(sink, digits[--count]);
    if (spec->minus) sink_pad(sink, ' ', pad);
}

static void format_string(struct sink* sink, const struct spec* spec,
                          const char* text) {
    if (!text) text = "(null)";
    size_t length = spec->precision >= 0
                        ? strnlen(text, (size_t)spec->precision)
                        : strlen(text);
    long pad = spec->width > (long)length ? spec->width - (long)length : 0;
    if (!spec->minus) sink_pad(sink, ' ', pad);
    for (size_t i = 0; i < length; i++) sink_putc(sink, text[i]);
    if (spec->minus) sink_pad(sink, ' ', pad);
}

static void format_core(struct sink* sink, const char* format, va_list args) {
    for (; *format; format++) {
        if (*format != '%') {
            sink_putc(sink, *format);
            continue;
        }
        format++;
        struct spec spec;
        memset(&spec, 0, sizeof(spec));
        spec.precision = -1;
        for (;; format++) {
            if (*format == '-') spec.minus = 1;
            else if (*format == '0') spec.zero = 1;
            else if (*format == '+') spec.plus = 1;
            else if (*format == ' ') spec.space = 1;
            else if (*format == '#') spec.alt = 1;
            else break;
        }
        if (*format == '*') {
            int w = va_arg(args, int);
            if (w < 0) { spec.minus = 1; w = -w; }
            spec.width = w;
            format++;
        } else {
            while (isdigit((unsigned char)*format))
                spec.width = spec.width * 10 + (*format++ - '0');
        }
        if (*format == '.') {
            format++;
            spec.precision = 0;
            if (*format == '*') {
                int p = va_arg(args, int);
                spec.precision = p < 0 ? -1 : p;
                format++;
            } else {
                while (isdigit((unsigned char)*format))
                    spec.precision = spec.precision * 10 + (*format++ - '0');
            }
        }
        for (;; format++) {
            if (*format == 'l') spec.length = spec.length >= 1 ? 2 : 1;
            else if (*format == 'h') spec.length = spec.length <= -1 ? -2 : -1;
            else if (*format == 'z' || *format == 't' || *format == 'j')
                spec.length = (sizeof(size_t) > sizeof(int)) ? 1 : 0;
            else break;
        }
        char conv = *format;
        if (!conv) break;
        switch (conv) {
        case 'd':
        case 'i': {
            long long v;
            if (spec.length == 2) v = va_arg(args, long long);
            else if (spec.length == 1) v = va_arg(args, long);
            else v = va_arg(args, int);
            if (spec.length == -1) v = (short)v;
            else if (spec.length == -2) v = (signed char)v;
            int negative = v < 0;
            unsigned long long m = negative ? 0ULL - (unsigned long long)v
                                            : (unsigned long long)v;
            format_unsigned(sink, &spec, m, 10, 0, negative);
            break;
        }
        case 'u':
        case 'x':
        case 'X':
        case 'o': {
            unsigned long long v;
            if (spec.length == 2) v = va_arg(args, unsigned long long);
            else if (spec.length == 1) v = va_arg(args, unsigned long);
            else v = va_arg(args, unsigned int);
            if (spec.length == -1) v = (unsigned short)v;
            else if (spec.length == -2) v = (unsigned char)v;
            unsigned base = conv == 'u' ? 10u : conv == 'o' ? 8u : 16u;
            format_unsigned(sink, &spec, v, base, conv == 'X', 0);
            break;
        }
        case 'p': {
            struct spec ps = spec;
            ps.alt = 1;
            ps.precision = -1;
            unsigned long long v = (unsigned long long)(size_t)va_arg(args, void*);
            if (v == 0) format_string(sink, &spec, "(nil)");
            else format_unsigned(sink, &ps, v, 16, 0, 0);
            break;
        }
        case 'c': {
            char c = (char)va_arg(args, int);
            long pad = spec.width > 1 ? spec.width - 1 : 0;
            if (!spec.minus) sink_pad(sink, ' ', pad);
            sink_putc(sink, c);
            if (spec.minus) sink_pad(sink, ' ', pad);
            break;
        }
        case 's':
            format_string(sink, &spec, va_arg(args, const char*));
            break;
        case '%':
            sink_putc(sink, '%');
            break;
        default:
            sink_putc(sink, '%');
            sink_putc(sink, conv);
            break;
        }
    }
}

int vsnprintf(char* buffer, size_t size, const char* format, va_list args) {
    struct sink sink = {buffer, size, 0, 0, -1};
    format_core(&sink, format, args);
    if (size) buffer[sink.len] = '\0';
    return (int)sink.total;
}

int snprintf(char* buffer, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

int vdprintf(int fd, const char* format, va_list args) {
    char chunk[128];
    struct sink sink = {chunk, sizeof(chunk), 0, 0, fd};
    format_core(&sink, format, args);
    sink_flush(&sink);
    return (int)sink.total;
}

int dprintf(int fd, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vdprintf(fd, format, args);
    va_end(args);
    return result;
}

int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vdprintf(1, format, args);
    va_end(args);
    return result;
}

int eprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vdprintf(2, format, args);
    va_end(args);
    return result;
}

/* --------------------------------------------------------------- getopt */

int optind = 1;
char* optarg;
int optopt;
static int g_optpos;

int getopt(int argc, char* const argv[], const char* optstring) {
    optarg = (char*)0;
    if (optind >= argc) return -1;
    const char* arg = argv[optind];
    if (g_optpos == 0) {
        if (arg[0] != '-' || arg[1] == '\0') return -1;
        if (arg[1] == '-' && arg[2] == '\0') {
            optind++;
            return -1;
        }
        g_optpos = 1;
    }
    char c = arg[g_optpos++];
    optopt = (unsigned char)c;
    const char* spec = (c == ':') ? (const char*)0 : strchr(optstring, c);
    if (!spec) {
        if (!arg[g_optpos]) { optind++; g_optpos = 0; }
        return '?';
    }
    if (spec[1] == ':') {
        if (arg[g_optpos]) {
            optarg = (char*)arg + g_optpos;
            optind++;
        } else if (optind + 1 < argc) {
            optarg = argv[optind + 1];
            optind += 2;
        } else {
            optind++;
            g_optpos = 0;
            return '?';
        }
        g_optpos = 0;
        return (unsigned char)c;
    }
    if (!arg[g_optpos]) { optind++; g_optpos = 0; }
    return (unsigned char)c;
}

/* -------------------------------------------------------------- leitura */

void rb_init(struct rbuf* reader, int fd) {
    reader->fd = fd;
    reader->pos = 0;
    reader->len = 0;
}

int rb_getc(struct rbuf* reader) {
    if (reader->pos == reader->len) {
        ssize_t count = read(reader->fd, reader->buf, RB_SIZE);
        if (count <= 0) return -1;
        reader->len = (size_t)count;
        reader->pos = 0;
    }
    return (unsigned char)reader->buf[reader->pos++];
}

long rb_getline(struct rbuf* reader, char* out, size_t cap) {
    size_t length = 0;
    int got = 0;
    int c;
    while ((c = rb_getc(reader)) >= 0) {
        got = 1;
        if (c == '\n') break;
        if (length + 1 < cap) out[length++] = (char)c;
    }
    if (!got) return -1;
    out[length] = '\0';
    return (long)length;
}

/* ------------------------------------------------------ escrita bufferizada */

void wb_init(struct wbuf* writer, int fd) {
    writer->fd = fd;
    writer->len = 0;
}

int wb_flush(struct wbuf* writer) {
    int result = 0;
    if (writer->len) result = write_all(writer->fd, writer->buf, writer->len);
    writer->len = 0;
    return result;
}

void wb_putc(struct wbuf* writer, char c) {
    if (writer->len == WB_SIZE) (void)wb_flush(writer);
    writer->buf[writer->len++] = c;
}

void wb_write(struct wbuf* writer, const char* data, size_t count) {
    while (count--) wb_putc(writer, *data++);
}

/* ---------------------------------------------------------------- regex */

static int g_re_icase;

static int re_char_eq(char a, char b) {
    if (a == b) return 1;
    return g_re_icase && tolower((unsigned char)a) == tolower((unsigned char)b);
}

/* Devolve o fim do atomo que comeca em re: \c, [conjunto] ou um caractere. */
static const char* re_atom_end(const char* re) {
    if (re[0] == '\\' && re[1]) return re + 2;
    if (re[0] == '[') {
        const char* p = re + 1;
        if (*p == '^') p++;
        if (*p == ']') p++;
        while (*p && *p != ']') p++;
        if (*p == ']') return p + 1;
        return re + 1; /* '[' sem fechar: literal */
    }
    return re + 1;
}

static int re_in_range(char low, char high, char c) {
    if (c >= low && c <= high) return 1;
    if (!g_re_icase) return 0;
    char lower = (char)tolower((unsigned char)c);
    char upper = (char)toupper((unsigned char)c);
    return (lower >= low && lower <= high) || (upper >= low && upper <= high);
}

static int re_atom_match(const char* re, const char* end, char c) {
    if (re[0] == '.') return 1;
    if (re[0] == '\\' && end == re + 2) return re_char_eq(re[1], c);
    if (re[0] == '[' && end > re + 1) {
        const char* close = end - 1;
        const char* p = re + 1;
        int negate = 0;
        int found = 0;
        if (*p == '^') { negate = 1; p++; }
        for (; p < close; p++) {
            if (p + 2 < close && p[1] == '-') {
                if (re_in_range(p[0], p[2], c)) found = 1;
                p += 2;
            } else if (re_char_eq(*p, c)) {
                found = 1;
            }
        }
        return found != negate;
    }
    return re_char_eq(re[0], c);
}

static int re_here(const char* re, const char* text) {
    for (;;) {
        if (*re == '\0') return 1;
        if (re[0] == '$' && re[1] == '\0') return *text == '\0';
        const char* end = re_atom_end(re);
        char quant = *end;
        if (quant == '*' || quant == '+' || quant == '?') {
            const char* rest = end + 1;
            size_t min = (quant == '+') ? 1 : 0;
            size_t max = (quant == '?') ? 1 : (size_t)-1;
            size_t n = 0;
            while (n < max && text[n] && re_atom_match(re, end, text[n])) n++;
            for (size_t k = n + 1; k-- > min;)
                if (re_here(rest, text + k)) return 1;
            return 0;
        }
        if (*text && re_atom_match(re, end, *text)) {
            re = end;
            text++;
            continue;
        }
        return 0;
    }
}

int re_match(const char* pattern, const char* text, int icase) {
    g_re_icase = icase;
    if (pattern[0] == '^') return re_here(pattern + 1, text);
    do {
        if (re_here(pattern, text)) return 1;
    } while (*text++);
    return 0;
}
