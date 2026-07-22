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

.section ".text.boot"
.global _start

.extern kernel_main

_start:
    /* Set up stack */
    ldr sp, =stack_top
    
    /* Call kernel main */
    bl kernel_main
    
    /* Halt if kernel returns */
1:  wfi
    b 1b

.align 4
stack_bottom:
    .skip 16384
stack_top:
