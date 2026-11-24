#!/usr/bin/env bash
# Compila el núcleo de Nintendo 64 (Mupen64Plus-Next) para Windows con llvm-mingw.
# Uso: scripts/build-core-mupen64plus.sh <carpeta_de_salida>
# Variables: LLVM_MINGW (ruta de llvm-mingw), MUPEN_COMMIT (versión fija). Necesita nasm.
set -euo pipefail
OUT="${1:?carpeta de salida}"
COMMIT="${MUPEN_COMMIT:-15bada85906593376e8fd697f1b0b3af3a20f512}"
WORK="$(mktemp -d)"
git init -q "$WORK/n64"
git -C "$WORK/n64" remote add origin https://github.com/libretro/mupen64plus-libretro-nx.git
git -C "$WORK/n64" fetch -q --depth 1 origin "$COMMIT"
git -C "$WORK/n64" checkout -q FETCH_HEAD
export PATH="$LLVM_MINGW/bin:$PATH"
# MSYSTEM=MINGW64 elige el dynarec x86_64 y trae Angrylion y ParaLLEl-RSP (no hacen falta
# OpenGL ni Vulkan). LDFLAGS propio: lld no acepta --version-script en Windows.
make -C "$WORK/n64" -j"$(nproc)" platform=win MSYSTEM=MINGW64 \
  CC=x86_64-w64-mingw32-clang CXX=x86_64-w64-mingw32-clang++ AR=llvm-ar \
  LDFLAGS="-shared -static -Wl,-no-undefined -pthread -lwinmm"
mkdir -p "$OUT"
x86_64-w64-mingw32-strip -s "$WORK/n64/mupen64plus_next_libretro.dll" -o "$OUT/mupen64plus_next_libretro.dll"
mkdir -p licencias
cp "$WORK/n64/LICENSE" licencias/Mupen64PlusNext-GPL-2.0.txt
echo "Núcleo listo: $OUT/mupen64plus_next_libretro.dll (Mupen64Plus-Next $COMMIT)"
