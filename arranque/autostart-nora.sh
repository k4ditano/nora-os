#!/bin/sh
# Nora OS: arranque desde GarlicOS. Va en /mnt/mmc/CFW/autostart/nora.sh
# (GarlicOS ejecuta esa carpeta antes de enseñar su menu).
#
#   - SELECT pulsado al encender        -> GarlicOS, como siempre
#   - Nora ha fallado 3 veces seguidas  -> GarlicOS (y se apunta en el log)
#   - si no: Nora, en la raiz REAL (la de Android, con la GPU). Al salir de
#     Nora, este script termina y GarlicOS sigue con su menu.
#
# Para desinstalar Nora: borrar este fichero. Nada mas.
N=/mnt/mmc/nora
LOG=$N/arranque.log
FALLOS=$N/fallos
B=$N/bin/busybox

[ -x $N/bin/pulsada ] || chmod 755 $N/bin/* 2>/dev/null
echo "== $(date) autostart" > $LOG

if $N/bin/pulsada 314; then
    echo "SELECT pulsado: arranca GarlicOS" >> $LOG
    exit 0
fi
n=$(cat $FALLOS 2>/dev/null || echo 0)
if [ "$n" -ge 3 ]; then
    echo "Nora ha fallado $n veces seguidas: arranca GarlicOS (borra $FALLOS para reintentar)" >> $LOG
    exit 0
fi

# un fallo mas hasta que Nora diga que ha ido bien (arranque.sh lo pone a 0)
echo $((n + 1)) > $FALLOS
sync
$B chroot /proc/1/root /system/bin/sh $N/arranque.sh >> $LOG 2>&1
echo "Nora terminada ($?), sigue GarlicOS" >> $LOG
sync
exit 0
