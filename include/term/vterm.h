#ifndef TERM_VTERM_H
#define TERM_VTERM_H

#include <kernel/types.h>

#define VTERM_WIDTH 80
#define VTERM_HEIGHT 25
#define VTERM_SCROLLBACK_SIZE 1000

typedef struct {
    int cursor_x;
    int cursor_y;
    uint8_t fg_color;
    uint8_t bg_color;
    char *scrollback_buffer;
    size_t scrollback_pos;
    size_t scrollback_size;
} vterm_t;

int vterm_init(vterm_t *vterm);
void vterm_putchar(vterm_t *vterm, char c);
void vterm_print(vterm_t *vterm, const char *str);
void vterm_clear(vterm_t *vterm);
void vterm_set_color(vterm_t *vterm, uint8_t fg, uint8_t bg);
void vterm_scroll_up(vterm_t *vterm);
void vterm_move_cursor(vterm_t *vterm, int x, int y);

#endif /* TERM_VTERM_H */
