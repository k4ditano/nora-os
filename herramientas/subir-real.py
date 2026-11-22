#!/usr/bin/env python3
"""Como subir.py pero escribe en la raiz REAL de la consola (la de Android),
desde el telnet de USB-Red, que vive en la raiz de las apps de GarlicOS.
    python3 subir-real.py local /ruta/en/consola [ip]"""
import socket, sys, os, time, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import consola
src, dst = sys.argv[1], sys.argv[2]
ip = sys.argv[3] if len(sys.argv) > 3 else "192.168.7.2"
R = "/tmp/busybox chroot /proc/1/root /system/bin/sh -c"
data = open(src, "rb").read()
consola.run(ip, f"{R} 'busybox nc -l -p 9000 > {dst}' & sleep 1; echo")
time.sleep(1)
s = socket.create_connection((ip, 9000), timeout=5)
s.sendall(data); s.shutdown(socket.SHUT_WR); time.sleep(1); s.close(); time.sleep(0.5)
out = consola.run(ip, f"{R} 'chmod 755 {dst}; busybox md5sum {dst}'")
remoto = next((l.split()[0] for l in out.splitlines() if l.strip().endswith(dst) and len(l.split()[0]) == 32), "?")
print("OK" if remoto == hashlib.md5(data).hexdigest() else "DISTINTO", remoto)
