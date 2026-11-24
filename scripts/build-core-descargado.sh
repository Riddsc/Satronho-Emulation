#!/usr/bin/env bash
# Núcleos que no se compilan aquí porque tardan demasiado (Flycast y PPSSPP): se bajan ya hechos de
# la página oficial de libretro (versión nocturna) y se les pone el nombre que usa la app.
# Uso: scripts/build-core-descargado.sh <núcleo> <windows|android> <carpeta_de_salida>
# Núcleos: flycast (Satronho Sueño) y ppsspp (Satronho Portátil). Las dos son GPL-2.0 (licencias en licencias/).
set -euo pipefail
CORE="${1:?núcleo}"
OS="${2:?windows o android}"
OUT="$(mkdir -p "${3:?carpeta de salida}" && cd "$3" && pwd)"
WORK="$(mktemp -d)"
BOT=https://buildbot.libretro.com/nightly

case "$OS" in
  windows) URL="$BOT/windows/x86_64/latest/${CORE}_libretro.dll.zip"; FILE="${CORE}_libretro.dll"; NAME="$FILE" ;;
  android) URL="$BOT/android/latest/arm64-v8a/${CORE}_libretro_android.so.zip"; FILE="${CORE}_libretro_android.so"; NAME="lib${CORE}_libretro_android.so" ;;
  *) echo "Sistema desconocido: $OS"; exit 1 ;;
esac
case "$CORE" in flycast|ppsspp) ;; *) echo "Núcleo desconocido: $CORE"; exit 1 ;; esac

curl -sSfL --retry 3 -o "$WORK/core.zip" "$URL"
unzip -q -o "$WORK/core.zip" -d "$WORK/x"
SRC="$(find "$WORK/x" -name "$FILE" | head -1)"
[ -n "$SRC" ] || { echo "El zip no trae $FILE"; exit 1; }
cp "$SRC" "$OUT/$NAME"
echo "Núcleo listo: $OUT/$NAME"
