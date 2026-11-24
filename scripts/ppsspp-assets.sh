#!/usr/bin/env bash
# Archivos que el núcleo de la PSP (PPSSPP) necesita en sistema/PPSSPP: letras, fuentes de la
# consola (flash0), lista de arreglos por juego y sombreadores. Se bajan del repositorio oficial.
# Uso: scripts/ppsspp-assets.sh <carpeta_de_salida>   (normalmente assets/system/PPSSPP)
set -euo pipefail
OUT="${1:?carpeta de salida}"
COMMIT="${PPSSPP_ASSETS_COMMIT:-master}"
WORK="$(mktemp -d)"
git init -q "$WORK/p"
git -C "$WORK/p" remote add origin https://github.com/hrydgard/ppsspp.git
git -C "$WORK/p" sparse-checkout set --no-cone /assets/flash0 /assets/shaders /assets/ppge_atlas.zim \
  /assets/ppge_atlas.meta /assets/compat.ini /assets/langregion.ini /assets/knownfuncs.ini
git -C "$WORK/p" fetch -q --depth 1 --filter=blob:none origin "$COMMIT"
git -C "$WORK/p" checkout -q FETCH_HEAD
mkdir -p "$OUT"
cp -r "$WORK/p/assets/." "$OUT/"
du -sh "$OUT"
