; /*
;  * AsterOS Kernel
;  * Autor: Pavel Kalaš
;  * Rok: 2026
;  *
;  */

[bits 16]

global disk_read_lba

; DS:SI -> adresa DAP struktury, DL -> číslo disku
disk_read_lba:
    push ax
    mov ah, 0x42
    int 0x13
    pop ax
    ret
