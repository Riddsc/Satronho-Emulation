# Satronho Emulation: cómo está hecho y cómo se mantiene

Memoria técnica del proyecto. Léela antes de tocar algo.

## Qué es

Hub de emuladores de la familia Satronho (por Ridds). Un frontend propio que carga núcleos
**libretro** (.dll) y les pone encima la capa Satronho: biblioteca, imagen, medidor, guías,
configuración sugerida. Primera consola: NES con Nestopia UE. Todo GPL 3.0 o posterior.

Decisiones de Ridds:
- Repo público (Actions gratis). Las builds se publican como **Releases `build-N`**, sin artifacts.
- Los créditos de terceros van en el README, `CREDITOS.md` y `licencias/` (no en la interfaz).
  En la app el crédito visible es «por Ridds».
- Mismo diseño en PC y Android, adaptado. Primero PC (Windows).
- Consolas pesadas (Switch, PS3…) irán como apps aparte que el hub abre (como Satronho NX).

## Nombres (decididos por Ridds)

Siempre «Satronho …», con un nombre que se entienda sin usar marcas, y la consola solo como
descripción («Satronho Octa · para juegos de NES»): Octa (NES), Hexa (SNES), 64 (N64), Bolsillo
(GB/GBC), Bolsillo Plus (GBA), Dual (DS/3DS), Cubo (GameCube/Wii), NX (Switch, app aparte),
Mega (Genesis), Anillo (Saturn), Sueño (Dreamcast), CD (PS1), CD2 (PS2), Portátil (PSP).

## Actualizaciones

`src/sys/updater.cpp` (solo Windows): consulta `releases/latest` con urlmon, compara `build-N`,
descarga el zip y escribe un `instalar.ps1` en %TEMP% que espera a que se cierre la app, hace
`Expand-Archive` + `Copy-Item` encima de la carpeta del programa y la vuelve a abrir.

## Varias consolas (fase 2)

- `src/sys/systems.cpp`: cada emulador (id, nombre, núcleo, extensiones, botones, forma del control,
  etiqueta del cartucho y valores por defecto del núcleo). Bolsillo y Bolsillo Plus comparten
  mGBA (y sus opciones). El núcleo se carga al abrir un juego (`App::EnsureCore`).
- La biblioteca escanea todas las extensiones y cada juego guarda su consola; la barra lateral filtra.
- Controles (`src/core/input.cpp`): perfiles por consola («Predeterminado» + los del usuario),
  jugador 1 y 2 con teclado y/o mando (por posición entre los conectados), atajos con tecla y
  botón. Códigos de mando: botones SDL, 200 = LT, 201 = RT. Dibujos de los controles en
  `src/ui/controllers.cpp` (distribución original, trazo propio, sin logos).
- Efectos: `Effect::Lcd` (rejilla) y `Effect::Auto` (CRT en consolas de casa, LCD en portátiles).
- Pantalla completa a 60 Hz en monitores de 120/144 Hz (`App::SetFullscreen`).

## Estructura

- `src/core/` — `core_host` (carga del núcleo, entorno libretro, estados, SRAM, opciones),
  `audio` (SDL_AudioStream con control dinámico de velocidad ±0.5 %), `input` (teclado, mandos,
  turbo, asignación), `video` (escala, filtro nítido suave, fondo ambiente, líneas, CRT).
- `src/sys/` — `hardware` (CPU, RAM, GPU por DXGI, pantalla; uso del proceso), `settings`
  (ajustes y la configuración sugerida de Ñoño), `library` (escaneo en segundo plano, tiempo de
  juego, favoritos).
- `src/ui/kit` — colores, tipografías, íconos dibujados, botones, interruptores, segmentados,
  deslizadores, filas de ajustes, banner de guía y portada de cartucho.
- `src/app.cpp` — ciclo principal, emulación (sincronía con la pantalla o por tiempo), adelanto
  de cuadros, rebobinar, estados, atajos. `src/app_ui.cpp` — todas las pantallas.
- `assets/` — logo, fuentes, guías (`guias/*.png`, los mismos de Satronho NX), `novedades.json`,
  `consejos.json`, `system/NstDatabase.xml`.
- Valores por defecto propios del núcleo: `OverrideDefault` en `App::Init` (por ahora, Nestopia
  sin filtro NTSC para que la imagen salga nítida).

## Compilar y probar

- CI: `.github/workflows/build.yml`, Linux + llvm-mingw → `satronho-emulation.exe` estático (solo
  DLL del sistema y UCRT) + `cores/nestopia_libretro.dll`. Push a `main` con cambios en código o
  assets → Release `build-N` con las notas de la primera entrada de `assets/novedades.json`.
- Local (entorno remoto): compila nativo en Linux con SDL3 de `release-3.4.20`, y para Windows con
  llvm-mingw (`cmake/llvm-mingw.cmake`). Revisión visual con Xvfb y el modo de revisión:
  `SATRONHO_QA="page=ajustes;tab=0;frames=50;shot=/tmp/x.png"` (también `rom=`, `menu=1`,
  `welcome=1`, `estados=1`, `fresh=1`). Con `portable.txt` junto al ejecutable los datos quedan en
  `user/`.
- Dear ImGui 1.93.0: ojo, `AddRect`, `PathStroke` y `AddPolyline` llevan el grosor **antes** de las
  banderas.

## Al agregar una consola

1. Script `scripts/build-core-<núcleo>.sh` y paso en el workflow.
2. Entrada en la barra lateral (hoy «Pronto»), extensiones, botones del mando y valores por
   defecto del núcleo.
3. Novedades en `assets/novedades.json` (la primera entrada son las notas del Release).

### Núcleos ya integrados (cómo se compilan)

- **Mega (BlastEm)**: `Makefile.libretro core` con `MSYSTEM=MINGW64` para que elija Windows x86_64
  (JIT de 68K y Z80). En Android usará los núcleos generados (`NEW_CORE=1`). Adelanto apagado
  (`runahead_ok = false`) hasta probar si su sonido vive dentro del estado.
- **Hexa (bsnes)**: `platform=win` con `SHARED` propio (lld no acepta `--version-script`) y
  `EXTRA_INCLUDES="-Wno-invalid-specialization -D_GNU_SOURCE"` (clang nuevo + `vasprintf`).
- Cada consola declara en `SystemDef` sus filas de botones (`face`), gatillos (`shoulders`) y si
  X/Y son turbos (`turbo`); la pantalla de Controles se arma sola con eso.
- Juegos libres para probar: muestras de SGDK (`sample/*/out/release/rom.bin`) y los demos de
  PeterLemon/SNES (`*.sfc`).

### Sonido

`App::EmulateTick` corre un cuadro de más o de menos cuando el búfer de sonido se aleja mucho de la
latencia elegida (pantallas de 120/144 Hz), y `AudioOut` ajusta la velocidad ±1 %. Cada minuto de
juego anota en el registro: cortes, descartes, búfer y tirones.
- **CD (Beetle PSX, software)**: `platform=win` con `SHARED` propio. Sin BIOS usa OpenBIOS (viene
  dentro del núcleo). El mando se conecta como DualShock (`port_device`), el stick derecho va al
  juego (`analog`), y `disc` activa el aviso de BIOS. La biblioteca esconde los .bin que nombra un
  .cue y los discos que nombra un .m3u; un .bin de más de 16 MB no se toma como juego de Mega.
  El rebobinado se limita a ~600 MB de estados.
- Hexa también va sin adelanto: el remuestreo de bsnes queda fuera del estado.
- **64 (Mupen64Plus-Next)**: `platform=win MSYSTEM=MINGW64` (dynarec x86_64, necesita nasm) y
  `LDFLAGS` propio. Por defecto Angrylion + ParaLLEl-RSP: no necesitan OpenGL ni Vulkan (el host
  todavía no ofrece contexto de hardware; GLideN64 y ParaLLEl-RDP quedan para cuando lo tenga).
  Botones C independientes (`alt-map`); las flechas del teclado mueven el stick si no están asignadas.
- **Dual (melonDS DS)**: CMake con el toolchain de llvm-mingw, sin OpenGL, con JIT. FreeBIOS
  incluida. La pantalla de abajo se toca con el mouse (`RETRO_DEVICE_POINTER`, `SystemDef::touch`).
- `CoreHost::LoadCore` reinicia el formato de imagen: bsnes no lo pide y heredaba el de otro núcleo.
