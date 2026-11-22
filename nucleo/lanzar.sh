#!/system/bin/sh
# Lanza el nucleo en la consola (raiz real), con la GPU ya arrancada (gpu/arrancar.sh).
#   lanzar.sh carpeta-de-app [segundos]   (sin segundos: hasta SELECT+START)
APP=${1:-inicio}
export NORA_APPS=/tmp/nora/apps
export TZ=${TZ:-CET-1CEST,M3.5.0,M10.5.0/3}   # hora de Madrid (la consola trae la de China)
export LD_LIBRARY_PATH=/tmp/gpu/lib NORA_HW=/tmp/gpu/hw NORA_SUENA=/tmp/nora/suena NORA_SONIDOS=/tmp/nora/sonidos
export LD_PRELOAD="/tmp/gpu/lib/libnoralog.so /tmp/gpu/lib/libhardware.so"
if [ -n "$2" ]; then
    /tmp/gpu/bin/nora "$APP" > /tmp/nora.log 2>&1 &
    P=$!
    sleep $2
    kill $P
    sleep 1
else
    exec /tmp/gpu/bin/nora "$APP"
fi
