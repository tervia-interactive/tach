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

.section .multiboot_header
header_start:
    /* magic */
    .long 0xe85250d6
    /* architecture (protected mode) */
    .long 0
    /* header length */
    .long header_end - header_start
    /* checksum */
    .long -(0xe85250d6 + 0 + (header_end - header_start))

    /* Optional tag: End tag */
    .short 0
    .short 0
    .long 8
header_end:

.section .text
.global _start
.type _start, @function

.extern kernel_main

_start:
    /* Set up stack */
    mov $stack_top, %esp
    
    /* Call kernel main */
    call kernel_main
    
    /* Halt if kernel returns */
    cli
1:  hlt
    jmp 1b

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
