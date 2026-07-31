#include <kernel/string.h>
#include <term/tty.h>
#include <hal/console.h>

static void tty_erase_one(tty_t* tty) {
    if (!tty->buf_pos) {
        return;
    }
    tty->buf_pos--;
    tty->buffer[tty->buf_pos] = '\0';
    if (tty->echo) {
        hal_console_write("\b \b", 3);
    }
}

static void tty_replace_line(tty_t* tty, const char* text) {
    while (tty->buf_pos) {
        tty_erase_one(tty);
    }
    size_t len = strlen(text);
    if (len >= tty->buf_size) {
        len = tty->buf_size - 1;
    }
    memcpy(tty->buffer, text, len);
    tty->buffer[len] = '\0';
    tty->buf_pos = len;
    if (tty->echo) {
        hal_console_write(tty->buffer, len);
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
    if (c == '\b' || c == 0x7F) {
        tty_erase_one(tty);
        return 0;
    }
    if (c == 0x15) { /* Ctrl+U */
        while (tty->buf_pos) {
            tty_erase_one(tty);
        }
        return 0;
    }
    if (c == 0x17) { /* Ctrl+W */
        while (tty->buf_pos && tty->buffer[tty->buf_pos - 1] == ' ') {
            tty_erase_one(tty);
        }
        while (tty->buf_pos && tty->buffer[tty->buf_pos - 1] != ' ') {
            tty_erase_one(tty);
        }
        return 0;
    }
    if (c == 0x10) { /* PS/2 up */
        tty_history_move(tty, -1);
        return 0;
    }
    if (c == 0x0E) { /* PS/2 down */
        tty_history_move(tty, 1);
        return 0;
    }
    if (c == '\n') {
        tty->buffer[tty->buf_pos] = '\0';
        tty_history_add(tty);
        if (tty->echo) {
            hal_console_putchar('\n');
        }
        return '\n';
    }
    if (c == '\t') {
        c = ' ';
    }
    if (c >= ' ' && c <= '~' && tty->buf_pos + 1 < tty->buf_size) {
        tty->buffer[tty->buf_pos++] = c;
        tty->buffer[tty->buf_pos] = '\0';
        if (tty->echo) {
            hal_console_putchar(c);
        }
        return c;
    }
    return 0;
}

ssize_t tty_readline(tty_t* tty, char* buf, size_t size) {
    if (!tty || !buf || size == 0) {
        return -1;
    }
    tty->buf_pos = 0;
    tty->buffer[0] = '\0';
    tty->history_cursor = (int)tty->history_count;

    while (1) {
        char c = hal_console_getchar();
        if (c == 27) {
            char bracket = hal_console_getchar();
            char key = hal_console_getchar();
            if (bracket == '[' && key == 'A') {
                tty_history_move(tty, -1);
            } else if (bracket == '[' && key == 'B') {
                tty_history_move(tty, 1);
            }
            continue;
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
