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

#include "kernel.h"
#include "vga.h"
#include "serial.h"

void kernel_init(void) {
    /* Initialize VGA text mode */
    vga_init();
    
    /* Initialize serial port for debugging */
    serial_init();
    
    /* Clear screen and print welcome message */
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void kernel_main(void) {
    kernel_init();
    
    vga_println("Welcome to tach Operating System");
    vga_println("Version: 0.1.0-dev");
    vga_println("");
    vga_println("System initialized successfully.");
    vga_println("Type 'help' for available commands.");
    
    /* Main kernel loop */
    while (1) {
        /* Idle loop - in a real OS, this would be the scheduler */
        __asm__ volatile ("hlt");
    }
}
