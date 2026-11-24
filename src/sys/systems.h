// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace Satronho {

// Forma del control que se dibuja en Controles.
enum class Layout { Nes, GameBoy, GameBoyAdvance, Genesis, Snes };

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
    bool turbo = false;        // X/Y del RetroPad son turbos de A/B (NES y Game Boy)
    // Cómo se agrupan las casillas en Controles: filas de botones de acción (centradas),
    // gatillos y el título del grupo del centro.
    std::vector<std::vector<int>> face;
    std::vector<int> shoulders;
    const char* center_title = "SELECT Y START";
    // Valores por defecto de Satronho para opciones del núcleo.
    std::vector<std::pair<const char*, const char*>> core_defaults;
};

const std::vector<SystemDef>& Systems();
const SystemDef* SystemById(const std::string& id);
const SystemDef* SystemForExtension(const std::string& ext);
std::vector<std::string> AllExtensions();
std::string CoreFile(const SystemDef& s); // nestopia_libretro.dll / .so
// «Satronho Octa (NES), Mega (Genesis)… y Bolsillo Plus (GBA)», para los textos de bienvenida.
std::string SystemsSentence();

struct SoonSystem {
    const char* name;
    const char* hint;
};
const std::vector<SoonSystem>& SoonSystems();

} // namespace Satronho
