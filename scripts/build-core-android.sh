#!/usr/bin/env bash
# Compila un núcleo para Android (arm64) con el NDK. Sale como «lib<núcleo>_libretro_android.so»,
# que es como Android exige llamar a las librerías que van dentro del APK.
# Uso: scripts/build-core-android.sh <núcleo> <carpeta_de_salida>
# Variables: ANDROID_NDK (ruta del NDK). Las versiones son las mismas que en Windows.
set -euo pipefail
CORE="${1:?núcleo}"
OUT="$(mkdir -p "${2:?carpeta de salida}" && cd "$2" && pwd)"
NDK="${ANDROID_NDK:?ruta del NDK}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"
JOBS="$(nproc)"

fetch() { # carpeta repo commit
  git init -q "$WORK/$1"
  git -C "$WORK/$1" remote add origin "$2"
  git -C "$WORK/$1" fetch -q --depth 1 origin "$3"
  git -C "$WORK/$1" checkout -q FETCH_HEAD
  git -C "$WORK/$1" submodule update -q --init --recursive --depth 1 2>/dev/null || true
}
commit_of() { grep -o "${1}:-[0-9a-f]*" "$HERE/scripts/$2" | head -1 | cut -d- -f2; }

ndk() { # carpeta_jni nombre_final [variables...]
  local dir="$1" name="$2"; shift 2
  "$NDK/ndk-build" --no-print-directory -j"$JOBS" -C "$dir" APP_ABI=arm64-v8a APP_PLATFORM=android-26 "$@"
  local so
  so="$(find "$dir/.." "$dir" -path '*libs/arm64-v8a/*.so' ! -name 'libc++_shared.so' 2>/dev/null | head -1)"
  [ -n "$so" ] || { echo "No salió la librería de $name"; exit 1; }
  "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" -s "$so" -o "$OUT/lib${name}_libretro_android.so"
}

cmake_android() { # carpeta_fuente carpeta_build [opciones...]
  local src="$1" build="$2"; shift 2
  cmake -S "$src" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_static -DCMAKE_POLICY_VERSION_MINIMUM=3.5 "$@"
  cmake --build "$build" -j"$JOBS"
}

case "$CORE" in
  nestopia)
    fetch c https://github.com/libretro/nestopia.git "$(commit_of NESTOPIA_COMMIT build-core-nestopia.sh)"
    ndk "$WORK/c/libretro/jni" nestopia ;;
  bsnes)
    fetch c https://github.com/libretro/bsnes.git "$(commit_of BSNES_COMMIT build-core-bsnes.sh)"
    ndk "$WORK/c/bsnes/target-libretro/jni" bsnes ;;
  genesis_plus_gx)
    fetch c https://github.com/libretro/Genesis-Plus-GX.git 099c962e7374eb84cae9bba180c9e0e49e767b40
    ndk "$WORK/c/libretro/jni" genesis_plus_gx ;;
  mednafen_psx_hw)
    fetch c https://github.com/libretro/beetle-psx-libretro.git "$(commit_of BEETLE_PSX_COMMIT build-core-beetle-psx.sh)"
    ndk "$WORK/c/jni" mednafen_psx_hw HAVE_HW=1 ;;
  mednafen_saturn)
    fetch c https://github.com/libretro/beetle-saturn-libretro.git "$(commit_of BEETLE_SATURN_COMMIT build-core-beetle-saturn.sh)"
    ndk "$WORK/c/jni" mednafen_saturn ;;
  mupen64plus_next)
    fetch c https://github.com/libretro/mupen64plus-libretro-nx.git "$(commit_of MUPEN_COMMIT build-core-mupen64plus.sh)"
    ndk "$WORK/c/libretro/jni" mupen64plus_next GLES3=1 ;;
  mgba)
    git clone -q --depth 1 --branch "$(grep -o 'MGBA_TAG:-[0-9.]*' "$HERE/scripts/build-core-mgba.sh" | cut -d- -f2)" https://github.com/mgba-emu/mgba.git "$WORK/c"
    cmake_android "$WORK/c" "$WORK/b" -DSKIP_GIT=ON -DBUILD_LIBRETRO=ON -DSKIP_LIBRARY=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF \
      -DBUILD_SHARED=OFF -DBUILD_STATIC=OFF -DBUILD_GL=OFF -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DUSE_EPOXY=OFF \
      -DUSE_DEBUGGERS=OFF -DUSE_EDITLINE=OFF -DUSE_FFMPEG=OFF -DUSE_ZLIB=OFF -DUSE_MINIZIP=OFF -DUSE_PNG=OFF \
      -DUSE_LIBZIP=OFF -DUSE_ELF=OFF -DUSE_LUA=OFF -DUSE_LZMA=OFF -DUSE_SQLITE3=OFF -DUSE_DISCORD_RPC=OFF
    "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" -s "$(find "$WORK/b" -name 'mgba_libretro*.so' | head -1)" -o "$OUT/libmgba_libretro_android.so" ;;
  melondsds)
    fetch c https://github.com/JesseTG/melonds-ds.git "$(commit_of MELONDS_COMMIT build-core-melonds.sh)"
    cmake_android "$WORK/c" "$WORK/b" -DENABLE_OPENGL=OFF -DENABLE_JIT=ON
    "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" -s "$(find "$WORK/b" -name 'melondsds_libretro*.so' | head -1)" -o "$OUT/libmelondsds_libretro_android.so" ;;
  *) echo "Núcleo desconocido: $CORE"; exit 1 ;;
esac
echo "Núcleo de Android listo: $CORE"
