#!/bin/sh
# Actualiza Nora en la tarjeta (nucleo, apps, lib) y la reinicia, sin
# reiniciar la consola. Para el dia a dia; la primera vez, arranque/instalar.sh.
#   arranque/actualizar.sh [ip]          todo
#   SOLO_APPS=1 arranque/actualizar.sh   sin subir el nucleo
set -e
cd "$(dirname "$0")/.."
IP=${1:-192.168.7.2}
H=herramientas
N=/mnt/mmc/nora
B=$N/bin/busybox          # Android no trae pidof, tail...
sube() { python3 $H/subir-real.py "$1" "$2" $IP | grep -v ^OK || true; }
orden() { python3 $H/consola.py $IP "$1" | grep -v "___F\|<\|^root@" || true; }

dirs="$N/lib"
for a in apps/*/; do dirs="$dirs $N/apps/$(basename $a)"; done
orden "mkdir -p $dirs"
for a in apps/*/; do
    for f in $a*.lua; do sube $f $N/apps/$(basename $a)/$(basename $f); done
done
for f in lib/*.lua; do sube $f $N/lib/$(basename $f); done
for f in sonidos/*.wav; do                     # la musica de menu pesa y no cambia: solo si falta
    [ "$(basename $f)" = musica-menu.wav ] && continue
    sube $f $N/sonidos/$(basename $f)
done
[ -n "$SOLO_APPS" ] || sube nucleo/nora $N/bin/nora

# parar la Nora que corre (SIGTERM: sale limpia y devuelve el menu de GarlicOS)
# y abrir la nueva, suelta de telnet, con el mismo entorno que arranque.sh
orden "kill \$($B pidof nora) 2>/dev/null; sleep 1; kill -9 \$($B pidof nora) 2>/dev/null; true"
orden "$N/bin/soltar /system/bin/sh -c 'export NORA_APPS=$N/apps NORA_SONIDOS=$N/sonidos NORA_SUENA=$N/bin/suena TZ=CET-1CEST,M3.5.0,M10.5.0/3 LD_LIBRARY_PATH=/tmp/gpu/lib NORA_HW=/tmp/gpu/hw LD_PRELOAD=\"/tmp/gpu/lib/libnoralog.so /tmp/gpu/lib/libhardware.so\"; cp $N/bin/nora /tmp/gpu/bin/nora; chmod 755 /tmp/gpu/bin/nora; exec /tmp/gpu/bin/nora inicio > $N/nora.log 2>&1'"
sleep 2
orden "$B pidof nora >/dev/null && echo 'Nora reiniciada' || { echo 'Nora no arranca:'; $B tail -5 $N/nora.log; }"
