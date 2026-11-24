# Continuar Satronho Emulation

Notas de traspaso entre sesiones de Claude. Lo más nuevo va arriba. Lee también `CLAUDE.md` (cómo
trabaja Ridds) y `SATRONHO.md` (cómo está hecho).

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
