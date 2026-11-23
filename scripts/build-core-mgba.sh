#!/usr/bin/env bash
# Compila el núcleo de Game Boy, Game Boy Color y Game Boy Advance (mGBA) para Windows con
# llvm-mingw. Lo usan Satronho Bolsillo y Satronho Bolsillo Plus.
# Uso: scripts/build-core-mgba.sh <carpeta_de_salida>
# Variables: LLVM_MINGW (ruta de llvm-mingw), MGBA_TAG (versión fija).
set -euo pipefail
OUT="${1:?carpeta de salida}"
TAG="${MGBA_TAG:-0.10.5}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"
git clone -q --depth 1 --branch "$TAG" https://github.com/mgba-emu/mgba.git "$WORK/mgba"
cmake -S "$WORK/mgba" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/llvm-mingw.cmake" -DLLVM_MINGW="$LLVM_MINGW" \
  -DCMAKE_SHARED_LINKER_FLAGS="-static" -DSKIP_GIT=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DBUILD_LIBRETRO=ON -DSKIP_LIBRARY=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_SHARED=OFF -DBUILD_STATIC=OFF \
  -DBUILD_GL=OFF -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DUSE_EPOXY=OFF -DUSE_DEBUGGERS=OFF -DUSE_EDITLINE=OFF \
  -DUSE_FFMPEG=OFF -DUSE_ZLIB=OFF -DUSE_MINIZIP=OFF -DUSE_PNG=OFF -DUSE_LIBZIP=OFF -DUSE_ELF=OFF -DUSE_LUA=OFF \
  -DUSE_LZMA=OFF -DUSE_SQLITE3=OFF -DUSE_DISCORD_RPC=OFF
cmake --build "$WORK/build" -j"$(nproc)"
mkdir -p "$OUT"
"$LLVM_MINGW/bin/x86_64-w64-mingw32-strip" -s "$WORK/build/mgba_libretro.dll" -o "$OUT/mgba_libretro.dll"
cp "$WORK/mgba/LICENSE" "$ROOT/licencias/mGBA-MPL-2.0.txt" 2>/dev/null || true
echo "Núcleo listo: $OUT/mgba_libretro.dll (mGBA $TAG)"
