// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/systems.h"

#include <algorithm>

#include "libretro.h"

namespace Satronho {

namespace {

using B = std::vector<std::pair<int, const char*>>;

const B kDpad{{RETRO_DEVICE_ID_JOYPAD_UP, "Arriba"},
              {RETRO_DEVICE_ID_JOYPAD_DOWN, "Abajo"},
              {RETRO_DEVICE_ID_JOYPAD_LEFT, "Izquierda"},
              {RETRO_DEVICE_ID_JOYPAD_RIGHT, "Derecha"}};

B With(B extra) {
    B b = kDpad;
    b.insert(b.end(), extra.begin(), extra.end());
    return b;
}

SystemDef Make(const char* id, const char* name, const char* games, const char* hint, const char* core,
               std::vector<std::string> ext, const char* label, unsigned color, Layout layout, bool handheld) {
    SystemDef s;
    s.id = id;
    s.name = name;
    s.full = std::string("Satronho ") + name;
    s.games = games;
    s.hint = hint;
    s.core = core;
    s.extensions = std::move(ext);
    s.label = label;
    s.color = color;
    s.layout = layout;
    s.handheld = handheld;
    s.runahead_ok = true;
    s.tilt = false;
    return s;
}

std::vector<SystemDef> Build() {
    using namespace std;
    vector<SystemDef> v;
    const B nintendo8{{RETRO_DEVICE_ID_JOYPAD_A, "A"},          {RETRO_DEVICE_ID_JOYPAD_B, "B"},
                      {RETRO_DEVICE_ID_JOYPAD_X, "A turbo"},    {RETRO_DEVICE_ID_JOYPAD_Y, "B turbo"},
                      {RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"}, {RETRO_DEVICE_ID_JOYPAD_START, "Start"}};
    const vector<vector<int>> ab_turbo{{RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A},
                                       {RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_X}};

    SystemDef octa = Make("octa", "Octa", "juegos de NES", "NES", "nestopia", {"nes", "fds", "unf", "unif"}, "OCTA", 0xFFD166,
                          Layout::Nes, false);
    octa.buttons = With(nintendo8);
    octa.turbo = true;
    octa.face = ab_turbo;
    octa.core_defaults = {{"nestopia_blargg_ntsc_filter", "disabled"}};
    v.push_back(octa);

    // Mega Drive / Genesis con el mando de 6 botones. BlastEm lee A en el B del RetroPad, B en A,
    // C en R, X en Y, Y en X, Z en L y Mode en Select.
    SystemDef mega = Make("mega", "Mega", "juegos de Genesis y Mega Drive", "Genesis", "blastem", {"md", "gen", "smd", "bin"},
                          "MEGA", 0xE5484D, Layout::Genesis, false);
    mega.buttons = With({{RETRO_DEVICE_ID_JOYPAD_B, "A"},
                         {RETRO_DEVICE_ID_JOYPAD_A, "B"},
                         {RETRO_DEVICE_ID_JOYPAD_R, "C"},
                         {RETRO_DEVICE_ID_JOYPAD_Y, "X"},
                         {RETRO_DEVICE_ID_JOYPAD_X, "Y"},
                         {RETRO_DEVICE_ID_JOYPAD_L, "Z"},
                         {RETRO_DEVICE_ID_JOYPAD_SELECT, "Mode"},
                         {RETRO_DEVICE_ID_JOYPAD_START, "Start"}});
    mega.face = {{RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_L},
                 {RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_R}};
    mega.center_title = "MODE Y START";
    mega.runahead_ok = false;
    v.push_back(mega);

    SystemDef hexa = Make("hexa", "Hexa", "juegos de SNES y Super Famicom", "SNES", "bsnes", {"sfc", "smc", "bs", "st"}, "HEXA",
                          0x5AA9FF, Layout::Snes, false);
    hexa.buttons = With({{RETRO_DEVICE_ID_JOYPAD_A, "A"},
                         {RETRO_DEVICE_ID_JOYPAD_B, "B"},
                         {RETRO_DEVICE_ID_JOYPAD_X, "X"},
                         {RETRO_DEVICE_ID_JOYPAD_Y, "Y"},
                         {RETRO_DEVICE_ID_JOYPAD_L, "L"},
                         {RETRO_DEVICE_ID_JOYPAD_R, "R"},
                         {RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
                         {RETRO_DEVICE_ID_JOYPAD_START, "Start"}});
    // En rombo, como en el mando: X arriba, Y y A al medio, B abajo.
    hexa.face = {{RETRO_DEVICE_ID_JOYPAD_X}, {RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_A}, {RETRO_DEVICE_ID_JOYPAD_B}};
    hexa.shoulders = {RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R};
    // bsnes remuestrea el sonido fuera del estado guardado: con adelanto, cada cuadro "del
    // futuro" deja sonido repetido y suena mal (Fighter's History, build 7).
    hexa.runahead_ok = false;
    v.push_back(hexa);

    // PlayStation con el DualShock: análogos, L1/L2/R1/R2 y clic de los sticks.
    SystemDef cd = Make("cd", "CD", "juegos de PlayStation", "PS1", "mednafen_psx", {"cue", "chd", "pbp", "m3u", "ccd"}, "CD",
                        0x5B7CFF, Layout::PlayStation, false);
    cd.buttons = With({{RETRO_DEVICE_ID_JOYPAD_B, "Cruz"},
                       {RETRO_DEVICE_ID_JOYPAD_A, "Círculo"},
                       {RETRO_DEVICE_ID_JOYPAD_Y, "Cuadrado"},
                       {RETRO_DEVICE_ID_JOYPAD_X, "Triángulo"},
                       {RETRO_DEVICE_ID_JOYPAD_L, "L1"},
                       {RETRO_DEVICE_ID_JOYPAD_R, "R1"},
                       {RETRO_DEVICE_ID_JOYPAD_L2, "L2"},
                       {RETRO_DEVICE_ID_JOYPAD_R2, "R2"},
                       {RETRO_DEVICE_ID_JOYPAD_L3, "L3"},
                       {RETRO_DEVICE_ID_JOYPAD_R3, "R3"},
                       {RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
                       {RETRO_DEVICE_ID_JOYPAD_START, "Start"}});
    cd.face = {{RETRO_DEVICE_ID_JOYPAD_X}, {RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_A}, {RETRO_DEVICE_ID_JOYPAD_B}};
    cd.shoulders = {RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L2,
                    RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_ID_JOYPAD_L3, RETRO_DEVICE_ID_JOYPAD_R3};
    cd.port_device = RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1); // DualShock
    cd.analog = true;
    cd.disc = true;
    cd.runahead_ok = false; // por probar con juegos reales; el estado pesa varios MB
    // Imagen al doble de resolución (polígonos más finos) y sin el tramado de la época.
    cd.core_defaults = {{"beetle_psx_internal_resolution", "2x"}, {"beetle_psx_dither_mode", "disabled"}};
    v.push_back(cd);

    SystemDef gb = Make("bolsillo", "Bolsillo", "juegos de Game Boy y Game Boy Color", "GB · GBC", "mgba", {"gb", "gbc", "sgb"},
                        "BOLSILLO", 0x9BBC0F, Layout::GameBoy, true);
    gb.buttons = With(nintendo8);
    gb.turbo = true;
    gb.face = ab_turbo;
    gb.runahead_ok = false; // mGBA guarda el sonido fuera del estado: con adelanto suena raro
    gb.tilt = true;
    gb.core_defaults = {{"mgba_skip_bios", "ON"}, {"mgba_sgb_borders", "OFF"}};
    v.push_back(gb);

    SystemDef gba = Make("bolsillo_plus", "Bolsillo Plus", "juegos de Game Boy Advance", "GBA", "mgba", {"gba"}, "PLUS", 0x6D5BD0,
                         Layout::GameBoyAdvance, true);
    gba.buttons = With({{RETRO_DEVICE_ID_JOYPAD_A, "A"},
                        {RETRO_DEVICE_ID_JOYPAD_B, "B"},
                        {RETRO_DEVICE_ID_JOYPAD_L, "L"},
                        {RETRO_DEVICE_ID_JOYPAD_R, "R"},
                        {RETRO_DEVICE_ID_JOYPAD_X, "A turbo"},
                        {RETRO_DEVICE_ID_JOYPAD_Y, "B turbo"},
                        {RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
                        {RETRO_DEVICE_ID_JOYPAD_START, "Start"}});
    gba.turbo = true;
    gba.face = ab_turbo;
    gba.shoulders = {RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R};
    gba.runahead_ok = false;
    gba.tilt = true;
    // El sonido de la GBA es áspero de fábrica; un filtro suave lo deja más limpio.
    gba.core_defaults = {{"mgba_skip_bios", "ON"}, {"mgba_audio_low_pass_filter", "enabled"}, {"mgba_audio_low_pass_range", "60"}};
    v.push_back(gba);
    return v;
}

} // namespace

const std::vector<SystemDef>& Systems() {
    static const std::vector<SystemDef> systems = Build();
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

std::string SystemsSentence() {
    std::string out;
    const auto& all = Systems();
    for (size_t i = 0; i < all.size(); ++i) {
        if (i) out += i + 1 == all.size() ? " y " : ", ";
        out += (i == 0 ? all[i].full : all[i].name) + " (" + all[i].hint + ")";
    }
    return out;
}

const std::vector<SoonSystem>& SoonSystems() {
    // En el orden en que van llegando.
    static const std::vector<SoonSystem> soon{
        {"64", "N64", "juegos de Nintendo 64"},
        {"Dual", "DS · 3DS", "juegos de Nintendo DS y 3DS"},
        {"Cubo", "GC · Wii", "juegos de GameCube y Wii"},
        {"Anillo", "Saturn", "juegos de Sega Saturn"},
        {"Sueño", "Dreamcast", "juegos de Dreamcast"},
        {"CD2", "PS2", "juegos de PlayStation 2"},
        {"Portátil", "PSP", "juegos de PSP"},
    };
    return soon;
}

} // namespace Satronho
