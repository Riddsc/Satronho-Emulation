// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace Satronho {

// Forma del control que se dibuja en Controles.
enum class Layout { Nes, GameBoy, GameBoyAdvance, Genesis, Snes, PlayStation, N64, DS, Saturn };

// Un archivo de BIOS que el núcleo puede usar (la persona pone el suyo; no se incluye).
struct BiosFile {
    const char* name;      // como lo busca el núcleo: "scph5501.bin"
    unsigned size;         // en bytes, para reconocerlo
    const char* keywords;  // palabras del nombre original que lo delatan: "jap|101"
    const char* sha1 = ""; // huella del archivo correcto (minúsculas), si se conoce
};

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
    std::vector<int> center = {2, 3}; // RETRO_DEVICE_ID_JOYPAD_SELECT y START
    bool touch = false;               // pantalla táctil (el mouse toca)
    unsigned port_device = 1;  // RETRO_DEVICE_JOYPAD; en CD, el DualShock
    bool analog = false;       // los sticks van al juego
    // Cuánto procesador pide (para Ñoño y los valores por defecto): 0 poco (8 y 16 bits),
    // 1 medio (PS1, DS), 2 mucho (Saturn), 3 muchísimo (64 con Angrylion, todo por software).
    int cpu_need = 0;
    bool Heavy() const { return cpu_need >= 1; } // estados de varios MB: rebobinar pesa
    bool disc = false;         // juegos en disco (cue, chd…)
    std::vector<BiosFile> bios; // BIOS que acepta (opcional salvo bios_required)
    bool bios_required = false;
    const char* bios_note = ""; // qué cambia con la BIOS original
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
    const char* games; // «juegos de Nintendo 64»
};
const std::vector<SoonSystem>& SoonSystems();

} // namespace Satronho
