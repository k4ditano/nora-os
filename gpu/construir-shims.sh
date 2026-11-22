#!/bin/sh
# Compila las librerias de relleno contra el bionic de la consola (bionic/),
# con hash sysv porque el enlazador de Android 4.4 no entiende gnu hash.
set -e
cd "$(dirname "$0")"
TC=~/rg35xx-kernel/armv7-eabihf--glibc--stable/bin/arm-linux-gcc
mkdir -p salida
FL="-O2 -fPIC -shared -nostdlib -fno-stack-protector -mfloat-abi=softfp -mfpu=neon -marm -Wl,--hash-style=sysv -Wl,-z,noexecstack -Lbionic"
$TC $FL -Wl,-soname,libhardware.so -o salida/libhardware.so shims/libhardware.c -lc -ldl -llog
$TC $FL -Wl,-soname,libcrypto.so -o salida/libcrypto.so shims/libcrypto.c -lc
$TC $FL -Wl,-soname,libnoralog.so -o salida/libnoralog.so shims/noralog.c -lc
$TC $FL -Wl,-soname,libsync.so -o salida/libsync.so shims/libsync.c -lc
$TC $FL -Wl,-soname,libunwind.so -o salida/libunwind.so shims/libunwind.c
ls -l salida
