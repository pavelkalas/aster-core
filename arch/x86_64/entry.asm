; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  * Vytvořeno jen tak z nudy a z chuti zkoušet nové věci.
;  *
;  * Boot asm — vstupní bod kernelu (x86_64, long mode).
;  *
;  * Tento soubor obsahuje _start, což je první kód, který kernel vykoná
;  * po přechodu do 64bitového módu (long mode) z bootloaderu.
;  *
;  * Co se děje:
;  *   1. CLI – zakáže přerušení (ještě nemáme IDT)
;  *   2. Napíše 'K' do VGA paměti na znamení, že kernel běží
;  *   3. Vyčistí BSS sekci (nulování)
;  *   4. Nastaví RSP na stack_top (16 KB zásobník v BSS)
;  *   5. Zavolá kmain() – hlavní C funkci jádra
;  *   6. Pokud kmain vrátí, vstoupí do nekonečné HLT smyčky
;  */

[bits 64]

section .text.boot
global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    cli                              ; zakázat přerušení (IDT není hotová)
    mov word [0xB800A], 0x0F4B       ; vypsat "K" do VGA (důkaz, že kernel běží)

    ; Vyčistit BSS sekci (nulování)
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    shr rcx, 3                       ; počet qwordů
    xor rax, rax
    cld
    rep stosq

    mov rsp, stack_top               ; nastavit stack pointer
    call kmain                       ; předat řízení C jádru
.halt:
    hlt                              ; pokud se kmain vrátí, zastavit CPU
    jmp .halt
section .bss
align 16
stack_bottom:
    resb 16384                       ; 16 KiB zásobník
stack_top:
