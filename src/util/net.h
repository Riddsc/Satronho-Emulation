// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "util/paths.h"

namespace Satronho::Net {

// Descarga una dirección a un archivo (se escribe primero a un .part y se renombra al final).
// En Windows usa urlmon (viene con el sistema); en otros sistemas, el programa curl.
bool Download(const std::string& url, const fs::path& file);

// Codifica un texto para usarlo dentro de una dirección web («Mario Bros.» → «Mario%20Bros.»).
std::string UrlEncode(const std::string& s);
std::string UrlDecode(const std::string& s);

} // namespace Satronho::Net
