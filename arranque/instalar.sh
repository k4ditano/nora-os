#!/bin/sh
# Instala (o actualiza) Nora en la tarjeta de la consola, por la red USB:
#   /mnt/mmc/nora/{bin,apps,sonidos,arranque.sh,red-usb}  y
#   /mnt/mmc/CFW/autostart/nora.sh  (el enganche: GarlicOS lo ejecuta al arrancar)
# La GPU ya esta en /mnt/mmc/nora/gpu (gpu/subir-gpu.sh).
# Para desinstalar: borrar /mnt/mmc/CFW/autostart/nora.sh.
#   arranque/instalar.sh [ip]
set -e
cd "$(dirname "$0")/.."
IP=${1:-192.168.7.2}
H=herramientas
R="/tmp/busybox chroot /proc/1/root"
N=/mnt/mmc/nora
sube() { python3 $H/subir-real.py "$1" "$2" $IP | grep -v ^OK || true; }
python3 $H/consola.py $IP "$R mkdir -p $N/bin $N/sonidos /mnt/mmc/CFW/autostart" >/dev/null
for a in apps/*/; do
    id=$(basename $a)
    python3 $H/consola.py $IP "$R mkdir -p $N/apps/$id" >/dev/null
    for f in $a*.lua; do sube $f $N/apps/$id/$(basename $f); done
done
for f in sonidos/*.wav; do sube $f $N/sonidos/$(basename $f); done
sube nucleo/nora $N/bin/nora
sube arranque/pulsada $N/bin/pulsada
sube arranque/soltar $N/bin/soltar
# busybox estatico de la propia consola (no va en el repo: lo trae traer-bionic o USB-Red)
sube ${NORA_BUSYBOX:-$HOME/garlicos-fork/usb/busybox-consola} $N/bin/busybox
sube ~/garlicos-fork/cara/suena $N/bin/suena
sube arranque/arranque.sh $N/arranque.sh
sube arranque/autostart-nora.sh /mnt/mmc/CFW/autostart/nora.sh
python3 $H/consola.py $IP "$R /system/bin/sh -c 'touch $N/red-usb; echo 0 > $N/fallos; chmod 755 $N/bin/* $N/arranque.sh /mnt/mmc/CFW/autostart/nora.sh; sync; ls $N $N/bin'" | grep -v "___F\|<"
echo "Nora instalada en la tarjeta ($IP)"
