@ crtbegin de nora-gcc: arranque de un ejecutable PIE de Android 4.4 (bionic).
@ El enlazador /system/bin/linker salta a _start con la pila de argumentos y
@ se la pasamos a __libc_init(raw_args, onexit, main, structors).
@ Direcciones relativas al pc: sin relocalizaciones en el codigo.
    .text
    .arm
    .global _start
    .type _start, %function
_start:
    mov     r0, sp
    mov     r1, #0
    ldr     r2, .Lmain
.Lpc1:
    add     r2, pc, r2
    ldr     r3, .Lestr
.Lpc2:
    add     r3, pc, r3
    bl      __libc_init(PLT)
    b       .
.Lmain:
    .word   main - (.Lpc1 + 8)
.Lestr:
    .word   estructores - (.Lpc2 + 8)

    .data
    .align 2
estructores:            @ structors_array_t: preinit, init, fini (vacios)
    .word 0, 0, 0
    .section .note.GNU-stack,"",%progbits
