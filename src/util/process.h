// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "util/paths.h"

namespace Satronho::Process {

// Abre un programa con sus argumentos y espera a que se cierre. Devuelve los segundos que estuvo
// abierto, o -1 si no se pudo abrir. Solo Windows (Satronho NX en PC); en otros sistemas, -1.
long long RunAndWait(const fs::path& exe, const std::vector<std::string>& args);

} // namespace Satronho::Process
