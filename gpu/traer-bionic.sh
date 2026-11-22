#!/bin/sh
# Trae de la consola su base de Android (bionic) para enlazar contra ella.
# Tienen que ser ESTAS (las del Android 4.4 de la consola), no las del NDK.
#   ./traer-bionic.sh [ip]
set -e
cd "$(dirname "$0")"
IP=${1:-192.168.7.2}
mkdir -p bionic
python3 - "$IP" <<'PY'
import sys, socket, time, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath('.')), 'herramientas'))
import consola
ip = sys.argv[1]
R = "/tmp/busybox chroot /proc/1/root /system/bin/sh -c"
for f in ['lib/libc.so', 'lib/libdl.so', 'lib/libcutils.so', 'lib/liblog.so', 'lib/libm.so', 'lib/libstdc++.so', 'bin/linker']:
    consola.run(ip, f"{R} 'busybox nc -l -p 9004 < /system/{f}' & sleep 1; echo")
    time.sleep(1)
    s = socket.create_connection((ip, 9004), timeout=5); d = b''
    while True:
        x = s.recv(65536)
        if not x: break
        d += x
    open('bionic/' + os.path.basename(f), 'wb').write(d); print(f, len(d))
PY
for l in bionic/*.so; do readelf --dyn-syms -W $l | awk '$7!="UND" && $8!=""{print $8}' | sed 's/@.*//'; done | sort -u > bionic/exporta.txt
