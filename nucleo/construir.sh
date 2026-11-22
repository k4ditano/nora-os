#!/bin/sh
# Compila el nucleo de Nora: ejecutable de Android 4.4 (nora-gcc) con LuaJIT
# estatico y las librerias GL de la GPU (las de la LeMaker Guitar).
set -e
cd "$(dirname "$0")"
R=..
[ -f atlas.h ] || python3 atlas.py
[ -f $R/terceros/luajit/src/libluajit.a ] || $R/terceros/construir-luajit.sh
[ -d $R/gpu/triangulo/include/EGL ] || { mkdir -p $R/gpu/triangulo/include; cp -r /usr/include/EGL /usr/include/GLES2 /usr/include/KHR $R/gpu/triangulo/include/; }
$R/bin/nora-gcc -O2 -Wall -Wno-unused-parameter -o nora nora.c ventana.c pinta.c sonido.c enlace.c imagenes.c \
    -I$R/gpu/triangulo/include -I$R/terceros/luajit/src -I$R/terceros/cabeceras \
    -L$R/gpu/blobs-guitar/vendor/lib/egl -L$R/gpu/salida \
    -Wl,-rpath-link,$R/gpu/blobs-guitar/vendor/lib:$R/gpu/salida \
    $R/terceros/luajit/src/libluajit.a \
    -l:libEGL_POWERVR_SGX544_115.so -l:libGLESv2_POWERVR_SGX544_115.so -l:libhardware.so -lm 2>&1 \
    | grep -v "uses VFP register arguments" || true
ls -l nora
