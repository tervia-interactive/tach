#include <tach.h>
#include <tachc.h>

/* tr [-d] [-s] CONJ1 [CONJ2]
 * Conjuntos aceitam intervalos (a-z), escapes (\n \t \\ \-) e as classes
 * [:upper:] [:lower:] [:digit:] [:alpha:] [:alnum:] [:space:].
 *   tr a-z A-Z      traduz       tr -d '\n'   apaga       tr -s ' '   comprime */

#define SET_MAX 512

static unsigned char g_set1[SET_MAX];
static unsigned char g_set2[SET_MAX];
static unsigned char g_map[256];
static unsigned char g_in_set1[256];
static unsigned char g_squeeze[256];
static struct rbuf g_reader;
static struct wbuf g_writer;

static int add(unsigned char* set, size_t* length, unsigned char value) {
    if (*length >= SET_MAX) return -1;
    set[(*length)++] = value;
    return 0;
}

static int add_class(unsigned char* set, size_t* length, const char* name,
                     size_t name_length) {
    for (int c = 0; c < 256; c++) {
        int member;
        if (name_length == 5 && strncmp(name, "upper", 5) == 0) member = isupper(c);
        else if (name_length == 5 && strncmp(name, "lower", 5) == 0) member = islower(c);
        else if (name_length == 5 && strncmp(name, "digit", 5) == 0) member = isdigit(c);
        else if (name_length == 5 && strncmp(name, "alpha", 5) == 0) member = isalpha(c);
        else if (name_length == 5 && strncmp(name, "alnum", 5) == 0) member = isalnum(c);
        else if (name_length == 5 && strncmp(name, "space", 5) == 0) member = isspace(c);
        else return -1;
        if (member && add(set, length, (unsigned char)c) < 0) return -1;
    }
    return 0;
}

/* Le um caractere (com escape) de *text e avanca. */
static unsigned char next_char(const char** text) {
    const char* p = *text;
    unsigned char value;
    if (*p == '\\' && p[1]) {
        p++;
        value = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p == 'r' ? '\r'
                : (unsigned char)*p;
    } else {
        value = (unsigned char)*p;
    }
    *text = p + 1;
    return value;
}

static int expand(const char* text, unsigned char* set, size_t* length) {
    *length = 0;
    while (*text) {
        if (text[0] == '[' && text[1] == ':') {
            const char* close = strstr(text + 2, ":]");
            if (close) {
                if (add_class(set, length, text + 2, (size_t)(close - (text + 2))) < 0)
                    return -1;
                text = close + 2;
                continue;
            }
        }
        unsigned char low = next_char(&text);
        if (*text == '-' && text[1]) {
            text++;
            unsigned char high = next_char(&text);
            if (high < low) return -1;
            for (unsigned int c = low; c <= high; c++)
                if (add(set, length, (unsigned char)c) < 0) return -1;
        } else if (add(set, length, low) < 0) {
            return -1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    int delete_mode = 0, squeeze_mode = 0;
    int c;
    while ((c = getopt(argc, argv, "ds")) != -1) {
        if (c == 'd') delete_mode = 1;
        else if (c == 's') squeeze_mode = 1;
        else {
            eprintf("usage: tr [-d] [-s] set1 [set2]\n");
            return 2;
        }
    }
    int sets = argc - optind;
    if (sets < 1 || sets > 2 || (!delete_mode && !squeeze_mode && sets != 2) ||
        (delete_mode && !squeeze_mode && sets != 1)) {
        eprintf("usage: tr [-d] [-s] set1 [set2]\n");
        return 2;
    }
    size_t length1 = 0, length2 = 0;
    if (expand(argv[optind], g_set1, &length1) < 0 ||
        (sets == 2 && expand(argv[optind + 1], g_set2, &length2) < 0)) {
        eprintf("tr: invalid set\n");
        return 2;
    }
    for (int i = 0; i < 256; i++) g_map[i] = (unsigned char)i;
    for (size_t i = 0; i < length1; i++) g_in_set1[g_set1[i]] = 1;
    if (sets == 2 && !delete_mode) {
        if (!length2) {
            eprintf("tr: set2 is empty\n");
            return 2;
        }
        for (size_t i = 0; i < length1; i++)
            g_map[g_set1[i]] = g_set2[i < length2 ? i : length2 - 1];
    }
    if (squeeze_mode) {
        const unsigned char* source = (sets == 2) ? g_set2 : g_set1;
        size_t source_length = (sets == 2) ? length2 : length1;
        for (size_t i = 0; i < source_length; i++) g_squeeze[source[i]] = 1;
    }

    rb_init(&g_reader, 0);
    wb_init(&g_writer, 1);
    int last = -1;
    int ch;
    while ((ch = rb_getc(&g_reader)) >= 0) {
        if (delete_mode && g_in_set1[ch]) continue;
        unsigned char out = (sets == 2 && !delete_mode) ? g_map[ch] : (unsigned char)ch;
        if (squeeze_mode && g_squeeze[out] && last == out) continue;
        wb_putc(&g_writer, (char)out);
        last = out;
    }
    return wb_flush(&g_writer) < 0 ? 1 : 0;
}
