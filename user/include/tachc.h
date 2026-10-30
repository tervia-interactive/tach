#ifndef TACH_USER_C_H
#define TACH_USER_C_H

/*
 * tachc.h - extensoes da libc de usuario (ctype, printf, getopt, strtol,
 * qsort, leitura por linhas e um casador de expressoes regulares pequeno).
 * Implementacao em libc/ext.c. Inclua <tach.h> antes.
 *
 * Observacoes de ABI/projeto:
 *  - Nao ha libgcc no link: evite divisao/modulo de 64 bits nos programas
 *    (em i686 vira __udivdi3). vsnprintf ja trata %llu/%lld sem libgcc.
 *  - Buffers grandes devem ser estaticos; a pilha de usuario e pequena.
 */

#include <stdarg.h>

/* ---------------------------------------------------------------- ctype */
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int isspace(int c);
int isupper(int c);
int islower(int c);
int isprint(int c);
int isxdigit(int c);
int toupper(int c);
int tolower(int c);

/* --------------------------------------------------------------- strings */
size_t strspn(const char* text, const char* accept);
size_t strcspn(const char* text, const char* reject);
char* strpbrk(const char* text, const char* accept);
int strcasecmp(const char* left, const char* right);
int strncasecmp(const char* left, const char* right, size_t count);
size_t strlcpy(char* destination, const char* source, size_t size);
/* Parte final de um caminho ("a/b/c" -> "c"); nao remove barras finais. */
const char* path_basename(const char* path);

/* ---------------------------------------------------------------- stdlib */
long strtol(const char* text, char** end, int base);
unsigned long strtoul(const char* text, char** end, int base);
int abs(int value);
/* Heapsort iterativo (nao recursivo, nao usa malloc). */
void qsort(void* base, size_t count, size_t size,
           int (*compare)(const void*, const void*));

/* ---------------------------------------------------------------- printf */
/* Formatos: %d %i %u %x %X %o %c %s %p %% com flags - 0 + espaco #,
 * largura e precisao (inclusive *), e modificadores hh h l ll z t j. */
int vsnprintf(char* buffer, size_t size, const char* format, va_list args);
int snprintf(char* buffer, size_t size, const char* format, ...)
    __attribute__((format(__printf__, 3, 4)));
int vdprintf(int fd, const char* format, va_list args);
int dprintf(int fd, const char* format, ...)
    __attribute__((format(__printf__, 2, 3)));
int printf(const char* format, ...) __attribute__((format(__printf__, 1, 2)));
int eprintf(const char* format, ...) __attribute__((format(__printf__, 1, 2)));
/* Escreve tudo (repete em escritas parciais). 0 em sucesso, -1 em erro. */
int write_all(int fd, const void* data, size_t count);

/* --------------------------------------------------------------- getopt */
extern int optind;
extern char* optarg;
extern int optopt;
/* Subconjunto POSIX: opcoes agrupadas (-abc), argumento colado (-n5) ou
 * separado (-n 5), "--" encerra. Devolve '?' em opcao invalida/sem argumento.
 * Para reiniciar o parse, faca optind = 1. */
int getopt(int argc, char* const argv[], const char* optstring);

/* -------------------------------------------------------------- leitura */
#define RB_SIZE 1024
struct rbuf {
    int fd;
    size_t pos;
    size_t len;
    char buf[RB_SIZE];
};
void rb_init(struct rbuf* reader, int fd);
/* Proximo byte (0..255) ou -1 em EOF/erro. */
int rb_getc(struct rbuf* reader);
/* Le uma linha sem o '\n' para out (trunca em cap-1; o resto e descartado).
 * Devolve o tamanho gravado ou -1 em EOF. cap deve ser >= 1. */
long rb_getline(struct rbuf* reader, char* out, size_t cap);

/* ------------------------------------------------------ escrita bufferizada */
#define WB_SIZE 1024
struct wbuf {
    int fd;
    size_t len;
    char buf[WB_SIZE];
};
void wb_init(struct wbuf* writer, int fd);
void wb_putc(struct wbuf* writer, char c);
void wb_write(struct wbuf* writer, const char* data, size_t count);
/* Descarrega o que estiver pendente. 0 em sucesso, -1 em erro. */
int wb_flush(struct wbuf* writer);

/* ---------------------------------------------------------------- regex */
/* Casador no estilo Kernighan/Pike estendido: ^ $ . * + ? [abc] [a-z] [^x]
 * e \c (literal). Procura em qualquer posicao de text. Devolve 1/0.
 * icase != 0 ignora diferenca de caixa. */
int re_match(const char* pattern, const char* text, int icase);
/* strstr opcionalmente sem diferenciar caixa. */
const char* str_find(const char* haystack, const char* needle, int icase);

#endif
