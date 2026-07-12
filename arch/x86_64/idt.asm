; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  * Vytvořeno jen tak z nudy a z chuti zkoušet nové věci.
;  */

[bits 64]
section .text
global arch_load_idt

arch_load_idt:
    lidt [rdi]
    ret
