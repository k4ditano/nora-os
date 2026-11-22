#!/usr/bin/env python3
"""Crea una tarjeta (o una imagen) de Nora OS para la RG35XX original, sin GarlicOS.

Parte de la imagen OFICIAL de Anbernic para la RG35XX (la bajas tú: no se puede
redistribuir), la graba, agranda la partición de juegos al tamaño de la tarjeta
y le pone Nora encima:

  - /misc/dmenu.bin (el guion que lanza el sistema de Anbernic) pasa a ser el
    arranque de Nora; el menú de serie queda en /misc/dmenu-anbernic.bin, de
    reserva (SELECT al encender, o Energía → menú de Anbernic)
  - el logo de arranque de Nora (el de Anbernic se guarda al lado)
  - /mnt/mmc/nora: el núcleo, las apps, la librería, los temas, los sonidos,
    las librerías de la GPU (de la LeMaker Guitar; gpu/bajar-blobs.sh) y busybox

Uso (desde el repo de Nora OS, con el núcleo compilado: nucleo/construir.sh):

  sudo python3 herramientas/crear-tarjeta.py --imagen RG35XX-oficial.img.xz --tarjeta /dev/sdX
  python3 herramientas/crear-tarjeta.py --imagen RG35XX-oficial.img --salida nora-os.img [--tamano 16G]

Con --tarjeta BORRA esa tarjeta entera (pide escribir su nombre para seguir).
Con --salida hace un archivo .img para grabarlo con cualquier programa (Etcher,
Raspberry Pi Imager, dd); --tamano lo hace de ese tamaño (si no, el de la imagen).

Las particiones FAT se tocan con mtools, sin montarlas.
"""
import argparse, gzip, json, lzma, os, shutil, subprocess, sys, tempfile, urllib.request, zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUSYBOX_URL = "https://busybox.net/downloads/binaries/1.31.0-defconfig-multiarch-musl/busybox-armv7l"
ENV = dict(os.environ, MTOOLS_SKIP_CHECK="1", MTOOLS_NO_VFAT="0")


def sale(msg):
    print(f"crear-tarjeta: {msg}", file=sys.stderr)
    sys.exit(1)


def corre(*args, entrada=None, capturar=False):
    r = subprocess.run(args, input=entrada, text=True, env=ENV,
                       stdout=subprocess.PIPE if capturar else None, stderr=subprocess.PIPE)
    if r.returncode != 0:
        sale(f"{' '.join(args)}: {r.stderr.strip()}")
    return r.stdout


def tamano(s):
    s = s.strip().upper()
    mult = {"K": 1 << 10, "M": 1 << 20, "G": 1 << 30, "T": 1 << 40}
    return int(float(s[:-1]) * mult[s[-1]]) if s[-1] in mult else int(s)


# ---------------------------------------------------------------- la imagen de Anbernic
def descomprimir(origen, tmp):
    """La imagen, descomprimida si hace falta (.gz, .xz, .zip)."""
    o = origen.lower()
    if o.endswith(".img"):
        return origen
    destino = os.path.join(tmp, "anbernic.img")
    print(f"descomprimiendo {os.path.basename(origen)}…")
    if o.endswith(".gz"):
        with gzip.open(origen, "rb") as f, open(destino, "wb") as g:
            shutil.copyfileobj(f, g, 1 << 22)
    elif o.endswith(".xz"):
        with lzma.open(origen, "rb") as f, open(destino, "wb") as g:
            shutil.copyfileobj(f, g, 1 << 22)
    elif o.endswith(".zip"):
        with zipfile.ZipFile(origen) as z:
            imgs = [n for n in z.namelist() if n.lower().endswith(".img")]
            if not imgs:
                sale("el .zip no trae ningún .img")
            with z.open(imgs[0]) as f, open(destino, "wb") as g:
                shutil.copyfileobj(f, g, 1 << 22)
    else:
        sale("formato no reconocido (vale .img, .img.gz, .img.xz o .zip; un .7z, descomprímelo antes)")
    return destino


def particiones(dispositivo):
    t = json.loads(corre("sfdisk", "-J", dispositivo, capturar=True))["partitiontable"]
    return t["partitions"], t.get("sectorsize", 512)


def fat(dispositivo, p, sector):
    """La partición p como la entiende mtools: archivo@@desplazamiento."""
    return f"{dispositivo}@@{p['start'] * sector}"


def lista_fat(img):
    return corre("mdir", "-b", "-i", img, "::/", capturar=True).upper()


def comprobar_anbernic(img_stock):
    partes, sector = particiones(img_stock)
    if len(partes) < 4:
        sale(f"la imagen tiene {len(partes)} particiones; la de la RG35XX tiene 4 (¿es la imagen oficial?)")
    misc = lista_fat(fat(img_stock, partes[0], sector))
    for f in ("UIMAGE", "DMENU.BIN", "RAMDISK.IMG"):
        if f not in misc:
            sale(f"la primera partición no tiene {f}: no parece la imagen de la RG35XX original")
    print(f"imagen de Anbernic reconocida: {len(partes)} particiones, /misc con uImage y dmenu.bin")
    return partes, sector


# ---------------------------------------------------------------- el destino
def comprobar_tarjeta(dev):
    info = json.loads(corre("lsblk", "-J", "-b", "-o", "NAME,RM,TRAN,SIZE,MODEL,MOUNTPOINTS,TYPE", dev,
                            capturar=True))["blockdevices"][0]
    if info.get("type") != "disk":
        sale(f"{dev} no es un disco entero (usa /dev/sdX, no /dev/sdX1)")
    montajes = []
    def recorre(n):
        montajes.extend(m for m in (n.get("mountpoints") or []) if m)
        for h in n.get("children") or []:
            recorre(h)
    recorre(info)
    for m in montajes:
        if m in ("/", "/boot", "/home", "/usr", "/var") or m.startswith("/boot"):
            sale(f"{dev} tiene montado {m}: eso es un disco del sistema, no la tarjeta")
    if not (info.get("rm") or info.get("tran") in ("usb", "mmc", "sd")):
        sale(f"{dev} no parece una tarjeta ni un disco USB (rm={info.get('rm')}, tran={info.get('tran')})")
    gb = int(info["size"]) / 1e9
    print(f"\n  {dev}: {info.get('model') or 'tarjeta'} de {gb:.1f} GB")
    print(f"  SE VA A BORRAR ENTERA. Para seguir, escribe {os.path.basename(dev)}: ", end="", flush=True)
    if input().strip() != os.path.basename(dev):
        sale("cancelado")
    for m in montajes:
        subprocess.run(["umount", m])
    return int(info["size"])


def copiar(origen, destino, total):
    print(f"grabando {total // (1 << 20)} MB en {destino}…")
    hecho = 0
    with open(origen, "rb") as f, open(destino, "r+b" if destino.startswith("/dev/") else "wb") as g:
        while True:
            b = f.read(1 << 22)
            if not b:
                break
            g.write(b)
            hecho += len(b)
            print(f"\r  {hecho * 100 // total} %", end="", flush=True)
        g.flush()
        os.fsync(g.fileno())
    print()


def agrandar_juegos(dest, es_tarjeta, sector, tmp):
    """La 4ª partición (juegos, FAT) hasta el final, conservando lo que tenía."""
    partes, _ = particiones(dest)
    p4 = partes[3]
    guardado = os.path.join(tmp, "p4")
    os.makedirs(guardado)
    img = fat(dest, p4, sector)
    if lista_fat(img).strip():
        corre("mcopy", "-s", "-n", "-i", img, "::/*", guardado)
    corre("sfdisk", "-q", "--no-reread", "--no-tell-kernel", "-N", "4", dest, entrada=", +\n", capturar=True)
    if es_tarjeta:
        subprocess.run(["blockdev", "--rereadpt", dest])
    p4 = particiones(dest)[0][3]
    kib = p4["size"] * sector // 1024
    corre("mkfs.vfat", "-F", "32", "-n", "NORA", f"--offset={p4['start']}", dest, str(kib))
    img = fat(dest, p4, sector)
    if os.listdir(guardado):
        corre("mcopy", "-s", "-o", "-i", img, *[os.path.join(guardado, n) for n in os.listdir(guardado)], "::/")
    print(f"partición de juegos: {p4['size'] * sector / 1e9:.1f} GB (FAT32, «NORA»)")
    return p4


# ---------------------------------------------------------------- Nora
def archivos_nora():
    """(archivo en el PC, ruta dentro de /mnt/mmc/nora)"""
    l = []
    def anade(de, a, obligatorio=False):
        r = os.path.join(REPO, de)
        if os.path.isfile(r):
            l.append((r, a))
        elif obligatorio:
            sale(f"falta {de} ({'nucleo/construir.sh' if 'nucleo' in de else 'mira el README'})")
    anade("nucleo/nora", "bin/nora", True)
    anade("arranque/pulsada", "bin/pulsada", True)
    anade("arranque/soltar", "bin/soltar", True)
    anade("herramientas/pulsar", "bin/pulsar")
    anade("arranque/arranque.sh", "arranque.sh", True)
    anade("arranque/dmenu-nora.sh", "dmenu-nora.sh", True)
    anade("arranque/logo/arranque.565", "arranque.565", True)
    git = subprocess.run(["git", "-C", REPO, "ls-files", "apps", "lib", "temas", "sonidos"],
                         capture_output=True, text=True).stdout.split()
    for r in git:
        if r.startswith("sonidos/") and (not r.endswith(".wav") or r.count("/") > 1):
            continue
        l.append((os.path.join(REPO, r), r))
    # la GPU: librerías de la LeMaker Guitar (gpu/bajar-blobs.sh) y las de relleno (gpu/construir-shims.sh)
    G = os.path.join(REPO, "gpu")
    blobs = os.path.join(G, "blobs-guitar", "vendor")
    if not os.path.isdir(blobs):
        sale("faltan las librerías de la GPU: gpu/bajar-blobs.sh")
    for d, a in ((os.path.join(blobs, "lib"), "gpu/lib"), (os.path.join(blobs, "lib", "egl"), "gpu/lib"),
                 (os.path.join(G, "salida"), "gpu/lib"), (os.path.join(blobs, "lib", "hw"), "gpu/hw")):
        for f in sorted(os.listdir(d)) if os.path.isdir(d) else []:
            if f.endswith(".so"):
                l.append((os.path.join(d, f), f"{a}/{f}"))
    for de, a in (("blobs-guitar/vendor/bin/pvrsrvctl", "gpu/bin/pvrsrvctl"),
                  ("despertar/despertar_gpu.ko", "gpu/despertar_gpu.ko"), ("arrancar-gpu.sh", "gpu/arrancar.sh")):
        if not os.path.isfile(os.path.join(G, de)):
            sale(f"falta gpu/{de}")
        l.append((os.path.join(G, de), a))
    return l


def busybox(tmp):
    """busybox estático para ARMv7 (GPL): el de herramientas/ o se baja de busybox.net."""
    local = os.path.join(REPO, "herramientas", "busybox-armv7l")
    if os.path.isfile(local):
        return local
    print(f"bajando busybox de {BUSYBOX_URL}…")
    destino = os.path.join(tmp, "busybox")
    urllib.request.urlretrieve(BUSYBOX_URL, destino)
    shutil.copy(destino, local)        # para la próxima vez
    return destino


def poner_nora(dest, partes, sector, tmp):
    misc = fat(dest, partes[0], sector)
    # el arranque: el menú de serie queda de reserva
    corre("mcopy", "-n", "-i", misc, "::/dmenu.bin", os.path.join(tmp, "dmenu-anbernic.bin"))
    with open(os.path.join(tmp, "dmenu-anbernic.bin"), "rb") as f:
        ya_nora = b"Nora OS" in f.read()
    if not ya_nora:
        if "DMENU-ANBERNIC.BIN" not in lista_fat(misc):
            corre("mren", "-i", misc, "::/dmenu.bin", "::/dmenu-anbernic.bin")
        else:
            corre("mdel", "-i", misc, "::/dmenu.bin")
    corre("mcopy", "-o", "-i", misc, os.path.join(REPO, "arranque/dmenu-nora.sh"), "::/dmenu.bin")
    logo = os.path.join(REPO, "arranque/logo/boot_logo.bmp.gz")
    if os.path.isfile(logo):
        if "BOOT_L~1.GZ" in lista_fat(misc) or "BOOT_LOGO.BMP.GZ" in lista_fat(misc):
            if "BOOT_LOGO-ANBERNIC.BMP.GZ" not in lista_fat(misc):
                corre("mren", "-i", misc, "::/boot_logo.bmp.gz", "::/boot_logo-anbernic.bmp.gz")
        corre("mcopy", "-o", "-i", misc, logo, "::/boot_logo.bmp.gz")
    print("arranque de Nora puesto en /misc (el de Anbernic, en dmenu-anbernic.bin)")

    juegos = fat(dest, particiones(dest)[0][3], sector)
    lista = archivos_nora() + [(busybox(tmp), "bin/busybox")]
    carpetas = sorted({os.path.dirname("nora/" + a) for _, a in lista})
    hechas = set()
    for c in carpetas:
        partes_c = c.split("/")
        for i in range(1, len(partes_c) + 1):
            sub = "/".join(partes_c[:i])
            if sub not in hechas:
                subprocess.run(["mmd", "-D", "s", "-i", juegos, "::/" + sub], env=ENV, capture_output=True)
                hechas.add(sub)
    total = 0
    for i, (de, a) in enumerate(lista):
        corre("mcopy", "-o", "-i", juegos, de, "::/nora/" + a)
        total += os.path.getsize(de)
        print(f"\r  Nora: {i + 1}/{len(lista)} archivos", end="", flush=True)
    print(f"\nNora puesta en /mnt/mmc/nora ({total // (1 << 20)} MB)")


def main():
    ap = argparse.ArgumentParser(description="Crea una tarjeta o imagen de Nora OS (RG35XX original)")
    ap.add_argument("--imagen", required=True, help="la imagen oficial de Anbernic (.img, .img.gz, .img.xz, .zip)")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--tarjeta", help="la tarjeta a grabar (p. ej. /dev/sdb): SE BORRA")
    g.add_argument("--salida", help="hacer un archivo .img en vez de grabar una tarjeta")
    ap.add_argument("--tamano", help="con --salida: tamaño de la imagen (p. ej. 16G); por defecto, el de la de Anbernic")
    a = ap.parse_args()
    for h in ("sfdisk", "mkfs.vfat", "mcopy", "mdir", "mren", "mmd"):
        if not shutil.which(h):
            sale(f"falta {h} (paquetes util-linux, dosfstools y mtools)")
    if a.tarjeta and os.geteuid() != 0:
        sale("para grabar una tarjeta hace falta root: sudo python3 herramientas/crear-tarjeta.py …")
    with tempfile.TemporaryDirectory(prefix="nora-tarjeta-") as tmp:
        stock = descomprimir(a.imagen, tmp)
        partes, sector = comprobar_anbernic(stock)
        tam_stock = os.path.getsize(stock)
        archivos_nora()                       # que no falte nada antes de borrar ninguna tarjeta
        if a.tarjeta:
            cap = comprobar_tarjeta(a.tarjeta)
            if cap < tam_stock:
                sale(f"la tarjeta ({cap / 1e9:.1f} GB) es más pequeña que la imagen ({tam_stock / 1e9:.1f} GB)")
            dest = a.tarjeta
            copiar(stock, dest, tam_stock)
        else:
            dest = a.salida
            copiar(stock, dest, tam_stock)
            if a.tamano:
                t = tamano(a.tamano)
                if t < tam_stock:
                    sale(f"--tamano es menor que la imagen de Anbernic ({tam_stock / 1e9:.1f} GB)")
                with open(dest, "r+b") as f:
                    f.truncate(t)
        agrandar_juegos(dest, bool(a.tarjeta), sector, tmp)
        poner_nora(dest, partes, sector, tmp)
        if a.tarjeta:
            subprocess.run(["sync"])
    print(f"\nlisto: {dest}. Al encender la RG35XX arranca Nora (SELECT al encender: el menú de Anbernic).")
    if a.salida:
        print("grábala con Etcher, Raspberry Pi Imager o dd. La primera vez sale la bienvenida.")


if __name__ == "__main__":
    main()
