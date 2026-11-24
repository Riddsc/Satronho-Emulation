// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "util/paths.h"

namespace Satronho::Chd {

// Convierte un disco .cue (con sus .bin, pistas de 2352 bytes: datos MODE1/MODE2 y audio) a un .chd
// comprimido (formato CHD v5 con el códec «cdzl», el que leen Beetle PSX y Beetle Saturn).
// progress(0..1) devuelve false para cancelar. Escribe primero a «.chd.part» y lo renombra al final.
bool FromCue(const fs::path& cue, const fs::path& out, std::string* error, const std::function<bool(float)>& progress = {});

// Tamaño de los archivos del disco (el .cue y sus pistas), para calcular el ahorro.
uint64_t DiscBytes(const fs::path& cue);

} // namespace Satronho::Chd
