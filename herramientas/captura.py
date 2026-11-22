#!/usr/bin/env python3
"""Captura lo que se ve en la pantalla de la RG35XX (framebuffer RGB565) a PNG.
Lee la pagina visible (/sys/class/graphics/fb0/pan), porque Nora cambia entre
tres paginas.     python3 captura.py salida.png [ip]"""
import socket, sys, os, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import consola
from PIL import Image
out = sys.argv[1]; ip = sys.argv[2] if len(sys.argv) > 2 else "192.168.7.2"
R = "/tmp/busybox chroot /proc/1/root /system/bin/sh -c"
orden = ("Y=$(busybox cut -d, -f2 /sys/class/graphics/fb0/pan); "
         "busybox dd if=/dev/fb0 bs=1280 skip=$Y count=480 2>/dev/null | busybox nc -l -p 9002")
consola.run(ip, f"{R} '{orden}' & sleep 1; echo")
time.sleep(1)
s = socket.create_connection((ip, 9002), timeout=5); data = b""
while len(data) < 640 * 480 * 2:
    d = s.recv(65536)
    if not d: break
    data += d
s.close()
Image.frombytes("RGB", (640, 480), data[:640*480*2], "raw", "BGR;16").save(out)
print(out, len(data))
