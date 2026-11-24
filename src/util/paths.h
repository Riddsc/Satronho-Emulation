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

// Android: carpeta pública de Satronho («Satronho Games» en la memoria del celular). Ahí van los
// juegos, las partidas y los estados para que no se borren al desinstalar la app. Vacía = todo
// queda en la carpeta de la app (antes de tener el permiso, y siempre en PC).
void SetPublicRoot(const fs::path& root);
const fs::path& PublicRoot();
fs::path PrivateSaves();     // donde iban las partidas antes de la carpeta pública
fs::path PrivateStates();    // …y los estados

// Conversión segura entre UTF-8 y rutas (en Windows las rutas son anchas).
fs::path FromUtf8(const std::string& s);
std::string ToUtf8(const fs::path& p);

// Carpetas guardadas en la configuración. En modo portátil (memoria USB) se guardan relativas
// a la carpeta del programa cuando están en la misma unidad, así siguen sirviendo aunque
// Windows le cambie la letra a la memoria (E: hoy, F: mañana).
std::string StoreFolder(const fs::path& folder); // ruta que se guarda en satronho.ini
fs::path ResolveFolder(const std::string& stored); // ruta real para usar
// Texto estable para identificar un archivo (tiempo de juego, favoritos, estados): la ruta
// completa, o relativa a la carpeta del programa en modo portátil.
std::string StableId(const fs::path& file);

// Nombre de archivo seguro para usar como clave (sin caracteres raros).
std::string SafeName(const std::string& s);

} // namespace Satronho::Paths
