# Satronho Emulation · por Ridds

Logo, guías (Samy, Troya, Ñoño, Juana y MechaRidds), interfaz y diseño: Ridds.

Software libre bajo la licencia GPL 3.0 o posterior. Código fuente:
https://github.com/Riddsc/Satronho-Emulation

Componentes de terceros incluidos (licencias completas en la carpeta `licencias`):

- Nestopia UE — núcleo de Satronho Octa — GPL 2.0 o posterior — https://github.com/libretro/nestopia
- mGBA — núcleo de Satronho Bolsillo y Bolsillo Plus — MPL 2.0 — https://github.com/mgba-emu/mgba
- BlastEm — núcleo de Satronho Mega en PC — GPL 3.0 — https://github.com/libretro/blastem
- Genesis Plus GX — núcleo de Satronho Mega en Android — licencia propia, no comercial (Satronho es gratis) — https://github.com/libretro/Genesis-Plus-GX
- bsnes — núcleo de Satronho Hexa — GPL 3.0 — https://github.com/libretro/bsnes
- Beetle PSX HW — núcleo de Satronho CD, con OpenGL y Vulkan (incluye OpenBIOS) — GPL 2.0 — https://github.com/libretro/beetle-psx-libretro
- Mupen64Plus-Next — núcleo de Satronho 64 — GPL 2.0 — https://github.com/libretro/mupen64plus-libretro-nx
- melonDS DS — núcleo de Satronho Dual (incluye FreeBIOS) — GPL 3.0 — https://github.com/JesseTG/melonds-ds
- Beetle Saturn — núcleo de Satronho Anillo — GPL 2.0 — https://github.com/libretro/beetle-saturn-libretro
- SDL 3 — zlib — https://github.com/libsdl-org/SDL
- Dear ImGui — MIT — https://github.com/ocornut/imgui
- libretro API (también la de Vulkan) — MIT — https://github.com/libretro/libretro-common
- Vulkan-Headers — Apache 2.0 / MIT — https://github.com/KhronosGroup/Vulkan-Headers
- stb — dominio público / MIT — https://github.com/nothings/stb
- miniz — abre los .zip — MIT — https://github.com/richgel999/miniz
- LZMA SDK (7-Zip) — abre los .7z — dominio público — https://www.7-zip.org/sdk.html
- Poppins y JetBrains Mono — SIL Open Font License 1.1

Las carátulas de los juegos se descargan de libretro-thumbnails (https://github.com/libretro-thumbnails) cuando hay internet; no vienen incluidas y son de sus autores.

NES, Super Nintendo, Game Boy, Game Boy Advance, Nintendo 64 y DS son marcas de Nintendo; Genesis, Mega Drive y Saturn, de Sega; PlayStation, de Sony. Se usan solo para describir con qué juegos funciona cada emulador. Este proyecto no está afiliado a ninguna de ellas y no incluye juegos.

Las versiones exactas de cada núcleo están fijadas en los scripts `scripts/build-core-*.sh` del código fuente.
