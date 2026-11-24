#!/usr/bin/env bash
# Compila el núcleo de Nintendo DS (melonDS DS) para Windows con llvm-mingw.
# Uso: scripts/build-core-melonds.sh <carpeta_de_salida>
# Variables: LLVM_MINGW (ruta de llvm-mingw), MELONDS_COMMIT (versión fija).
set -euo pipefail
OUT="${1:?carpeta de salida}"
COMMIT="${MELONDS_COMMIT:-f394adbacb5722ee97c1b37c8064da9a25818310}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"
git init -q "$WORK/ds"
git -C "$WORK/ds" remote add origin https://github.com/JesseTG/melonds-ds.git
git -C "$WORK/ds" fetch -q --depth 1 origin "$COMMIT"
git -C "$WORK/ds" checkout -q FETCH_HEAD
# Render por software (sin OpenGL) y JIT. Sin BIOS usa FreeBIOS, que viene adentro.
cmake -S "$WORK/ds" -B "$WORK/ds/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$HERE/cmake/llvm-mingw.cmake" -DENABLE_OPENGL=OFF -DENABLE_JIT=ON
cmake --build "$WORK/ds/build" -j"$(nproc)"
mkdir -p "$OUT"
"$LLVM_MINGW/bin/x86_64-w64-mingw32-strip" -s "$WORK/ds/build/src/libretro/melondsds_libretro.dll" -o "$OUT/melondsds_libretro.dll"
mkdir -p licencias
cp "$WORK/ds/LICENSE" licencias/melonDS-DS-GPL-3.0.txt
echo "Núcleo listo: $OUT/melondsds_libretro.dll (melonDS DS $COMMIT)"
