// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace Satronho::Paths {

// Prepara las carpetas. Modo portátil si existe "portable.txt" junto al ejecutable.
void Init();

const fs::path& Base();      // carpeta del ejecutable
const fs::path& Assets();    // assets/
const fs::path& Cores();     // cores/
const fs::path& User();      // datos del usuario
fs::path Saves();            // partidas de la batería (.srm)
fs::path States();           // estados guardados
fs::path System();           // BIOS y bases de datos de los núcleos
fs::path Screenshots();      // capturas
fs::path ConfigFile();       // satronho.ini
fs::path LibraryFile();      // biblioteca.ini
fs::path LogFile();          // satronho.log
bool Portable();

// Conversión segura entre UTF-8 y rutas (en Windows las rutas son anchas).
fs::path FromUtf8(const std::string& s);
std::string ToUtf8(const fs::path& p);

// Nombre de archivo seguro para usar como clave (sin caracteres raros).
std::string SafeName(const std::string& s);

} // namespace Satronho::Paths
