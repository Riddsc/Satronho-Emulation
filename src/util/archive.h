// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "util/paths.h"

namespace Satronho::Archive {

// Juegos dentro de .zip (miniz) y .7z (LZMA SDK), sin que la persona tenga que descomprimirlos.

struct Entry {
    std::string name;  // ruta dentro del archivo, en UTF-8 («Carpeta/Juego (USA).nes»)
    uint64_t size = 0;
    uint32_t index = 0;
};

bool IsArchive(const std::string& ext); // "zip" o "7z"
bool List(const fs::path& archive, std::vector<Entry>& out);
// Saca un archivo a la memoria (los juegos de cartucho no tocan el disco).
bool ExtractToMemory(const fs::path& archive, const std::string& name, std::vector<uint8_t>& out);
// Saca todo a una carpeta (los juegos de disco: .cue con sus .bin). progress(0..1) opcional; si
// devuelve false, se cancela (y ExtractAll devuelve false).
bool ExtractAll(const fs::path& archive, const fs::path& dir, const std::function<bool(float)>& progress = {});
// Escribe un .zip nuevo con estos archivos (nombre dentro del .zip y contenido). Para los respaldos.
bool WriteZip(const fs::path& out, const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files);

} // namespace Satronho::Archive
