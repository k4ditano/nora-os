@ Arranque minimo de un ejecutable de Android 4.4 (bionic) sin el NDK:
@ el enlazador /system/bin/linker salta a _start con la pila de argumentos,
@ y se la pasamos a __libc_init(raw_args, onexit, main, structors).
    .text
    .arm
    .global _start
_start:
    mov     r0, sp
    mov     r1, #0
    ldr     r2, =main
    ldr     r3, =estructores
    bl      __libc_init
    b       .

    .data
    .align 2
estructores:            @ structors_array_t: preinit, init, fini (vacios)
    .word 0, 0, 0
