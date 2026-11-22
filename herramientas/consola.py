#!/usr/bin/env python3
"""Ejecuta una orden en la consola por telnet (busybox telnetd sin login).

    python3 consola.py 192.168.7.2 "ifconfig"
    python3 consola.py 192.168.7.2 "cat /proc/modules"
Si se omite la IP, la lee de ~/.rg35xx-ip
"""
import socket, sys, os, time

def run(ip, cmd, timeout=12, enc="utf-8"):
    s = socket.create_connection((ip, 23), timeout=6)
    s.settimeout(1.5)
    buf = b""
    # tragar la negociacion telnet inicial
    t0 = time.time()
    while time.time() - t0 < 2:
        try:
            d = s.recv(4096)
            if not d: break
            buf += d
        except socket.timeout:
            break
    # responder IAC WONT/DONT a todo para que no se quede esperando
    resp = bytearray()
    i = 0
    while i < len(buf):
        if buf[i] == 255 and i + 2 < len(buf):
            op, opt = buf[i+1], buf[i+2]
            if op in (251, 252):   resp += bytes([255, 254, opt])
            elif op in (253, 254): resp += bytes([255, 252, opt])
            i += 3
        else:
            i += 1
    if resp: s.sendall(bytes(resp))

    marca = "___FIN___"
    s.sendall((cmd + "; echo ___F''IN___\n").encode(enc, "replace"))
    out = b""
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            d = s.recv(4096)
            if not d: break
            out += d
            if marca.encode() in out: break
        except socket.timeout:
            continue
    s.close()
    txt = out.decode("utf-8", "replace")
    # limpiar eco del comando y la marca
    lineas = [l for l in txt.splitlines() if marca not in l and cmd not in l]
    return "\n".join(lineas).strip()

if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0].count(".") == 3:
        ip, cmd = a[0], " ".join(a[1:])
    else:
        p = os.path.expanduser("~/.rg35xx-ip")
        if not os.path.exists(p): sys.exit("no se la IP: pasala o guarda ~/.rg35xx-ip")
        ip, cmd = open(p).read().strip(), " ".join(a)
    if not cmd: sys.exit("falta la orden")
    print(run(ip, cmd))
