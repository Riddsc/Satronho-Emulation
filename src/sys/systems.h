// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace Satronho {

// Forma del control que se dibuja en Controles.
enum class Layout { Nes, GameBoy, GameBoyAdvance };

// Un emulador de la familia Satronho dentro del hub.
struct SystemDef {
    std::string id;            // "octa"
    std::string name;          // "Octa"
    std::string full;          // "Satronho Octa"
    std::string games;         // "juegos de NES" (solo para describir)
    std::string hint;          // texto corto en la barra lateral: "NES"
    std::string core;          // nombre base del núcleo: "nestopia" -> nestopia_libretro.dll
    std::vector<std::string> extensions;
    std::string label;         // texto de la etiqueta del cartucho
    unsigned color;
    Layout layout;
    bool handheld;
    bool runahead_ok;          // el adelanto de cuadros funciona bien con este núcleo
    bool tilt;                 // tiene juegos con sensor de inclinación
    std::vector<std::pair<int, const char*>> buttons; // botones del RetroPad que usa y su nombre
    // Valores por defecto de Satronho para opciones del núcleo.
    std::vector<std::pair<const char*, const char*>> core_defaults;
};

const std::vector<SystemDef>& Systems();
const SystemDef* SystemById(const std::string& id);
const SystemDef* SystemForExtension(const std::string& ext);
std::vector<std::string> AllExtensions();
std::string CoreFile(const SystemDef& s); // nestopia_libretro.dll / .so

struct SoonSystem {
    const char* name;
    const char* hint;
};
const std::vector<SoonSystem>& SoonSystems();

} // namespace Satronho
