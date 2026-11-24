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
    SystemDef cd = Make("cd", "CD", "juegos de PlayStation", "PS1", "mednafen_psx_hw", {"cue", "chd", "pbp", "m3u", "ccd"}, "CD",
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
    // Los nombres que busca Beetle PSX (por región). Una BIOS con otro modelo en el nombre
    // (SCPH-1001, 7001…) se guarda con el nombre de su región, que el núcleo sí abre.
    cd.bios = {{"scph5501.bin", 524288, "5501|1001|7001|7501|101|usa|america|ntsc-u|ntscu", "0555c6fae8906f3f09baf5988f00e55f88e9f30b"},
               {"scph5500.bin", 524288, "5500|1000|3000|3500|7000|jap|japan|ntsc-j|ntscj", "b05def971d8ec59f346f2d9ac21fb742e3eb6917"},
               {"scph5502.bin", 524288, "5502|1002|7002|7502|eur|europe|pal", "f6bc2d1f5eb6593de7d089c425ac681d6fffd3f0"},
               {"scph5503.bin", 524288, "5503"}, {"scph7003.bin", 524288, "7003"}, {"scph5552.bin", 524288, "5552"}};
    cd.bios_note = "Sin BIOS se usa OpenBIOS, libre e incluida. Con la original sale el logo de PlayStation y algunos juegos andan mejor.";
    cd.runahead_ok = false; // por probar con juegos reales; el estado pesa varios MB
    cd.cpu_need = 1;
    // Imagen al doble de resolución (polígonos más finos) y sin el tramado de la época.
    // El desentrelazado adaptativo evita las rayas en los modos de alta resolución (las peleas
    // de Bloody Roar, por ejemplo).
    // Beetle PSX HW: dibuja con la tarjeta gráfica (OpenGL o Vulkan) o con el procesador; el motor
    // y la resolución los pone Satronho según Ajustes » Gráficos (App::ApplyGpuChoice).
    cd.core_defaults = {{"beetle_psx_hw_internal_resolution", "2x"},
                        {"beetle_psx_hw_dither_mode", "disabled"},
                        {"beetle_psx_hw_deinterlacer", "fastmad"},
                        {"beetle_psx_hw_filter", "nearest"}};
    v.push_back(cd);

    // Nintendo 64 con los botones C independientes (opción alt-map del núcleo).
    SystemDef n64 = Make("n64", "64", "juegos de Nintendo 64", "N64", "mupen64plus_next", {"n64", "z64", "v64"}, "64", 0x3DBE6B,
                         Layout::N64, false);
    n64.buttons = With({{RETRO_DEVICE_ID_JOYPAD_B, "A"},
                        {RETRO_DEVICE_ID_JOYPAD_Y, "B"},
                        {RETRO_DEVICE_ID_JOYPAD_L2, "Z"},
                        {RETRO_DEVICE_ID_JOYPAD_SELECT, "L"},
                        {RETRO_DEVICE_ID_JOYPAD_R2, "R"},
                        {RETRO_DEVICE_ID_JOYPAD_X, "C arriba"},
                        {RETRO_DEVICE_ID_JOYPAD_A, "C abajo"},
                        {RETRO_DEVICE_ID_JOYPAD_L, "C izquierda"},
                        {RETRO_DEVICE_ID_JOYPAD_R, "C derecha"},
                        {RETRO_DEVICE_ID_JOYPAD_START, "Start"}});
    n64.face = {{RETRO_DEVICE_ID_JOYPAD_X},
                {RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R},
                {RETRO_DEVICE_ID_JOYPAD_A},
                {RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_B}};
    n64.center = {RETRO_DEVICE_ID_JOYPAD_START};
    n64.center_title = "START";
    n64.shoulders = {RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_R2, RETRO_DEVICE_ID_JOYPAD_L2};
    n64.analog = true;
    n64.runahead_ok = false;
    n64.cpu_need = 3;
    // El dibujo lo elige Satronho (App::ApplyGpuChoice): GLideN64 con OpenGL, ParaLLEl-RDP con
    // Vulkan o Angrylion con el procesador. Por defecto (sin tarjeta gráfica), Angrylion.
    n64.core_defaults = {{"mupen64plus-rdp-plugin", "angrylion"},
                         {"mupen64plus-rsp-plugin", "parallel"},
                         {"mupen64plus-alt-map", "True"}};
    v.push_back(n64);

    // Nintendo DS: las dos pantallas una arriba de la otra; la de abajo se toca con el mouse.
    SystemDef ds = Make("dual", "Dual", "juegos de Nintendo DS", "DS", "melondsds", {"nds", "dsi"}, "DUAL", 0x48C6E8,
                        Layout::DS, true);
    ds.buttons = With({{RETRO_DEVICE_ID_JOYPAD_A, "A"},
                       {RETRO_DEVICE_ID_JOYPAD_B, "B"},
                       {RETRO_DEVICE_ID_JOYPAD_X, "X"},
                       {RETRO_DEVICE_ID_JOYPAD_Y, "Y"},
                       {RETRO_DEVICE_ID_JOYPAD_L, "L"},
                       {RETRO_DEVICE_ID_JOYPAD_R, "R"},
                       {RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
                       {RETRO_DEVICE_ID_JOYPAD_START, "Start"}});
    ds.face = {{RETRO_DEVICE_ID_JOYPAD_X}, {RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_A}, {RETRO_DEVICE_ID_JOYPAD_B}};
    ds.shoulders = {RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R};
    ds.touch = true;
    ds.bios = {{"bios7.bin", 16384, "bios7|arm7"}, {"bios9.bin", 4096, "bios9|arm9"}, {"firmware.bin", 262144, "firmware|fw"}};
    ds.bios_note = "Sin BIOS se usa FreeBIOS, libre e incluida. Con la original (bios7, bios9 y firmware) y el modo «Native» en las "
                   "opciones de abajo sale el menú de la DS al arrancar.";
    // Tres formas de ver las pantallas, que se cambian con el atajo «Cambiar pantallas»
    // (R3 del RetroPad en melonDS DS): una arriba de la otra, lado a lado y una grande con la
    // otra pequeña.
    ds.core_defaults = {{"melonds_show_current_layout", "disabled"},
                        {"melonds_number_of_screen_layouts", "3"},
                        {"melonds_screen_layout1", "top-bottom"},
                        {"melonds_screen_layout2", "left-right"},
                        {"melonds_screen_layout3", "hybrid-top"}};
    ds.runahead_ok = false;
    ds.cpu_need = 1;
    v.push_back(ds);

    // Sega Saturn con el mando de 6 botones (como el de la Mega, con L y R arriba).
    SystemDef ss = Make("anillo", "Anillo", "juegos de Sega Saturn", "Saturn", "mednafen_saturn", {"cue", "chd", "ccd", "m3u"},
                        "ANILLO", 0xB07CFF, Layout::Saturn, false);
    // Igual que en BlastEm: A = B del RetroPad, B = A, C = R, X = Y, Y = X, Z = L; L y R = L2 y R2.
    ss.buttons = With({{RETRO_DEVICE_ID_JOYPAD_B, "A"},
                       {RETRO_DEVICE_ID_JOYPAD_A, "B"},
                       {RETRO_DEVICE_ID_JOYPAD_R, "C"},
                       {RETRO_DEVICE_ID_JOYPAD_Y, "X"},
                       {RETRO_DEVICE_ID_JOYPAD_X, "Y"},
                       {RETRO_DEVICE_ID_JOYPAD_L, "Z"},
                       {RETRO_DEVICE_ID_JOYPAD_L2, "L"},
                       {RETRO_DEVICE_ID_JOYPAD_R2, "R"},
                       {RETRO_DEVICE_ID_JOYPAD_START, "Start"}});
    ss.face = {{RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_L},
               {RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_R}};
    ss.shoulders = {RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2};
    ss.center = {RETRO_DEVICE_ID_JOYPAD_START};
    ss.center_title = "START";
    ss.disc = true;
    ss.runahead_ok = false;
    ss.cpu_need = 2;
    ss.bios = {{"mpr-17933.bin", 524288, "17933|usa|us|eur|eu|america|europe|v1.00"}, {"sega_101.bin", 524288, "101|jap|jp|japan"}};
    ss.bios_required = true;
    ss.bios_note = "La Saturn necesita su BIOS original: mpr-17933.bin para juegos de América y Europa, sega_101.bin para los de "
                   "Japón. No existe una libre que la reemplace.";
    ss.core_defaults = {{"beetle_saturn_deinterlacer", "bob"}};
    v.push_back(ss);

    SystemDef gb = Make("bolsillo", "Bolsillo", "juegos de Game Boy y Game Boy Color", "GB · GBC", "mgba", {"gb", "gbc", "sgb"},
                        "BOLSILLO", 0x9BBC0F, Layout::GameBoy, true);
    gb.buttons = With(nintendo8);
    gb.turbo = true;
    gb.face = ab_turbo;
    gb.runahead_ok = false; // mGBA guarda el sonido fuera del estado: con adelanto suena raro
    gb.tilt = true;
    gb.bios = {{"gb_bios.bin", 256, "dmg|gb_bios"}, {"gbc_bios.bin", 2304, "cgb|gbc"}, {"sgb_bios.bin", 256, "sgb"}};
    gb.bios_note = "No hace falta. Con la original y «Saltar la intro» apagado abajo, sale el logo al arrancar.";
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
    gba.bios = {{"gba_bios.bin", 16384, "gba"}};
    gba.bios_note = "No hace falta. Con la original y «Saltar la intro» apagado abajo, sale la animación de inicio.";
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
#if defined(__ANDROID__)
    // Android instala los núcleos junto a la app y exige «lib…so». Mega usa Genesis Plus GX
    // (BlastEm solo tiene su recompilador para PC).
    const std::string core = s.id == "mega" ? std::string("genesis_plus_gx") : s.core;
    return "lib" + core + "_libretro_android.so";
#elif defined(_WIN32)
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
        {"Cubo", "GC · Wii", "juegos de GameCube y Wii"},
        {"Sueño", "Dreamcast", "juegos de Dreamcast"},
        {"CD2", "PS2", "juegos de PlayStation 2"},
        {"Portátil", "PSP", "juegos de PSP"},
        {"Dual", "3DS", "juegos de Nintendo 3DS, que se suman a Satronho Dual"},
    };
    return soon;
}

} // namespace Satronho
