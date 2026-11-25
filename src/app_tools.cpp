// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Página Herramientas: prueba de mandos en vivo, Memory Cards de PS1 (ver y borrar partidas),
// carátulas, conversión a .chd y accesos a las carpetas de Satronho.

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <fstream>

#include "app.h"
#include "native_page.h"
#include "util/i18n.h"
#include "imgui.h"
#include "ui/kit.h"
#include "util/log.h"
#include "util/paths.h"

namespace Satronho {

using namespace Ui;

namespace {
std::string Fmt(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof buf, Tr(fmt), args);
    va_end(args);
    return buf;
}

void Muted(const char* text, float size) {
    text = Tr(text);
    FontScope f(GetFonts().regular, size);
    ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
    ImGui::TextWrapped("%s", Tr(text));
    ImGui::PopStyleColor();
}

void OpenPath(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
    const std::string url = "file:///" + Paths::ToUtf8(p);
    SDL_OpenURL(url.c_str());
}

// ---------------------------------------------------------------- Memory Card de PS1
// 128 KB: 16 bloques de 8 KB. El bloque 0 es el directorio: 15 entradas de 128 bytes (una por
// bloque de datos). Estado 0x51 = primer bloque de una partida, 0x52/0x53 = bloques que siguen;
// 0xA0 libre. En el primer bloque de cada partida está el título (Shift-JIS, a 64 bytes del inicio).
constexpr size_t kCardSize = 128 * 1024;
constexpr size_t kBlock = 8192;
constexpr size_t kFrame = 128;

struct CardSave {
    int block;          // 1..15
    int blocks;         // cuántos ocupa
    std::string code;   // «BASLUS-00067…»
    std::string title;  // título que se ve en la consola
};

// Shift-JIS de las memory cards: casi siempre letras y números «de ancho completo». Se pasan a
// ASCII; lo que no se entiende queda como espacio.
std::string SjisToAscii(const uint8_t* p, size_t n) {
    std::string out;
    for (size_t i = 0; i < n && p[i]; ++i) {
        const uint8_t c = p[i];
        if (c < 0x80) {
            out += static_cast<char>(c);
            continue;
        }
        if (i + 1 >= n) break;
        const unsigned w = (c << 8) | p[i + 1];
        ++i;
        if (w == 0x8140) out += ' ';
        else if (w >= 0x824F && w <= 0x8258) out += static_cast<char>('0' + (w - 0x824F));
        else if (w >= 0x8260 && w <= 0x8279) out += static_cast<char>('A' + (w - 0x8260));
        else if (w >= 0x8281 && w <= 0x829A) out += static_cast<char>('a' + (w - 0x8281));
        else if (w == 0x8146) out += ':';
        else if (w == 0x8144) out += '.';
        else if (w == 0x815E) out += '/';
        else if (w == 0x817C) out += '-';
        else if (w == 0x8169) out += '(';
        else if (w == 0x816A) out += ')';
        else if (w == 0x8143) out += ',';
        else if (w == 0x8149) out += '!';
        else if (w == 0x8148) out += '?';
        else if (w == 0x8195) out += '&';
        else if (w == 0x8193) out += '%';
        else if (w == 0x815B || w == 0x815C || w == 0x815D) out += '-';
        else out += ' ';
    }
    // Espacios de más.
    std::string t;
    for (char ch : out)
        if (!(ch == ' ' && (t.empty() || t.back() == ' '))) t += ch;
    while (!t.empty() && t.back() == ' ') t.pop_back();
    return t;
}

std::vector<CardSave> ReadCard(const std::vector<uint8_t>& d) {
    std::vector<CardSave> out;
    if (d.size() < kCardSize || d[0] != 'M' || d[1] != 'C') return out;
    for (int b = 1; b <= 15; ++b) {
        const uint8_t* f = d.data() + b * kFrame;
        if (f[0] != 0x51) continue;
        CardSave s;
        s.block = b;
        const uint32_t size = f[4] | (f[5] << 8) | (f[6] << 16) | (static_cast<uint32_t>(f[7]) << 24);
        s.blocks = std::clamp(static_cast<int>((size + kBlock - 1) / kBlock), 1, 15);
        s.code.assign(reinterpret_cast<const char*>(f + 0x0A), strnlen(reinterpret_cast<const char*>(f + 0x0A), 20));
        s.title = SjisToAscii(d.data() + b * kBlock + 4, 64);
        if (s.title.empty()) s.title = s.code;
        out.push_back(s);
    }
    return out;
}

// Borra una partida: marca sus bloques como libres y rehace el control (XOR) de cada entrada.
void DeleteSave(std::vector<uint8_t>& d, int first) {
    int b = first;
    for (int guard = 0; guard < 15 && b >= 1 && b <= 15; ++guard) {
        uint8_t* f = d.data() + b * kFrame;
        const uint8_t state = f[0];
        const uint16_t next = static_cast<uint16_t>(f[8] | (f[9] << 8));
        f[0] = state == 0x51 ? 0xA1 : state == 0x52 ? 0xA2 : state == 0x53 ? 0xA3 : 0xA0;
        uint8_t x = 0;
        for (size_t i = 0; i < kFrame - 1; ++i) x ^= f[i];
        f[kFrame - 1] = x;
        if (next == 0xFFFF) break;
        b = next + 1;
    }
}
} // namespace

void App::DrawTools() {
    Title("Herramientas", "Cosas útiles para tus juegos, tus mandos y tus partidas");
    ImGui::Dummy({0, S(4)});
    GuideSays(GuideId::Nono, "Aquí tienes lo que no es jugar: probar tus mandos, mirar las partidas de tus Memory Cards y abrir las carpetas de Satronho.");
    ImGui::Dummy({0, S(14)});
    const float avail = ImGui::GetContentRegionAvail().x;
    const float col_w = compact_ ? avail : std::min((avail - S(18)) * 0.5f, S(760));

    auto card = [&](const char* id, const char* title, Icon icon, unsigned color, float h, auto&& body) {
        PanelBegin(id, {col_w, h}, Color::Surface, 16, {20, 18});
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddCircleFilled({p.x + S(18), p.y + S(18)}, S(18), U32(color, 0.16f));
        DrawIcon(dl, icon, {p.x + S(18), p.y + S(18)}, S(18), U32(color));
        ImGui::SetCursorScreenPos({p.x + S(48), p.y + S(6)});
        {
            FontScope f(GetFonts().bold, 18);
            ImGui::TextUnformatted(Tr(title));
        }
        ImGui::SetCursorScreenPos({p.x, p.y + S(46)});
        body();
        PanelEnd();
    };

    // ---------------- Prueba de mandos
    card("##t_mandos", "Prueba de mandos", Icon::Gamepad, Color::Amber, S(330), [&]() {
        const auto pads = input_.Gamepads();
        if (pads.empty()) {
            Muted("No hay mandos conectados. Conecta uno y aparece aquí al momento.", 14);
            ImGui::Dummy({0, S(6)});
        }
        for (const auto& pd : pads) {
            FontScope f(GetFonts().semibold, 14.5f);
            ImGui::TextUnformatted(Tr(Fmt("Mando %d · %s", pd.slot + 1, pd.name.c_str()).c_str()));
            SDL_Gamepad* gp = SDL_GetGamepadFromID(pd.id);
            if (!gp) continue;
            // Palancas (círculos con su punto) y gatillos (barras).
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float r = S(34);
            for (int side = 0; side < 2; ++side) {
                const ImVec2 c(p.x + r + S(4) + side * (r * 2 + S(20)), p.y + r + S(6));
                const float x = SDL_GetGamepadAxis(gp, side ? SDL_GAMEPAD_AXIS_RIGHTX : SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
                const float y = SDL_GetGamepadAxis(gp, side ? SDL_GAMEPAD_AXIS_RIGHTY : SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
                dl->AddCircleFilled(c, r, U32(Color::Bg));
                dl->AddCircle(c, r, U32(Color::Border), 0, S(1.5f));
                dl->AddCircle(c, r * input_.deadzone, U32(Color::Border, 0.6f), 0, S(1));
                dl->AddLine({c.x - r, c.y}, {c.x + r, c.y}, U32(Color::Border, 0.5f));
                dl->AddLine({c.x, c.y - r}, {c.x, c.y + r}, U32(Color::Border, 0.5f));
                dl->AddCircleFilled({c.x + x * r, c.y + y * r}, S(6), U32(Color::Accent));
                FontScope fl(GetFonts().medium, 11.5f);
                const char* lbl = side ? "Palanca der." : "Palanca izq.";
                dl->AddText({c.x - ImGui::CalcTextSize(lbl).x * 0.5f, c.y + r + S(4)}, U32(Color::Dim), Tr(lbl));
            }
            const float bx = p.x + r * 4 + S(52);
            for (int t = 0; t < 2; ++t) {
                const float v = SDL_GetGamepadAxis(gp, t ? SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
                const ImVec2 a(bx + t * S(26), p.y + S(6)), b(a.x + S(16), a.y + r * 2);
                dl->AddRectFilled(a, b, U32(Color::Bg), S(5));
                dl->AddRectFilled({a.x, b.y - (b.y - a.y) * std::clamp(v, 0.0f, 1.0f)}, b, U32(Color::Gold), S(5));
                FontScope fl(GetFonts().medium, 11.5f);
                const char* lbl = t ? "RT" : "LT";
                dl->AddText({a.x + S(8) - ImGui::CalcTextSize(lbl).x * 0.5f, b.y + S(4)}, U32(Color::Dim), Tr(lbl));
            }
            // Botones presionados ahora.
            std::string down;
            for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b)
                if (SDL_GetGamepadButton(gp, static_cast<SDL_GamepadButton>(b))) down += (down.empty() ? "" : "  ") + Input::PadCodeLabel(b, pd.family);
            ImGui::SetCursorScreenPos({bx + S(66), p.y + S(8)});
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + col_w - (bx - p.x) - S(110));
            {
                FontScope fb(GetFonts().semibold, 14);
                ImGui::PushStyleColor(ImGuiCol_Text, V4(down.empty() ? Color::Dim : Color::Gold));
                ImGui::TextWrapped("%s", Tr(down.empty() ? "Presiona botones para verlos aquí." : down.c_str()));
                ImGui::PopStyleColor();
            }
            ImGui::PopTextWrapPos();
            ImGui::SetCursorScreenPos({p.x, p.y + r * 2 + S(30)});
            ImGui::Dummy({0, S(4)});
            if (Button(Fmt("Vibrar##v%d", pd.slot).c_str(), ButtonKind::Ghost, {0, S(32)}, Icon::Sparkle, true))
                SDL_RumbleGamepad(gp, 0xC000, 0xC000, 400);
            ImGui::Dummy({0, S(8)});
        }
        Muted(input_.last_input.empty() ? "El teclado también cuenta: presiona una tecla." : ("Último botón: " + input_.last_input).c_str(), 13);
    });
    if (!compact_) ImGui::SameLine(0, S(18));

    // ---------------- Memory Cards de PS1
    card("##t_memcards", "Memory Cards de PS1", Icon::Save, Color::Accent, S(330), [&]() {
        Muted("Cada juego de Satronho CD tiene su propia Memory Card. Aquí ves qué partidas guarda y puedes borrar las que ya no quieras.", 13);
        ImGui::Dummy({0, S(4)});
        const fs::path dir = Paths::Saves() / "cd";
        std::error_code ec;
        std::vector<fs::path> cards;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
            if (it->path().extension() == ".srm" && it->file_size(ec) == kCardSize) cards.push_back(it->path());
        std::sort(cards.begin(), cards.end());
        if (cards.empty()) {
            Muted("Todavía no hay Memory Cards: aparecen cuando guardes en un juego de PS1.", 14);
            return;
        }
        ImGui::BeginChild("##mc_lista", {0, S(210)});
        for (const auto& c : cards) {
            std::vector<uint8_t> d(kCardSize);
            {
                std::ifstream f(c, std::ios::binary);
                f.read(reinterpret_cast<char*>(d.data()), static_cast<std::streamsize>(d.size()));
            }
            const auto saves = ReadCard(d);
            int used = 0;
            for (const auto& s : saves) used += s.blocks;
            {
                FontScope f(GetFonts().semibold, 14);
                ImGui::TextUnformatted(Tr(Paths::ToUtf8(c.stem()).c_str()));
            }
            ImGui::SameLine();
            Muted(Fmt("  ·  %d de 15 bloques", used).c_str(), 12.5f);
            if (saves.empty()) Muted("   Vacía.", 13);
            for (const auto& s : saves) {
                ImGui::PushID(Fmt("%s%d", Paths::ToUtf8(c).c_str(), s.block).c_str());
                ImGui::Bullet();
                {
                    FontScope f(GetFonts().medium, 13.5f);
                    ImGui::TextUnformatted((s.title + "  " + (s.blocks == 1 ? std::string(Tr("(1 bloque)")) : Fmt("(%d bloques)", s.blocks))).c_str());
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(Tr("Borrar"))) {
                    ImGui::OpenPopup("##borrar_mc");
                }
                if (ImGui::BeginPopup("##borrar_mc")) {
                    FontScope f(GetFonts().medium, 14);
                    ImGui::TextUnformatted(Tr(Fmt("¿Borrar «%s»? No se puede deshacer.", s.title.c_str()).c_str()));
                    if (ImGui::Button(Tr("Sí, borrar"))) {
                        // Copia de seguridad antes de tocar nada.
                        fs::copy_file(c, fs::path(c).concat(".bak"), fs::copy_options::overwrite_existing, ec);
                        std::vector<uint8_t> e = d;
                        DeleteSave(e, s.block);
                        std::ofstream o(c, std::ios::binary | std::ios::trunc);
                        o.write(reinterpret_cast<const char*>(e.data()), static_cast<std::streamsize>(e.size()));
                        Log::Info("Memory Card %s: borrada «%s»", Paths::ToUtf8(c.filename()).c_str(), s.title.c_str());
                        Toast("Partida borrada (queda una copia .bak por si acaso).", GuideId::Samy, 4);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(Tr("No"))) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            ImGui::Dummy({0, S(6)});
        }
        ImGui::EndChild();
    });

    ImGui::Dummy({0, S(18)});

    // ---------------- Carátulas
    card("##t_caratulas", "Carátulas", Icon::Grid, Color::Teal, S(200), [&]() {
        Muted("Se descargan solas de libretro-thumbnails cuando hay internet. Clic derecho en un juego para poner la tuya.", 13);
        ImGui::Dummy({0, S(8)});
        if (Button("Buscar carátulas ahora", ButtonKind::Secondary, {0, 0}, Icon::Refresh, true)) {
            covers_.QueueDownloads(library_.Snapshot());
            Toast("Buscando carátulas…", GuideId::Troya, 3);
        }
        ImGui::SameLine(0, S(8));
        if (Button("Abrir la carpeta", ButtonKind::Ghost, {0, 0}, Icon::Folder, true)) OpenPath(Paths::User() / "caratulas");
    });
    if (!compact_) ImGui::SameLine(0, S(18));

    // ---------------- Carpetas
    card("##t_carpetas", "Carpetas de Satronho", Icon::Folder, Color::Green, S(200), [&]() {
        auto btn = [&](const char* label, const fs::path& p) {
            if (Button(label, ButtonKind::Secondary, {0, S(34)}, Icon::Folder, true)) OpenPath(p);
        };
        btn("Datos", Paths::User());
        ImGui::SameLine(0, S(8));
        btn("Partidas", Paths::Saves());
        ImGui::SameLine(0, S(8));
        btn("Estados", Paths::User() / "estados");
        btn("Capturas", Paths::User() / "capturas");
        ImGui::SameLine(0, S(8));
        btn("BIOS", Paths::System());
        ImGui::SameLine(0, S(8));
        btn("Registro (log)", Paths::User());
    });

    ImGui::Dummy({0, S(18)});
    // ---------------- Partidas: respaldar y restaurar (también entre PC y celular)
    card("##t_partidas", "Partidas", Icon::Save, Color::Pink, S(200), [&]() {
        Muted("Un .zip con las partidas y los estados de todos tus juegos. Se restaura aquí o en el celular, y cada partida vuelve a su juego aunque el "
              "archivo se llame distinto.",
              13);
        ImGui::Dummy({0, S(8)});
        if (Button("Respaldar todas las partidas", ButtonKind::Primary, {0, 0}, Icon::Save, true)) BackupAndShare(library_.Snapshot(), "todas");
        ImGui::SameLine(0, S(8));
        if (Button("Restaurar un respaldo", ButtonKind::Secondary, {0, 0}, Icon::Folder, true)) {
            static const SDL_DialogFileFilter zip[] = {{"Respaldo de Satronho (.zip)", "zip"}};
            SDL_ShowOpenFileDialog(&RestoreDialogCallback, this, window_, zip, 1, Paths::ToUtf8(BackupFolder()).c_str(), false);
        }
    });

    ImGui::Dummy({0, S(18)});
    // ---------------- Discos a .chd (el mismo panel de Ajustes » Biblioteca)
    PanelBegin("##t_chd", {compact_ ? avail : col_w * 2 + S(18), 0}, Color::Surface, 16, {20, 18});
    DrawChdPanel();
    PanelEnd();
}


// ---------------------------------------------------------------- interfaz nativa (Android)

void App::NativePageTools(NPage& p) {
    p.Section("");
    p.Guide("nono", "Aquí tienes lo que no es jugar: probar tus mandos, mirar las partidas de tus Memory Cards y abrir las carpetas de Satronho.");

    p.Section("Partidas", "Un .zip con las partidas y los estados de todos tus juegos (y los de Satronho NX). Se restaura aquí, en otro celular o en el PC.");
    p.Action("respaldar_todo", "Respaldar todas las partidas", "", "guardar", [this] { BackupAndShare(library_.Snapshot(), "todas"); }, "primario");
    p.Action("restaurar", "Restaurar un respaldo", "Elige el .zip. Cada partida vuelve a su juego aunque el archivo se llame distinto.", "cargar",
             [] { NativeUi::Emit("{\"t\":\"restaurar\"}"); });

    // Prueba de mandos (en vivo).
    p.Section("Prueba de mandos");
    const auto pads = input_.Gamepads();
    if (pads.empty()) p.Info("No hay mandos conectados. Conecta uno y aparece aquí al momento.", "", "mando");
    for (const auto& pd : pads) {
        SDL_Gamepad* gp = SDL_GetGamepadFromID(pd.id);
        std::string down;
        if (gp)
            for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b)
                if (SDL_GetGamepadButton(gp, static_cast<SDL_GamepadButton>(b))) down += (down.empty() ? "" : "  ") + Input::PadCodeLabel(b, pd.family);
        const int slot = pd.slot;
        p.Action("vibrar" + std::to_string(slot), Fmt("Mando %d · %s", pd.slot + 1, pd.name.c_str()),
                 down.empty() ? Tr("Presiona botones para verlos aquí.") : down, "mando", [gp] {
                     if (gp) SDL_RumbleGamepad(gp, 0xA000, 0xA000, 400);
                 }, "", "", "Vibrar");
    }

    // Memory Cards de PS1.
    p.Section("Memory Cards de PS1", "Cada juego de Satronho CD tiene su propia Memory Card. Aquí ves qué partidas guarda y puedes borrar las que ya no quieras.");
    const fs::path dir = Paths::Saves() / "cd";
    std::error_code ec;
    std::vector<fs::path> cards;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().extension() == ".srm" && it->file_size(ec) == kCardSize) cards.push_back(it->path());
    std::sort(cards.begin(), cards.end());
    if (cards.empty()) p.Info("Todavía no hay Memory Cards: aparecen cuando guardes en un juego de PS1.", "", "guardar");
    for (const auto& c : cards) {
        std::vector<uint8_t> d(kCardSize);
        {
            std::ifstream f(c, std::ios::binary);
            f.read(reinterpret_cast<char*>(d.data()), static_cast<std::streamsize>(d.size()));
        }
        const auto saves = ReadCard(d);
        int used = 0;
        for (const auto& s : saves) used += s.blocks;
        p.Info(Paths::ToUtf8(c.stem()), saves.empty() ? Tr("Vacía.") : TrFmt("%d de 15 bloques", used), "guardar");
        for (const auto& s : saves) {
            const int block = s.block;
            const std::string title = s.title;
            p.Action("mc_" + Paths::ToUtf8(c.filename()) + "_" + std::to_string(block), "   " + s.title,
                     s.blocks == 1 ? Tr("(1 bloque)") : TrFmt("(%d bloques)", s.blocks), "", [this, c, block, title] {
                         std::error_code e2;
                         std::vector<uint8_t> data(kCardSize);
                         {
                             std::ifstream f(c, std::ios::binary);
                             f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
                         }
                         fs::copy_file(c, fs::path(c).concat(".bak"), fs::copy_options::overwrite_existing, e2);
                         DeleteSave(data, block);
                         std::ofstream o(c, std::ios::binary | std::ios::trunc);
                         o.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
                         Log::Info("Memory Card %s: borrada «%s»", Paths::ToUtf8(c.filename()).c_str(), title.c_str());
                         Toast("Partida borrada (queda una copia .bak por si acaso).", GuideId::Samy, 4);
                     }, "peligro", TrFmt("¿Borrar «%s»? No se puede deshacer.", s.title.c_str()), "Borrar");
        }
    }

    // Carátulas.
    p.Section("Carátulas", "Se descargan solas de libretro-thumbnails cuando hay internet.");
    p.Action("caratulas", "Buscar carátulas ahora", covers_.Downloading() ? TrFmt("Buscando… faltan %d", covers_.Remaining()) : "", "imagen", [this] {
        settings_.covers = true;
        SaveConfig();
        covers_.QueueDownloads(library_.Snapshot());
        Toast("Buscando carátulas…", GuideId::Troya, 3);
    });
}

} // namespace Satronho
