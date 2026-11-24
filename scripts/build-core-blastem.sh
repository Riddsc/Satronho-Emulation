#!/usr/bin/env bash
# Compila el núcleo de Genesis / Mega Drive (BlastEm) para Windows con llvm-mingw.
# Uso: scripts/build-core-blastem.sh <carpeta_de_salida>
# Variables: LLVM_MINGW (ruta de llvm-mingw), BLASTEM_COMMIT (versión fija).
set -euo pipefail
OUT="${1:?carpeta de salida}"
COMMIT="${BLASTEM_COMMIT:-aa11275a99b3ea0ee477fed2844bb7d9f9faf3f6}"
WORK="$(mktemp -d)"
git init -q "$WORK/blastem"
git -C "$WORK/blastem" remote add origin https://github.com/libretro/blastem.git
git -C "$WORK/blastem" fetch -q --depth 1 origin "$COMMIT"
git -C "$WORK/blastem" checkout -q FETCH_HEAD
export PATH="$LLVM_MINGW/bin:$PATH"
# MSYSTEM=MINGW64 hace que Makefile.libretro elija Windows x86_64 (JIT de 68K y Z80).
make -C "$WORK/blastem" -f Makefile.libretro core -j"$(nproc)" \
  MSYSTEM=MINGW64 CC=x86_64-w64-mingw32-clang AR=llvm-ar
mkdir -p "$OUT"
x86_64-w64-mingw32-strip -s "$WORK/blastem/blastem_libretro.dll" -o "$OUT/blastem_libretro.dll"
mkdir -p licencias
cp "$WORK/blastem/COPYING" licencias/BlastEm-GPL-3.0.txt
echo "Núcleo listo: $OUT/blastem_libretro.dll (BlastEm $COMMIT)"
