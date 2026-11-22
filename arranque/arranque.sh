#!/system/bin/sh
# Nora OS: el arranque de verdad, en la raiz REAL (la de Android). Lo lanza
# /misc/dmenu.bin (arranque propio, NORA_MODO=propio) o, encima de GarlicOS,
# CFW/autostart/nora.sh. Todo lo que pasa va a /mnt/mmc/nora/arranque.log.
#
# Al salir Nora: 10 = apagar, 11 = reiniciar (se hace aqui, ordenado); 12 = a
# GarlicOS (vuelve quien nos lanzo). Si se cae despues de funcionar un rato y
# Nora arranca sola, se vuelve a abrir.
N=/mnt/mmc/nora
echo "-- $(date) arranque.sh en la raiz real (uptime $(busybox cut -d' ' -f1 /proc/uptime) s, modo ${NORA_MODO:-garlic})"

if [ ! -f /tmp/nora-preparada ]; then        # una vez por encendido (/tmp se borra al apagar)
# 0. pantalla de carga al instante: el logo en crudo RGB565, mientras se levanta la GPU
busybox fbset -g 640 480 640 1440 16 2>/dev/null
busybox dd if=$N/arranque.565 of=/dev/fb0 bs=1280 count=480 2>/dev/null

# 1. la GPU (despertar -> pvrsrvkm -> pvrsrvctl -> dc_owl)
sh $N/gpu/arrancar.sh
busybox tail -4 $N/gpu/arrancar.log

# 2. red por el cable USB con telnet (solo si existe $N/red-usb: deja el
#    puerto en modo dispositivo, asi que el pincho Wi-Fi no tendra corriente)
if [ -f $N/red-usb ]; then
  (
    A=/sys/class/android_usb/android0
    M=/sys/monitor/usb_port/config
    echo 0 > $M/run; echo 1 > $M/idpin_debug
    echo 0 > $A/enable; echo 10d6 > $A/idVendor; echo 0c02 > $A/idProduct
    echo Nora > $A/iProduct; echo rndis > $A/functions; echo 1 > $A/enable
    echo USB_A_OUT > $M/usb_con_msg; sleep 1; echo USB_B_IN > $M/usb_con_msg
    sleep 2
    ifconfig rndis0 192.168.7.2 netmask 255.255.255.0 up
    cp $N/bin/busybox /tmp/busybox 2>/dev/null; chmod 755 /tmp/busybox
    $N/bin/soltar /system/bin/busybox telnetd -F -l /system/bin/sh
    echo "red USB lista: $(ifconfig rndis0 | busybox grep -o 'inet addr:[0-9.]*'), telnet"
  ) &      # en segundo plano: Nora no espera a la red
fi
touch /tmp/nora-preparada
fi

# 3. Nora
export NORA_APPS=$N/apps NORA_SONIDOS=$N/sonidos NORA_SUENA=$N/bin/suena
export TZ=CET-1CEST,M3.5.0,M10.5.0/3
export LD_LIBRARY_PATH=/tmp/gpu/lib NORA_HW=/tmp/gpu/hw
export LD_PRELOAD="/tmp/gpu/lib/libnoralog.so /tmp/gpu/lib/libhardware.so"
cp $N/bin/nora /tmp/gpu/bin/nora && chmod 755 /tmp/gpu/bin/nora
# arrancar a tope; luego, a demanda (la bateria lo agradece)
echo interactive > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null
while true; do
  echo "abro Nora (uptime $(busybox cut -d' ' -f1 /proc/uptime) s)"
  [ -f $N/nora.log ] && mv $N/nora.log $N/nora.log.1
  T0=$(busybox date +%s)
  /tmp/gpu/bin/nora inicio > $N/nora.log 2>&1 &
  NP=$!
  # a los 20 s funcionando, el arranque ha ido bien (aunque luego se apague a lo bruto)
  ( sleep 20; [ -d /proc/$NP ] && { echo 0 > $N/fallos; sync; } ) &
  wait $NP
  R=$?
  T1=$(busybox date +%s)
  echo "nora salio con $R tras $((T1 - T0)) s"
  # si ha durado mas de 20 s, o ha salido porque se le ha pedido, ha ido bien
  case $R in 0|10|11|12) echo 0 > $N/fallos ;; *) [ $((T1 - T0)) -gt 20 ] && echo 0 > $N/fallos ;; esac
  sync
  case $R in
    10) echo "apagando"; sync; reboot -p || busybox poweroff -f; sleep 30 ;;
    11) echo "reiniciando"; sync; reboot || busybox reboot -f; sleep 30 ;;
    0|12) exit $R ;;
    *) if [ "$NORA_MODO" = propio ] && [ $((T1 - T0)) -gt 20 ]; then
         echo "Nora se ha caido tras funcionar un rato: se vuelve a abrir"
         busybox tail -5 $N/nora.log
         continue
       fi
       exit $R ;;
  esac
done
