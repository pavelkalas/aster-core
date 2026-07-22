; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  * Vytvořeno jen tak z nudy a z chuti zkoušet nové věci.
;  *
;  * GDT asm — načtení GDT a přenačtení segmentových registrů (x86_64).
;  *
;  * arch_load_gdt:
;  *   Přijme ukazatel na strukturu gdt_ptr (limit + base) v RDI
;  *   a provede LGDT (Load Global Descriptor Table).
;  *
;  * arch_reload_segments:
;  *   Nastaví DS, ES, FS, GS, SS na data segment (0x10)
;  *   a provede long jump (RETFQ) pro přenačtení CS na kódový segment (0x08).
;  */

[bits 64]
section .text
global arch_load_gdt
global arch_reload_segments

arch_load_gdt:
    lgdt [rdi]                       ; načíst GDT (předáno v RDI z cpu.c)
    ret

arch_reload_segments:
    mov ax, 0x10                     ; data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    push qword 0x08                  ; kódový segment selector (CS)
    lea rax, [rel .flush]            ; adresa pro návrat z far jumpu
    push rax
    retfq                            ; far return -> přenačte CS a skočí na .flush
.flush:
    ret

