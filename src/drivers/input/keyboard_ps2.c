#include <drivers/input/keyboard.h>
#include <hal/io.h>

#define PS2_STATUS_PORT 0x64
#define PS2_DATA_PORT   0x60

#define PS2_STATUS_OUT_FULL 0x01
#define PS2_STATUS_IN_FULL  0x02
#define PS2_STATUS_AUX      0x20

#define KB_ACK    0xFA
#define KB_RESEND 0xFE
#define KB_CMD_LEDS 0xED

/* Codes returned to the TTY layer for non-printable keys. They follow the
 * usual Ctrl+letter values so the serial console and the keyboard share the
 * same line-editing bindings. */
#define KEY_CODE_HOME   0x01 /* Ctrl+A */
#define KEY_CODE_LEFT   0x02 /* Ctrl+B */
#define KEY_CODE_DELETE 0x04 /* Ctrl+D */
#define KEY_CODE_END    0x05 /* Ctrl+E */
#define KEY_CODE_RIGHT  0x06 /* Ctrl+F */
#define KEY_CODE_DOWN   0x0E /* Ctrl+N */
#define KEY_CODE_UP     0x10 /* Ctrl+P */

static bool g_lshift;
static bool g_rshift;
static bool g_ctrl;
static bool g_alt;
static bool g_caps;
static bool g_numlock = true;
static bool g_extended;
static unsigned g_skip;      /* bytes still to swallow (Pause = E1 ...) */
static char g_pending;
static uint8_t g_down[16];   /* pressed state, one bit per base scancode */

static const char g_keymap[128] = {
    0, 27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0,
    [0x56] = '\\', /* ISO extra key (ABNT2/ISO layouts) */
    [0x73] = '/',  /* ABNT2 "/ ?" key */
};

static const char g_shift_keymap[128] = {
    0, 27, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0,
    [0x56] = '|',
    [0x73] = '?',
};

/* Keypad block 0x47..0x53 with Num Lock on. */
static const char g_keypad[13] = {
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

static bool kb_wait_write(void) {
    for (int i = 0; i < 100000; i++) {
        if (!(hal_port_in(PS2_STATUS_PORT) & PS2_STATUS_IN_FULL)) {
            return true;
        }
    }
    return false;
}

static bool kb_wait_ack(void) {
    for (int i = 0; i < 100000; i++) {
        if (hal_port_in(PS2_STATUS_PORT) & PS2_STATUS_OUT_FULL) {
            uint8_t reply = hal_port_in(PS2_DATA_PORT);
            if (reply == KB_ACK) {
                return true;
            }
            if (reply == KB_RESEND) {
                return false;
            }
        }
    }
    return false;
}

static void kb_update_leds(void) {
    uint8_t mask = (uint8_t)((g_numlock ? 2 : 0) | (g_caps ? 4 : 0));
    if (!kb_wait_write()) return;
    hal_port_out(PS2_DATA_PORT, KB_CMD_LEDS);
    if (!kb_wait_ack()) return;
    if (!kb_wait_write()) return;
    hal_port_out(PS2_DATA_PORT, mask);
    (void)kb_wait_ack();
}

static void kb_mark(uint8_t key, bool down) {
    if (key >= 128) return;
    if (down) {
        g_down[key >> 3] |= (uint8_t)(1u << (key & 7));
    } else {
        g_down[key >> 3] &= (uint8_t)~(1u << (key & 7));
    }
}

static bool kb_is_letter(char c) {
    return c >= 'a' && c <= 'z';
}

static char keyboard_translate(void) {
    while (hal_port_in(PS2_STATUS_PORT) & PS2_STATUS_OUT_FULL) {
        bool from_aux = (hal_port_in(PS2_STATUS_PORT) & PS2_STATUS_AUX) != 0;
        uint8_t code = hal_port_in(PS2_DATA_PORT);

        if (from_aux) {
            continue; /* mouse byte, not ours */
        }
        if (g_skip) {
            g_skip--;
            continue;
        }
        if (code == 0xE1) {
            g_skip = 5; /* Pause/Break sends 6 bytes in total */
            continue;
        }
        if (code == 0xE0) {
            g_extended = true;
            continue;
        }
        if (code == KB_ACK || code == KB_RESEND || code == 0x00 || code == 0xFF) {
            continue; /* protocol bytes / buffer overrun marker */
        }

        bool released = (code & 0x80u) != 0;
        uint8_t key = code & 0x7Fu;
        bool extended = g_extended;
        g_extended = false;

        if (extended) {
            switch (key) {
            case 0x2A: case 0x36: /* fake shifts around PrtSc/Insert/arrows */
                continue;
            case 0x1D: g_ctrl = !released; continue; /* right ctrl */
            case 0x38: g_alt = !released; continue;  /* AltGr */
            default: break;
            }
            if (released) {
                continue;
            }
            switch (key) {
            case 0x48: return KEY_CODE_UP;
            case 0x50: return KEY_CODE_DOWN;
            case 0x4B: return KEY_CODE_LEFT;
            case 0x4D: return KEY_CODE_RIGHT;
            case 0x47: return KEY_CODE_HOME;
            case 0x4F: return KEY_CODE_END;
            case 0x53: return KEY_CODE_DELETE;
            case 0x1C: return '\n'; /* keypad Enter */
            case 0x35: return g_ctrl ? 0 : '/'; /* keypad / */
            default: continue; /* PgUp/PgDn/Insert/media keys: ignored */
            }
        }

        switch (key) {
        case KB_SCANCODE_LSHIFT: g_lshift = !released; kb_mark(key, !released); continue;
        case KB_SCANCODE_RSHIFT: g_rshift = !released; kb_mark(key, !released); continue;
        case KB_SCANCODE_LCTRL:  g_ctrl = !released; kb_mark(key, !released); continue;
        case KB_SCANCODE_LALT:   g_alt = !released; kb_mark(key, !released); continue;
        default: break;
        }

        bool was_down = key < 128 && (g_down[key >> 3] & (1u << (key & 7)));
        kb_mark(key, !released);
        if (released) {
            continue;
        }

        if (key == KB_SCANCODE_CAPSLOCK) {
            if (!was_down) { /* ignore typematic repeat */
                g_caps = !g_caps;
                kb_update_leds();
            }
            continue;
        }
        if (key == KB_SCANCODE_NUMLOCK) {
            if (!was_down) {
                g_numlock = !g_numlock;
                kb_update_leds();
            }
            continue;
        }

        /* Keypad block (non-extended 0x47..0x53). */
        if (key >= 0x47 && key <= 0x53) {
            bool shift = g_lshift || g_rshift;
            if (g_numlock != shift) { /* Shift temporarily inverts Num Lock */
                return g_keypad[key - 0x47];
            }
            switch (key) {
            case 0x47: return KEY_CODE_HOME;
            case 0x48: return KEY_CODE_UP;
            case 0x4B: return KEY_CODE_LEFT;
            case 0x4D: return KEY_CODE_RIGHT;
            case 0x4F: return KEY_CODE_END;
            case 0x50: return KEY_CODE_DOWN;
            case 0x53: return KEY_CODE_DELETE;
            case 0x4A: return '-';
            case 0x4E: return '+';
            default: continue;
            }
        }

        if (key < 128) {
            bool shift = g_lshift || g_rshift;
            char base = g_keymap[key];
            char c;
            if (kb_is_letter(base)) {
                c = (shift != g_caps) ? g_shift_keymap[key] : base;
            } else {
                c = shift ? g_shift_keymap[key] : base;
            }
            if (!c) {
                continue;
            }
            if (g_ctrl) {
                if (kb_is_letter(base)) {
                    return (char)(base & 0x1F);
                }
                if (base == ' ') return 0; /* Ctrl+Space = NUL: swallow */
                if (base == '[') return 27;
                if (base == '\\') return 0x1C;
                if (base == ']') return 0x1D;
            }
            return c;
        }
    }
    return 0;
}

int keyboard_ps2_init(void) {
    g_lshift = g_rshift = g_ctrl = g_alt = g_caps = false;
    g_numlock = true;
    g_extended = false;
    g_skip = 0;
    g_pending = 0;
    for (unsigned i = 0; i < sizeof(g_down); i++) {
        g_down[i] = 0;
    }
    while (hal_port_in(PS2_STATUS_PORT) & PS2_STATUS_OUT_FULL) {
        (void)hal_port_in(PS2_DATA_PORT);
    }
    kb_update_leds();
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
    if (scancode >= 128) {
        return false;
    }
    return (g_down[scancode >> 3] & (1u << (scancode & 7))) != 0;
}

bool keyboard_caps_lock(void) {
    return g_caps;
}

bool keyboard_num_lock(void) {
    return g_numlock;
}
