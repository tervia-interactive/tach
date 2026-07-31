/* tach - VGA Text Mode Driver */
/*
 * Drives the standard 80x25 VGA text-mode framebuffer at physical
 * 0xB8000. Only valid on i686/x86_64 in the (still identity-mapped)
 * low-memory region the bootstrap page tables cover. Not used on other
 * architectures.
 */
#include <kernel/types.h>
#include <drivers/video/vga_text.h>
#include <hal/io.h>

#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA  0x3D5
#define VGA_CURSOR_HIGH 0x0E
#define VGA_CURSOR_LOW  0x0F

static volatile uint16_t* const vga_buffer = (volatile uint16_t*)0xB8000;

static int cursor_x = 0;
static int cursor_y = 0;
static vga_color_t color_fg = VGA_LIGHT_GREY;
static vga_color_t color_bg = VGA_BLACK;

static inline uint16_t vga_entry(char c, vga_color_t fg, vga_color_t bg) {
    return (uint16_t)(unsigned char)c | (uint16_t)((fg | (bg << 4)) << 8);
}

static void vga_move_hw_cursor(void) {
    uint16_t pos = (uint16_t)(cursor_y * VGA_WIDTH + cursor_x);
    hal_port_out(VGA_CRTC_INDEX, VGA_CURSOR_LOW);
    hal_port_out(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
    hal_port_out(VGA_CRTC_INDEX, VGA_CURSOR_HIGH);
    hal_port_out(VGA_CRTC_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', color_fg, color_bg);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    vga_move_hw_cursor();
}

void vga_init(void) {
    color_fg = VGA_LIGHT_GREY;
    color_bg = VGA_BLACK;
    vga_clear();
}

void vga_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    vga_move_hw_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    color_fg = fg;
    color_bg = bg;
}

void vga_putchar(char c, int x, int y, vga_color_t fg, vga_color_t bg) {
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) {
        return;
    }
    vga_buffer[y * VGA_WIDTH + x] = vga_entry(c, fg, bg);
}

void vga_print(const char* str, int x, int y, vga_color_t fg, vga_color_t bg) {
    while (*str) {
        if (*str == '\n') {
            x = 0;
            y++;
        } else {
            vga_putchar(*str, x, y, fg, bg);
            x++;
            if (x >= VGA_WIDTH) {
                x = 0;
                y++;
            }
        }
        str++;
    }
}

static void vga_scroll_up_one_line(void) {
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color_fg, color_bg);
    }
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, color_fg, color_bg);
        cursor_x++;
    }

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll_up_one_line();
        cursor_y = VGA_HEIGHT - 1;
    }

    vga_move_hw_cursor();
}
