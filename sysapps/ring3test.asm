; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  */
;
; /*
;  * Ring 3 test – jednoduchý uživatelský program.
;  * Vypíše zprávu přes syscall a ukončí se.
;  * Používá RIP-relativní adresování, aby fungoval nezávisle
;  * na tom, kam je zkopírován.
;  */

[bits 64]

global ring3_test_start
global ring3_test_end

ring3_test_start:
    ; Syscall write(0, message)
    mov rax, 0                      ; syscall: write
    lea rdi, [rel message]          ; arg1: ukazatel na text (RIP-relativní)
    syscall

    ; Syscall exit(1)
    mov rax, 1                      ; syscall: exit
    syscall

    ; Sem by se nemělo dostat
    jmp $

message:
    db "Ahoj z Ring 3!", 10, 0

ring3_test_end:
