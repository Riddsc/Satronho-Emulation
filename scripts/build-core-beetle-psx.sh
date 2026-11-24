#!/usr/bin/env bash
# Compila el núcleo de PlayStation (Beetle PSX, render por software) para Windows con llvm-mingw.
# Uso: scripts/build-core-beetle-psx.sh <carpeta_de_salida>
# Variables: LLVM_MINGW (ruta de llvm-mingw), BEETLE_PSX_COMMIT (versión fija).
set -euo pipefail
OUT="${1:?carpeta de salida}"
COMMIT="${BEETLE_PSX_COMMIT:-fb70b69a1aef1f60d9fc139ab7df34a5742ccdfd}"
WORK="$(mktemp -d)"
git init -q "$WORK/beetle"
git -C "$WORK/beetle" remote add origin https://github.com/libretro/beetle-psx-libretro.git
git -C "$WORK/beetle" fetch -q --depth 1 origin "$COMMIT"
git -C "$WORK/beetle" checkout -q FETCH_HEAD
export PATH="$LLVM_MINGW/bin:$PATH"
# lld no acepta --version-script en Windows: se reemplaza SHARED. Sin BIOS usa OpenBIOS (incluida).
make -C "$WORK/beetle" -j"$(nproc)" platform=win \
  CC=x86_64-w64-mingw32-clang CXX=x86_64-w64-mingw32-clang++ \
  SHARED="-shared -static -Wl,-no-undefined"
mkdir -p "$OUT"
x86_64-w64-mingw32-strip -s "$WORK/beetle/mednafen_psx_libretro.dll" -o "$OUT/mednafen_psx_libretro.dll"
mkdir -p licencias
cp "$WORK/beetle/COPYING" licencias/BeetlePSX-GPL-2.0.txt
echo "Núcleo listo: $OUT/mednafen_psx_libretro.dll (Beetle PSX $COMMIT)"
