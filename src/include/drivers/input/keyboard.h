/* tach - Keyboard Header */
#ifndef _DRIVERS_KEYBOARD_H
#define _DRIVERS_KEYBOARD_H

#include <kernel/types.h>
#include <stdint.h>

/* Keyboard scancode constants */
#define KB_SCANCODE_ESCAPE 0x01
#define KB_SCANCODE_BACKSPACE 0x0E
#define KB_SCANCODE_TAB 0x0F
#define KB_SCANCODE_ENTER 0x1C
#define KB_SCANCODE_LSHIFT 0x2A
#define KB_SCANCODE_RSHIFT 0x36

/* Initialize keyboard driver */
int keyboard_init(void);

/* Read next character from keyboard buffer */
char keyboard_readchar(void);

/* Check if key is pressed */
bool keyboard_is_pressed(uint8_t scancode);

#endif /* _DRIVERS_KEYBOARD_H */
