# Continuar Satronho Emulation

Notas de traspaso entre sesiones de Claude. Lo más nuevo va arriba. Lee también `CLAUDE.md` (cómo
trabaja Ridds) y `SATRONHO.md` (cómo está hecho).

## Estado al 24 de noviembre de 2026 (build 17)

Ridds probó la build 16: la resolución automática de CD no coincidía con su pantalla (1440p → 4×), pidió
gráficos **por consola** con las mismas opciones donde sirvan, todo mejor explicado, una barra de consolas
para elegir cuál ajustar y el módulo Satronho como en NX (Medidor, Configuración sugerida, con imágenes).
Rama `build-17`. Lo siguiente: **agregar consolas** (parte C, ver abajo).

- **Gráficos por consola** (`src/app_graphics.cpp`, reemplaza `DrawSettingsVideo`/`DrawGpuSettings`):
  barra de consolas (`gfx_sys_`, QA `gfx=cd`), imagen propia o general (`video_for`), motor y resolución
  (CD/64), opciones conocidas de cada núcleo con explicación por valor (`KnownOptions`) y las demás de
  imagen del núcleo plegadas («Más opciones», `gfx_more_`; las de HDR se ocultan). Abajo, «Para toda la app».
- Las opciones del núcleo se guardan en `nucleo.<consola>.<opción>` (como Ajustes » Núcleo) con
  `SetCoreOptionFor`; `MigrateGfxOptions` pasa las `graficos.*` de la build 16 y las borra.
- **Resolución automática**: `NearestResStep` (la más parecida al alto de la pantalla; empate → la mayor),
  con tope por tarjeta (integrada 2×, <4 GB 4×, si no 16×). En 1440p CD queda en 8×.
- **Módulo Satronho** (`SatronhoModuleMenu`, `app_bar.cpp`): el mismo menú en la barra del juego y en la
  barra de inicio, con la carita de cada guía. En el juego, Novedades/Sugerida/Guías/Gráficos/Ajustes se
  abren **encima** (`overlay_page_`, `DrawGameOverlay`), el juego espera; Esc o «Volver al juego» cierra.
  QA: `encima=3` (número de `Page`).
- **Barra de inicio en PC** (`DrawHomeBar`, no en Android/compacto): Archivo, Vista, Consolas,
  Herramientas, Satronho, Ayuda. QA: `inicio_menu=Satronho`. QA nuevo `bajar=N` desplaza Ajustes.
- Traducciones de todo lo nuevo en `scripts/idiomas/traducciones/extra.txt`.

### Falta probar (pedírselo a Ridds)

1. CD en automático en su pantalla 1440p (debería decir 8×) y que se vea bien. 2. Cambiar opciones de
cada consola y ver que se apliquen al jugar. 3. Módulo Satronho dentro del juego (pantalla completa y
ventana) y en Android con el dedo.

## Estado al 24 de noviembre de 2026 (build 16)

Ridds probó la build 15 y pidió: la X cerraba sin preguntar, Controles y Gráficos como en Satronho NX
(capturas de NX), más resoluciones, clic derecho completo, carrusel que distinga consolas, módulo
Satronho como en NX, ícono de perrito para los guías y la app en español, inglés, francés, portugués e
italiano. Todo de una vez, pulido. Después de esta build: **agregar consolas** (parte C, ver abajo).
Rama `build-16` (una sesión se cortó a mitad de los idiomas; se retomó y terminó).

- **X sin preguntar**: SDL manda `WINDOW_CLOSE_REQUESTED` y `QUIT` con el mismo clic; la regla de la
  build 15 («segunda X cierra») los contaba como dos. Arreglado en `RequestQuit`.
- **Controles NX** (`DrawControllerEditor`): mando grande al centro, recuadros por grupo alrededor,
  palancas en vivo, «Asignar con» mando/teclado, opciones abajo.
- **Gráficos NX** (`DrawGpuSettings`): tarjeta, API, resoluciones exactas con su alto real y la más
  parecida a la pantalla; opciones de imagen de cada núcleo (MSAA, supermuestreo, PGXP, 16:9, filtros).
- **Barra del juego**: menús Vista y Gráficos, módulo Satronho (novedades, sugerida, guías, medidor, logo).
- **Clic derecho** (`GameContextMenu`): carátula, tiempo de juego, .chd, verificar (CRC32), copiar,
  acceso directo (`util/shortcut.cpp`).
- **Idiomas** (`util/i18n.*`, `assets/idiomas/*.json`, herramientas y fuentes en `scripts/idiomas/`, ver
  su LEEME). `Tr()` en el kit, `Fmt`, `Toast`, `Muted`; el resto se envolvió con `envolver_tr.py`.
  Ajuste `interfaz.idioma` (vacío = el del sistema). Modo de revisión: `idioma=en`.
  **Pendiente**: el chat de los guías responde en español; textos nuevos hay que agregarlos a las traducciones.

### Falta probar (pedírselo a Ridds)

1. Que la X pregunte siempre. 2. Controles y Gráficos nuevos con su mando Xbox y su RX 9070 XT.
3. Cambiar de idioma y recorrer las pantallas buscando textos en español o cortados.

## Estado al 24 de noviembre de 2026 (build 15)

Ridds probó la build 14 en su PC (Ryzen 7 5800XT, RX 9070 XT, 1440p/escala 1.25, mando Xbox) y pidió
todo de una vez, para PC y Android. Se trabajó en la rama `build-15` y se unió a `main`.

### Lo que salió del log de Ridds y se arregló

- **No podía cerrar la app**: con el menú grande del juego abierto, `DrawPauseMenu` le robaba el foco
  cada cuadro a la pregunta «¿Cerrar?» (modal) y nunca se veía. Ahora no roba el foco si hay una
  ventana emergente; una segunda X/Alt+F4 cierra de una vez; y si la pregunta no logra verse en 2 s
  se hace lo pedido igual (`confirm_frames_`).
- **Cierre 0xC0000005** en `ImFontAtlasBakedSetFontGlyphBitmap` (se ubicó recompilando el mismo commit
  con el mismo llvm-mingw: el código sale idéntico byte a byte, así se puede buscar el símbolo). Causa
  más probable: tamaño de letra inválido. Parche en `external/imgui/imgui_draw.cpp` (marcado
  «Satronho»): tamaños entre 1 y 512 y glifos de medidas imposibles se saltan.
- El informe de cierre (`util/log.cpp`) ahora anota la dirección que falló y la **cadena de llamadas**
  (24 niveles, módulo+desplazamiento). El log también anota cada pantalla que se abre.
- **La cruceta del dibujo no se iluminaba** hacia arriba ni a la izquierda (rectángulo de alto/ancho
  cero en `DPad`).
- **Botones C de la 64 sin mando**: nuevos códigos de palanca como botón (`kPadStick`..209,
  `leftstick_up`…`rightstick_right` en el ini). Los C van en la palanca derecha; migración automática.

### Qué trae

- **Barra de menús del juego** (`app_bar.cpp`): Juego, Imagen, Sonido, Controles, Herramientas,
  Satronho, Ayuda + barra de estado. En ventana siempre (el juego se dibuja entre las barras:
  `VideoOutGl::SetArea` / viewport en el clásico); en pantalla completa Esc del **teclado** la
  despliega (`Input::menu_from_keyboard`); con el mando Guía/Select+Start sigue abriendo el menú
  grande. Android: el botón de menú del control en pantalla la despliega (el control se esconde).
  Un menú abierto pausa el juego (`bar_menu_active_`). Se apaga en Apariencia (`interfaz.barra_juego`).
- **Resolución con nombres** (`App::ResLabel`, `GpuResSteps`): pasos reales de cada núcleo.
- **«Auto» limpio en 3D** (`App::VideoSettings`).
- **Controles estilo DuckStation**: puertos a la izquierda, consola en un combo, casillas compactas
  (88 px), el dibujo usa el alto que sobra. Palancas del dibujo en vivo (`Ui::SetLiveSticks`).
- **Biblioteca**: botón «Vista» (cuadrícula/carrusel/lista, tamaño `biblioteca.tamano`, orden,
  favoritos, aplicar a todas); vista en lista (`DrawGameRow`); menú de clic derecho en `GameContextMenu`.
- **Herramientas** (`app_tools.cpp`, `Page::Tools`): prueba de mandos, Memory Cards de PS1 (lee el
  directorio de la tarjeta, títulos Shift-JIS → ASCII, borrar con copia `.bak`), carátulas, carpetas, .chd.

### Falta probar (pedírselo a Ridds)

1. Que la pregunta al cerrar salga siempre (con y sin el menú del juego abierto).
2. La barra en pantalla completa con Esc, y en ventana; que el juego no quede tapado.
3. Los C de la 64 en la palanca derecha con su mando Xbox; la cruceta iluminada.
4. Si vuelve el cierre de las letras: el log trae la cadena de llamadas; recompilar el mismo commit
   con llvm-mingw 20251104 y buscar las direcciones con `llvm-nm`.
5. Android en un teléfono real (sigue sin probarse) y la llave de firma en los secretos.

### Ideas que quedan

- Parte C (emuladores nuevos): plan acordado con Ridds — Dreamcast y PSP como núcleos; GameCube/Wii,
  PS2, 3DS y **Satronho NX** como apps que el Hub descarga, configura y abre (repo de NX aparte,
  dirección de descarga leída de un archivo que Ridds controla; llaves y firmware los pone cada
  persona con un paso guiado; en PC se pueden importar desde Eden/Yuzu/Ryujinx; en Android se pasan
  a NX por intent). Ñoño avisa si el equipo alcanza antes de descargar.
- Editor de Memory Cards más completo (copiar entre tarjetas, exportar/importar .mcs).

## Estado al 24 de noviembre de 2026 (etapa 2: partes A y B)

Ridds pidió A y B de una sola vez, sin preguntas («me voy a dormir»). La parte C (emuladores
nuevos: Cubo, Sueño, CD2, Portátil, 3DS) queda **para cuando él la pida**. Se trabajó en la rama
`etapa-2` y se unió a `main`. Publicada como `build-14`: el Release lleva el zip de Windows y el APK de Android (verificado).

### Qué trae

- **.zip y .7z** (`util/archive.*`, miniz + LZMA SDK en `external/`, librería `satronho_archivos`).
  La biblioteca los lista; si un archivo trae varios juegos, `DrawArchivePicker` pregunta cuál.
  Cartuchos: se cargan en memoria (`CoreHost::LoadGame(…, data)`). Discos: se sacan a
  `user/cache/descomprimidos/<clave>` (marca `.listo`, tope 4 GB, se borra lo más viejo:
  `TrimArchiveCache`). Ajustes » Biblioteca » «Vaciar la caché».
- **.chd propio** (`util/chd.*`): CHD v5 con el códec cdzl, pistas MODE1/2352, MODE2/2352 y AUDIO,
  pregap INDEX 00. Comprobado con chdman (mismas huellas SHA1) y Beetle lo abre. Siempre opcional:
  Ajustes » Biblioteca » «Discos a .chd», con «Borrar los originales al terminar» apagado de fábrica.
  Si hay `.chd` y `.cue` con el mismo nombre, la biblioteca muestra solo el `.chd`.
- **Android** (`android/`, `src/sys/android.*`): Android 8+, solo arm64. Gradle compila el mismo
  `CMakeLists.txt` como `libmain.so` (SDL3 compartido, GLES 3). `SatronhoActivity.java` copia
  `assets/`, pide «acceso a todos los archivos», descarga, vibra e instala APKs (actualizador).
  Los núcleos los compila `scripts/build-core-android.sh` (8; **Mega usa Genesis Plus GX** en
  Android porque BlastEm no compila para ARM; su licencia prohíbe venderlo: Satronho debe seguir
  gratis y sin anuncios). CI: `.github/workflows/android.yml` (en `etapa-2` solo compila; desde
  `build.yml` en `main` sube `SatronhoEmulation-build-N-android-arm64.apk` + `.sha256` al Release).
- **Control en pantalla** (`app_touch.cpp`): modo táctil y barra de íconos si la pantalla es chica;
  cruceta/stick, botones por consola, gatillos, menú, vibración; editor para mover grupos
  (`tactil.pos.<consola>.<grupo>`), tamaño y transparencia. Explorador de archivos propio en Android.
- **Acabados**: presentación animada (`DrawSplash`), fondo con la carátula borrosa del juego
  señalado (`DrawBackdrop`, `Covers::GetBlur`), guías que se mueven (`Ui::GuideBob`), fundido al
  entrar al juego. Ñoño explica .zip/.7z/.chd en el chat.

### Firma del APK (pendiente de Ridds)

El proxy de esta sesión no deja escribir secretos de GitHub. Ridds debe crear en el repo
(Settings » Secrets and variables » Actions) `ANDROID_KEYSTORE_B64` y `ANDROID_KEYSTORE_PASS` con la
llave que le mandé. Mientras falten, el CI firma con una llave provisional distinta en cada
ejecución: esos APK no se actualizan uno sobre otro (hay que desinstalar e instalar).

### Probado (Linux)

CD con OpenGL y Vulkan desde un .zip, 64 con ParaLLEl-RDP desde un .7z, elegir juego en un zip con
varios, conversión a .chd, control en pantalla y editor, presentación, novedades. Windows compila
con llvm-mingw sin avisos. El APK compila en el CI con los 8 núcleos.

### Falta probar en un teléfono real (no hay dispositivo en la sesión)

1. Que arranque, pida el permiso de archivos y encuentre los juegos.
2. CD y 64 con GLES (GLideN64) y con Vulkan en un Android de gama media/alta.
3. Control en pantalla: toques simultáneos, vibración, editor.
4. Actualizar desde la app (necesita la llave definitiva).
5. Mandos Bluetooth en Android.

### Ideas que quedan

- Parte C: emuladores nuevos. Vulkan sin copia a memoria. RetroAchievements. Trucos con Satronho
  Cheats. Versión de Android para x86_64 (emuladores de PC) si Ridds la quiere.

## Estado al 24 de noviembre de 2026 (etapa 1 completa)

Etapa 1 hecha en una sola tanda (pedido de Ridds): acabados, guías con las que se habla, temas, carrusel,
confirmación al salir, novedades de Juana, carátulas y motor gráfico OpenGL + Vulkan. Se trabajó en la
rama `etapa-1` (con push frecuente para no perder nada si se corta la sesión) y se unió a `main`.

Publicada como `build-13`.

### Qué trae

- **Motor gráfico OpenGL** (`src/gfx/`): ventana con contexto OpenGL 3.3 core, ImGui con
  `imgui_impl_opengl3`, imagen del juego con sombreadores (`video_gl.cpp`: nítido suave, líneas, CRT con
  máscara, LCD, viñeta y fondo ambiente). Funciones de GL cargadas a mano (`gl.h`, sin enlazar
  opengl32). Si OpenGL 3.3 no arranca, cae solo al motor clásico (`core/video.cpp`, SDL_Renderer).
  Ajustes » Imagen » Motor gráfico: «auto»/«opengl» = OpenGL; otro valor = clásico con ese driver.
  `SATRONHO_GFX=opengl|software|direct3d11…` lo fuerza (pruebas).
- **Dibujo por tarjeta gráfica para los núcleos** (`hw_render.cpp`): `SET_HW_RENDER` con OpenGL (contexto
  propio del núcleo, compartido con el de la ventana; framebuffer + textura compartida; valla al
  volver) y Vulkan (`vulkan_bridge.cpp`: carga vulkan-1.dll al vuelo, negociación v1 con
  `create_device`, interfaz v5; cada cuadro se copia a memoria y se sube a una textura de OpenGL).
  `CoreHost` llama `Enter/Leave` alrededor de run, serialize, reset y unload.
- **CD** ahora es Beetle PSX **HW** (`mednafen_psx_hw`, opciones `beetle_psx_hw_*`; las viejas
  `beetle_psx_*` se migran solas en `App::Init`). **64** usa GLideN64 (OpenGL) o ParaLLEl-RDP (Vulkan);
  Angrylion queda para equipos sin tarjeta. `App::ApplyGpuChoice` pone motor y resolución antes de
  cargar el juego (`gpu.<consola>.motor` 0 auto/1 GL/2 VK/3 CPU y `gpu.<consola>.resolucion`).
  Automático: Vulkan en NVIDIA/AMD con Vulkan, OpenGL en el resto; resolución según el alto de la
  pantalla, máx. 2× en integradas.
- **Temas** (`Ui::Themes`, 5) y **logo de fondo** con opacidad 10–100 % (`interfaz.tema`,
  `interfaz.marca_agua`); los colores de `Ui::Color` ahora son variables.
- **Carrusel** por consola (`biblioteca.vista.<consola|todas>`), **carátulas** (`sys/covers.cpp`:
  libretro-thumbnails por nombre exacto o por índice con la región más parecida; `caratulas/`,
  `caratulas/propias/`, `.nada` = no se encontró, se reintenta al mes).
- **Preguntar al salir** (app y juego, con «No volver a preguntar»: `avisos.salir_app/juego`).
- **Novedades de Juana** en ventana al detectar build nueva.
- **Conversación con los guías** (`sys/guide_chat.cpp`, página Guías): reglas por palabras clave, sin
  internet, con datos reales (equipo, veredictos de Ñoño, juegos, mandos, atajos, BIOS, actualizador)
  y botones que llevan a la pantalla. Reparto de apariciones (`PickGuide`): MechaRidds 40 %, el resto
  Samy/Troya 65 %, Ñoño 25 %, Juana 10 %.

### Probado (Linux, Mesa llvmpipe/lavapipe)

- CD con OpenGL y con Vulkan (demo PSX hecha con un .exe de PeterLemon/PSX en un .cue), estados con
  miniatura, menú, salir. 64 con GLideN64, ParaLLEl-RDP (Vulkan) y Angrylion (pruebas RDP de
  PeterLemon/N64). Motor clásico y OpenGL en todas las páginas nuevas. Compila para Windows con
  llvm-mingw sin avisos, y Beetle PSX HW también.

### Falta probar en Windows (pedírselo a Ridds)

1. CD y 64 en una tarjeta NVIDIA/AMD real con Vulkan y con OpenGL; en una Intel con OpenGL.
2. Cambiar de consola varias veces seguidas (CD con Vulkan → 64 con OpenGL → CD) en una sesión.
3. Carátulas con internet (índice de thumbnails.libretro.com y GitHub).
4. Lo de la lista de la build 12 sigue pendiente (vibración, cambio de disco, F2 en Dual…).

### Ideas que quedan

- Vulkan sin copia a memoria (interop `GL_EXT_memory_object_win32`) para resoluciones muy altas.
- Adelanto de cuadros con segunda instancia; RetroAchievements; trucos con Cuaderno de Trucos.

## Estado al 23 de noviembre de 2026 (build 12)

Publicada como Release `build-12` (zip + `.sha256`, verificado). La ejecución 11 del CI falló antes
de empezar por unas comillas mal escapadas en el workflow (ya arreglado), por eso no existe
`build-11`. La caché de núcleos ya quedó llena: las próximas builds tardan mucho menos.

**Fase actual:** hub con 9 emuladores (Octa, Mega, Hexa, CD, 64, Dual, Anillo, Bolsillo, Bolsillo
Plus). Solo Windows x64 se publica; Linux compila para pruebas. Android y las consolas pesadas
(Cubo, Sueño, CD2, Portátil, 3DS) son fases siguientes: **no empezarlas sin que Ridds lo pida.**

### Qué se hizo en la build 12 (revisión completa + mejoras, sin cambiar de fase)

Arreglos:
- Partidas `.srm` por consola (`partidas/<consola>/`), con migración desde la carpeta vieja (solo si
  el tamaño coincide) y copia `.srm.bak`.
- BIOS de PS1: nombres que de verdad abre Beetle PSX (5500/5501/5502/5503/5552/7003) y
  reconocimiento por SHA-1; SCPH-1001/7001… se guardan con el nombre de su región.
- Opciones de núcleo por consola (Bolsillo y Bolsillo Plus ya no se pisan); migración automática
  desde las claves viejas por núcleo.
- Búsqueda de juegos en fila (no se pierde una carpeta agregada durante una búsqueda) y cancelable.
- Adelanto de cuadros explicado como «solo NES» en la interfaz y en Ñoño.
- Rebobinar comprimido (parches XOR) y apagado por defecto en consolas pesadas.
- Mandos recordados por GUID; turbo con contador propio; cambio de disco con la tapa abierta 45
  cuadros; frecuencia de sonido que cambia a mitad del juego; Alt+F4 sin reiniciar; nombres de
  perfil validados; modo portátil con rutas relativas; registro de núcleos con candado; opciones v1
  con traducción; actualizador con SHA-256 y reemplazo seguro de carpetas.
- Textos viejos que solo hablaban de NES/Game Boy; aviso de marcas completo.

Mejoras:
- Guardado automático al salir + «Continuar» desde la biblioteca; «Deshacer la carga».
- Vibración (Rumble Pak, DualShock); aviso cuando una tecla choca con un atajo.
- Atajo «Cambiar pantallas» en Dual (F2 / clic del stick derecho → R3 en melonDS DS) y 3 formas de
  pantalla por defecto; el mouse no se esconde en Dual.
- Imagen propia por consola; Ñoño dice cómo va cada consola; pausar al perder el foco (opcional).
- Ocultar juegos; pantalla «Cargando…»; barra lateral desplazable; lista de discos en el menú.
- **Avance rápido apagado de fábrica y sin botón de mando** (pedido de Ridds: el gatillo chocaba).
- CI: núcleos en caché, prueba de arranque antes de publicar, `.sha256` del zip, publica solo desde
  `main`. Versión real en Propiedades del `.exe`. Registro de cierres inesperados en Windows.

### Probado

- Compila sin avisos en Linux (GCC) y en Windows (llvm-mingw 20251104, como el CI).
- Modo de revisión en todas las páginas principales, sin cierres.
- Con un juego real (Nestopia nativo + homebrew `hello.nes`): guardar/cargar ranura, guardado al
  salir, «Continuar» (carga la ranura automática), menú de estados.
- Rebobinado: prueba con 200 estados de 4 MB (con un cambio de tamaño): reconstrucción exacta.

### Falta probar en Windows con juegos reales (pedírselo a Ridds o hacerlo si hay forma)

1. **Cambiar de núcleo varias veces** (N64 → NES → N64, DS → GBA → DS). Descargar un `.dll` con
   hilos (Mupen64Plus-Next, melonDS DS) puede colgar la app. Si pasa, el registro ahora anota el
   archivo y la posición del fallo. Posible arreglo: no hacer `SDL_UnloadObject` de esos núcleos.
2. Vibración con un mando real en un juego de N64 con Rumble Pak y uno de PS1 con DualShock.
3. Cambio de disco en un juego de PS1 de varios discos (¿45 cuadros alcanzan?).
4. Atajo F2 en Dual (¿R3 durante 3 cuadros basta para melonDS DS?).
5. Partidas migradas desde `partidas/` (build 10) a `partidas/<consola>/`.
6. Actualizar desde la build 10 a la 11 con el botón de la app (el instalador viejo no comprueba
   la huella; desde la 11 sí).

### Ideas que quedan (sin empezar; preguntar a Ridds antes)

- Dibujo por tarjeta gráfica (contexto OpenGL/Vulkan en el host) → GLideN64 / ParaLLEl-RDP en 64 y
  Beetle PSX HW: es lo que más rendimiento daría a 64 y CD. Es un cambio grande (otra fase).
- Adelanto de cuadros con segunda instancia del núcleo (para que sirva en SNES, GBA…).
- Portadas reales (libretro-thumbnails), trucos (se podría conectar con su app Cuaderno de Trucos),
  RetroAchievements, versión Android.
