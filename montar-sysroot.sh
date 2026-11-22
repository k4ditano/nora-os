#!/bin/sh
# Monta sysroot/: cabeceras de bionic (Android 4.4.2, AOSP) + las librerias de la
# propia consola (gpu/traer-bionic.sh) + el crtbegin de nora. Lo usa bin/nora-gcc.
set -e
cd "$(dirname "$0")"
F=sysroot-fuentes/bionic
if [ ! -d $F/libc ]; then
    git clone -q --depth 1 --filter=blob:none --sparse -b android-4.4.2_r1 \
        https://android.googlesource.com/platform/bionic $F
    git -C $F sparse-checkout set libc/include libc/arch-arm/include libc/kernel/common libc/kernel/arch-arm libm/include
fi
[ -f gpu/bionic/libc.so ] || { echo "faltan las librerias de la consola: gpu/traer-bionic.sh"; exit 1; }
rm -rf sysroot && mkdir -p sysroot/usr/include sysroot/usr/lib
cp -r $F/libc/include/* $F/libc/arch-arm/include/* $F/libc/kernel/common/* $F/libc/kernel/arch-arm/* $F/libm/include/* sysroot/usr/include/
cp -r $F/libm/include/arm/* sysroot/usr/include/
cp gpu/bionic/*.so sysroot/usr/lib/
TC=${NORA_TC:-$HOME/rg35xx-kernel/armv7-eabihf--glibc--stable/bin/arm-linux-gcc}
$TC -c -fPIC -marm -mfloat-abi=softfp -o sysroot/usr/lib/crtbegin.o sysroot-fuentes/crt/crtbegin.S
echo "sysroot listo"
