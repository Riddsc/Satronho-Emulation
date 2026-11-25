# Continuar Satronho Emulation

Notas de traspaso entre sesiones de Claude. Lo más nuevo va arriba. Lee también `CLAUDE.md` (cómo
trabaja Ridds) y `SATRONHO.md` (cómo está hecho).

## Build 25 en curso (rama `build-25`): pantallas nativas de Android, como Satronho NX

Ridds vio Satronho NX como app aparte (video) y pidió que el hub en Android funcione **exactamente como NX**:
pantallas nativas (Kotlin + Jetpack Compose), control en pantalla nativo, biblioteca en vertical y juego
acostado. **NX queda para la build 26, enlazada** (el hub la abre como app aparte; no va dentro). Esto reemplaza
el «build 25 = NX» de abajo. Plan aprobado: tres APK de prueba antes de publicar la 25.

- **Prueba 1 (subida, commit 41a0abc)**: Release `prueba-25` (marcado como prueba; la app no lo ofrece como
  actualización; versionCode 25 para instalar encima de la 24). Lo sube `android.yml` en cada push a `build-25`.
- Arquitectura: una sola actividad. `SDLActivity` ahora extiende `androidx.activity.ComponentActivity`
  (Compose necesita su ciclo de vida). `SatronhoUi.instalar` pone un `ComposeView` encima de la superficie de
  SDL. Kotlin → C++: órdenes de texto (`Puente.orden("jugar|clave")`, atendidas en `App::NativeCommand`).
  C++ → Kotlin: estado y biblioteca en JSON con número de versión (`NativeStateJson`, `NativeLibraryJson`,
  leídos cada cuadro con Choreographer) y avisos sueltos (`NativeUi::Emit`). Control: `Puente.tactil(bits,…)`
  → `NativeApplyTouch`. Archivos: `src/sys/native_ui.*`, `src/app_native.cpp`, `android/.../*.kt`.
- Lo que todavía no es nativo se sigue viendo con ImGui: si `NativeNeedsImGui()` (Ajustes, explorador,
  bienvenida, preguntas, editor del control…) Kotlin se esconde y los toques van a SDL. «Atrás» lo atiende
  primero Kotlin (`SatronhoActivity.dispatchKeyEvent` → `SatronhoUi.atras`); si no lo usa, va a SDL.
- `TouchReserve` vale 0 en modo nativo (el control va encima y la imagen conserva su forma); sin barra de menús.
- Cruceta nativa: cardinales 72°, diagonales 18°, centro muerto 30 %, histéresis 6°; `tactil.solo4` en el ini.
- Ñoño (juego lento): ahora guarda la resolución **por juego** (`SlowLower`), tarjeta nativa en la esquina.
- **Prueba 2 (subida, junta la 2 y la 3 que había planeado; Ridds pidió ir rápido)**: todo nativo en el
  celular. Ajustes: C++ describe cada pantalla con `NPage` (`src/native_page.h`, páginas en
  `app_native_pages.cpp`; Gráficos/Núcleo en `app_graphics.cpp`, Herramientas en `app_tools.cpp`) y Kotlin la
  dibuja (`Ajustes.kt`). Kotlin abre con `ajustes|página|arg`, cambia con `ajuste|id|valor` (los «setters» se
  guardan en `native_setters_` cada vez que se arma la página, cada 0,25 s). Nativo también: explorador
  (`Ventanas.kt`), bienvenida, confirmar salir, recibir juegos y mudanza (`progreso`), .zip con varios juegos,
  chat con los guías, ajustes de un juego, editor del control (arrastrar grupos → `tactil_mover`), medidor y
  estado del juego. `NativeNeedsImGui()` ya devuelve false en el celular. Mando físico: `SatronhoUi.teclaMando`.
- Arreglos tras la prueba 1 (video de Ridds): letras negras (faltaba LocalContentColor), textos nuevos sin
  traducir (agregados a `scripts/idiomas/traducciones/extra.txt` + `armar.py`), «Lista» = «Ready» (ahora
  «En lista»), guías con nombre en su color y reparto (Samy 32, Troya 26, Ñoño 16, Juana 14, MechaRidds 12, cada
  20 s), menú «Más» de PC con el ícono encima del texto. Logo de Satronho Emulation al frente (presentación,
  encabezado, Ajustes, cajón, carga).
- **Prueba 3 (subida, 93792ea)**: estética de Satronho. `Mandos.kt` es el dibujo de PC pasado a Kotlin sin
  nada de Android (controles de `controllers.cpp`, botones del control en pantalla de `app_touch.cpp` y
  cartuchos de `DrawCartridge`), sobre la interfaz `Pintor`; `Lienzo.kt` lo pinta en Compose. Se puede probar
  fuera de Android con kotlinc (bajado de GitHub) y un Pintor de java.awt que guarda PNG. Vista carrusel
  (`vista.modo == 3`, HorizontalPager). Chat con preguntas sugeridas (`sugerencia|i`).
- **NX va en la build 26** (Ridds lo confirmó). La rama `android` de Riddsc/satronho-nx ya usa
  `applicationId "dev.satronho.nx"` («Satronho NX»); `main` sigue con el de Eden (escritorio). El repo de NX
  es **público**. Pendiente de Ridds: ¿el hub descarga NX para cualquiera o solo lo abre si ya está instalado?
- Pendiente: navegar la biblioteca nativa con un mando (hoy las teclas del mando van a SDL); el medidor sigue
  siendo el de ImGui; el APK pesa ~41,7 MB (24: 35,2 MB) por Compose sin minificar.

## Estado al 25 de noviembre de 2026 (build 24)

Ridds mandó el registro de Android (Mali-G78, 8 hilos, 120 Hz, app en inglés): WALL-E (PSP) andaba bien, pero al
volver a la biblioteca la app se cerraba (también en PC). Pidió mejorar el control en pantalla (X, O, «Cross»…)
y dejó trabajar solo, sin capturas ni aprobaciones (se fue a dormir).

- **Cierre al salir de PSP** (`CoreHost::UnloadGame`): se llamaba `context_destroy` **después** de
  `retro_unload_game`; PPSSPP borra `ctx` en `retro_unload_game` y su `context_destroy` lo usaba → cierre.
  Ahora va antes, como RetroArch (`ContextDestroy` de PPSSPP está hecho para eso: para el hilo y suelta la GPU).
  Probado con Beetle PSX (OpenGL y Vulkan): sale bien. PPSSPP no se pudo probar aquí (sandbox de 2 núcleos).
- **Control en pantalla nuevo** (`app_touch.cpp`: `LookFor`, `DrawDisc`, `DrawPill`, `DrawGlyph`): relieve,
  sombra, brillo y hundido al apretar; colores y figuras por consola (busca por el nombre del botón en
  `SystemDef::buttons`: «Triángulo», «C arriba»…). Cruceta sólida con flechas grabadas; palanca con base y
  cabeza. Para probar sin juego: un .cue con un .bin vacío abre OpenBIOS en Beetle PSX (`/tmp/psx/prueba.cue`)
  y `tactil=1` muestra el control; para ver otras consolas se puede cambiar `sys_` en `DrawTouchPad` a mano.
- **Textos de PPSSPP**: `scripts/ppsspp-assets.sh` baja también `lang/` (es_ES, es_LA, en_US, fr_FR, pt_BR,
  pt_PT, it_IT).
- La caché de sombreadores de PPSSPP no se puede guardar («Mode not yet supported (libretro vfs): ab»): es del
  propio núcleo con libretro (pasa igual en RetroArch); no tiene arreglo desde el hub.
- Las ventanas emergentes sí salen en las capturas de QA con `rom=`; con `page=` a veces no (no investigado).

### Falta probar (pedírselo a Ridds)

- Volver a la biblioteca desde WALL-E (PSP) en PC y celular sin que se cierre; y desde un juego de Sueño.
- Cómo se ve el control nuevo en cada consola; la transparencia (Ajustes » Controles) sigue funcionando.
- Lo pendiente de la 23: Vigilante 8 a 2×, Twisted Metal/DBZ con el DualShock.

### Siguiente (aprobado por Ridds): build 25 = Satronho NX

- Repo `Riddsc/satronho-nx` (fork de Eden). Todavía usa `applicationId` `dev.eden.eden_emulator`: darle uno propio
  (p. ej. `com.ridds.satronho.nx`) y publicar el APK en sus Releases. El hub lo baja, lo instala, revisa el
  equipo (Ñoño: los Mali van peor que los Adreno) y abre los juegos con un `Intent` VIEW (Eden acepta
  `content://` con `application/octet-stream`; el hub necesitará un FileProvider).

## Estado al 24 de noviembre de 2026 (build 23)

Ridds probó la build 22 en el celular (pantalla de 1440 px de alto, app en inglés): Vigilante 8 iba a 6–23
cuadros por segundo en los menús y se congelaba en la pantalla de carga (0 cuadros). Twisted Metal no
respondía a los controles, igual que DBZ. En Ajustes la resolución automática de CD decía **8×**.

- **Resolución automática en Android** (`GpuResFor`): tope 2× (CD 480 de alto, 64 480, Sueño 960, Portátil
  544). El tope de PC solo reconocía tarjetas de PC (Intel/NVIDIA…), y en el celular elegía la más parecida
  a 1440 px: 8×. Nota en Gráficos solo en Android.
- **DualShock en digital** (`cd.core_defaults`): `beetle_psx_hw_analog_toggle=enabled` (antes «disabled» =
  siempre analógico, y los juegos viejos no lo aceptan) y combo `l1+r1+select`. Menú Juego » «Botón Analog
  del DualShock» aprieta el combo 1,3 s (`analog_pulse_`). «Mando normal» por juego sigue.
- **Ñoño y el juego lento** (`WatchSlowGame`/`DrawSlowPrompt` en `app_extra.cpp`): consolas con `gpu`,
  pasados 12 s, si va por debajo de 2/3 de su velocidad durante 6 s se pausa y ofrece bajar un escalón:
  guarda en la consola (o en el juego si tenía la suya), guarda la ranura 0 y reabre con «Continuar». Una
  vez por juego. Ya en la más baja: aviso.
- **Sonido que frenaba el juego**: el ajuste de cuadros según el búfer de sonido podía dejar el juego en 0
  cuadros para siempre si la salida no consumía. Si pasan 0,4 s sin correr cuadros, se vacía y se sigue
  (log «El sonido no avanzaba…»). Puede haber sido parte del congelamiento.
- **Recortar arriba** (`beetle_psx_hw_initial_scanline`, 2 de fábrica): la rayita de basura de Vigilante 8.
- **Enviar el registro** (Ajustes » Rendimiento, MechaRidds): junta el final de `satronho.anterior.log` y
  `satronho.log` (60 KB cada uno), lo guarda como `satronho-registro.txt` en «Satronho Games» y abre
  Compartir de Android (`compartirTexto` en `SatronhoActivity`); en PC lo copia al portapapeles.
  `Log::Flush()` nuevo.
- Modo de revisión: las ventanas emergentes (`BeginPopupModal`: salir, ajustes del juego, juego lento) no
  salen en las capturas aunque se dibujan; no es de esta build.

### Falta probar (pedírselo a Ridds)

- Vigilante 8 en el celular: que diga 2× en automática, que vaya fluido y cargue el nivel.
- Twisted Metal y DBZ con el DualShock (sin cambiar a «Mando normal»).
- Que Ñoño ofrezca bajar la resolución si se sube a mano a 8× y el juego va lento.
- «Enviar el registro» por WhatsApp.
- Sigue pendiente lo de la build 22: un juego de Dreamcast y uno de PSP.

## Estado al 24 de noviembre de 2026 (build 22)

Ridds probó la build 21: lo de WhatsApp, el permiso y la mudanza de carpeta funcionan. Reportó sonido
perdido, cruceta táctil cruzada y que no encontraba «Mando normal» en Dragon Ball GT. Aprobó empezar la
**parte C** con Dreamcast y PSP dentro del hub. Rama `build-22`.

- **Sonido** (`AudioOut::Reopen`): al abrir cada juego se cierra y se abre la salida de sonido (antes el
  primer juego sonaba y los siguientes no, en Android). `Update()` reanuda el dispositivo si quedó en pausa
  sin pedirlo (log «La salida de sonido estaba en pausa sin pedirlo»). Al arrancar se escribe en el log
  «Sonido: N Hz, volumen …» y, si el volumen es 0, un aviso. **Si sigue mudo, pedirle el log a Ridds.**
- **Cruceta táctil** (`app_touch.cpp`): rectas ±30°, diagonales ±15°, histéresis de 8° (guarda `Finger::dir`)
  y zona muerta 0,22·r. Palanca: zona muerta 0,15 y un eje se anula si es menor que 0,38 del total. Las
  consolas con palanca (menos la 64) tienen palanca chica bajo la cruceta.
- **Ajustes de este juego con el juego andando**: menú Juego » «Ajustes de este juego…»; el mando se cambia
  al momento con `CoreHost::SetPortDevice` (motor/resolución: al volver a abrir). Dragon Ball GT puede que
  igual no ande con mando normal: es el juego.
- **Satronho Sueño** (`dc`, id `sueno`, Flycast) y **Satronho Portátil** (`psp`, id `portatil`, PPSSPP):
  `SystemDef::gpu` (consola con motor y resolución propios) y `gpu_cpu` (Sueño no puede con el procesador).
  Resoluciones: Sueño altos exactos 4:3 (480…2880), Portátil múltiplos de 272. Dibujos `DrawDreamcast` y
  `DrawPsp`. BIOS de Sueño opcional (`dc/dc_boot.bin`, `dc/dc_flash.bin`; sin ella `flycast_hle_bios`).
  Detección de discos en `library.cpp`: CHD con «CHGD» = Dreamcast, .pbp con PSISOIMG/PSTITLEIMG = PS1, si no
  PSP, pista con «SEGA SEGAKATANA» = Dreamcast, y pistas de la ruta (carpeta «dreamcast», «psp»…). `.gdi` se
  lee como hoja de pistas.
- **Núcleos de Sueño y Portátil**: no se compilan (tardan horas); `scripts/build-core-descargado.sh` los baja
  de buildbot.libretro.com (nocturna) para Windows y Android. Desde el sandbox buildbot da 403: **no se
  pudieron probar con juegos aquí**. Los archivos de sistema de PPSSPP (`flash0`, letras, `compat.ini`,
  shaders) los baja `scripts/ppsspp-assets.sh` a `assets/system/PPSSPP` (no van en git) y `Paths::Init`
  copia todo `assets/system` a `sistema/` (también en Android).

### Falta probar (pedírselo a Ridds)

- Que el sonido siga en el segundo y tercer juego (PC y celular).
- Cruceta táctil: arriba sin izquierda/derecha; diagonales; palanca chica en CD.
- Un juego de Dreamcast (.gdi/.chd) y uno de PSP (.iso/.cso) en PC y en el celular: que abran, la imagen,
  la resolución, el control en pantalla y que caigan en su consola.

### Ideas que quedan

- Parte C, siguiente: Cubo (GameCube/Wii), CD2 (PS2), 3DS y Satronho NX como apps aparte que el hub baja,
  configura y abre, con Ñoño revisando el equipo antes.
- Fijar versión de Flycast/PPSSPP (hoy nocturna) si alguna se rompe.

## Estado al 24 de noviembre de 2026 (build 21)

Ridds mandó un video del celular (build 19) y pidió revisar el repo entero antes de la parte C. Rama
`build-20`; se publicó como **build-21** porque la prueba completa del CI en la rama (`gh workflow run
build.yml --ref build-20`) gastó el número 20 (el número del Release es el de la ejecución de `build.yml`;
para probar en una rama sin gastar números basta `android.yml`). Lo siguiente sigue siendo **agregar consolas** (parte C, ver «Ideas que quedan» de la build 15).

- **Biblioteca que no bajaba** (desde la build 18): en `DrawLibrary` el encabezado comparaba posiciones de
  pantalla con `0.0f` (`narrow ? search_bottom : 0.0f`); al bajar, el título queda arriba con posiciones
  negativas y los juegos se pegaban arriba en cada cuadro (página infinita). Ahora compara con `after_title`.
- **Página que se iba de lado**: `Ui::BackToTop()` ponía el botón con `Scroll.x/y`, eso agrandaba la página
  y crecía con cada desplazamiento. Ahora el botón no cuenta como contenido (guarda y devuelve
  `CursorMaxPos`). Helper nuevo `Ui::KeepContentSize` (lo usa también el carrusel: sus carátulas de los
  bordes agrandaban la página de lado). Cada pantalla y cada consola empiezan arriba (`page_scroll_reset_`).
- **Prueba de desplazamiento** (`desplazar=1`, `App::QaScrollTest`): baja y mueve de lado; si la página se
  corre de lado o sigue creciendo, la app sale con **código 2**. En el CI (`build.yml`) con 40 juegos falsos en
  13 pantallas. Crecer una vez está bien (paneles que se miden al verlos, como en Novedades).
- **Abrir juegos**: ▶ de verdad en las tarjetas (antes solo dibujo); con el dedo, primer toque elige y el
  segundo abre (tarjeta y fila); doble toque con `MouseDoubleClickMaxDist` S(36) y 0,40 s en táctil.
- **Discos en .zip/.7z en segundo plano** (`BeginUnpack`/`PollUnpack`/`DrawUnpack`): barra de avance (en
  bytes en .zip; los .7z van de un bloque y muestran barrita que va y viene) y Cancelar. Al terminar se pide
  el juego otra vez (`start_resume_` guarda si era «Continuar»). `Archive::ExtractAll` ahora recibe
  `bool(float)` (false = cancelar). QA: `descomprimir=archivo.zip`.
- **Android: carpeta pública «Satronho Games»** (`Paths::SetPublicRoot`): `Juegos/<Consola>`, `Partidas` y
  `Estados` (Saves/States la usan si está puesta). `SetupPublicFolder` al arrancar y cada 1,5 s hasta tener
  el permiso; la primera vez `BeginDataMove` muda lo de la carpeta de la app (renombra o copia+verifica+quita),
  pasa las claves de juego viejas a las nuevas (`Library::Rekey`, `Library::KeyFor`) y renombra los estados.
  Marca: `android.carpeta_publica` en satronho.ini. Cambiar de lugar: Ajustes » Biblioteca » Cambiar
  (explorador con `browser_for_root_`; mueve todo). Android 8–10: `tieneAccesoArchivos`/`pedirAccesoArchivos`
  ahora piden WRITE_EXTERNAL_STORAGE (código 7731). QA: `publica=/ruta` (prueba la mudanza en PC).
- **Margen de espacio**: `biblioteca.margen_gb` (2 GB, 1–32 en Ajustes » Biblioteca) y
  `SystemDef::margin_gb` para consolas que pidan más (PS2…). Lo usan la importación y `BeginUnpack`.
  Ajustes » Biblioteca muestra lo libre y cuánto ocupa cada consola en la carpeta de juegos.
- **Control táctil a los costados** (`TouchReserve`): acostado, el juego se dibuja entre las columnas del
  control (`SetArea(lx, …)`, `game_x_off_` para el lápiz de Dual); Select/Start y la pausa van a los costados
  (en vertical, como antes). El medidor se corre (`MeterSides`).
- **Barra del juego sin fondo en Android**: ImGui recortaba el fondo (la ventana es solo la barra de
  menús). Arreglado con `PushClipRect`. Aviso de inicio según dedo/mando/teclado.
- **Ajustes por juego** (`DrawGameSettings`, menú del juego › «Ajustes de este juego…»; con el dedo, toque
  largo `LongPressMenu`): tipo de mando (DualShock o normal, `ajuste.mando` en biblioteca.ini), motor y
  resolución en CD/64 (`game_api_`/`game_res_` solo al abrir). QA: `ajustes_juego=ruta`.
- Textos: `DrawEmptySystem` (consola sin juegos), plurales dentro de la frase, `TrFmt` y `HumanSize` ahora
  en `util/i18n.h`, sin el «nueve» fijo. «Verificar el archivo» ya no deja el hilo suelto. Sin avisos del
  compilador (Linux y llvm-mingw).
- El botón verde con un mando que se vio en el video (arriba a la derecha, también en la biblioteca) **no es
  de Satronho**: es el atajo de juegos del celular (Game Booster de Samsung o similar).

### Falta probar (pedírselo a Ridds)

1. Bajar en la biblioteca (Cuadrícula y Lista) con el dedo y con la rueda: llega al final y no se corre de lado.
2. Tocar un juego y volver a tocarlo (o su ▶) para abrir; toque largo para el menú.
3. Abrir un disco en .zip: barra de avance y Cancelar; la segunda vez abre al instante.
4. Al abrir la build 21 en el celular: que pida el permiso, mude todo a «Satronho Games» (con tiempos y
   favoritos) y que los juegos de WhatsApp lleguen ahí.
5. Dragon Ball GT Final Bout con «Ajustes de este juego» › Mando normal: ¿se va el aviso del mando?
6. El control en pantalla a los costados en CD y en 64 (que nada tape la imagen y todo se alcance con los dedos).
7. Pendientes de la build 18 que siguen sin probar: vertical (Ajustes, Controles, chat con teclado) y Atrás /
   deslizar desde arriba dentro del juego.

### Ideas que quedan

- Copiar los assets en un hilo al primer arranque de Android (hoy `prepareFiles` va en el hilo principal).
- Los .7z grandes se sacan de un solo bloque en memoria: un disco de 700 MB en .7z pide 700 MB de RAM.
- Traducir las notas de Novedades (hoy salen en español en todos los idiomas).

## Estado al 24 de noviembre de 2026 (build 19)

- Arreglo: aviso de ImGui «SetCursorPos… submit an item» lo causaba `Ui::BackToTop()` (movía el cursor y lo
  devolvía sin elemento después); ahora pone un `Dummy` al final.
- Carrusel: se arrastra con mouse o dedo (`car_drag_`…), inercia al soltar; `carousel_rect_` evita que
  `TouchScroll` mueva la página al deslizar de lado sobre él (se borra cada cuadro y lo pone el carrusel).
- Ridds ya puso los secretos `ANDROID_KEYSTORE_B64` y `ANDROID_KEYSTORE_PASS`: desde la build 19 el APK sale
  con la llave de Satronho (la llave está en /home/claude/llave en la sesión que la creó; Ridds tiene copia).
  Esta vez tuvo que desinstalar la anterior (firma provisional).
- WhatsApp «Abrir con» funciona (probado por Ridds). Si solo Satronho abre .zip, Android lo abre directo.

## Estado al 24 de noviembre de 2026 (build 18)

Ridds quiso pasar juegos al celular por WhatsApp y no había app que abriera el .zip («no quiero nada
externo»). Mandó un video del celular: había que forzar horizontal, esconder barras y cámara, arreglar el
modo vertical (texto de una letra por línea, búsqueda encima del título, Controles cortado), el menú de
arriba del juego en Android y el desplazamiento con el dedo (la barra de consolas casi no se movía).
Rama `build-18`. Lo siguiente sigue siendo **agregar consolas** (parte C).

- **Abrir con / Compartir** (Android): filtros VIEW/SEND/SEND_MULTIPLE en el manifiesto. `SatronhoActivity`
  copia el `content://` a `<datos>/recibidos` (revisa espacio con StatFs) y C++ lo lee con
  `Android::Received()`/`ReceiveStatus()`. `app_import.cpp`: un .zip/.7z con un solo juego de cartucho queda
  comprimido; con varios (o de disco) se descomprime y cada juego va a `<datos>/Juegos/<Consola>/` (los .cue con
  sus pistas en una carpeta); BIOS a `system`; no repite (nombre + tamaño, también dentro de .zip). Al terminar
  agrega la carpeta Juegos y busca. QA: `recibir=/ruta/archivo.zip`. Ojo: `<datos>` es la carpeta de la app
  (se borra si se desinstala la app).
- **Pantalla Android**: `setOrientationBis` anulado (SDL ponía FULL_USER por ser ventana redimensionable);
  `Android::SetOrientation` con `interfaz.orientacion` (0 horizontal, 1 girar, 2 vertical). Siempre pantalla
  completa (`SetFullscreen`). `ApplySafeArea` achica la pantalla de ImGui durante el cuadro por la cámara
  (insets de SDL: solo barras + recorte, se quitaron los de gestos en `SDLSurface`) y por el teclado
  (`SDLActivity.sImeBottom`). QA: `recorte=l,t,r,b`, `teclado=1`, `ventana=720x1560` (quita el mínimo).
- **Desplazar con el dedo** (`TouchScroll`): el primer movimiento decide el sentido; busca la ventana que se
  puede mover en ese sentido. **Gráficos**: consolas en cuadrícula sin desplazamiento, fijas arriba
  (`##gfx_opciones` es lo que se desplaza), secciones plegables (`gfx_open_`), `Ui::BackToTop()`.
- **Vertical**: `RowBegin` se apila si la fila mide menos de 560; `DrawBottomNav` (vertical y compacto);
  pestañas de Ajustes en fila arriba en compacto; búsqueda bajo el título y héroe «Seguir jugando» angosto;
  Controles con `next()` que baja de línea lo que no cabe.
- **Menú del juego en Android**: Atrás lo despliega; deslizar desde el borde de arriba (`edge_swipe_`); al pausar;
  al desconectarse un mando. Con el dedo y la barra abierta el juego espera.
- **Guías en tu idioma**: `guide_chat.cpp` traduce con `I18n::Tr` (Fmt y textos) y `ToSpanishKeywords` pasa
  palabras clave de en/fr/pt/it al español para entender la pregunta.
- En el video, lo diminuto en horizontal era por la grabación (vertical), no por la escala: la escala ya usa
  la densidad de Android. Revisar con el log («Escala de la interfaz») si Ridds lo ve chico.

### Falta probar (pedírselo a Ridds)

1. WhatsApp » tocar un .zip » Satronho (y por Compartir). 2. Que abra en horizontal sin barras y la cámara
no tape nada. 3. Vertical: Ajustes, Controles, biblioteca, chat con teclado. 4. Atrás y deslizar desde
arriba dentro del juego.

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
