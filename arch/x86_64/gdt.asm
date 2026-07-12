; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  * Vytvořeno jen tak z nudy a z chuti zkoušet nové věci.
;  */

[bits 64]
section .text
global arch_load_gdt
global arch_reload_segments

arch_load_gdt:
    lgdt [rdi]
    ret

arch_reload_segments:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    push qword 0x08
    lea rax, [rel .flush]
    push rax
    retfq
.flush:
    ret
