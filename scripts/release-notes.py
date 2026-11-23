#!/usr/bin/env python3
"""Escribe las notas del Release a partir de la primera entrada de assets/novedades.json."""
import json, sys

build = sys.argv[1]
data = json.load(open("assets/novedades.json", encoding="utf-8"))
b = data["builds"][0]
tipos = {"nuevo": "Nuevo", "mejora": "Mejora", "arreglo": "Arreglo"}
lines = [f"## {b['titulo']}", ""]
for c in b["cambios"]:
    lines.append(f"- **{tipos.get(c['tipo'], c['tipo'].capitalize())}:** {c['texto']}")
lines += [
    "",
    "### Cómo instalar",
    "",
    f"1. Descarga `SatronhoEmulation-build-{build}-windows-x64.zip`.",
    "2. Descomprime la carpeta donde quieras y abre `satronho-emulation.exe`.",
    "3. Agrega la carpeta donde tienes tus juegos de NES. Satronho Emulation no incluye juegos.",
    "",
    "¿Quieres todo en una sola carpeta (por ejemplo en una memoria USB)? Crea un archivo vacío llamado `portable.txt` junto al programa.",
]
open("release-notes.md", "w", encoding="utf-8").write("\n".join(lines) + "\n")

# La primera entrada de las novedades del paquete lleva el número real de la build.
if len(sys.argv) > 2:
    pkg = sys.argv[2]
    data["builds"][0]["version"] = f"Build {build}"
    json.dump(data, open(pkg, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
