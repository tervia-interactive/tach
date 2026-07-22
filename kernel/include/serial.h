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

#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

/* COM1 port address */
#define SERIAL_COM1 0x3F8

/* Initialize serial port */
void serial_init(void);

/* Check if serial transmit buffer is empty */
int serial_is_transmit_empty(void);

/* Write a character to serial */
void serial_write_char(char c);

/* Write a string to serial */
void serial_write(const char* str);

#endif /* SERIAL_H */
