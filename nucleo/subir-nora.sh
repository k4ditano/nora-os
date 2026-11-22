#!/bin/sh
# Sube a la consola el nucleo, las apps y los sonidos (a la RAM: /tmp/nora).
#   nucleo/subir-nora.sh [ip]
set -e
cd "$(dirname "$0")/.."
IP=${1:-192.168.7.2}
H=herramientas
R="/tmp/busybox chroot /proc/1/root"
python3 $H/consola.py $IP "$R mkdir -p /tmp/nora/sonidos /tmp/nora/lib /tmp/gpu/bin" >/dev/null
for a in apps/*/; do
    id=$(basename $a)
    python3 $H/consola.py $IP "$R mkdir -p /tmp/nora/apps/$id" >/dev/null
    for f in $a*.lua; do python3 $H/subir-real.py $f /tmp/nora/apps/$id/$(basename $f) $IP | grep -v ^OK || true; done
done
for f in lib/*.lua; do python3 $H/subir-real.py $f /tmp/nora/lib/$(basename $f) $IP | grep -v ^OK || true; done
for f in sonidos/*.wav; do python3 $H/subir-real.py $f /tmp/nora/sonidos/$(basename $f) $IP | grep -v ^OK || true; done
python3 $H/subir-real.py nucleo/nora /tmp/gpu/bin/nora $IP | grep -v ^OK || true
python3 $H/subir-real.py nucleo/lanzar.sh /tmp/nora/lanzar.sh $IP | grep -v ^OK || true
echo "subido a $IP"
