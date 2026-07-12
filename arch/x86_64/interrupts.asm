; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  * Vytvořeno jen tak z nudy a z chuti zkoušet nové věci.
;  */

[bits 64]
section .text

extern interrupt_dispatch

global isr_stub_table

%macro ISR_NOERR 1
global isr_%1
isr_%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr_%1
isr_%1:
    push qword %1
    jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 128

isr_common:
    cld
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call interrupt_dispatch

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq

section .rodata
isr_stub_table:
    dq isr_0
    dq isr_1
    dq isr_2
    dq isr_3
    dq isr_4
    dq isr_5
    dq isr_6
    dq isr_7
    dq isr_8
    dq isr_9
    dq isr_10
    dq isr_11
    dq isr_12
    dq isr_13
    dq isr_14
    dq isr_15
    dq isr_16
    dq isr_17
    dq isr_18
    dq isr_19
    dq isr_20
    dq isr_21
    dq isr_22
    dq isr_23
    dq isr_24
    dq isr_25
    dq isr_26
    dq isr_27
    dq isr_28
    dq isr_29
    dq isr_30
    dq isr_31
    dq isr_32
    dq isr_33
    dq isr_128
