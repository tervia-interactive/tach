/* Renomeia os simbolos que user/libc/ext.c define (prefixo t_) para que o
 * teste de host convivam com a libc do sistema sem colisao. Use depois de
 * incluir os cabecalhos do sistema. */
#ifndef USERLIBC_RENAME_H
#define USERLIBC_RENAME_H

#define isdigit t_isdigit
#define isalpha t_isalpha
#define isalnum t_isalnum
#define isspace t_isspace
#define isupper t_isupper
#define islower t_islower
#define isprint t_isprint
#define isxdigit t_isxdigit
#define toupper t_toupper
#define tolower t_tolower
#define strspn t_strspn
#define strcspn t_strcspn
#define strpbrk t_strpbrk
#define strcasecmp t_strcasecmp
#define strncasecmp t_strncasecmp
#define strlcpy t_strlcpy
#define path_basename t_path_basename
#define str_find t_str_find
#define strtol t_strtol
#define strtoul t_strtoul
#define abs t_abs
#define qsort t_qsort
#define write_all t_write_all
#define vsnprintf t_vsnprintf
#define snprintf t_snprintf
#define vdprintf t_vdprintf
#define dprintf t_dprintf
#define printf t_printf
#define eprintf t_eprintf
#define optind t_optind
#define optarg t_optarg
#define optopt t_optopt
#define getopt t_getopt
#define rb_init t_rb_init
#define rb_getc t_rb_getc
#define rb_getline t_rb_getline
#define wb_init t_wb_init
#define wb_putc t_wb_putc
#define wb_write t_wb_write
#define wb_flush t_wb_flush
#define re_match t_re_match

#endif
