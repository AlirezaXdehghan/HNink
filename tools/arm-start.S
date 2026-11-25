.syntax unified
.arm
.text
.global _start
.type _start,%function
_start:
    mov r5, r0
    ldr r1, [sp], #4
    mov r2, sp
    bic sp, sp, #7
    mov r3, #0
    push {r3}
    push {r2}
    push {r5}
    push {r3}
    ldr r0, =main
    bl __libc_start_main
    udf #0
.size _start,.-_start

.global __aeabi_memclr
.global __aeabi_memclr4
.global __aeabi_memclr8
__aeabi_memclr:
__aeabi_memclr4:
__aeabi_memclr8:
    mov r2, r1
    mov r1, #0
    b memset

.global __aeabi_memcpy
.global __aeabi_memcpy4
.global __aeabi_memcpy8
__aeabi_memcpy:
__aeabi_memcpy4:
__aeabi_memcpy8:
    b memcpy
.section .note.GNU-stack,"",%progbits
