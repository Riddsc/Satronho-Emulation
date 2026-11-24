<p align="center">
  <img src="dist/branding/icon.png" alt="Satronho Emulation" width="160">
</p>

<h1 align="center">Satronho Emulation</h1>
<p align="center"><b>por Ridds</b> · La casa de los emuladores de la familia Satronho</p>

---

Un solo lugar para tus consolas, con una imagen cuidada, controles que responden al instante, un medidor en vivo y configuraciones sugeridas para tu equipo. Te acompañan los guías de la familia Satronho: **Samy**, **Troya**, **Ñoño**, **Juana** y **MechaRidds**.

Cada emulador de la familia tiene su nombre Satronho:

| Emulador | Para juegos de | Estado |
|---|---|---|
| **Satronho Octa** | NES / Famicom | Disponible |
| **Satronho Bolsillo** | Game Boy y Game Boy Color | Disponible |
| **Satronho Bolsillo Plus** | Game Boy Advance | Disponible |
| **Satronho Mega** | Genesis / Mega Drive | Disponible |
| **Satronho Hexa** | Super Nintendo / Super Famicom | Disponible |
| Satronho CD | PlayStation | Pronto |
| Satronho 64, Cubo, Dual, Anillo, Sueño, CD2, Portátil | Nintendo 64, GameCube/Wii, DS/3DS, Saturn, Dreamcast, PlayStation 2, PSP | Más adelante |
| [Satronho NX](https://github.com/Riddsc/satronho-nx) | Switch | App aparte |

## Lo que trae

- **Biblioteca** con portadas de cartucho dibujadas para cada juego, búsqueda, favoritos, orden por recientes o más jugados, tiempo de juego y «Seguir jugando».
- **Imagen**: filtro «Nítido suave» (píxeles cuadrados y sin temblores a cualquier tamaño), escala entera, líneas de barrido, pantalla CRT con máscara de fósforo y viñeta, rejilla LCD para las portátiles, fondo ambiente con los colores del juego y pantalla completa a 60 Hz en monitores de 120/144 Hz.
- **Controles**: el control de cada consola dibujado con sus botones iluminándose en vivo, perfiles por consola, Jugador 1 y 2, asignación automática o uno por uno, atajos asignables y nombres de botones según tu mando (Xbox, PlayStation o Switch). Además: adelanto de cuadros, rebobinar, avance rápido y turbo.
- **Estados**: 8 ranuras con miniatura y fecha. Las partidas de batería se guardan solas.
- **Medidor Satronho** (compacto o detallado): FPS, velocidad, tiempo del núcleo, gráfica de cuadros, tirones, CPU, RAM y sonido.
- **Configuración sugerida**: Ñoño revisa tu procesador, memoria, tarjeta gráfica y pantalla, y con «Aplícalo por mí» deja todo listo.
- **Sonido** con control dinámico de velocidad, para que no se corte ni se atrase.
- **Actualizaciones** desde la app: Juana avisa cuando sale una build nueva y la instala con un clic.

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
scripts/build-core-mgba.sh build/cores
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw.cmake
cmake --build build
```

También compila de forma nativa en Linux (`cmake -S . -B build && cmake --build build`), útil para probar.

## Créditos

Logo, guías, interfaz y diseño: **Ridds**.

Satronho Emulation es software libre bajo la licencia **GPL 3.0 o posterior** (ver `LICENSE`) y está construido sobre el trabajo de estos proyectos, a los que agradecemos:

| Proyecto | Para qué | Licencia |
|---|---|---|
| [Nestopia UE](https://github.com/libretro/nestopia) | Núcleo de Satronho Octa | GPL 2.0 o posterior |
| [mGBA](https://github.com/mgba-emu/mgba) | Núcleo de Satronho Bolsillo y Bolsillo Plus | MPL 2.0 |
| [BlastEm](https://github.com/libretro/blastem) | Núcleo de Satronho Mega | GPL 3.0 |
| [bsnes](https://github.com/libretro/bsnes) | Núcleo de Satronho Hexa | GPL 3.0 |
| [SDL 3](https://github.com/libsdl-org/SDL) | Ventana, sonido, mandos y dibujo | zlib |
| [Dear ImGui](https://github.com/ocornut/imgui) | Base de la interfaz | MIT |
| [libretro API](https://github.com/libretro/libretro-common) | Conexión con los núcleos | MIT |
| [stb](https://github.com/nothings/stb) | Lectura y guardado de PNG | Dominio público / MIT |
| [Poppins](https://github.com/google/fonts/tree/main/ofl/poppins) | Tipografía | SIL OFL 1.1 |
| [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) | Tipografía de números | SIL OFL 1.1 |

Los textos completos de las licencias están en la carpeta `licencias/`.

Los nombres de las consolas se usan solo para describir con qué juegos funciona cada emulador. NES, Super Nintendo, Game Boy, Nintendo 64, GameCube, Wii, DS y Switch son marcas de Nintendo; Genesis, Mega Drive, Saturn y Dreamcast, de Sega; PlayStation, de Sony. Este proyecto no está afiliado a ninguna de ellas.
