#!/usr/bin/env python3
"""Sube un archivo a la consola por nc (ella escucha, el PC conecta: no choca con ufw).

    python3 subir.py local /ruta/en/consola [ip]
"""
import socket, sys, os, time, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import consola

src, dst = sys.argv[1], sys.argv[2]
ip = sys.argv[3] if len(sys.argv) > 3 else "192.168.7.2"
data = open(src, "rb").read()
consola.run(ip, f"(busybox nc -l -p 9000 > {dst} &)")
time.sleep(1)
s = socket.create_connection((ip, 9000), timeout=5)
s.sendall(data); s.shutdown(socket.SHUT_WR); time.sleep(1); s.close()
time.sleep(1)
out = consola.run(ip, f"chmod 755 {dst}; busybox md5sum {dst}")
remoto = next((l.split()[0] for l in out.splitlines() if l.endswith(dst) and len(l.split()[0]) == 32), "?")
local = hashlib.md5(data).hexdigest()
print("OK" if remoto == local else "DISTINTO", local, remoto)
