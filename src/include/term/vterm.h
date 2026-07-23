/* tach - Virtual Terminal Header */
#ifndef _TERM_VTERM_H
#define _TERM_VTERM_H

#include <kernel/types.h>
#include <stdint.h>

/* Virtual terminal dimensions */
#define VTERM_WIDTH 80
#define VTERM_HEIGHT 25
#define VTERM_SCROLLBACK 1000

/* Virtual terminal structure */
typedef struct {
    uint16_t* buffer;
    int cursor_x;
    int cursor_y;
    int scroll_start;
    int scroll_end;
    uint8_t fg_color;
    uint8_t bg_color;
} vterm_t;

/* Initialize virtual terminal */
void vterm_init(vterm_t* vt);

/* Put character on virtual terminal */
void vterm_putchar(vterm_t* vt, char c);

/* Clear virtual terminal */
void vterm_clear(vterm_t* vt);

/* Scroll virtual terminal */
void vterm_scroll(vterm_t* vt, int lines);

#endif /* _TERM_VTERM_H */
