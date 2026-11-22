#!/system/bin/sh
# Nora OS: el arranque propio, sin pasar por GarlicOS.
#
# Es /misc/dmenu.bin: lo ejecuta /system/dmenu/dmenu_ln (de Anbernic) al
# encender. El de GarlicOS queda guardado en /misc/dmenu-garlic.bin y se usa:
#   - con SELECT pulsado al encender
#   - si Nora falla 3 veces seguidas al arrancar (ese encendido; el siguiente lo reintenta)
#   - si Nora lo pide (Energia > Ir a GarlicOS)
# Si tampoco hay GarlicOS: modo rescate (red por el cable USB y telnet).
# Ponerlo: arranque/arranque-propio.sh · quitarlo: arranque/arranque-propio.sh --quitar
N=/mnt/mmc/nora
GARLIC=/misc/dmenu-garlic.bin
LOG=$N/arranque.log
FALLOS=$N/fallos

rescate() {
  echo "modo rescate: red por el cable USB (192.168.7.2) y telnet" >> $LOG; sync
  A=/sys/class/android_usb/android0
  M=/sys/monitor/usb_port/config
  echo 0 > $M/run; echo 1 > $M/idpin_debug
  echo 0 > $A/enable; echo 10d6 > $A/idVendor; echo 0c02 > $A/idProduct
  echo Nora > $A/iProduct; echo rndis > $A/functions; echo 1 > $A/enable
  echo USB_A_OUT > $M/usb_con_msg; sleep 1; echo USB_B_IN > $M/usb_con_msg
  sleep 2
  ifconfig rndis0 192.168.7.2 netmask 255.255.255.0 up
  busybox telnetd -l /system/bin/sh
  while true; do sleep 60; done
}
garlic() {
  echo "$1: arranca GarlicOS" >> $LOG; sync
  [ -f $GARLIC ] && exec /system/bin/sh $GARLIC
  rescate
}

# la tarjeta (loadapp.sh ya la monta; por si acaso)
busybox grep -q " /mnt/mmc " /proc/mounts || mount -t vfat -o rw,utf8,noatime,nodiratime /dev/block/mmcblk0p4 /mnt/mmc
[ -f $N/arranque.sh ] && [ -f $N/bin/nora ] || garlic "no hay Nora en la tarjeta"
echo "== $(date) arranque propio de Nora" > $LOG

# lo de siempre al arrancar (como GarlicOS): todos los nucleos, a tope, tarjetas sin ahorro
echo 0xF > /sys/devices/system/cpu/autoplug/plug_mask
echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo noop > /sys/devices/b0238000.mmc/mmc_host/mmc0/emmc_boot_card/block/mmcblk0/queue/scheduler 2>/dev/null
echo noop > /sys/devices/b0230000.mmc/mmc_host/mmc1/sd_card/block/mmcblk1/queue/scheduler 2>/dev/null
echo on > /sys/devices/b0238000.mmc/mmc_host/mmc0/power/control 2>/dev/null
echo on > /sys/devices/b0230000.mmc/mmc_host/mmc1/power/control 2>/dev/null

# la segunda tarjeta, si la hay
mkdir -p /mnt/SDCARD
D=/dev/block/mmcblk1p1; [ -e $D ] || D=/dev/block/mmcblk1
if [ -e $D ]; then
  mount -t vfat -o rw,utf8,noatime $D /mnt/SDCARD 2>/dev/null || mount -t exfat -o rw,utf8,noatime $D /mnt/SDCARD 2>/dev/null
fi

[ -x $N/bin/pulsada ] || chmod 755 $N/bin/* 2>/dev/null
if $N/bin/pulsada 314; then garlic "SELECT pulsado"; fi

while true; do
  n=$(cat $FALLOS 2>/dev/null || echo 0)
  if [ "$n" -ge 3 ]; then
    echo 0 > $FALLOS      # esta vez GarlicOS; al encender otra vez se vuelve a intentar con Nora
    garlic "Nora ha fallado $n veces seguidas al arrancar (el próximo encendido lo reintenta)"
  fi
  echo $((n + 1)) > $FALLOS; sync
  NORA_MODO=propio /system/bin/sh $N/arranque.sh >> $LOG 2>&1
  R=$?
  echo "arranque.sh termino ($R)" >> $LOG; sync
  [ "$R" = 12 ] && garlic "Nora pide GarlicOS"
  sleep 1
done
