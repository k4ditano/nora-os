#!/bin/sh
# Hace que Nora arranque sola al encender, sin GarlicOS: pone arranque/dmenu-nora.sh
# como /misc/dmenu.bin (guardando el de GarlicOS en /misc/dmenu-garlic.bin) y quita
# el autostart de GarlicOS. Se deshace con --quitar. Por la red del cable USB (telnet).
#   arranque/arranque-propio.sh [--quitar] [ip]
set -e
cd "$(dirname "$0")/.."
QUITAR=; [ "$1" = --quitar ] && { QUITAR=1; shift; }
IP=${1:-192.168.7.2}
H=herramientas
N=/mnt/mmc/nora
B=$N/bin/busybox
sube() { python3 $H/subir-real.py "$1" "$2" $IP | grep -v ^OK || true; }
orden() { python3 $H/consola.py $IP "$1" | grep -v "___F\|<\|^root@" | grep -v "^$" || true; }

if [ -n "$QUITAR" ]; then
    orden "mount -o remount,rw /misc && [ -f /misc/dmenu-garlic.bin ] && cp /misc/dmenu-garlic.bin /misc/dmenu.bin && rm /misc/dmenu-garlic.bin && echo 'dmenu.bin de GarlicOS repuesto'; [ -f $N/autostart-garlic.sh ] && mv $N/autostart-garlic.sh /mnt/mmc/CFW/autostart/nora.sh && echo 'autostart de GarlicOS repuesto'; sync; mount -o remount,ro /misc"
    echo "listo: al encender arranca GarlicOS (y Nora desde su autostart)"
    exit 0
fi

sube arranque/arranque.sh $N/arranque.sh
sube arranque/dmenu-nora.sh $N/dmenu-nora.sh
# guardar el de GarlicOS (solo si el que hay no es ya el nuestro), y poner el nuestro
orden "mount -o remount,rw /misc && if $B grep -q 'Nora OS' /misc/dmenu.bin; then echo 'ya era el de Nora'; elif [ -f /misc/dmenu-garlic.bin ]; then echo 'OJO: ya hay dmenu-garlic.bin y dmenu.bin no es de Nora: no toco nada'; exit 1; else cp /misc/dmenu.bin /misc/dmenu-garlic.bin && echo 'dmenu.bin de GarlicOS guardado en dmenu-garlic.bin'; fi"
orden "$B cmp -s /misc/dmenu.bin /misc/dmenu-garlic.bin && echo 'copia comprobada' || $B grep -q 'Nora OS' /misc/dmenu.bin && echo 'copia ya hecha antes'"
orden "/system/bin/sh -n $N/dmenu-nora.sh && cp $N/dmenu-nora.sh /misc/dmenu.bin && chmod 755 /misc/dmenu.bin && echo 'dmenu.bin de Nora puesto'; [ -f /mnt/mmc/CFW/autostart/nora.sh ] && mv /mnt/mmc/CFW/autostart/nora.sh $N/autostart-garlic.sh && echo 'autostart de GarlicOS quitado'; sync; mount -o remount,ro /misc"
orden "$B head -3 /misc/dmenu.bin; ls -la /misc/dmenu*"
echo "listo: al encender arranca Nora sola (SELECT = GarlicOS). Deshacer: $0 --quitar"
