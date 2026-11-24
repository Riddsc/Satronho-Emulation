// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Idiomas de la interfaz. Los textos se escriben en español en el código y Tr() los cambia por su
// traducción (assets/idiomas/<código>.json: {"texto en español": "traducción"}). Lo que no está
// traducido se muestra en español.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "util/paths.h"

namespace Satronho::I18n {

// Idiomas disponibles: código y nombre en su propio idioma.
const std::vector<std::pair<std::string, std::string>>& Languages();
// Idioma del sistema (si es uno de los disponibles; si no, inglés; español si es español).
std::string SystemLanguage();
// Carga el idioma (código «es», «en», «fr», «pt» o «it»; vacío = el del sistema).
void Load(const fs::path& dir, const std::string& code);
const std::string& Current();

const char* Tr(const char* es);
std::string Tr(const std::string& es);

} // namespace Satronho::I18n

namespace Satronho {
using I18n::Tr;
// Traduce el formato y después pone los datos («Quedan %s libres»).
std::string TrFmt(const char* fmt, ...);
// Tamaño para personas: «850 MB», «1.4 GB».
std::string HumanSize(uint64_t bytes);
}
