// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "util/paths.h"

namespace Satronho::Strip {

// Agrega abajo de la imagen (RGBA) la franja de Satronho: logo, título, consola y lo de la derecha
// (la fecha). Cambia h por el alto nuevo.
bool Add(std::vector<uint8_t>& rgba, unsigned w, unsigned& h, const std::string& title, const std::string& subtitle, const std::string& right,
         uint32_t accent, const fs::path& assets);

} // namespace Satronho::Strip
