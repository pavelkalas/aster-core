; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  */
;
; /*
;  * Syscall entry – obsluha SYSCALL instrukce z ring 3.
;  *
;  * Při SYSCALL CPU automaticky:
;  *   RCX = RIP (návratová adresa uživatele)
;  *   R11 = RFLAGS
;  *   RIP = LSTAR (adresa syscall_entry)
;  *   CS  = STAR[47:32] (kernel code segment)
;  *   SS  = STAR[47:32] + 8 (kernel stack segment)
;  *
;  * Při SYSRET (návrat do ring 3):
;  *   CS  = (STAR[63:48] + 16) | 3
;  *   SS  = STAR[63:48] + 8
;  *   RIP = RCX
;  *   RFLAGS = R11
;  */

[bits 64]
section .text
global syscall_entry
global syscall_init
extern syscall_handler
extern g_kernel_syscall_stack_top

syscall_entry:
    ; Přepnout na kernel stack (z globální proměnné)
    mov rsp, [rel g_kernel_syscall_stack_top]

    ; Uložit uživatelský kontext
    push rcx                         ; user RIP
    push r11                         ; user RFLAGS

    ; Zavolat C handler:
    ;   syscall_handler(number, a1, a2, a3)
    ;   RDI = syscall number (z RAX)
    ;   RSI = arg1 (z RDI)
    ;   RDX = arg2 (z RSI)
    ;   RCX = arg3 (z RDX)
    push rdi                         ; pro jistotu, kdyby handler upravil
    push rsi
    push rdx

    mov rcx, rdx                     ; arg3 = RDX
    mov rdx, rsi                     ; arg2 = RSI
    mov rsi, rdi                     ; arg1 = RDI
    mov rdi, rax                     ; number = RAX
    call syscall_handler

    pop rdx
    pop rsi
    pop rdi

    ; Obnovit uživatelský kontext
    pop r11                          ; user RFLAGS
    pop rcx                          ; user RIP

    ; SYSRET zpět do ring 3
    ; RAX obsahuje návratovou hodnotu z handleru
    o64 sysret

; ------------------------------------------------------------
; Inicializace SYSCALL – zapíše MSR registry
; ------------------------------------------------------------
syscall_init:
    ; Povolit SYSCALL/SYSRET v EFER MSR (bit 0 = SCE)
    mov ecx, 0xC0000080             ; EFER MSR
    rdmsr
    or eax, (1 << 0)                ; nastavit SCE (Syscall Enable)
    wrmsr

    ; STAR MSR – segment selectory
    ;   STAR[47:32] = 0x0008 (kernel CS pro SYSCALL)
    ;   STAR[63:48] = 0x0010 (SYSRET: CS = (0x10 + 16) | 3 = 0x23, SS = 0x10 + 8 = 0x18)
    mov ecx, 0xC0000081             ; STAR MSR
    mov edx, 0x0010                 ; high: user CS base (0x20 - 16)
    mov eax, 0x0008                 ; low: kernel CS
    wrmsr

    ; LSTAR MSR – adresa syscall_entry (RIP po SYSCALL)
    mov ecx, 0xC0000082             ; LSTAR MSR
    mov rax, syscall_entry
    mov rdx, rax
    shr rdx, 32
    wrmsr

    ; CSTAR MSR – compatibility mode (nepoužíváme)
    mov ecx, 0xC0000083
    xor edx, edx
    xor eax, eax
    wrmsr

    ; SF_MASK MSR – RFLAGS maska pro SYSCALL
    mov ecx, 0xC0000084
    mov edx, 0
    mov eax, 0x200                   ; mask IF (bit 9)
    wrmsr

    ret

; ------------------------------------------------------------
; Kernel stack pro syscall handler
; ------------------------------------------------------------
section .bss
align 16
g_kernel_syscall_stack:
    resb 4096
g_kernel_syscall_stack_top:
