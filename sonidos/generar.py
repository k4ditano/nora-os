#!/usr/bin/env python3
"""Sonidos del sistema de Nora, generados por codigo (WAV 44,1 kHz estereo 16 bits).
Cortos y suaves: un gesto = un sonido."""
import wave, struct, math, random, os
R = 44100
AQUI = os.path.dirname(os.path.abspath(__file__))

def guarda(nombre, muestras, vol=0.8):
    m = max(1e-9, max(abs(v) for v in muestras))
    with wave.open(os.path.join(AQUI, nombre + ".wav"), "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(R)
        w.writeframes(b"".join(struct.pack("<hh", int(32000 * vol * v / m), int(32000 * vol * v / m)) for v in muestras))

def tono(f0, f1, dur, forma=lambda x: math.sin(x), ataque=0.004, cola=0.6):
    n = int(R * dur); fase = 0; out = []
    for i in range(n):
        t = i / n
        fase += 2 * math.pi * (f0 + (f1 - f0) * t) / R
        env = min(1, i / (R * ataque)) * (1 - t) ** (1 / cola)
        out.append(forma(fase) * env)
    return out

suave = lambda x: math.sin(x) + 0.25 * math.sin(2 * x)
guarda("mover", tono(1400, 1300, 0.035, suave), 0.45)                       # tic
guarda("abrir", tono(600, 1200, 0.09, suave) + tono(1200, 1500, 0.07, suave), 0.6)
guarda("volver", tono(1100, 600, 0.11, suave), 0.55)
guarda("menu", tono(800, 800, 0.05, suave) + [0] * int(R * 0.02) + tono(1200, 1200, 0.07, suave), 0.5)
print("sonidos generados")
