#include <sys/syscall.h>

    .text
    .globl _start

#if defined(__x86_64__)

_start:
    mov     $SYS_write, %rax
    mov     $1, %rdi
    lea     addr1(%rip), %rsi
    mov     $(addr2-addr1), %rdx
    syscall

    mov     $SYS_exit, %rax
    mov     $812, %rdi
    syscall

#elif defined(__aarch64__)  /* ARM64 (PIE) */

_start:
    mov     x0, #1                   // fd
    adrp    x1, addr1                // buf = PC相对 (PIE)
    add     x1, x1, :lo12:addr1
    mov     x2, #(addr2-addr1)       // count
    mov     x8, #SYS_write           // syscall number
    svc     #0                       // software interrupt

    mov     x0, #812                 // status
    mov     x8, #SYS_exit
    svc     #0

#else
#error "Unsupported architecture"
#endif

addr1:
    .ascii "\033[01;31mHello, OS World\033[0m\n"
addr2:
