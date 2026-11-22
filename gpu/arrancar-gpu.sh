#!/system/bin/sh
# Arranca la GPU (SGX544) de la RG35XX. Hay que correrlo en la raiz REAL (la
# de Android, con /system/bin/linker), no en la de las apps de GarlicOS:
#   chroot /proc/1/root /system/bin/sh /mnt/mmc/nora/gpu/arrancar.sh
# Orden importante:
#   1. despertar_gpu.ko marca el nodo del device tree como "okay" (en memoria)
#   2. pvrsrvkm.ko la sondea e inicializa      (NUNCA descargarlo: su salida peta)
#   3. pvrsrvctl --start inicializa los servicios desde espacio de usuario
#   4. dc_owl.ko registra la pantalla en la GPU (para gralloc y las ventanas)
# Luego los programas GL se lanzan con:
#   LD_LIBRARY_PATH=/tmp/gpu/lib NORA_HW=/tmp/gpu/hw \
#   LD_PRELOAD="/tmp/gpu/lib/libnoralog.so /tmp/gpu/lib/libhardware.so" programa
D=/mnt/mmc/nora/gpu
LOG=$D/arrancar.log
: > $LOG
log() { echo "$*" >> $LOG; sync; }
if busybox grep -q "^pvrsrvkm " /proc/modules; then
    log "pvrsrvkm ya estaba cargado"
else
    insmod $D/despertar_gpu.ko >> $LOG 2>&1; log "despertar=$?"
    insmod /misc/modules/pvrsrvkm.ko >> $LOG 2>&1; log "pvrsrvkm=$?"
fi
mkdir -p /tmp/gpu && cp -r $D/lib $D/hw $D/bin /tmp/gpu/ && chmod 755 /tmp/gpu/bin/*
# las variables SOLO para pvrsrvctl: a los programas de glibc les rompe el LD_PRELOAD
LD_LIBRARY_PATH=/tmp/gpu/lib NORA_HW=/tmp/gpu/hw \
LD_PRELOAD="/tmp/gpu/lib/libnoralog.so /tmp/gpu/lib/libhardware.so" \
    /tmp/gpu/bin/pvrsrvctl --start --no-module >> $LOG 2>&1 &
P=$!
sleep 3
kill $P 2>/dev/null && log "pvrsrvctl seguia vivo a los 3 s (reintentando): FALLO" || log "pvrsrvctl termino"
# 4. el display class, SIEMPRE despues de pvrsrvctl (antes = oops del kernel).
#    Sin el, gralloc dice "No devices" y EGL no arranca.
if busybox grep -q "^dc_owl " /proc/modules; then
    log "dc_owl ya estaba cargado"
else
    insmod /misc/modules/dc_owl.ko >> $LOG 2>&1; log "dc_owl=$?"
fi
busybox dmesg | busybox grep -iE "despertar|pvr|sgx|GPU Init" | busybox tail -12 >> $LOG
cat /proc/pvr/version >> $LOG 2>&1
sync
