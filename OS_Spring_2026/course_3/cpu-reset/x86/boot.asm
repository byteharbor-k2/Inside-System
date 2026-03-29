bits 16

org 0x7c00

_start:
    cli                 ; Disable interrupts
    mov dx, 0x0604      ; QEMU debug exit I/O port
    mov ax, 0x2000
    out dx, ax          ; Exit QEMU

    times 510-($-$$) db 0
    dw 0xaa55           ; Boot signature
