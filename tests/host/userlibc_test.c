/* Teste de host da libc de usuario (user/libc/ext.c): printf, strtol, qsort,
 * getopt, regex e os leitores/escritores com buffer. ext.c e compilado a
 * parte com -include userlibc_rename.h; aqui o rename vem depois dos
 * cabecalhos do sistema. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include "userlibc_rename.h"
#include <tachc.h>

static int g_checks, g_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (!(cond)) {                                                       \
            g_failures++;                                                    \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                    \
    } while (0)

#define FMT(expected, ...)                                                   \
    do {                                                                     \
        char buf_[128];                                                      \
        int n_ = snprintf(buf_, sizeof(buf_), __VA_ARGS__);                  \
        if (strcmp(buf_, expected) != 0)                                     \
            fprintf(stderr, "  got '%s', want '%s'\n", buf_, expected);      \
        CHECK(strcmp(buf_, expected) == 0);                                  \
        CHECK(n_ == (int)strlen(expected));                                  \
    } while (0)

static void test_printf(void) {
    FMT("42", "%d", 42);
    FMT("-42", "%d", -42);
    FMT("   42", "%5d", 42);
    FMT("42   |", "%-5d|", 42);
    FMT("00042", "%05d", 42);
    FMT("-0042", "%05d", -42);
    FMT("+5", "%+d", 5);
    FMT(" 5", "% d", 5);
    FMT("ff", "%x", 255);
    FMT("FF", "%X", 255);
    FMT("0x2a", "%#x", 42);
    FMT("10", "%o", 8);
    FMT("010", "%#o", 8);
    FMT("4294967295", "%u", 4294967295u);
    FMT("18446744073709551615", "%llu", 18446744073709551615ULL);
    FMT("-9223372036854775808", "%lld", (-9223372036854775807LL - 1));
    FMT("12345678901", "%llu", 12345678901ULL);
    FMT("12", "%zu", (size_t)12);
    FMT("-7", "%ld", -7L);
    FMT("44", "%hhd", 300);
    FMT("4464", "%hu", 70000);
    FMT("   42", "%*d", 5, 42);
    FMT("", "%.0d", 0);
    FMT("007", "%.3d", 7);
    FMT("a", "%c", 'a');
    FMT("%", "%%");
    FMT("hello", "%s", "hello");
    FMT("hel", "%.3s", "hello");
    FMT("ab    |", "%-6s|", "ab");
    FMT("    ab|", "%6.2s|", "abcdef");
    FMT("(null)", "%s", (char*)0);
    FMT("x=3 y=-4", "x=%d y=%d", 3, -4);

    char small[4];
    int n = snprintf(small, sizeof(small), "%s", "abcdef");
    CHECK(n == 6);
    CHECK(strcmp(small, "abc") == 0);
    CHECK(snprintf((char*)0, 0, "%d", 12345) == 5);
}

static void test_strtol(void) {
    char* end;
    CHECK(strtol("123", &end, 10) == 123 && *end == '\0');
    CHECK(strtol("-45", &end, 10) == -45);
    CHECK(strtol(" 42", &end, 10) == 42);
    CHECK(strtol("+7", &end, 10) == 7);
    CHECK(strtol("0x1f", &end, 0) == 31);
    CHECK(strtol("077", &end, 0) == 63);
    CHECK(strtol("ff", &end, 16) == 255);
    CHECK(strtol("12abc", &end, 10) == 12 && strcmp(end, "abc") == 0);
    const char* none = "zzz";
    CHECK(strtol(none, &end, 10) == 0 && end == none);
    CHECK(strtoul("4294967295", &end, 10) == 4294967295UL);
    CHECK(abs(-5) == 5 && abs(5) == 5);
}

static int int_compare(const void* a, const void* b) {
    int x = *(const int*)a, y = *(const int*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static void test_qsort(void) {
    int small[] = {5, 2, 9, 1, 5, 6, -3, 0};
    qsort(small, sizeof(small) / sizeof(small[0]), sizeof(int), int_compare);
    for (size_t i = 1; i < sizeof(small) / sizeof(small[0]); i++)
        CHECK(small[i - 1] <= small[i]);

    static int big[1000];
    unsigned seed = 12345;
    for (int i = 0; i < 1000; i++) {
        seed = seed * 1103515245u + 12345u;
        big[i] = (int)((seed >> 16) % 500);
    }
    qsort(big, 1000, sizeof(int), int_compare);
    int sorted = 1;
    for (int i = 1; i < 1000; i++)
        if (big[i - 1] > big[i]) sorted = 0;
    CHECK(sorted);

    qsort(big, 0, sizeof(int), int_compare); /* nao deve quebrar */
    qsort(big, 1, sizeof(int), int_compare);
}

static void test_getopt(void) {
    char* argv[] = {(char*)"prog", (char*)"-ab", (char*)"-n", (char*)"5",
                    (char*)"-x7", (char*)"file", (char*)"-z", (char*)0};
    int argc = 7;
    optind = 1;
    CHECK(getopt(argc, argv, "abn:x:") == 'a');
    CHECK(getopt(argc, argv, "abn:x:") == 'b');
    CHECK(getopt(argc, argv, "abn:x:") == 'n' && strcmp(optarg, "5") == 0);
    CHECK(getopt(argc, argv, "abn:x:") == 'x' && strcmp(optarg, "7") == 0);
    CHECK(getopt(argc, argv, "abn:x:") == -1);
    CHECK(optind == 5 && strcmp(argv[optind], "file") == 0);

    char* bad[] = {(char*)"prog", (char*)"-q", (char*)"--", (char*)"-a", (char*)0};
    optind = 1;
    CHECK(getopt(3, bad, "a") == '?' && optopt == 'q');
    CHECK(getopt(3, bad, "a") == -1);
    CHECK(optind == 3);
}

static void test_regex(void) {
    CHECK(re_match("^ab*c$", "abbbc", 0));
    CHECK(re_match("^ab*c$", "ac", 0));
    CHECK(!re_match("^ab*c$", "abbbd", 0));
    CHECK(re_match("a.c", "xxabcx", 0));
    CHECK(!re_match("^abc", "xabc", 0));
    CHECK(re_match("c$", "abc", 0));
    CHECK(!re_match("c$", "abcd", 0));
    CHECK(re_match("colou?r", "color", 0));
    CHECK(re_match("colou?r", "colour", 0));
    CHECK(re_match("[0-9]+", "ab12", 0));
    CHECK(!re_match("^[^0-9]+$", "ab12", 0));
    CHECK(re_match("^[^0-9]+$", "abcd", 0));
    CHECK(re_match("HELLO", "say hello", 1));
    CHECK(!re_match("hello", "say HELLO", 0));
    CHECK(re_match("a\\.b", "a.b", 0));
    CHECK(!re_match("a\\.b", "axb", 0));
    CHECK(re_match("x*", "", 0));
    CHECK(re_match("^$", "", 0));
    CHECK(!re_match("^$", "a", 0));
    CHECK(!re_match("a+b", "b", 0));
    CHECK(re_match("a+b", "caab", 0));
    CHECK(re_match("[a-c]x", "bx", 0));
    CHECK(re_match("[a-c]", "B", 1));
    CHECK(!re_match("[a-c]", "B", 0));
    CHECK(re_match(".*foo.*bar", "xx foo yy bar", 0));
    CHECK(str_find("Hello World", "WORLD", 1) != (const char*)0);
    CHECK(str_find("Hello World", "WORLD", 0) == (const char*)0);
}

static void test_strings(void) {
    CHECK(strcasecmp("AbC", "aBc") == 0);
    CHECK(strcasecmp("a", "b") < 0);
    CHECK(strncasecmp("HELLO", "help", 3) == 0);
    CHECK(strspn("aabbcc", "ab") == 4);
    CHECK(strcspn("abc,def", ",") == 3);
    CHECK(strpbrk("hello", "lo") != (char*)0);
    char buf[4];
    CHECK(strlcpy(buf, "abcdef", sizeof(buf)) == 6);
    CHECK(strcmp(buf, "abc") == 0);
    CHECK(strcmp(path_basename("/a/b/c.txt"), "c.txt") == 0);
    CHECK(strcmp(path_basename("plain"), "plain") == 0);
    CHECK(toupper('a') == 'A' && tolower('Z') == 'z');
    CHECK(isdigit('7') && !isdigit('x') && isxdigit('F') && isspace('\t'));
}

static void test_buffers(void) {
    int fds[2];
    CHECK(pipe(fds) == 0);
    const char text[] = "one\ntwo\n\nlast";
    CHECK(write(fds[1], text, sizeof(text) - 1) == (ssize_t)(sizeof(text) - 1));
    close(fds[1]);
    struct rbuf reader;
    char line[16];
    rb_init(&reader, fds[0]);
    CHECK(rb_getline(&reader, line, sizeof(line)) == 3 && strcmp(line, "one") == 0);
    CHECK(rb_getline(&reader, line, sizeof(line)) == 3 && strcmp(line, "two") == 0);
    CHECK(rb_getline(&reader, line, sizeof(line)) == 0 && line[0] == '\0');
    CHECK(rb_getline(&reader, line, sizeof(line)) == 4 && strcmp(line, "last") == 0);
    CHECK(rb_getline(&reader, line, sizeof(line)) == -1);
    close(fds[0]);

    /* linha maior que o buffer: trunca e descarta o resto */
    CHECK(pipe(fds) == 0);
    CHECK(write(fds[1], "abcdefghij\nxy\n", 14) == 14);
    close(fds[1]);
    rb_init(&reader, fds[0]);
    char tiny[4];
    CHECK(rb_getline(&reader, tiny, sizeof(tiny)) == 3 && strcmp(tiny, "abc") == 0);
    CHECK(rb_getline(&reader, tiny, sizeof(tiny)) == 2 && strcmp(tiny, "xy") == 0);
    close(fds[0]);

    /* escrita bufferizada maior que WB_SIZE + dprintf */
    CHECK(pipe(fds) == 0);
    struct wbuf writer;
    wb_init(&writer, fds[1]);
    for (int i = 0; i < 3000; i++) wb_putc(&writer, (char)('a' + i % 26));
    CHECK(wb_flush(&writer) == 0);
    CHECK(dprintf(fds[1], "%s-%d", "ab", 7) == 4);
    close(fds[1]);
    static char got[4096];
    size_t total = 0;
    for (;;) {
        ssize_t n = read(fds[0], got + total, sizeof(got) - total);
        if (n <= 0) break;
        total += (size_t)n;
    }
    close(fds[0]);
    CHECK(total == 3004);
    CHECK(got[0] == 'a' && got[2999] == (char)('a' + 2999 % 26));
    CHECK(memcmp(got + 3000, "ab-7", 4) == 0);
}

int main(void) {
    test_printf();
    test_strtol();
    test_qsort();
    test_getopt();
    test_regex();
    test_strings();
    test_buffers();
    if (g_failures) {
        fprintf(stderr, "userlibc: %d/%d checks failed\n", g_failures, g_checks);
        return 1;
    }
    printf("userlibc: %d checks passed\n", g_checks);
    return 0;
}
