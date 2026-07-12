; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  */

[org 0x7C00]
[bits 16]

STAGE2_SECTORS equ 16
STAGE2_LBA     equ 1

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, boot_msg
    call print_string

    mov word [dap + 2], STAGE2_SECTORS
    mov word [dap + 4], 0x8000
    mov word [dap + 6], 0x0000
    mov dword [dap + 8], STAGE2_LBA
    mov dword [dap + 12], 0

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    jmp 0x0000:0x8000

disk_error:
    mov si, disk_msg
    call print_string
    jmp $

print_string:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x0F
    int 0x10
    jmp print_string
.done:
    ret

boot_drive: db 0
boot_msg: db "Aster Bootloader stage1", 13, 10, 0
disk_msg: db "Disk read error", 13, 10, 0

align 8
dap:
    db 0x10
    db 0
    dw 0
    dw 0
    dw 0
    dd 0
    dd 0

times 510 - ($ - $$) db 0
dw 0xAA55
