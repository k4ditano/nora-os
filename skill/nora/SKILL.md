---
name: nora
description: Crear, probar y arreglar apps y juegos para Nora OS, el sistema de la consola Anbernic RG35XX (Lua sobre la GPU, 640x480, botones de mando). Úsala cuando pidan "un juego para Nora", "una app para la consola", "para la RG35XX", "hazme un Snake/Tetris/plataformas para Nora", o cambiar, depurar o mejorar una app de Nora; también para instalar apps en la consola, verla (capturas), pulsar sus botones o leer su registro con nora-puente.
---

# Apps y juegos para Nora

Nora OS corre en una Anbernic RG35XX: pantalla de 640×480, cruceta, A B X Y, L R L2 R2,
SELECT START y MENU. Una app es una **carpeta con `app.lua`** (LuaJIT) que rellena la
tabla `app` con funciones; el núcleo las llama 60 veces por segundo y dibuja por GPU.
El PC habla con la consola a través de **`nora-puente`**, que ya está instalado y
corriendo como servicio: con él instalas la app, la ves, pulsas sus botones y lees sus errores.

**Antes de escribir código lee [referencia/api.md](referencia/api.md)** (la API entera, corta).
Para juegos, mira también [referencia/patrones.md](referencia/patrones.md) y el ejemplo
completo [ejemplos/cometas/](ejemplos/cometas/) (SVG, PNG, cruceta mantenida, sonido, récord).
La app más pequeña es [ejemplos/hola/](ejemplos/hola/).

## Cómo trabajar

1. **¿Está la consola?** `nora-puente apps` lista sus apps. Si dice que la consola no está
   conectada, haz la app igual y dile al usuario que la conecte (cable USB-C) para probarla.
2. **Dónde**: si el usuario no dice otra cosa, en `~/Proyectos/nora-apps/<id>/`. El id es
   la carpeta: minúsculas, números, `-`, `_` (p. ej. `snake`, `mi-juego`).
3. **Escribe** `info.lua` (nombre, icono, color, orden) y `app.lua`, y las imágenes y
   sonidos que hagan falta (en `img/` y `sonidos/`; ver *patrones*: SVG a mano, PNG y WAV con Python).
4. **Instala y ábrela**:
   ```sh
   nora-puente instalar ~/Proyectos/nora-apps/snake --abrir
   ```
   Contesta `abierta y funcionando`, o `FALLA al abrirse` seguido del error de Lua con
   archivo, línea y traza (y sale con código 3). Si falla, corrige y vuelve a instalar.
5. **Mírala**: `nora-puente captura <archivo.png>` (en tu carpeta temporal) y abre la imagen. Comprueba que se lee,
   que nada se sale de la pantalla y que se parece a lo que querías.
6. **Juega**: `nora-puente pulsar A`, `nora-puente pulsar DERECHA --ms 600` (mantener),
   `nora-puente pulsar ABAJO ABAJO A` (seguidos: cada uno `--ms` pulsado, por defecto 90, y
   70 ms entre uno y otro); captura otra vez para ver el efecto.
   Botones: `A B X Y L R L2 R2 SELECT START ARRIBA ABAJO IZQUIERDA DERECHA`.
   **Cada orden tarda ~0,25 s en llegar, y varía**: sirve para menús y para comprobar que
   algo responde, no para jugar en tiempo real. Para probar un juego rápido (Snake, plataformas),
   pon arriba de `app.lua` una constante de pruebas (`local PRUEBAS = false`) que lo ralentice
   o lo pare entre turnos, y saca el estado con `nora.log` para decidir qué pulsar; vuelve a
   dejarla en `false` (y comprueba que abre) antes de terminar.
7. **Depura**: `nora.log(...)` en la app y `nora-puente log` para leerlo: las últimas ~300
   líneas de todas las apps (también aperturas y errores con su traza), cada una con la hora
   y el id de la app delante (no lo pongas tú). `-n 20` las últimas 20, `--app snake` solo esa.
   `nora-puente log -f` se queda siguiendo y no acaba: úsalo solo con `timeout`.
8. **Para que el usuario siga tocando**: `nora-puente vigilar <carpeta>` en una terminal suya
   recarga la app en la consola cada vez que se guarda un archivo (recarga en caliente).

Otras órdenes (todas en `nora-puente --help`):
- `nora-puente abrir <id>` abre una app que ya está en la consola (dice si abre o su error).
- `nora-puente borrar <id>` quita una app; sus datos guardados se quedan.
- `nora-puente borrar-datos <id>` borra lo guardado con `nora.salvar` (récords de pruebas…).
- `instalar --limpiar` instala desde cero (borra en la consola los archivos que ya no estén en
  la carpeta; los datos guardados no los toca). `--id nombre` si la carpeta se llama distinto.
- Nunca lances `nora-puente` sin orden: es el servicio, que ya está corriendo.

## Reglas que no se ven a simple vista

- **B vuelve al inicio** (`nora.volver()`) en el nivel de arriba de toda app (menú del juego,
  partida en marcha). Dentro de una pausa o un submenú, B puede ser "atrás"; desde ahí, otra B ya sale. MENU es del sistema.
- **Solo Latin-1 en los textos**: español sí; emojis, flechas (←↑), ★, ♥, ✓ no (salen "?"). Para eso, `nora.icono` o imágenes.
- **Nada de `<text>` en los SVG** (no se dibuja). Texto, con `nora.texto`.
- `y` de `nora.texto` es la parte de **arriba** del texto. Mide con `nora.ancho` para centrar o encajar.
- Carga imágenes y sonidos **al principio de `app.lua`**, no en `pinta`. Rutas relativas a la carpeta de la app.
- Velocidades en px/s multiplicadas por `dt`. Movimiento continuo con `nora.pulsado`, acciones con `app.boton`.
- Un error deja la app parada con la pantalla roja: `instalar` te lo dice; no des la app por buena sin ver `abierta y funcionando` y una captura.
- Guardar récords/partidas: `nora.salvar` / `nora.cargar` (texto; tablas con `json`). Si al
  probar dejas un récord falso, bórralo con `nora-puente borrar-datos <id>`.
- Sonidos del sistema: `local SONIDOS = os.getenv("NORA_SONIDOS")` y `SONIDOS .. "/mover.wav"`. Los tuyos, con ruta relativa: `nora.suena("sonidos/salto.wav")`. Desde el PC no se puede oír: compruébalo con el usuario.
- La pantalla es de **16 bits**: los degradados y los colores muy oscuros se ven a escalones o tramados. Mejor colores planos.
- `app.lua` se ejecuta entero cada vez que se abre la app (y en cada recarga): lo de arriba es la inicialización.
- Pantalla pequeña y se mira de cerca: textos de `"peq"` (24 px) como mínimo, botones y cosas importantes grandes, buen contraste.

## Antes de dar la app por terminada

- [ ] `instalar` dice `abierta y funcionando`.
- [ ] Una captura de cada pantalla (menú, juego, fin) se ve bien.
- [ ] Has jugado un poco con `pulsar` y no ha salido ningún error (`nora-puente log`).
- [ ] B vuelve al inicio; el juego se puede reiniciar sin salir.
- [ ] `info.lua` con nombre, icono y color.
- [ ] Dile al usuario dónde está la carpeta y que `nora-puente vigilar <carpeta>` le deja retocarla en vivo.
