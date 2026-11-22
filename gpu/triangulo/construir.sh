#!/bin/sh
# Compila un programa de prueba como ejecutable de bionic (Android 4.4), softfp.
#   ./construir.sh [triangulo|ventana]
set -e
cd "$(dirname "$0")"
P=${1:-triangulo}
# cabeceras de EGL/GLES2 (Khronos) del sistema, y el stub que pide glibc en softfp
if [ ! -d include/EGL ]; then
    mkdir -p include/gnu && cp -r /usr/include/EGL /usr/include/GLES2 /usr/include/KHR include/
    echo "/* vacio: compilamos softfp para bionic */" > include/gnu/stubs-soft.h
fi
TC=~/rg35xx-kernel/armv7-eabihf--glibc--stable/bin
$TC/arm-linux-gcc -O2 -marm -mfloat-abi=softfp -mfpu=neon -nostdlib -fno-stack-protector -Iinclude \
  -o $P arranque.S $P.c \
  -Wl,--dynamic-linker=/system/bin/linker -Wl,--hash-style=sysv -Wl,-z,noexecstack -Wl,--allow-shlib-undefined \
  -L../bionic -L../blobs-guitar/vendor/lib/egl -L../salida \
  -Wl,-rpath-link,../bionic:../blobs-guitar/vendor/lib:../salida \
  -l:libEGL_POWERVR_SGX544_115.so -l:libGLESv2_POWERVR_SGX544_115.so -l:libhardware.so -lc -lm -ldl \
  $($TC/arm-linux-gcc -print-libgcc-file-name) 2>&1 | grep -v "uses VFP register arguments" || true
