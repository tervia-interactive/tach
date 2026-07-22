/* tach Operating System - VGA Text Mode Header */
#ifndef _DRIVERS_VGA_TEXT_H
#define _DRIVERS_VGA_TEXT_H

#include <kernel/types.h>
#include <stdint.h>

/* VGA text mode dimensions */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* VGA colors */
typedef enum {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15
} vga_color_t;

/* Initialize VGA text mode */
void vga_init(void);

/* Clear screen */
void vga_clear(void);

/* Set cursor position */
void vga_set_cursor(int x, int y);

/* Print character at position */
void vga_putchar(char c, int x, int y, vga_color_t fg, vga_color_t bg);

/* Print string at position */
void vga_print(const char* str, int x, int y, vga_color_t fg, vga_color_t bg);

#endif /* _DRIVERS_VGA_TEXT_H */
