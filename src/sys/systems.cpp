// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/systems.h"

#include <algorithm>

#include "libretro.h"

namespace Satronho {

const std::vector<SystemDef>& Systems() {
    static const std::vector<SystemDef> systems{
        {"octa", "Octa", "Satronho Octa", "juegos de NES", "NES", "nestopia", {"nes", "fds", "unf", "unif"}, "OCTA", 0xFFD166,
         Layout::Nes, false,
         {{RETRO_DEVICE_ID_JOYPAD_UP, "Arriba"},
          {RETRO_DEVICE_ID_JOYPAD_DOWN, "Abajo"},
          {RETRO_DEVICE_ID_JOYPAD_LEFT, "Izquierda"},
          {RETRO_DEVICE_ID_JOYPAD_RIGHT, "Derecha"},
          {RETRO_DEVICE_ID_JOYPAD_A, "A"},
          {RETRO_DEVICE_ID_JOYPAD_B, "B"},
          {RETRO_DEVICE_ID_JOYPAD_X, "A turbo"},
          {RETRO_DEVICE_ID_JOYPAD_Y, "B turbo"},
          {RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
          {RETRO_DEVICE_ID_JOYPAD_START, "Start"}},
         {{"nestopia_blargg_ntsc_filter", "disabled"}}},
        {"bolsillo", "Bolsillo", "Satronho Bolsillo", "juegos de Game Boy y Game Boy Color", "GB · GBC", "mgba", {"gb", "gbc", "sgb"},
         "BOLSILLO", 0x9BBC0F, Layout::GameBoy, true,
         {{RETRO_DEVICE_ID_JOYPAD_UP, "Arriba"},
          {RETRO_DEVICE_ID_JOYPAD_DOWN, "Abajo"},
          {RETRO_DEVICE_ID_JOYPAD_LEFT, "Izquierda"},
          {RETRO_DEVICE_ID_JOYPAD_RIGHT, "Derecha"},
          {RETRO_DEVICE_ID_JOYPAD_A, "A"},
          {RETRO_DEVICE_ID_JOYPAD_B, "B"},
          {RETRO_DEVICE_ID_JOYPAD_X, "A turbo"},
          {RETRO_DEVICE_ID_JOYPAD_Y, "B turbo"},
          {RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
          {RETRO_DEVICE_ID_JOYPAD_START, "Start"}},
         {{"mgba_skip_bios", "ON"}, {"mgba_sgb_borders", "OFF"}}},
        {"bolsillo_plus", "Bolsillo Plus", "Satronho Bolsillo Plus", "juegos de Game Boy Advance", "GBA", "mgba", {"gba"}, "PLUS",
         0x6D5BD0, Layout::GameBoyAdvance, true,
         {{RETRO_DEVICE_ID_JOYPAD_UP, "Arriba"},
          {RETRO_DEVICE_ID_JOYPAD_DOWN, "Abajo"},
          {RETRO_DEVICE_ID_JOYPAD_LEFT, "Izquierda"},
          {RETRO_DEVICE_ID_JOYPAD_RIGHT, "Derecha"},
          {RETRO_DEVICE_ID_JOYPAD_A, "A"},
          {RETRO_DEVICE_ID_JOYPAD_B, "B"},
          {RETRO_DEVICE_ID_JOYPAD_L, "L"},
          {RETRO_DEVICE_ID_JOYPAD_R, "R"},
          {RETRO_DEVICE_ID_JOYPAD_X, "A turbo"},
          {RETRO_DEVICE_ID_JOYPAD_Y, "B turbo"},
          {RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
          {RETRO_DEVICE_ID_JOYPAD_START, "Start"}},
         {{"mgba_skip_bios", "ON"}}},
    };
    return systems;
}

const SystemDef* SystemById(const std::string& id) {
    for (const auto& s : Systems())
        if (s.id == id) return &s;
    return nullptr;
}

const SystemDef* SystemForExtension(const std::string& ext) {
    for (const auto& s : Systems())
        if (std::find(s.extensions.begin(), s.extensions.end(), ext) != s.extensions.end()) return &s;
    return nullptr;
}

std::vector<std::string> AllExtensions() {
    std::vector<std::string> out;
    for (const auto& s : Systems()) out.insert(out.end(), s.extensions.begin(), s.extensions.end());
    return out;
}

std::string CoreFile(const SystemDef& s) {
#ifdef _WIN32
    return s.core + "_libretro.dll";
#elif defined(__APPLE__)
    return s.core + "_libretro.dylib";
#else
    return s.core + "_libretro.so";
#endif
}

const std::vector<SoonSystem>& SoonSystems() {
    static const std::vector<SoonSystem> soon{
        {"Mega", "Genesis"}, {"Hexa", "SNES"}, {"CD", "PS1"}, {"64", "N64"}, {"Dual", "DS · 3DS"}, {"Cubo", "GC · Wii"},
    };
    return soon;
}

} // namespace Satronho
