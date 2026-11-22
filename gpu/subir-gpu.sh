#!/bin/sh
# Sube a la tarjeta de la consola todo lo de la GPU: librerias de la Guitar,
# las de relleno, pvrsrvctl y el modulo que la despierta.
#   ./subir-gpu.sh [ip]     (por defecto 192.168.7.2, el cable)
IP=${1:-192.168.7.2}
G=$(cd "$(dirname "$0")" && pwd)
T=$G/../herramientas
D=/mnt/mmc/nora/gpu
python3 $T/consola.py $IP "mkdir -p $D/lib $D/hw $D/bin" >/dev/null
for f in $G/blobs-guitar/vendor/lib/*.so $G/blobs-guitar/vendor/lib/egl/*.so $G/salida/*.so; do
    python3 $T/subir.py "$f" $D/lib/$(basename "$f") $IP | grep -v ^OK
done
for f in $G/blobs-guitar/vendor/lib/hw/*.so; do python3 $T/subir.py "$f" $D/hw/$(basename "$f") $IP | grep -v ^OK; done
python3 $T/subir.py $G/blobs-guitar/vendor/bin/pvrsrvctl $D/bin/pvrsrvctl $IP | grep -v ^OK
python3 $T/subir.py $G/despertar/despertar_gpu.ko $D/despertar_gpu.ko $IP | grep -v ^OK
python3 $T/consola.py $IP "sync; ls $D/lib | busybox wc -l" | tail -2
