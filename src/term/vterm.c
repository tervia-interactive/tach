#include <kernel/string.h>
#include <term/vterm.h>

static uint16_t vterm_cell(vterm_t* vt, char c) {
    uint8_t color = (uint8_t)(vt->fg_color | (vt->bg_color << 4));
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

void vterm_init(vterm_t* vt) {
    if (!vt) {
        return;
    }
    memset(vt, 0, sizeof(*vt));
    vt->buffer = vt->cells;
    vt->fg_color = 7;
    vt->bg_color = 0;
    vt->scroll_end = VTERM_HEIGHT - 1;
    vterm_clear(vt);
}

void vterm_clear(vterm_t* vt) {
    if (!vt || !vt->buffer) {
        return;
    }
    for (size_t i = 0; i < VTERM_WIDTH * VTERM_HEIGHT; i++) {
        vt->buffer[i] = vterm_cell(vt, ' ');
    }
    vt->cursor_x = 0;
    vt->cursor_y = 0;
}

void vterm_scroll(vterm_t* vt, int lines) {
    if (!vt || !vt->buffer || lines <= 0) {
        return;
    }
    if (lines >= VTERM_HEIGHT) {
        vterm_clear(vt);
        return;
    }
    size_t retained = (size_t)(VTERM_HEIGHT - lines) * VTERM_WIDTH;
    memmove(vt->buffer, vt->buffer + lines * VTERM_WIDTH,
            retained * sizeof(uint16_t));
    for (size_t i = retained; i < VTERM_WIDTH * VTERM_HEIGHT; i++) {
        vt->buffer[i] = vterm_cell(vt, ' ');
    }
    vt->cursor_y -= lines;
    if (vt->cursor_y < 0) {
        vt->cursor_y = 0;
    }
}

void vterm_putchar(vterm_t* vt, char c) {
    if (!vt || !vt->buffer) {
        return;
    }
    if (c == '\n') {
        vt->cursor_x = 0;
        vt->cursor_y++;
    } else if (c == '\r') {
        vt->cursor_x = 0;
    } else if (c == '\b') {
        if (vt->cursor_x > 0) {
            vt->cursor_x--;
        }
    } else {
        vt->buffer[vt->cursor_y * VTERM_WIDTH + vt->cursor_x] =
            vterm_cell(vt, c);
        vt->cursor_x++;
    }
    if (vt->cursor_x >= VTERM_WIDTH) {
        vt->cursor_x = 0;
        vt->cursor_y++;
    }
    if (vt->cursor_y >= VTERM_HEIGHT) {
        vterm_scroll(vt, 1);
        vt->cursor_y = VTERM_HEIGHT - 1;
    }
}
