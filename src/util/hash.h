// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "util/paths.h"

namespace Satronho::Hash {

// Huellas en hexadecimal (minúsculas). Se usan para reconocer BIOS y para comprobar que la
// actualización descargada llegó completa. Devuelven "" si el archivo no se pudo leer.
std::string Sha1File(const fs::path& file);
std::string Sha256File(const fs::path& file);
std::string Sha256(const void* data, size_t size);

} // namespace Satronho::Hash
