#include <drivers/input/keyboard.h>
#include <hal/io.h>

#define PS2_STATUS_PORT 0x64
#define PS2_DATA_PORT   0x60

static bool g_shift;
static bool g_extended;
static char g_pending;

static const char g_keymap[128] = {
    0, 27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0
};

static const char g_shift_keymap[128] = {
    0, 27, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0
};

static char keyboard_translate(void) {
    while (hal_port_in(PS2_STATUS_PORT) & 0x01) {
        uint8_t code = hal_port_in(PS2_DATA_PORT);

        if (code == 0xE0) {
            g_extended = true;
            continue;
        }

        bool released = (code & 0x80u) != 0;
        uint8_t key = code & 0x7Fu;
        if (key == KB_SCANCODE_LSHIFT || key == KB_SCANCODE_RSHIFT) {
            g_shift = !released;
            g_extended = false;
            continue;
        }
        if (released) {
            g_extended = false;
            continue;
        }

        if (g_extended) {
            g_extended = false;
            if (key == 0x48) return 0x10; /* up */
            if (key == 0x50) return 0x0E; /* down */
            if (key == 0x4B) return 0x02; /* left */
            if (key == 0x4D) return 0x06; /* right */
            continue;
        }
        if (key < 128) {
            char c = g_shift ? g_shift_keymap[key] : g_keymap[key];
            if (c) {
                return c;
            }
        }
    }
    return 0;
}

int keyboard_ps2_init(void) {
    g_shift = false;
    g_extended = false;
    g_pending = 0;
    while (hal_port_in(PS2_STATUS_PORT) & 0x01) {
        (void)hal_port_in(PS2_DATA_PORT);
    }
    return 0;
}

int keyboard_init(void) {
    return keyboard_ps2_init();
}

int keyboard_available(void) {
    if (!g_pending) {
        g_pending = keyboard_translate();
    }
    return g_pending != 0;
}

char keyboard_readchar(void) {
    while (!keyboard_available()) {}
    char c = g_pending;
    g_pending = 0;
    return c;
}

bool keyboard_is_pressed(uint8_t scancode) {
    (void)scancode;
    return false;
}
