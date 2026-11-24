// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "util/paths.h"

namespace Satronho::Shortcut {

// Crea en el escritorio un acceso directo que abre Satronho Emulation con ese juego. Solo Windows;
// en otros sistemas devuelve false.
bool CreateOnDesktop(const std::string& name, const fs::path& rom, const fs::path& icon_hint, std::string* error);

} // namespace Satronho::Shortcut
