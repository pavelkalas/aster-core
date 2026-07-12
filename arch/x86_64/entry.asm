; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  * Vytvořeno jen tak z nudy a z chuti zkoušet nové věci.
;  */

[bits 64]

section .text.boot
global _start
extern kmain

_start:
    cli
    mov word [0xB800A], 0x0F4B
    mov rsp, stack_top
    call kmain

.halt:
    hlt
    jmp .halt

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
