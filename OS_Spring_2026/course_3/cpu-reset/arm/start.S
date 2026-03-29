.globl _start

_start:
    # Semihosting exit syscall
    # x0 = syscall number (0x18 = exit)
    # x1 = exit code
    mov x0, #0x18
    mov x1, #0

    b go

go:
    hlt #0xf000

    # Should never reach here
1:
    wfi
    b 1b
