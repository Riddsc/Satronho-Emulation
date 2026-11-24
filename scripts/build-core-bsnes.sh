#!/usr/bin/env bash
# Compila el núcleo de SNES (bsnes) para Windows con llvm-mingw.
# Uso: scripts/build-core-bsnes.sh <carpeta_de_salida>
# Variables: LLVM_MINGW (ruta de llvm-mingw), BSNES_COMMIT (versión fija).
set -euo pipefail
OUT="${1:?carpeta de salida}"
COMMIT="${BSNES_COMMIT:-63ee22f7af8f5d1ada295c2c98fd7bf261fa62a7}"
WORK="$(mktemp -d)"
git init -q "$WORK/bsnes"
git -C "$WORK/bsnes" remote add origin https://github.com/libretro/bsnes.git
git -C "$WORK/bsnes" fetch -q --depth 1 origin "$COMMIT"
git -C "$WORK/bsnes" checkout -q FETCH_HEAD
export PATH="$LLVM_MINGW/bin:$PATH"
# lld no acepta --version-script en Windows: se reemplaza SHARED. El clang nuevo marca como error
# que nall especialice is_signed<int128_t>, y vasprintf solo se declara con _GNU_SOURCE.
make -C "$WORK/bsnes" -j"$(nproc)" platform=win \
  CC=x86_64-w64-mingw32-clang CXX=x86_64-w64-mingw32-clang++ \
  SHARED="-shared -static -Wl,-no-undefined" \
  EXTRA_INCLUDES="-Wno-invalid-specialization -D_GNU_SOURCE"
mkdir -p "$OUT"
x86_64-w64-mingw32-strip -s "$WORK/bsnes/bsnes_libretro.dll" -o "$OUT/bsnes_libretro.dll"
mkdir -p licencias
cp "$WORK/bsnes/GPLv3.txt" licencias/bsnes-GPL-3.0.txt
echo "Núcleo listo: $OUT/bsnes_libretro.dll (bsnes $COMMIT)"
