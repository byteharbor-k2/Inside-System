.globl _start

_start:
    # SBI system shutdown (sbi_ext_base = 0x10, sbi_ext_base_shutdown = 0x08)
    # a7 = SBI extension ID (0x10 for base)
    # a6 = function ID (0x08 for shutdown)
    li a7, 0x10
    li a6, 0x08
    j go

go:
    ecall

    # Should never reach here
1:
    wfi
    j 1b
