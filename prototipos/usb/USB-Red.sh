#!/bin/sh
# Pone el USB-C de la RG35XX en modo dispositivo como tarjeta de red (RNDIS)
# y abre telnet en 192.168.7.2. Copiar a Roms/APPS/ ; sale en Apps.
# Todo lo que pasa queda en /mnt/mmc/usb-red.log (en la tarjeta).
LOG=/mnt/mmc/usb-red.log
A=/sys/class/android_usb/android0
M=/sys/monitor/usb_port/config
log() { echo "$*" >> "$LOG"; sync; }
w() { echo "$1" > "$2" 2>>"$LOG" && log "ok  $2 = $1" || log "MAL $2 = $1"; }

: > "$LOG"
log "== usb-red $(date)"
log "-- antes:"; for f in run idpin_debug usb_con_msg; do log "$f: $(cat $M/$f 2>&1)"; done; for f in functions enable state idVendor idProduct; do log "$f: $(cat $A/$f 2>&1)"; done
log "monitor: $(ls $M 2>&1 | tr '\n' ' ')"

# 1. que el monitor no nos devuelva a host, y forzar rol dispositivo
w 0 $M/run
w 1 $M/idpin_debug
[ -x /usbmond.sh ] && /usbmond.sh USB_A_OUT >>"$LOG" 2>&1

# 2. gadget android con solo rndis (adb necesita adbd y puede atascar)
w 0 $A/enable
w 10d6 $A/idVendor
w 0c02 $A/idProduct
w RG35XX $A/iProduct
w rndis $A/functions
w 1 $A/enable
# 2b. lo que faltaba: decirle al chip que pase de host a dispositivo.
#     El init.rc lo fuerza a host con USB_A_IN ("usb3.0 force host").
w USB_A_OUT $M/usb_con_msg
sleep 1
w USB_B_IN $M/usb_con_msg
sleep 3
log "state: $(cat $A/state 2>&1)"

# 3. buscar la interfaz nueva y darle IP
IF=""
for n in rndis0 usb0 eth0; do [ -e /sys/class/net/$n ] && { IF=$n; break; }; done
log "interfaces: $(ls /sys/class/net | tr '\n' ' ')"
if [ -n "$IF" ]; then
    ifconfig $IF 192.168.7.2 netmask 255.255.255.0 up >>"$LOG" 2>&1
    log "IP: $(ifconfig $IF 2>&1 | grep -o 'inet addr:[0-9.]*')"
    # telnetd DESENGANCHADO (GarlicOS mata lo que lanza la app al cerrarla) y
    # con busybox propio: desde las apps no siempre se ve /system/bin.
    log "ve: busybox=$(ls /system/bin/busybox 2>&1) raiz=$(ls -l /proc/self/root 2>&1 | sed 's/.*-> //') PATH=$PATH"
    cp /mnt/mmc/CFW/usb/soltar /mnt/mmc/CFW/usb/busybox /tmp/ 2>>"$LOG"
    chmod 755 /tmp/soltar /tmp/busybox
    # el telnet entra en la raiz REAL (la de Android, con /system/bin/linker),
    # no en la de las apps de GarlicOS; y deja el mismo busybox en la /tmp real
    # para que las herramientas del PC sirvan desde las dos raices
    cp /tmp/busybox /proc/1/root/tmp/busybox 2>>"$LOG"
    printf '#!/tmp/busybox sh\nexec /tmp/busybox chroot /proc/1/root /system/bin/sh\n' > /tmp/entrar
    chmod 755 /tmp/entrar
    /tmp/soltar /tmp/busybox telnetd -F -l /tmp/entrar
    sleep 1
    if /tmp/busybox ps | /tmp/busybox grep -q "[t]elnetd"; then
        log "telnetd en 192.168.7.2:23"
    else
        log "telnetd NO arranco"
    fi
    log "RESULTADO: listo en $IF"
else
    log "RESULTADO: no aparecio interfaz de red"
fi
sync
# 4. ensenarlo en la pantalla de la consola (fbtext, el de WiFi-Info)
mkdir -p /tmp/wifibin
cp /mnt/mmc/CFW/wifi/bin/fbtext /tmp/wifibin/ 2>/dev/null
chmod 755 /tmp/wifibin/fbtext 2>/dev/null
/tmp/wifibin/fbtext "$LOG" 30 >/dev/null 2>&1
