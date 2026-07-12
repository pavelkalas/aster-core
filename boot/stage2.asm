; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  */

[org 0x8000]
[bits 16]

jmp start2

%include "disk.asm"

KERNEL_SECTORS     equ 256
KERNEL_START_LBA   equ 17
KERNEL_LOAD_SEG    equ 0x1000
PML4_ADDR          equ 0x00070000
PDPT_ADDR          equ 0x00071000
PD_ADDR            equ 0x00072000

start2:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x9000
    sti

    mov [boot_drive], dl

    mov si, msg_stage2
    call print_string

    call enable_a20
    call load_kernel
    jc load_error

    mov si, msg_kernel_loaded
    call print_string

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp 0x08:protected_entry

load_error:
    mov si, msg_load_error
    call print_string
    jmp $

enable_a20:
    in al, 0x92
    or al, 0x02
    out 0x92, al
    ret

load_kernel:
    mov word [remaining_sectors], KERNEL_SECTORS
    mov word [current_seg], KERNEL_LOAD_SEG
    mov dword [current_lba], KERNEL_START_LBA

.next_chunk:
    mov ax, [remaining_sectors]
    test ax, ax
    jz .done

    cmp ax, 127
    jbe .use_rest
    mov ax, 127

.use_rest:
    mov [chunk_sectors], ax

    mov word [dap + 2], ax
    mov word [dap + 4], 0x0000
    mov ax, [current_seg]
    mov word [dap + 6], ax
    mov eax, [current_lba]
    mov dword [dap + 8], eax
    mov dword [dap + 12], 0

    mov dl, [boot_drive]
    mov si, dap
    call disk_read_lba
    jc .error

    mov ax, [chunk_sectors]
    shl ax, 5
    add [current_seg], ax

    mov eax, [current_lba]
    movzx ebx, word [chunk_sectors]
    add eax, ebx
    mov [current_lba], eax

    mov ax, [remaining_sectors]
    sub ax, [chunk_sectors]
    mov [remaining_sectors], ax
    jmp .next_chunk

.error:
    stc
    ret

.done:
    clc
    ret

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
msg_stage2: db "Aster Bootloader stage2", 13, 10, 0
msg_kernel_loaded: db "Kernel read OK", 13, 10, 0
msg_load_error: db "Kernel load error", 13, 10, 0

align 8
dap:
    db 0x10
    db 0x00
    dw 0
    dw 0
    dw 0
    dd 0

remaining_sectors: dw 0
chunk_sectors: dw 0
current_seg: dw 0
current_lba: dd 0
    dd 0

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

[bits 32]
protected_entry:
    mov word [0xB8000], 0x0F50

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9FC00

    mov esi, 0x00010000
    mov edi, 0x00100000
    mov ecx, (KERNEL_SECTORS * 512) / 4
    rep movsd

    call setup_paging

    mov word [0xB8002], 0x0F47

    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    mov eax, PML4_ADDR
    mov cr3, eax

    mov eax, cr0
    or eax, 0x80000001
    mov cr0, eax

    mov word [0xB8004], 0x0F4C

    jmp 0x18:long_mode_entry

setup_paging:
    mov edi, PML4_ADDR
    mov ecx, (4096 * 3) / 4
    xor eax, eax
    rep stosd

    mov dword [PML4_ADDR], PDPT_ADDR | 0x03
    mov dword [PML4_ADDR + 4], 0

    mov dword [PDPT_ADDR], PD_ADDR | 0x03
    mov dword [PDPT_ADDR + 4], 0

    mov edi, PD_ADDR
    xor ebx, ebx
    mov ecx, 512

.map_pd:
    mov eax, ebx
    or eax, 0x83
    mov dword [edi], eax
    mov dword [edi + 4], 0
    add ebx, 0x200000
    add edi, 8
    loop .map_pd

    ret

[bits 64]
long_mode_entry:
    mov word [0xB8006], 0x0F4D

    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x200000

    mov word [0xB8008], 0x0F4A

    mov rax, 0x00100000
    jmp rax

halt64:
    hlt
    jmp halt64
