; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  * IDT asm — načtení IDT (x86_64).
;  *
;  * arch_load_idt:
;  *   Přijme ukazatel na strukturu idt_ptr (limit + base) v RDI
;  *   a provede LIDT (Load Interrupt Descriptor Table).
;  *   Poté už jsou všechna přerušení směrována přes IDT.
;  */

[bits 64]
section .text
global arch_load_idt

arch_load_idt:
    lidt [rdi]                       ; načíst IDT (předáno v RDI z cpu.c)
    ret

