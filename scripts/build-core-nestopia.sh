#!/usr/bin/env bash
# Compila el núcleo de NES (Nestopia UE) para Windows con llvm-mingw.
# Uso: scripts/build-core-nestopia.sh <carpeta_de_salida>
# Variables: LLVM_MINGW (ruta de llvm-mingw), NESTOPIA_COMMIT (versión fija).
set -euo pipefail
OUT="${1:?carpeta de salida}"
COMMIT="${NESTOPIA_COMMIT:-39e4b456cea6f5e962ce3e65bcdcd7313711a905}"
WORK="$(mktemp -d)"
git init -q "$WORK/nestopia"
git -C "$WORK/nestopia" remote add origin https://github.com/libretro/nestopia.git
git -C "$WORK/nestopia" fetch -q --depth 1 origin "$COMMIT"
git -C "$WORK/nestopia" checkout -q FETCH_HEAD
export PATH="$LLVM_MINGW/bin:$PATH"
make -C "$WORK/nestopia/libretro" -j"$(nproc)" platform=win \
  CC=x86_64-w64-mingw32-clang CXX=x86_64-w64-mingw32-clang++ \
  SHARED="-shared -static -Wl,-no-undefined"
mkdir -p "$OUT"
x86_64-w64-mingw32-strip -s "$WORK/nestopia/libretro/nestopia_libretro.dll" -o "$OUT/nestopia_libretro.dll"
echo "Núcleo listo: $OUT/nestopia_libretro.dll (Nestopia UE $COMMIT)"
