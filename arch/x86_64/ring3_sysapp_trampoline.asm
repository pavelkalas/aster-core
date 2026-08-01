; Ring3 trampoline pro spousteni sysapps.
; Zavola ukazatel na funkci, pak vyvola software interrupt pro navrat do kernelu.

[bits 64]
section .text
global ring3_sysapp_start
global ring3_sysapp_end
global ring3_sysapp_entry_ptr

ring3_sysapp_start:
    mov rax, [rel ring3_sysapp_entry_ptr]
    call rax
    int 0x81
.hang:
    hlt
    jmp .hang

align 8
ring3_sysapp_entry_ptr:
    dq 0

ring3_sysapp_end:
