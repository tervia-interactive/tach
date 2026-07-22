#ifndef DRIVERS_VIDEO_VGA_TEXT_H
#define DRIVERS_VIDEO_VGA_TEXT_H

#include <kernel/types.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

typedef enum {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15
} vga_color_t;

int vga_init(void);
void vga_clear(void);
void vga_putchar(char c);
void vga_print(const char *str);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_set_cursor(int x, int y);

#endif /* DRIVERS_VIDEO_VGA_TEXT_H */
