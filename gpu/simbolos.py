#!/usr/bin/env python3
"""Lista los simbolos dinamicos SIN DEFINIR de un ELF ARM de 32 bits leyendo
PT_DYNAMIC (DT_SYMTAB/DT_STRTAB/DT_HASH), sin fiarse de las cabeceras de
seccion (las libs de Android a veces las traen recortadas)."""
import struct, sys
def und(path):
    b = open(path, 'rb').read()
    phoff, = struct.unpack_from('<I', b, 0x1c); phnum, = struct.unpack_from('<H', b, 0x2c)
    loads, dyn = [], None
    for i in range(phnum):
        t, off, va, pa, fsz, msz, fl, al = struct.unpack_from('<8I', b, phoff + 32 * i)
        if t == 1: loads.append((va, off, fsz))
        if t == 2: dyn = (off, fsz)
    v2o = lambda va: next(off + va - v for v, off, sz in loads if v <= va < v + sz)
    d = {}
    for i in range(0, dyn[1], 8):
        tag, val = struct.unpack_from('<iI', b, dyn[0] + i)
        if tag == 0: break
        d.setdefault(tag, val)
    sym, strt, h = v2o(d[6]), v2o(d[5]), v2o(d[4])
    n, = struct.unpack_from('<I', b, h + 4)
    out = []
    for i in range(1, n):
        nm, val, sz, info, oth, shn = struct.unpack_from('<IIIBBH', b, sym + 16 * i)
        if shn == 0 and nm:
            out.append(b[strt + nm: b.index(b'\0', strt + nm)].decode())
    return out
for p in sys.argv[1:]:
    for s in und(p): print(s)
