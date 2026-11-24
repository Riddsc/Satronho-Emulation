# Satronho Emulation: cómo está hecho y cómo se mantiene

Memoria técnica del proyecto. Léela antes de tocar algo. Para el estado actual, lo pendiente y
cómo trabajar con Ridds, mira también `CONTINUAR.md`.

## Qué es

Hub de emuladores de la familia Satronho (por Ridds). Un frontend propio que carga núcleos
**libretro** (.dll) y les pone encima la capa Satronho: biblioteca, imagen, medidor, guías,
configuración sugerida. Hoy son 9 emuladores (ver «Nombres» y `src/sys/systems.cpp`). Todo GPL 3.0 o
posterior.

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
descarga el zip, comprueba su SHA-256 con el asset `…windows-x64.zip.sha256` (lo publica el CI desde
la build 12) y escribe un `instalar.ps1` en %TEMP% que espera a que se cierre la app, hace
`Expand-Archive`, aparta `cores/`, `assets/` y `licencias/` como `.viejo`, copia lo nuevo (si falla,
las devuelve) y vuelve a abrir la app. `user/` (modo portátil) nunca se toca.

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
- `src/gfx/` — motor OpenGL (`gl`, `video_gl`) y dibujo por tarjeta gráfica de los núcleos
  (`hw_render` para OpenGL, `vulkan_bridge` para Vulkan). Ver `CONTINUAR.md` (24 de noviembre).
- `src/app_extra.cpp` — temas, logo de fondo, carrusel, carátulas, preguntas al salir, novedades de
  Juana, conversación con los guías y la elección de motor/resolución de CD y 64.
- `src/sys/covers` (carátulas), `src/sys/guide_chat` (respuestas de los guías), `src/util/net`
  (descargas).
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
  DLL del sistema y UCRT) + los 8 núcleos en `cores/` (en caché de Actions: se recompilan solo si
  cambia un `scripts/build-core-*.sh` o la versión de llvm-mingw). Antes de publicar compila también
  para Linux y abre la app en modo de revisión (offscreen + render software) en varias páginas; si
  se cae, no hay Release. Push a `main` con cambios en código o assets → Release `build-N` (N =
  número de ejecución del workflow) con el zip, su `.sha256` y las notas de la primera entrada de
  `assets/novedades.json`. Desde otra rama compila y prueba, pero no publica.
- Local (entorno remoto): compila nativo en Linux con SDL3 de `release-3.4.20`, y para Windows con
  llvm-mingw (`cmake/llvm-mingw.cmake`). Revisión visual con Xvfb y el modo de revisión:
  `SATRONHO_QA="page=ajustes;tab=0;frames=50;shot=/tmp/x.png"` (también `rom=`, `continuar=1` (con
  `rom=`, como el botón Continuar), `carpeta=` (agrega una carpeta), `menu=1` + `seccion=N`,
  `welcome=1`, `estados=1`, `fresh=1`, `sistema=`, `ctrl=`). Con `portable.txt` junto al ejecutable
  los datos quedan en `user/`. Sin pantalla: `SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy`
  (y `SDL_RENDER_DRIVER=software` si no hay EGL). Para probar con un juego de verdad en Linux:
  compilar el núcleo nativo (p. ej. `make -C nestopia/libretro`) y ponerlo en `cores/`; hay homebrew
  libre en GitHub (p. ej. `nesdoug/01_Hello`, `BUILD/hello.nes`).
- Dear ImGui 1.93.0: ojo, `AddRect`, `PathStroke` y `AddPolyline` llevan el grosor **antes** de las
  banderas. Las banderas de esquinas son `ImDrawFlags_RoundTop/Left/…` (no `RoundCorners…`).
- En Windows, `windows.h` define macros como `LoadImage`: no usar esos nombres para funciones propias.
- Modo de revisión, además: `page=guias` + `preguntar=…`, `tema=N`, `vista=1` (con `sistema=`),
  `salir=1|2`, `novedades_ventana=1`, `gpu=1|2|3` y `res=N` (CD y 64).

- Modo de revisión (etapa 2): `abrir=1`, `tactil=1` (control en pantalla), `editar_tactil=1`,
  `explorador=1`, `escala=N` (simula pantallas chicas; **no** dejarlo guardado), `chd=1`,
  `presentacion=1` (la presentación se salta en QA si no se pide). Con `rom=` no sale el selector de
  juegos de un .zip: se abre el primero.

- Modo de revisión (build 15): `ventana=WxH` (tamaño de la ventana), `escala_ui=N`, `barra=1`
  (despliega la barra del juego), `barra_menu=Juego|Imagen|…` (abre ese menú), `salir_tarde=1|2`
  (pregunta de salir a mitad de la prueba), `presionar=<bits>` (simula botones en Controles),
  `page=herramientas`, `vista=2` (lista). `qa.sh` acepta el tamaño de Xvfb como 3er argumento.
- Modo de revisión (build 20): `desplazar=1` (prueba de desplazamiento; sale con código 2 si falla),
  `descomprimir=archivo.zip` (disco en segundo plano), `publica=/ruta` (mudanza a la carpeta pública de
  Android, también en PC), `ajustes_juego=ruta` (ventana de ajustes de un juego).
- Botones que flotan sobre la página (como «Volver arriba») o cosas que se salen a propósito (carátulas del
  carrusel) no deben contar como contenido: usar `Ui::KeepContentSize`. Si cuentan, la página se estira.
- Parche propio en Dear ImGui (`external/imgui/imgui_draw.cpp`, buscar «Satronho»): al actualizar
  ImGui hay que volver a ponerlo.
- Cierres en Windows: el log trae `exe+desplazamiento` y la cadena de llamadas. Para saber qué
  función es: `git worktree add /tmp/wtN <commit>`, compilar igual que el CI (mismo llvm-mingw,
  `-DSATRONHO_BUILD=N`), comprobar que `.text` sale igual y buscar con `llvm-nm` (direcciones de 9 cifras).

## Android

- Proyecto en `android/` (Gradle + NDK, minSdk 26, solo arm64-v8a). Compila `CMakeLists.txt` como
  `libmain.so` con SDL3 compartido, GLES 3 y EGL. Las fuentes de Java de SDL están copiadas en
  `android/app/src/main/java/org/libsdl/app`; la actividad propia es `SatronhoActivity.java`.
- Diferencias en C++: `Gl::Es()` (sombreadores sin `#version`, el encabezado lo pone `Gl::Header()`),
  swizzle en vez de BGRA, 1555 → 565, `Paths::Init` usa la carpeta de la app, `Net::Download` y la
  vibración van por JNI (`sys/android.cpp`), el núcleo se busca como `lib<núcleo>_libretro_android.so`
  en `nativeLibraryDir` (archivo `nucleos.txt`). Mega usa Genesis Plus GX.
- No se puede compilar en el entorno remoto (SDK/NDK bloqueados): solo en el CI (`android.yml`).
  Firma: secretos `ANDROID_KEYSTORE_B64` y `ANDROID_KEYSTORE_PASS`; sin ellos, llave provisional.
- Sin artifacts de Actions: los núcleos van en la caché y el APK directo al Release.
- Carpeta pública «Satronho Games» (build 20): con el permiso de archivos, `Paths::PublicRoot()` tiene la
  ruta y ahí van `Juegos/<Consola>`, `Partidas` y `Estados`. La configuración, la caché y las BIOS siguen en
  la carpeta de la app. Ver `SetupPublicFolder` y `BeginDataMove` (app_import.cpp).

## Datos del usuario (dónde queda cada cosa)

- `partidas/<consola>/<juego>.srm` (+ `.srm.bak`, copia antes de la primera escritura de cada sesión).
  El núcleo también guarda ahí lo suyo (tarjetas de memoria de PS1, `.bkr` de Saturn): la carpeta se
  le da por `GET_SAVE_DIRECTORY` con `CoreHost::SetSaveDir` (el texto queda fijo en memoria, hay
  núcleos que guardan el puntero). Hasta la build 10 iban sueltas en `partidas/`: si no existe la
  nueva, `LoadSram` toma la vieja **solo si mide lo mismo** (si no, es de otra consola).
- `estados/<clave>-<título>.estadoN` (+ `.png`); la ranura automática es `.estado-auto`.
- `satronho.ini`: ajustes, controles (`perfil.<consola>.<perfil>.tN/mN`, `jugador.N.mando_id` = GUID
  del mando), atajos (`atajo.N`, N = orden del enum `Hotkey`: **los nuevos van al final**), opciones
  de núcleo por consola (`nucleo.<consola>.<clave>` y `nucleo.<consola>.@migrado`; antes de la build
  12 eran por núcleo, `nucleo.<NombreDelNúcleo>.…`, y se siguen leyendo hasta la migración) e imagen
  por consola (`video.<consola>.propio/escala/filtro/efecto/intensidad`).
- `biblioteca.ini`: por clave de juego (`favorito`, `tiempo`, `ultima`, `oculto`). La clave es el
  FNV-1a de la ruta en minúsculas; en modo portátil, de la ruta relativa a la carpeta del programa
  (`Paths::StableId`), y se migra sola desde la clave vieja (`legacy_key`). Las carpetas de juegos
  en modo portátil se guardan relativas (`Paths::StoreFolder` / `ResolveFolder`).

## Piezas que conviene conocer

- Rebobinar (`src/core/rewind.cpp`): guarda el último estado entero y, por cada paso atrás, un parche
  XOR comprimido (tramos iguales se saltan). Tope por segundos (`rewind_seconds`) y por memoria
  (`rewind_mb`). En consolas con `cpu_need >= 1` (PS1, DS, Saturn, 64) solo si `rewind_heavy`.
- Adelanto de cuadros: solo en núcleos con `runahead_ok` (hoy solo Octa).
- Mandos (`Input::ResolvePads`): cada jugador conserva su mando conectado; si no, lo recupera por
  GUID; si no, toma el libre en su posición. Turbo con contador propio (`turbo_tick_`).
- Vibración: `RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE` → `CoreHost::rumble` → `Input::ApplyRumble`
  (órdenes de 250 ms que se renuevan; en pausa o menú se corta).
- Discos: `RequestDisk` abre la tapa, espera 45 cuadros y pone el disco (`ServiceDiskSwap`).
- Abrir un juego pasa por `RequestStart` (dibuja «Cargando…» y lo abre en el cuadro siguiente).
  `StartGame` directo solo en el modo de revisión.
- Ajustes » Núcleo no carga el `.dll` de cada consola que se mira: usa `core_cache_` (copia de las
  opciones del núcleo tomada la primera vez que se cargó) y guarda directo en `satronho.ini`.
- BIOS: `SystemDef::bios` con nombre, tamaño, palabras clave y SHA-1 si se conoce (las de PS1 salen
  del código de Beetle PSX). `App::ImportBios` reconoce por huella, nombre, tamaño + palabra clave y,
  al final, solo tamaño.
- Cierres inesperados (Windows): `Log::InstallCrashHandler` anota el código y `archivo+desplazamiento`
  (exe o dll del núcleo) en `satronho.log`.

## Al agregar una consola

1. Script `scripts/build-core-<núcleo>.sh` y paso en el workflow.
2. `SystemDef` en `src/sys/systems.cpp`: extensiones, botones, forma del control, `cpu_need`, BIOS
   (con SHA-1 si el núcleo la verifica), `core_defaults` (comprobar las claves en el código del
   núcleo, en la versión fijada) y `runahead_ok` (solo si su sonido vive dentro del estado). Quitarla
   de `SoonSystems()`.
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
- **CD (Beetle PSX HW)**: `platform=win HAVE_HW=1` con `SHARED` propio (`mednafen_psx_hw_libretro.dll`).
  Dibuja con OpenGL, Vulkan o software según `beetle_psx_hw_renderer`. Sin BIOS usa OpenBIOS (viene
  dentro del núcleo). El mando se conecta como DualShock (`port_device`), el stick derecho va al
  juego (`analog`), y `disc` activa el aviso de BIOS. La biblioteca esconde los .bin que nombra un
  .cue y los discos que nombra un .m3u; un .bin de más de 16 MB no se toma como juego de Mega.
  El rebobinado se limita a ~600 MB de estados.
- Hexa también va sin adelanto: el remuestreo de bsnes queda fuera del estado.
- **64 (Mupen64Plus-Next)**: `platform=win MSYSTEM=MINGW64` (dynarec x86_64, necesita nasm) y
  `LDFLAGS` propio. Trae GLideN64, ParaLLEl-RDP y Angrylion; Satronho elige según el motor
  (`App::ApplyGpuChoice`). GLideN64 va con el RSP HLE; ParaLLEl-RDP y Angrylion con ParaLLEl-RSP.
  Botones C independientes (`alt-map`); las flechas del teclado mueven el stick si no están asignadas.
- **Dual (melonDS DS)**: CMake con el toolchain de llvm-mingw, sin OpenGL, con JIT. FreeBIOS
  incluida. La pantalla de abajo se toca con el mouse (`RETRO_DEVICE_POINTER`, `SystemDef::touch`).
- `CoreHost::LoadCore` reinicia el formato de imagen: bsnes no lo pide y heredaba el de otro núcleo.
- **Anillo (Beetle Saturn)**: como Beetle PSX (`platform=win`, `SHARED` propio). Pide BIOS
  (`bios_required`): sin ella `StartGame` lleva a Ajustes » Núcleo. Los discos (.cue/.ccd/.m3u)
  se reparten entre CD y Anillo mirando la primera pista («SEGA SEGASATURN»); los .chd, por el nombre
  de la carpeta.
- BIOS: `SystemDef::bios` lista los archivos que reconoce cada núcleo; `App::ImportBios` copia el
  archivo elegido con el nombre correcto (por nombre, tamaño y pistas como «jap» o «usa»).
