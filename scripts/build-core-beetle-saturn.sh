#!/usr/bin/env bash
# Compila el núcleo de Sega Saturn (Beetle Saturn) para Windows con llvm-mingw.
# Uso: scripts/build-core-beetle-saturn.sh <carpeta_de_salida>
# Variables: LLVM_MINGW (ruta de llvm-mingw), BEETLE_SATURN_COMMIT (versión fija).
set -euo pipefail
OUT="${1:?carpeta de salida}"
COMMIT="${BEETLE_SATURN_COMMIT:-7126d65b83b65b4316736434ecca2f5ff4f1a3dd}"
WORK="$(mktemp -d)"
git init -q "$WORK/ss"
git -C "$WORK/ss" remote add origin https://github.com/libretro/beetle-saturn-libretro.git
git -C "$WORK/ss" fetch -q --depth 1 origin "$COMMIT"
git -C "$WORK/ss" checkout -q FETCH_HEAD
export PATH="$LLVM_MINGW/bin:$PATH"
# lld no acepta --version-script en Windows: se reemplaza SHARED. Necesita la BIOS de cada quien.
make -C "$WORK/ss" -j"$(nproc)" platform=win \
  CC=x86_64-w64-mingw32-clang CXX=x86_64-w64-mingw32-clang++ \
  SHARED="-shared -static -Wl,-no-undefined"
mkdir -p "$OUT"
x86_64-w64-mingw32-strip -s "$WORK/ss/mednafen_saturn_libretro.dll" -o "$OUT/mednafen_saturn_libretro.dll"
mkdir -p licencias
cp "$WORK/ss/COPYING" licencias/BeetleSaturn-GPL-2.0.txt
echo "Núcleo listo: $OUT/mednafen_saturn_libretro.dll (Beetle Saturn $COMMIT)"
