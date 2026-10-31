#include <kernel/string.h>
#include <term/tty.h>
#include <hal/console.h>

/* Editing keys. These are the control codes produced by the PS/2 driver and
 * by the ANSI escape parser below, so both inputs behave the same way. */
#define K_CTRL_A 0x01 /* home */
#define K_CTRL_B 0x02 /* left */
#define K_CTRL_C 0x03 /* cancel line */
#define K_CTRL_D 0x04 /* delete under cursor */
#define K_CTRL_E 0x05 /* end */
#define K_CTRL_F 0x06 /* right */
#define K_CTRL_K 0x0B /* kill to end of line */
#define K_CTRL_L 0x0C /* clear screen */
#define K_CTRL_N 0x0E /* history down */
#define K_CTRL_P 0x10 /* history up */
#define K_CTRL_U 0x15 /* kill to start of line */
#define K_CTRL_W 0x17 /* kill previous word */

static void tty_out_char(tty_t* tty, char c) {
    if (tty->echo) {
        hal_console_putchar(c);
    }
}

static void tty_back(tty_t* tty, size_t count) {
    while (count--) {
        tty_out_char(tty, '\b');
    }
}

/* Redraw buffer[from..len), blank `pad` extra cells, then park the console
 * cursor back at tty->cursor. */
static void tty_redraw_tail(tty_t* tty, size_t from, size_t pad) {
    if (!tty->echo) {
        return;
    }
    size_t len = tty->buf_pos;
    if (from < len) {
        hal_console_write(tty->buffer + from, len - from);
    }
    for (size_t i = 0; i < pad; i++) {
        hal_console_putchar(' ');
    }
    tty_back(tty, (len - from) + pad - (tty->cursor - from));
}

static void tty_move_to(tty_t* tty, size_t target) {
    if (target > tty->buf_pos) {
        target = tty->buf_pos;
    }
    while (tty->cursor > target) {
        tty->cursor--;
        tty_out_char(tty, '\b');
    }
    while (tty->cursor < target) {
        tty_out_char(tty, tty->buffer[tty->cursor]);
        tty->cursor++;
    }
}

static void tty_delete_at(tty_t* tty, size_t pos) {
    if (pos >= tty->buf_pos) {
        return;
    }
    memmove(tty->buffer + pos, tty->buffer + pos + 1, tty->buf_pos - pos);
    tty->buf_pos--;
    if (tty->cursor > pos) {
        tty->cursor--;
    }
}

static void tty_backspace(tty_t* tty) {
    if (!tty->cursor) {
        return;
    }
    tty_out_char(tty, '\b');
    size_t pos = tty->cursor - 1;
    tty_delete_at(tty, pos);
    tty_redraw_tail(tty, pos, 1);
}

static void tty_delete_forward(tty_t* tty) {
    if (tty->cursor >= tty->buf_pos) {
        return;
    }
    tty_delete_at(tty, tty->cursor);
    tty_redraw_tail(tty, tty->cursor, 1);
}

void tty_insert_text(tty_t* tty, const char* text, size_t len) {
    if (!tty || !text) {
        return;
    }
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c < ' ' || c > '~' || tty->buf_pos + 1 >= tty->buf_size) {
            continue;
        }
        memmove(tty->buffer + tty->cursor + 1, tty->buffer + tty->cursor,
                tty->buf_pos - tty->cursor + 1);
        tty->buffer[tty->cursor++] = c;
        tty->buf_pos++;
        if (tty->echo) {
            if (tty->cursor == tty->buf_pos) {
                hal_console_putchar(c);
            } else {
                tty_redraw_tail(tty, tty->cursor - 1, 0);
            }
        }
    }
}

static void tty_clear_line(tty_t* tty) {
    tty_move_to(tty, tty->buf_pos);
    while (tty->cursor) {
        tty_backspace(tty);
    }
}

static void tty_replace_line(tty_t* tty, const char* text) {
    tty_clear_line(tty);
    tty->buffer[0] = '\0';
    tty_insert_text(tty, text, strlen(text));
}

static void tty_kill_to_start(tty_t* tty) {
    while (tty->cursor) {
        tty_backspace(tty);
    }
}

static void tty_kill_to_end(tty_t* tty) {
    while (tty->cursor < tty->buf_pos) {
        tty_delete_forward(tty);
    }
}

static void tty_kill_word(tty_t* tty) {
    while (tty->cursor && tty->buffer[tty->cursor - 1] == ' ') {
        tty_backspace(tty);
    }
    while (tty->cursor && tty->buffer[tty->cursor - 1] != ' ') {
        tty_backspace(tty);
    }
}

static void tty_history_move(tty_t* tty, int direction) {
    if (!tty->history_count) {
        return;
    }
    if (tty->history_cursor < 0) {
        tty->history_cursor = (int)tty->history_count;
    }
    tty->history_cursor += direction;
    if (tty->history_cursor < 0) {
        tty->history_cursor = 0;
    }
    if (tty->history_cursor > (int)tty->history_count) {
        tty->history_cursor = (int)tty->history_count;
    }
    if (tty->history_cursor == (int)tty->history_count) {
        tty_replace_line(tty, "");
        return;
    }
    size_t oldest = tty->history_count < TTY_HISTORY_SIZE ? 0 : tty->history_head;
    size_t slot = (oldest + (size_t)tty->history_cursor) % TTY_HISTORY_SIZE;
    tty_replace_line(tty, tty->history[slot]);
}

static void tty_history_add(tty_t* tty) {
    if (!tty->buf_pos) {
        return;
    }
    if (tty->history_count) {
        size_t last = (tty->history_head + TTY_HISTORY_SIZE - 1) % TTY_HISTORY_SIZE;
        if (strcmp(tty->history[last], tty->buffer) == 0) {
            return;
        }
    }
    strncpy(tty->history[tty->history_head], tty->buffer, TTY_BUFFER_SIZE - 1);
    tty->history[tty->history_head][TTY_BUFFER_SIZE - 1] = '\0';
    tty->history_head = (tty->history_head + 1) % TTY_HISTORY_SIZE;
    if (tty->history_count < TTY_HISTORY_SIZE) {
        tty->history_count++;
    }
}

static void tty_redraw_all(tty_t* tty) {
    size_t cursor = tty->cursor;
    if (tty->prompt_fn) {
        tty->prompt_fn();
    }
    if (tty->echo && tty->buf_pos) {
        hal_console_write(tty->buffer, tty->buf_pos);
    }
    tty->cursor = tty->buf_pos;
    tty_move_to(tty, cursor);
}

static void tty_complete(tty_t* tty) {
    if (!tty->complete_fn) {
        tty_insert_text(tty, " ", 1);
        return;
    }
    char out[TTY_BUFFER_SIZE];
    out[0] = '\0';
    tty->complete_listed = false;
    size_t len = tty->complete_fn(tty, out, sizeof(out));
    if (len >= sizeof(out)) {
        len = sizeof(out) - 1;
    }
    if (tty->complete_listed) {
        /* The list is already on screen: update the buffer silently and
         * redraw prompt + line below it. */
        bool echo = tty->echo;
        tty->echo = false;
        tty_insert_text(tty, out, len);
        tty->echo = echo;
        tty->complete_listed = false;
        tty_redraw_all(tty);
        return;
    }
    tty_insert_text(tty, out, len);
}

void tty_init(tty_t* tty) {
    if (!tty) {
        return;
    }
    memset(tty, 0, sizeof(*tty));
    tty->mode = TTY_MODE_COOKED;
    tty->echo = true;
    tty->buf_size = TTY_BUFFER_SIZE;
    tty->history_cursor = -1;
}

void tty_set_mode(tty_t* tty, int mode) {
    if (tty && (mode == TTY_MODE_RAW || mode == TTY_MODE_COOKED)) {
        tty->mode = mode;
    }
}

char tty_process_char(tty_t* tty, char c) {
    if (!tty) {
        return 0;
    }
    if (tty->mode == TTY_MODE_RAW) {
        return c;
    }
    if (c == '\r') {
        c = '\n';
    }
    switch (c) {
    case '\b':
    case 0x7F:
        tty_backspace(tty);
        return 0;
    case K_CTRL_A: tty_move_to(tty, 0); return 0;
    case K_CTRL_E: tty_move_to(tty, tty->buf_pos); return 0;
    case K_CTRL_B: if (tty->cursor) tty_move_to(tty, tty->cursor - 1); return 0;
    case K_CTRL_F: tty_move_to(tty, tty->cursor + 1); return 0;
    case K_CTRL_D: tty_delete_forward(tty); return 0;
    case K_CTRL_K: tty_kill_to_end(tty); return 0;
    case K_CTRL_U: tty_kill_to_start(tty); return 0;
    case K_CTRL_W: tty_kill_word(tty); return 0;
    case K_CTRL_P: tty_history_move(tty, -1); return 0;
    case K_CTRL_N: tty_history_move(tty, 1); return 0;
    case K_CTRL_L:
        hal_console_clear();
        tty_redraw_all(tty);
        return 0;
    case K_CTRL_C:
        tty_move_to(tty, tty->buf_pos);
        if (tty->echo) {
            hal_console_write("^C\n", 3);
        }
        tty->buf_pos = 0;
        tty->cursor = 0;
        tty->buffer[0] = '\0';
        return '\n';
    case '\t':
        tty_complete(tty);
        return 0;
    case '\n':
        tty_move_to(tty, tty->buf_pos);
        tty->buffer[tty->buf_pos] = '\0';
        tty_history_add(tty);
        if (tty->echo) {
            hal_console_putchar('\n');
        }
        return '\n';
    default:
        break;
    }
    if (c >= ' ' && c <= '~' && tty->buf_pos + 1 < tty->buf_size) {
        tty_insert_text(tty, &c, 1);
        return c;
    }
    return 0;
}

/* Wait a little for the next byte of an escape sequence; a lone ESC key
 * press must not block the line editor. */
static int tty_next_escape_byte(char* out) {
    for (unsigned spin = 0; spin < 400000; spin++) {
        if (hal_console_input_available()) {
            *out = hal_console_getchar();
            return 1;
        }
    }
    return 0;
}

/* Translate ESC [ A/B/C/D/H/F, ESC [ n ~ and ESC O x into editing codes. */
static char tty_parse_escape(void) {
    char c;
    if (!tty_next_escape_byte(&c)) {
        return 0;
    }
    if (c != '[' && c != 'O') {
        return 0;
    }
    int param = 0;
    while (tty_next_escape_byte(&c)) {
        if (c >= '0' && c <= '9') {
            param = param * 10 + (c - '0');
            continue;
        }
        if (c == ';') {
            param = 0;
            continue;
        }
        switch (c) {
        case 'A': return K_CTRL_P;
        case 'B': return K_CTRL_N;
        case 'C': return K_CTRL_F;
        case 'D': return K_CTRL_B;
        case 'H': return K_CTRL_A;
        case 'F': return K_CTRL_E;
        case '~':
            if (param == 1 || param == 7) return K_CTRL_A;
            if (param == 3) return K_CTRL_D;
            if (param == 4 || param == 8) return K_CTRL_E;
            return 0;
        default:
            return 0;
        }
    }
    return 0;
}

ssize_t tty_readline(tty_t* tty, char* buf, size_t size) {
    if (!tty || !buf || size == 0) {
        return -1;
    }
    tty->buf_pos = 0;
    tty->cursor = 0;
    tty->buffer[0] = '\0';
    tty->history_cursor = (int)tty->history_count;

    while (1) {
        char c = hal_console_getchar();
        if (c == 27) {
            c = tty_parse_escape();
            if (!c) {
                continue;
            }
        }
        if (tty_process_char(tty, c) == '\n') {
            size_t len = tty->buf_pos;
            if (len >= size) {
                len = size - 1;
            }
            memcpy(buf, tty->buffer, len);
            buf[len] = '\0';
            return (ssize_t)len;
        }
    }
}
