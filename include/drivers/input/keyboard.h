#ifndef DRIVERS_INPUT_KEYBOARD_H
#define DRIVERS_INPUT_KEYBOARD_H

#include <kernel/types.h>

#define KEYBOARD_BUFFER_SIZE 256

typedef enum {
    KEY_NONE = 0,
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
    KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_ENTER, KEY_BACKSPACE, KEY_TAB, KEY_SPACE, KEY_ESCAPE,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_SHIFT, KEY_CTRL, KEY_ALT,
    KEY_CAPSLOCK, KEY_NUMLOCK, KEY_SCROLLLOCK
} key_code_t;

typedef struct {
    key_code_t code;
    uint8_t pressed;
    uint32_t timestamp;
} key_event_t;

int keyboard_init(void);
int keyboard_read(key_event_t *event);
int keyboard_poll(void);
void keyboard_set_leds(uint8_t leds);

#endif /* DRIVERS_INPUT_KEYBOARD_H */
