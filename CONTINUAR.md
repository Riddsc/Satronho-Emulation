# Continuar Satronho Emulation

Notas de traspaso entre sesiones de Claude. Lo más nuevo va arriba. Lee también `CLAUDE.md` (cómo
trabaja Ridds) y `SATRONHO.md` (cómo está hecho).

## Estado al 23 de noviembre de 2026 (build 11)

**Fase actual:** hub con 9 emuladores (Octa, Mega, Hexa, CD, 64, Dual, Anillo, Bolsillo, Bolsillo
Plus). Solo Windows x64 se publica; Linux compila para pruebas. Android y las consolas pesadas
(Cubo, Sueño, CD2, Portátil, 3DS) son fases siguientes: **no empezarlas sin que Ridds lo pida.**

### Qué se hizo en la build 11 (revisión completa + mejoras, sin cambiar de fase)

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
