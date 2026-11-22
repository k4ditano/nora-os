#!/bin/sh
# Baja y compila LuaJIT 2.1 para la consola (Android 4.4, ARMv7 softfp) con
# nora-gcc. buildvm corre en el PC: necesita gcc -m32 (lib32-glibc).
set -e
cd "$(dirname "$0")"
[ -d luajit ] || git clone -q -b v2.1 https://github.com/LuaJIT/LuaJIT.git luajit
TC=$(dirname ${NORA_TC:-$HOME/rg35xx-kernel/armv7-eabihf--glibc--stable/bin/arm-linux-gcc})
G=$(cd .. && pwd)/bin/nora-gcc
make -C luajit -j8 HOST_CC="gcc -m32" CROSS="" CC="$G" TARGET_LD="$G" \
    TARGET_AR="$TC/arm-linux-ar rcus" TARGET_STRIP="$TC/arm-linux-strip" \
    TARGET_SYS=Linux BUILDMODE=static
ls -l luajit/src/luajit luajit/src/libluajit.a
