/*
 * tach - A minimal open source Operating System
 * Copyright 2026 Tervia Interactive™
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VGA_H
#define VGA_H

#include <stdint.h>

/* VGA text mode buffer address */
#define VGA_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* Initialize VGA text mode */
void vga_init(void);

/* Clear the screen */
void vga_clear(void);

/* Set text color */
void vga_set_color(uint8_t fg, uint8_t bg);

/* Print a character */
void vga_putchar(char c);

/* Print a string */
void vga_print(const char* str);

/* Print a string with newline */
void vga_println(const char* str);

#endif /* VGA_H */
