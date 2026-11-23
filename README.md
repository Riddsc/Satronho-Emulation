<p align="center">
  <img src="dist/branding/icon.png" alt="Satronho Emulation" width="160">
</p>

<h1 align="center">Satronho Emulation</h1>
<p align="center"><b>por Ridds</b> · La casa de los emuladores de la familia Satronho</p>

---

Un solo lugar para tus consolas, con una imagen cuidada, controles que responden al instante, un medidor en vivo y configuraciones sugeridas para tu equipo. Te acompañan los guías de la familia Satronho: **Samy**, **Troya**, **Ñoño**, **Juana** y **MechaRidds**.

Empezamos con la **NES**. Pronto llegan más consolas.

## Lo que trae

- **Biblioteca** con portadas de cartucho dibujadas para cada juego, búsqueda, favoritos, orden por recientes o más jugados, tiempo de juego y «Seguir jugando».
- **Imagen**: filtro «Nítido suave» (píxeles cuadrados y sin temblores a cualquier tamaño), escala entera, líneas de barrido, pantalla CRT con máscara de fósforo y viñeta, y fondo ambiente con los colores del juego.
- **Controles**: adelanto de cuadros (responden más rápido que en la consola original), rebobinar, avance rápido, turbo, mandos con conexión en caliente y asignación de botones con un clic.
- **Estados**: 8 ranuras con miniatura y fecha. Las partidas de batería se guardan solas.
- **Medidor Satronho** (compacto o detallado): FPS, velocidad, tiempo del núcleo, gráfica de cuadros, tirones, CPU, RAM y sonido.
- **Configuración sugerida**: Ñoño revisa tu procesador, memoria, tarjeta gráfica y pantalla, y con «Aplícalo por mí» deja todo listo.
- **Sonido** con control dinámico de velocidad, para que no se corte ni se atrase.

## Descargar

Las builds para Windows (x64) están en [Releases](https://github.com/Riddsc/Satronho-Emulation/releases). Descomprime la carpeta y abre `satronho-emulation.exe`.

¿Todo en una sola carpeta (por ejemplo, en una memoria USB)? Crea un archivo vacío llamado `portable.txt` junto al programa.

Satronho Emulation **no incluye juegos**. Usa copias de los cartuchos que tienes.

## Atajos

| Tecla | Mando | Acción |
|---|---|---|
| Esc | Guía o Select+Start | Menú del juego |
| F5 / F9 | | Guardar / cargar estado |
| F6 / F7 | | Cambiar de ranura |
| Tab (mantener) | Gatillo derecho | Avance rápido |
| Retroceso (mantener) | Gatillo izquierdo | Rebobinar |
| P | | Pausa |
| Ctrl+R | | Reiniciar |
| F3 | | Medidor |
| F12 | | Captura |
| F11 o Alt+Enter | | Pantalla completa |

## Compilar

Las builds oficiales las hace GitHub Actions desde Linux con [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (ver `.github/workflows/build.yml`). En local:

```bash
export LLVM_MINGW=/ruta/a/llvm-mingw
scripts/build-core-nestopia.sh build/cores
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw.cmake
cmake --build build
```

También compila de forma nativa en Linux (`cmake -S . -B build && cmake --build build`), útil para probar.

## Créditos

Logo, guías, interfaz y diseño: **Ridds**.

Satronho Emulation es software libre bajo la licencia **GPL 3.0 o posterior** (ver `LICENSE`) y está construido sobre el trabajo de estos proyectos, a los que agradecemos:

| Proyecto | Para qué | Licencia |
|---|---|---|
| [Nestopia UE](https://github.com/libretro/nestopia) | Núcleo de NES | GPL 2.0 o posterior |
| [SDL 3](https://github.com/libsdl-org/SDL) | Ventana, sonido, mandos y dibujo | zlib |
| [Dear ImGui](https://github.com/ocornut/imgui) | Base de la interfaz | MIT |
| [libretro API](https://github.com/libretro/libretro-common) | Conexión con los núcleos | MIT |
| [stb](https://github.com/nothings/stb) | Lectura y guardado de PNG | Dominio público / MIT |
| [Poppins](https://github.com/google/fonts/tree/main/ofl/poppins) | Tipografía | SIL OFL 1.1 |
| [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) | Tipografía de números | SIL OFL 1.1 |

Los textos completos de las licencias están en la carpeta `licencias/`.

NES es una marca de Nintendo. Este proyecto no está afiliado a Nintendo.
