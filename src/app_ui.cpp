// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

#include "app.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "libretro.h"
#include "ui/controllers.h"
#include "util/log.h"

namespace Satronho {

using namespace Ui;

namespace {

constexpr ImGuiWindowFlags kFull = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

std::string Fmt(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    return buf;
}

std::string Lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

void OpenFolder(const fs::path& p) {
    std::string url = "file:///" + Paths::ToUtf8(p);
    for (auto& c : url)
        if (c == '\\') c = '/';
    SDL_OpenURL(url.c_str());
}

void Muted(const char* text, float size = 13.5f) {
    FontScope f(GetFonts().regular, size);
    ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

void Heading(const char* text, float size = 18) {
    FontScope f(GetFonts().semibold, size);
    ImGui::TextUnformatted(text);
}

unsigned TierColor(const std::string& tier) {
    if (tier == "Alto") return Color::Green;
    if (tier == "Equilibrado") return Color::Teal;
    return Color::Amber;
}


std::string RendererName(const std::string& d) {
    if (d.empty() || d == "auto") return "Automático";
    if (d == "direct3d11") return "Direct3D 11";
    if (d == "direct3d12") return "Direct3D 12";
    if (d == "direct3d") return "Direct3D 9";
    if (d == "opengl") return "OpenGL";
    if (d == "opengles2") return "OpenGL ES";
    if (d == "vulkan") return "Vulkan";
    if (d == "metal") return "Metal";
    if (d == "software") return "Software";
    return d;
}

std::string NiceValue(const CoreOption::Value& v) {
    if (v.label != v.value) return v.label;
    static const std::map<std::string, std::string> names{
        {"enabled", "Activado"}, {"disabled", "Desactivado"}, {"auto", "Automático"}, {"true", "Sí"}, {"false", "No"},
        {"none", "Ninguno"}, {"default", "Predeterminado"}, {"on", "Activado"}, {"off", "Desactivado"}};
    auto it = names.find(v.value);
    return it != names.end() ? it->second : v.label;
}

} // namespace

void App::GuideSays(GuideId id, const char* text, float width) {
    const GuideInfo& g = Guide(id);
    if (settings_.guides) {
        GuideBanner(guides_[static_cast<int>(id)], g.name, g.role, g.color, text, width);
    } else {
        Muted(text, 14);
    }
}

// ---------------------------------------------------------------- estructura

void App::DrawUi() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if (in_game_) {
        DrawGameLayer();
        DrawToasts();
        return;
    }

    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, V4(Color::Bg));
    ImGui::Begin("##principal", nullptr, kFull);
    ImGui::PopStyleColor();

    const float side_w = S(252);
    DrawSidebar(side_w);
    ImGui::SameLine(0, 0);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(38), S(30)});
    ImGui::BeginChild("##contenido", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    switch (page_) {
    case Page::Library: DrawLibrary(); break;
    case Page::Controls: DrawControls(); break;
    case Page::Suggested: DrawSuggested(); break;
    case Page::News: DrawNews(); break;
    case Page::Settings: DrawSettings(); break;
    case Page::About: DrawAbout(); break;
    }
    ImGui::Dummy({0, S(24)});
    ImGui::EndChild();
    ImGui::End();

    if (page_ == Page::Library) DrawGuideBubble();
    DrawToasts();
    if (show_welcome_) DrawWelcome();
}

void App::DrawSidebar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V4(Color::Side));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(16), S(22)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {S(6), S(4)});
    ImGui::BeginChild("##lateral", {width, 0}, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    // Marca.
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float ls = S(48);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (logo_) dl->AddImageRounded(logo_.Ref(), {p.x + S(4), p.y}, {p.x + S(4) + ls, p.y + ls}, {0, 0}, {1, 1}, IM_COL32_WHITE, S(12));
        {
            FontScope f(GetFonts().bold, 19);
            dl->AddText({p.x + ls + S(16), p.y + S(2)}, U32(Color::Text), "Satronho");
        }
        {
            FontScope f(GetFonts().semibold, 13);
            dl->AddText({p.x + ls + S(16), p.y + S(26)}, U32(Color::Accent), "Emulation");
        }
        ImGui::Dummy({width, ls + S(22)});
    }

    char count[16];
    std::snprintf(count, sizeof count, "%zu", games_.size());
    SectionLabel("   JUGAR");
    if (NavItem("Biblioteca", Icon::Library, page_ == Page::Library && lib_system_.empty(), games_.empty() ? nullptr : count)) {
        page_ = Page::Library;
        lib_system_.clear();
    }
    ImGui::Dummy({0, S(10)});
    SectionLabel("   CONSOLAS");
    for (const auto& sys : Systems()) {
        size_t n = 0;
        for (const auto& g : games_) n += g.system == sys.id;
        const std::string badge = n ? std::to_string(n) : std::string("Lista");
        const bool sel = page_ == Page::Library && lib_system_ == sys.id;
        if (NavItem(sys.name.c_str(), Icon::None, sel, badge.c_str(), false, sys.color, sys.hint.c_str())) {
            page_ = Page::Library;
            lib_system_ = sys.id;
        }
        // Ícono: el control (o la portátil) de esa consola en miniatura.
        const ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
        const bool hov = ImGui::IsItemHovered();
        DrawControllerMini(ImGui::GetWindowDrawList(), sys.layout, {r0.x + S(24), (r0.y + r1.y) * 0.5f},
                           sys.handheld ? S(26) : S(30), sel || hov ? 1.0f : 0.75f);
    }
    {
        std::string names;
        for (const auto& c : SoonSystems()) names += (names.empty() ? "" : ", ") + std::string(c.name);
        NavItem("Más consolas", Icon::Gamepad, false, "Pronto", true, Color::Accent);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) HelpTooltip(("Vienen en camino: " + names + ".").c_str());
    }
    ImGui::Dummy({0, S(10)});
    SectionLabel("   SATRONHO");
    if (NavItem("Configuración sugerida", Icon::Sparkle, page_ == Page::Suggested, nullptr, false, Color::Teal)) page_ = Page::Suggested;
    const bool update = updater_.GetState() == Updater::State::Available;
    if (NavItem("Novedades", Icon::Bell, page_ == Page::News, update ? "Nueva" : nullptr, false, Color::Pink)) page_ = Page::News;
    if (NavItem("Controles", Icon::Gamepad, page_ == Page::Controls, nullptr, false, Color::Amber)) page_ = Page::Controls;
    if (NavItem("Ajustes", Icon::Gear, page_ == Page::Settings)) page_ = Page::Settings;
    if (NavItem("Acerca de", Icon::Info, page_ == Page::About)) page_ = Page::About;

    // Pie: autor y build.
    const float foot_h = S(58);
    const float y = ImGui::GetWindowHeight() - foot_h - S(16);
    if (ImGui::GetCursorPosY() < y) ImGui::SetCursorPosY(y);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float w = ImGui::GetContentRegionAvail().x;
    dl->AddRectFilled(p, {p.x + w, p.y + foot_h}, U32(Color::Surface), S(14));
    if (guides_[0]) dl->AddImageRounded(guides_[0].Ref(), {p.x + S(10), p.y + S(10)}, {p.x + S(48), p.y + S(48)}, {0, 0}, {1, 1}, IM_COL32_WHITE, S(19));
    {
        FontScope f(GetFonts().semibold, 13.5f);
        dl->AddText({p.x + S(58), p.y + S(10)}, U32(Color::Text), "por Ridds");
    }
    {
        FontScope f(GetFonts().regular, 12);
        const std::string b = build_ > 0 ? Fmt("Build %d", build_) : std::string("Versión de desarrollo");
        dl->AddText({p.x + S(58), p.y + S(30)}, U32(Color::Dim), b.c_str());
    }
    ImGui::Dummy({w, foot_h});

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------- biblioteca

void App::DrawCoreMissing() {
    PanelBegin("##sin_nucleo", {0, 0}, 0x2A1622);
    std::string names;
    for (const auto& n : missing_cores_) names += (names.empty() ? "" : ", ") + n;
    IconText(Icon::Info, ("Falta el núcleo de: " + names).c_str(), U32(Color::Red));
    Muted("Esos emuladores necesitan sus archivos en la carpeta «cores» junto al programa. Vuelve a descargar la build completa desde los Releases.");
    PanelEnd();
    ImGui::Dummy({0, S(12)});
}

void App::DrawLibrary() {
    if (games_dirty_) {
        games_ = library_.Snapshot();
        games_dirty_ = false;
    }
    if (!missing_cores_.empty()) DrawCoreMissing();

    // Juegos de la consola elegida.
    const SystemDef* lib_sys = lib_system_.empty() ? nullptr : SystemById(lib_system_);
    std::vector<const Game*> shown;
    for (const auto& g : games_)
        if (!lib_sys || g.system == lib_sys->id) shown.push_back(&g);

    // Encabezado.
    const float avail = ImGui::GetContentRegionAvail().x;
    const ImVec2 top = ImGui::GetCursorScreenPos();
    const char* plural = shown.size() == 1 ? "juego" : "juegos";
    if (lib_sys) Title(lib_sys->full.c_str(), Fmt("Para %s · %zu %s", lib_sys->games.c_str(), shown.size(), plural).c_str());
    else Title("Biblioteca", Fmt("Todas tus consolas · %zu %s", shown.size(), plural).c_str());
    const float after_title = ImGui::GetCursorScreenPos().y;

    // Herramientas a la derecha.
    {
        const float search_w = std::min(S(280), avail * 0.32f);
        FontScope f(GetFonts().medium, 14);
        const float add_w = ImGui::CalcTextSize("Agregar carpeta").x + ImGui::GetFontSize() + S(44);
        const float open_w = S(42);
        float x = top.x + avail - add_w;
        ImGui::SetCursorScreenPos({x, top.y + S(6)});
        if (Button("Agregar carpeta", ButtonKind::Primary, {add_w, S(42)}, Icon::Plus, true)) AddFolderDialog();
        x -= open_w + S(8);
        ImGui::SetCursorScreenPos({x, top.y + S(6)});
        if (IconButton("##abrir", Icon::File, S(42), U32(Color::Muted), "Abrir un juego sin agregarlo")) OpenRomDialog();
        x -= open_w + S(4);
        ImGui::SetCursorScreenPos({x, top.y + S(6)});
        if (IconButton("##rescan", Icon::Refresh, S(42), U32(Color::Muted), "Volver a buscar juegos")) Rescan();
        x -= search_w + S(10);
        ImGui::SetCursorScreenPos({x, top.y + S(6)});
        const ImVec2 sp = ImGui::GetCursorScreenPos();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {S(40), S(11)});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(12));
        ImGui::SetNextItemWidth(search_w);
        ImGui::InputTextWithHint("##buscar", "Buscar en la biblioteca", search_, sizeof search_);
        ImGui::PopStyleVar(2);
        DrawIcon(ImGui::GetWindowDrawList(), Icon::Search, {sp.x + S(20), sp.y + S(21)}, S(15), U32(Color::Dim));
    }
    ImGui::SetCursorScreenPos({top.x, std::max(after_title, top.y + S(56)) + S(14)});

    if (shown.empty()) {
        if (library_.Scanning()) {
            Spinner(S(12), Color::Accent);
            ImGui::SameLine();
            Muted("Buscando juegos…");
        } else {
            DrawEmptyLibrary();
        }
        return;
    }

    // Seguir jugando.
    const std::string query = Lower(search_);
    if (query.empty()) {
        const Game* recent = nullptr;
        for (const Game* g : shown)
            if (g->last_played > 0 && (!recent || g->last_played > recent->last_played)) recent = g;
        if (recent) {
            DrawContinueHero(*recent, avail);
            ImGui::Dummy({0, S(10)});
        }
    }

    // Orden y filtros.
    {
        if (Segmented("##orden", &settings_.sort, {"A-Z", "Recientes", "Más jugados"})) SaveConfig();
        ImGui::SameLine(0, S(12));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        if (IconButton("##favs", settings_.favorites_only ? Icon::StarFill : Icon::Star, S(36),
                       U32(settings_.favorites_only ? Color::Gold : Color::Muted), "Solo favoritos", settings_.favorites_only)) {
            settings_.favorites_only = !settings_.favorites_only;
            SaveConfig();
        }
        (void)p;
        if (library_.Scanning()) {
            ImGui::SameLine(0, S(16));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(8));
            Spinner(S(9), Color::Accent);
            ImGui::SameLine();
            Muted("Actualizando…", 13);
        }
    }
    ImGui::Dummy({0, S(6)});

    std::vector<const Game*> list;
    for (const Game* g : shown) {
        if (settings_.favorites_only && !g->favorite) continue;
        if (!query.empty() && Lower(g->title).find(query) == std::string::npos) continue;
        list.push_back(g);
    }
    std::sort(list.begin(), list.end(), [&](const Game* a, const Game* b) {
        if (settings_.sort == 1 && a->last_played != b->last_played) return a->last_played > b->last_played;
        if (settings_.sort == 2 && a->playtime_s != b->playtime_s) return a->playtime_s > b->playtime_s;
        return Lower(a->title) < Lower(b->title);
    });

    if (list.empty()) {
        ImGui::Dummy({0, S(20)});
        Muted(settings_.favorites_only && query.empty() ? "Aún no marcas favoritos. Usa la estrella de un juego." : "No hay juegos con ese nombre.", 15);
        return;
    }

    // Cuadrícula.
    const float gap = S(18);
    const float min_w = S(188);
    const float width = ImGui::GetContentRegionAvail().x;
    const int cols = std::max(1, static_cast<int>((width + gap) / (min_w + gap)));
    const float cw = std::floor((width - gap * (cols - 1)) / cols);
    const float ch = cw * 0.95f + S(78);
    const int rows = (static_cast<int>(list.size()) + cols - 1) / cols;

    ImGuiListClipper clipper;
    clipper.Begin(rows, ch + gap);
    while (clipper.Step()) {
        for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r) {
            const ImVec2 row0 = ImGui::GetCursorScreenPos();
            for (int c = 0; c < cols; ++c) {
                const int i = r * cols + c;
                if (i >= static_cast<int>(list.size())) break;
                ImGui::SetCursorScreenPos({row0.x + c * (cw + gap), row0.y});
                const Game& g = *list[i];
                if (DrawGameCard(g, cw, ch, g.key == selected_key_)) {
                    StartGame(g.path);
                    return;
                }
            }
            ImGui::SetCursorScreenPos({row0.x, row0.y + ch + gap});
            ImGui::Dummy({0, 0});
        }
    }
}

void App::DrawContinueHero(const Game& g, float width) {
    const float h = S(150);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float hue = static_cast<float>(g.hue);
    dl->AddRectFilled(p, {p.x + width, p.y + h}, U32(Color::Surface), S(20));
    dl->AddRectFilledMultiColor({p.x + S(150), p.y + 1}, {p.x + width - S(20), p.y + h - 1}, Hsv(hue, 0.6f, 0.30f, 0.0f), Hsv(hue, 0.6f, 0.35f, 0.55f),
                                Hsv(hue, 0.6f, 0.35f, 0.55f), Hsv(hue, 0.6f, 0.30f, 0.0f));
    dl->AddRect(p, {p.x + width, p.y + h}, Hsv(hue, 0.5f, 0.7f, 0.35f), S(20));
    const SystemDef* gs = SystemById(g.system);
    DrawCartridge(dl, {p.x + S(14), p.y + S(12)}, {p.x + S(144), p.y + h - S(12)}, g.hue, g.title, S(14), false,
                  gs ? gs->layout : Layout::Nes, gs ? gs->label.c_str() : "OCTA");

    const float tx = p.x + S(170);
    {
        FontScope f(GetFonts().semibold, 12);
        dl->AddText({tx, p.y + S(24)}, U32(Color::Gold), "SEGUIR JUGANDO");
    }
    Ellipsize(dl, GetFonts().bold, S(26), {tx, p.y + S(44)}, U32(Color::Text), g.title, width - S(420), 1);
    {
        FontScope f(GetFonts().regular, 14);
        const std::string meta = FormatLastPlayed(g.last_played) + "  ·  " + FormatPlaytime(g.playtime_s) + " jugado";
        dl->AddText({tx, p.y + S(86)}, U32(Color::Muted), meta.c_str());
    }

    // Botones.
    const float bw = S(150);
    ImGui::SetCursorScreenPos({p.x + width - bw - S(26), p.y + (h - S(48)) * 0.5f});
    {
        FontScope f(GetFonts().semibold, 15);
        if (Button("Continuar##hero", ButtonKind::Primary, {bw, S(48)}, Icon::Play, true)) StartGame(g.path);
    }
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({width, h});
}

bool App::DrawGameCard(const Game& g, float w, float h, bool selected) {
    bool play = false;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(g.key.c_str());
    ImGui::SetNextItemAllowOverlap();
    const bool clicked = ImGui::InvisibleButton("##card", {w, h});
    const bool hovered = ImGui::IsItemHovered();
    const bool focused = ImGui::IsItemFocused();
    if (clicked) selected_key_ = g.key;
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) play = true;
    if (focused && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown))) play = true;
    if (ImGui::BeginPopupContextItem("##menu")) {
        if (ImGui::MenuItem("Jugar")) play = true;
        if (ImGui::MenuItem(g.favorite ? "Quitar de favoritos" : "Marcar como favorito")) {
            library_.ToggleFavorite(g.key);
            games_dirty_ = true;
        }
        if (ImGui::MenuItem("Abrir la carpeta del juego")) OpenFolder(g.path.parent_path());
        ImGui::EndPopup();
    }

    const ImGuiID anim_id = ImGui::GetID("##anim");
    ImGuiStorage* st = ImGui::GetStateStorage();
    float hv = st->GetFloat(anim_id, 0.0f);
    hv = std::clamp(hv + (hovered || focused ? 1.0f : -1.0f) * ImGui::GetIO().DeltaTime * 10.0f, 0.0f, 1.0f);
    st->SetFloat(anim_id, hv);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float lift = hv * S(4);
    const ImVec2 a(p.x, p.y - lift), b(p.x + w, p.y + h - lift);
    const float rr = S(16);
    if (hv > 0) dl->AddRectFilled({a.x + S(2), a.y + S(10)}, {b.x - S(2), b.y + S(8)}, IM_COL32(0, 0, 0, static_cast<int>(70 * hv)), rr);
    dl->AddRectFilled(a, b, U32(Color::Surface), rr);
    const float cover_h = w * 0.95f;
    const SystemDef* gs = SystemById(g.system);
    DrawCartridge(dl, a, {b.x, a.y + cover_h}, g.hue, g.title, rr, hv > 0.5f, gs ? gs->layout : Layout::Nes, gs ? gs->label.c_str() : "OCTA");
    if (selected || hv > 0) dl->AddRect(a, b, U32(selected ? Color::Accent : Color::Surface3, selected ? 1.0f : hv), rr, S(selected ? 2.0f : 1.5f));

    // Botón de jugar al pasar el mouse.
    if (hv > 0.01f) {
        const ImVec2 c(b.x - S(30), a.y + cover_h - S(30));
        dl->AddCircleFilled({c.x, c.y + S(2)}, S(21) * hv, IM_COL32(0, 0, 0, 90));
        dl->AddCircleFilled(c, S(21) * hv, U32(Color::Accent));
        DrawIcon(dl, Icon::Play, {c.x + S(2), c.y}, S(15) * hv, U32(0x14101F));
    }

    // Estrella de favorito.
    {
        const ImVec2 c(b.x - S(24), a.y + S(24));
        ImGui::SetCursorScreenPos({c.x - S(16), c.y - S(16)});
        const bool show = g.favorite || hv > 0.01f;
        if (ImGui::InvisibleButton("##fav", {S(32), S(32)})) {
            library_.ToggleFavorite(g.key);
            games_dirty_ = true;
        }
        const bool fh = ImGui::IsItemHovered();
        if (show) {
            dl->AddCircleFilled(c, S(15), IM_COL32(15, 13, 23, 170));
            DrawIcon(dl, g.favorite ? Icon::StarFill : Icon::Star, c, S(15), U32(g.favorite || fh ? Color::Gold : Color::Text, g.favorite ? 1.0f : 0.8f));
        }
    }

    // Texto.
    Ellipsize(dl, GetFonts().semibold, S(15), {a.x + S(14), a.y + cover_h + S(12)}, U32(Color::Text), g.title, w - S(28), 1);
    {
        FontScope f(GetFonts().regular, 12.5f);
        std::string meta = g.playtime_s > 0 ? FormatPlaytime(g.playtime_s) : std::string("Sin jugar");
        if (lib_system_.empty()) {
            if (const SystemDef* gsys = SystemById(g.system)) meta += "  ·  " + gsys->name;
        } else if (!g.tags.empty()) {
            meta += "  ·  " + g.tags.front();
        }
        Ellipsize(dl, GetFonts().regular, S(12.5f), {a.x + S(14), a.y + cover_h + S(38)}, U32(Color::Muted), meta, w - S(28), 1);
    }

    ImGui::PopID();
    ImGui::SetCursorScreenPos({p.x + w, p.y});
    return play;
}

void App::DrawEmptyLibrary() {
    const float w = std::min(S(640), ImGui::GetContentRegionAvail().x);
    ImGui::Dummy({0, S(20)});
    const float x0 = ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - w) * 0.5f;
    ImGui::SetCursorPosX(x0);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (logo_) ImGui::GetWindowDrawList()->AddImage(logo_.Ref(), {p.x + w * 0.5f - S(70), p.y}, {p.x + w * 0.5f + S(70), p.y + S(140)});
    ImGui::Dummy({w, S(150)});
    {
        FontScope f(GetFonts().bold, 26);
        const char* t = "Tu biblioteca está lista para llenarse";
        ImGui::SetCursorPosX(x0 + (w - ImGui::CalcTextSize(t).x) * 0.5f);
        ImGui::TextUnformatted(t);
    }
    ImGui::Dummy({0, S(6)});
    ImGui::SetCursorPosX(x0);
    GuideSays(GuideId::Samy,
              "Agrega la carpeta donde guardas tus juegos (NES, Game Boy, Game Boy Color o Game Boy Advance) y yo busco también en las subcarpetas. "
              "Si prefieres, arrastra un juego a la ventana y lo abrimos de una.",
              w);
    ImGui::Dummy({0, S(12)});
    const float bw = S(200);
    ImGui::SetCursorPosX(x0 + (w - bw * 2 - S(12)) * 0.5f);
    {
        FontScope f(GetFonts().semibold, 15);
        if (Button("Agregar carpeta##vacia", ButtonKind::Primary, {bw, S(50)}, Icon::Folder, true)) AddFolderDialog();
        ImGui::SameLine(0, S(12));
        if (Button("Abrir un juego##vacia", ButtonKind::Secondary, {bw, S(50)}, Icon::File, true)) OpenRomDialog();
    }
    ImGui::Dummy({0, S(18)});
    {
        FontScope f(GetFonts().regular, 12.5f);
        const char* t = "Satronho Emulation no incluye juegos. Usa copias de los cartuchos que tienes.";
        ImGui::SetCursorPosX(x0 + (w - ImGui::CalcTextSize(t).x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Dim));
        ImGui::TextUnformatted(t);
        ImGui::PopStyleColor();
    }
}

// ---------------------------------------------------------------- configuración sugerida

void App::DrawSuggested() {
    Title("Configuración sugerida", "Ñoño revisa tu equipo y deja todo listo para jugar");
    ImGui::Dummy({0, S(6)});
    const Suggestion s = Suggest(hw_, settings_);
    GuideSays(GuideId::Nono, s.summary.c_str());
    ImGui::Dummy({0, S(14)});

    const float avail = ImGui::GetContentRegionAvail().x;
    const bool two = avail > S(820);
    const float col_w = two ? (avail - S(18)) * 0.5f : avail;

    PanelBegin("##equipo", {col_w, 0});
    Heading("Tu equipo");
    ImGui::Dummy({0, S(4)});
    auto row = [&](Icon icon, const char* label, const std::string& value) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, {p.x + S(38), p.y + S(38)}, U32(Color::Surface2), S(10));
        DrawIcon(dl, icon, {p.x + S(19), p.y + S(19)}, S(18), U32(Color::Teal));
        ImGui::SetCursorScreenPos({p.x + S(50), p.y - S(1)});
        ImGui::BeginGroup();
        {
            FontScope f(GetFonts().medium, 12.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Dim));
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(10));
        {
            FontScope f(GetFonts().semibold, 14.5f);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + col_w - S(110));
            ImGui::TextWrapped("%s", value.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::EndGroup();
        ImGui::Dummy({0, S(4)});
    };
    row(Icon::Cpu, "PROCESADOR", hw_.cpu + Fmt("  ·  %d hilos", hw_.threads));
    row(Icon::Memory, "MEMORIA RAM", Fmt("%.1f GB", hw_.ram_mb / 1024.0));
    row(Icon::Chip, "TARJETA GRÁFICA", hw_.vram_mb > 0 ? hw_.gpu + Fmt("  ·  %d GB", (hw_.vram_mb + 512) / 1024) : hw_.gpu);
    row(Icon::Monitor, "PANTALLA", Fmt("%d × %d a %.0f Hz", hw_.display_w, hw_.display_h, hw_.refresh));
    row(Icon::Palette, "MOTOR GRÁFICO", RendererName(hw_.renderer));
    PanelEnd();

    if (two) ImGui::SameLine(0, S(18));
    PanelBegin("##recomendacion", {col_w, 0});
    Heading("Lo que te recomiendo");
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(3));
    Chip(s.tier.c_str(), TierColor(s.tier), true);
    ImGui::Dummy({0, S(2)});
    for (const auto& c : s.changes) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        DrawIcon(ImGui::GetWindowDrawList(), Icon::Check, {p.x + S(9), p.y + S(11)}, S(14), U32(Color::Teal));
        ImGui::SetCursorScreenPos({p.x + S(26), p.y});
        FontScope f(GetFonts().medium, 14.5f);
        ImGui::TextUnformatted(c.c_str());
    }
    ImGui::Dummy({0, S(6)});
    for (const auto& r : s.reasons) Muted(r.c_str(), 13);
    ImGui::Dummy({0, S(10)});
    {
        FontScope f(GetFonts().semibold, 15);
        if (Button("Aplícalo por mí", ButtonKind::Primary, {S(210), S(48)}, Icon::Sparkle, true)) ApplySuggestion();
    }
    if (settings_.preset == s.tier) {
        ImGui::SameLine(0, S(14));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(14));
        Chip("Aplicada", Color::Green);
    }
    PanelEnd();
}

// ---------------------------------------------------------------- novedades

void App::DrawNews() {
    Title("Novedades", "Lo nuevo de Satronho Emulation");
    ImGui::Dummy({0, S(6)});
    const std::string intro = build_ > 0
        ? Fmt("Estás en la build %d. Aquí te cuento qué trae cada versión: lo que agregamos, mejoramos y arreglamos.", build_)
        : std::string("Esta es una versión de desarrollo. Aquí te cuento qué trae cada build.");
    GuideSays(GuideId::Juana, intro.c_str());
    ImGui::Dummy({0, S(14)});
    DrawUpdatePanel();

    const Json& builds = news_["builds"];
    if (!builds.IsArray() || builds.size() == 0) {
        Muted("No hay novedades para mostrar.");
        return;
    }
    for (size_t i = 0; i < builds.size(); ++i) {
        const Json& b = builds[i];
        ImGui::PushID(static_cast<int>(i));
        PanelBegin("##build", {0, 0});
        {
            FontScope f(GetFonts().semibold, 12);
            ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Pink));
            ImGui::TextUnformatted((b["version"].String() + "  ·  " + b["fecha"].String()).c_str());
            ImGui::PopStyleColor();
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(6));
        Heading(b["titulo"].String().c_str(), 19);
        ImGui::Dummy({0, S(2)});
        for (const auto& c : b["cambios"].arr) {
            const std::string tipo = c["tipo"].String();
            const unsigned col = tipo == "nuevo" ? Color::Green : tipo == "mejora" ? Color::Accent : Color::Amber;
            const char* label = tipo == "nuevo" ? "Nuevo" : tipo == "mejora" ? "Mejora" : "Arreglo";
            const ImVec2 p = ImGui::GetCursorScreenPos();
            Chip(label, col);
            ImGui::SetCursorScreenPos({p.x + S(78), p.y - S(1)});
            FontScope f(GetFonts().regular, 14.5f);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - S(4));
            ImGui::TextWrapped("%s", c["texto"].String().c_str());
            ImGui::PopTextWrapPos();
            ImGui::Dummy({0, S(2)});
        }
        PanelEnd();
        ImGui::Dummy({0, S(12)});
        ImGui::PopID();
    }
}

void App::DrawUpdatePanel() {
    const Updater::State st = updater_.GetState();
    PanelBegin("##actualizar", {0, 0}, st == Updater::State::Available ? 0x2A1B2C : Color::Surface);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    DrawIcon(ImGui::GetWindowDrawList(), Icon::Refresh, {p.x + S(12), p.y + S(13)}, S(18), U32(Color::Pink));
    ImGui::SetCursorScreenPos({p.x + S(32), p.y});
    ImGui::BeginGroup();
    switch (st) {
    case Updater::State::Available:
        Heading(Fmt("¡Ya salió la build %d!", updater_.LatestBuild()).c_str(), 17);
        Muted("Se descarga, se instala encima y Satronho Emulation se vuelve a abrir solo. Tus juegos, partidas y ajustes no se tocan.", 13.5f);
        break;
    case Updater::State::Checking: Heading("Buscando actualizaciones…", 16); break;
    case Updater::State::Downloading: Heading("Descargando la build nueva…", 16); break;
    case Updater::State::Ready: Heading("Instalando…", 16); break;
    case Updater::State::UpToDate: Heading("Tienes la última build", 16); break;
    case Updater::State::Failed: Heading("No se pudo revisar si hay builds nuevas", 16); Muted(updater_.Error().c_str(), 13); break;
    default:
        Heading("Actualizaciones", 16);
        if (!updater_.Supported()) Muted("Las actualizaciones automáticas son para Windows.", 13);
        break;
    }
    ImGui::Dummy({0, S(4)});
    const float row_y = ImGui::GetCursorPosY();
    if (st == Updater::State::Available) {
        if (Button("Actualizar ahora", ButtonKind::Primary, {0, 0}, Icon::Load, true)) {
            if (in_game_) StopGame();
            updater_.Install();
        }
        ImGui::SameLine();
    } else if (st == Updater::State::Checking || st == Updater::State::Downloading) {
        Spinner(S(10), Color::Pink);
        ImGui::SameLine();
    } else if (updater_.Supported() && build_ > 0) {
        if (Button("Buscar ahora", ButtonKind::Secondary, {0, 0}, Icon::Refresh, true)) updater_.Check(build_);
        ImGui::SameLine();
    }
    if (Button("Ver en GitHub", ButtonKind::Ghost, {0, 0}, Icon::File, true)) SDL_OpenURL(updater_.ReleaseUrl().c_str());
    ImGui::SameLine(0, S(18));
    ImGui::SetCursorPosY(row_y + S(9));
    if (Toggle("##auto", &settings_.auto_update)) SaveConfig();
    ImGui::SameLine(0, S(10));
    ImGui::SetCursorPosY(row_y + S(12));
    {
        FontScope f(GetFonts().medium, 13.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
        ImGui::TextUnformatted("Buscar al abrir");
        ImGui::PopStyleColor();
    }
    ImGui::EndGroup();
    PanelEnd();
    ImGui::Dummy({0, S(14)});
}

// ---------------------------------------------------------------- ajustes

void App::DrawSettings() {
    Title("Ajustes", "Todo se guarda al momento");
    ImGui::Dummy({0, S(8)});

    struct TabDef {
        SettingsTab tab;
        const char* name;
        Icon icon;
    };
    static const TabDef tabs[] = {
        {SettingsTab::Video, "Imagen", Icon::Monitor},       {SettingsTab::Audio, "Sonido", Icon::Volume},
        {SettingsTab::Controls, "Controles", Icon::Gamepad}, {SettingsTab::Performance, "Rendimiento", Icon::Forward},
        {SettingsTab::Meter, "Medidor", Icon::Meter},        {SettingsTab::Core, "Núcleo", Icon::Chip},
        {SettingsTab::Library, "Biblioteca", Icon::Folder},  {SettingsTab::Guides, "Guías", Icon::Heart},
    };
    ImGui::BeginChild("##tabs", {S(210), 0}, 0, ImGuiWindowFlags_NoScrollbar);
    for (const auto& t : tabs)
        if (NavItem(t.name, t.icon, tab_ == t.tab)) tab_ = t.tab;
    ImGui::EndChild();
    ImGui::SameLine(0, S(24));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(28), S(24)});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V4(Color::Surface));
    ImGui::BeginChild("##panel_ajustes", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    switch (tab_) {
    case SettingsTab::Video: DrawSettingsVideo(); break;
    case SettingsTab::Audio: DrawSettingsAudio(); break;
    case SettingsTab::Controls:
        page_ = Page::Controls;
        tab_ = SettingsTab::Video;
        break;
    case SettingsTab::Performance: DrawSettingsPerformance(); break;
    case SettingsTab::Meter: DrawSettingsMeter(); break;
    case SettingsTab::Core: DrawSettingsCore(); break;
    case SettingsTab::Library: DrawSettingsLibrary(); break;
    case SettingsTab::Guides: DrawSettingsGuides(); break;
    default: break;
    }
    ImGui::Dummy({0, S(10)});
    ImGui::EndChild();
}

void App::DrawSettingsVideo() {
    GuideSays(GuideId::Troya,
              "«Nítido suave» deja los píxeles cuadrados y sin bordes temblorosos a cualquier tamaño. "
              "Para sentirte frente al televisor de antes, prueba la «Pantalla CRT».");
    ImGui::Dummy({0, S(16)});
    bool changed = false;

    RowBegin("Pantalla completa", "También con F11 o Alt+Enter.");
    bool fs = settings_.fullscreen;
    if (Toggle("##fs", &fs)) {
        SetFullscreen(fs);
        changed = true;
    }
    RowEnd();

    RowBegin("Pantalla completa a 60 Hz", "Si tu pantalla es de 120 o 144 Hz, al jugar en pantalla completa la cambio a 60 Hz: las consolas "
                                          "van a 60 cuadros y así el movimiento queda perfectamente parejo.");
    if (Toggle("##fs60", &settings_.fullscreen_60hz)) {
        if (settings_.fullscreen) SetFullscreen(true);
        changed = true;
    }
    RowEnd();

    RowBegin("Tamaño de la imagen", "Ajustar llena la pantalla sin deformar. Entero usa solo múltiplos exactos (píxeles perfectos, con bordes).");
    changed |= Segmented("##escala", &settings_.scale_mode, {"Ajustar", "Entero", "Estirar"}, S(320));
    RowEnd();

    RowBegin("Filtro", "Cómo se agrandan los píxeles.");
    changed |= Segmented("##filtro", &settings_.filter, {"Píxel exacto", "Nítido suave", "Suave"}, S(320));
    RowEnd();

    RowBegin("Efecto de pantalla", "«Auto» usa televisor CRT en las consolas de casa y pantalla LCD en las portátiles.");
    changed |= Segmented("##efecto", &settings_.effect, {"Ninguno", "Líneas", "CRT", "LCD", "Auto"}, S(340));
    RowEnd();

    if (settings_.effect != 0) {
        RowBegin("Intensidad del efecto", nullptr);
        changed |= SliderPercent("##intensidad", &settings_.effect_strength, S(320));
        RowEnd();
    }

    RowBegin("Fondo ambiente", "Llena los bordes con los colores del juego, suaves y desenfocados.");
    changed |= Toggle("##amb", &settings_.ambient);
    RowEnd();

    RowBegin("Sincronización vertical", "Evita que la imagen se corte. Déjala activada salvo que sepas lo que haces.");
    if (Toggle("##vsync", &settings_.vsync)) {
        SDL_SetRenderVSync(renderer_, settings_.vsync ? 1 : 0);
        changed = true;
    }
    RowEnd();

    // Motores gráficos disponibles en este equipo.
    std::vector<std::string> names{"Automático"};
    std::vector<std::string> values{"auto"};
    for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
        const std::string d = SDL_GetRenderDriver(i);
        if (d == "software" || d == "gpu") continue;
        names.push_back(RendererName(d));
        values.push_back(d);
    }
    int cur = 0;
    for (size_t i = 0; i < values.size(); ++i)
        if (values[i] == settings_.renderer) cur = static_cast<int>(i);
    RowBegin("Motor gráfico", Fmt("Se aplica al volver a abrir Satronho Emulation. Ahora: %s.", RendererName(hw_.renderer).c_str()).c_str());
    if (ComboStyled("##motor", &cur, names, S(320))) {
        settings_.renderer = values[cur];
        changed = true;
        Toast("El motor gráfico cambia la próxima vez que abras Satronho Emulation.", GuideId::Troya, 4);
    }
    RowEnd();

    static const float scales[] = {0.0f, 0.9f, 1.0f, 1.15f, 1.3f, 1.5f};
    int si = 0;
    for (int i = 0; i < 6; ++i)
        if (std::fabs(settings_.ui_scale - scales[i]) < 0.02f) si = i;
    RowBegin("Tamaño de la interfaz", "«Auto» la agranda en pantallas 1440p y 4K para que todo se lea cómodo.");
    if (Segmented("##ui", &si, {"Auto", "90 %", "100 %", "115 %", "130 %", "150 %"}, S(400))) {
        settings_.ui_scale = scales[si];
        UpdateScale();
        changed = true;
    }
    RowEnd();

    if (changed) SaveConfig();
}

void App::DrawSettingsAudio() {
    bool changed = false;
    RowBegin("Volumen", nullptr);
    if (SliderPercent("##vol", &settings_.volume, S(320))) {
        ApplyAudioSettings();
        changed = true;
    }
    RowEnd();
    RowBegin("Latencia", "Menos es más rápido, pero si el sonido chisporrotea súbela un poco.");
    if (SliderIntStyled("##lat", &settings_.latency_ms, 24, 160, "%d ms", S(320))) {
        ApplyAudioSettings();
        changed = true;
    }
    RowEnd();
    RowBegin("Silenciar al avanzar rápido", "Evita el sonido acelerado mientras mantienes Tab.");
    changed |= Toggle("##ffm", &settings_.mute_fast_forward);
    RowEnd();
    if (!audio_.IsOpen()) {
        IconText(Icon::Info, "No hay salida de sonido disponible en este momento.", U32(Color::Orange));
    }
    if (changed) SaveConfig();
}

void App::DrawSettingsPerformance() {
    GuideSays(GuideId::Nono,
              "El adelanto de cuadros hace que los controles respondan más rápido que en la consola original. "
              "Con 1 cuadro casi nadie nota diferencias raras; con 2 puede haber saltitos en algunos juegos.");
    ImGui::Dummy({0, S(16)});
    bool changed = false;
    RowBegin("Adelanto de cuadros", "Reduce el retraso entre presionar un botón y verlo en pantalla.");
    changed |= Segmented("##ra", &settings_.runahead, {"Apagado", "1 cuadro", "2 cuadros"}, S(320));
    RowEnd();
    RowBegin("Rebobinar", "Mantén Retroceso (o el gatillo izquierdo) para volver atrás en el tiempo.");
    changed |= Toggle("##rw", &settings_.rewind);
    RowEnd();
    if (settings_.rewind) {
        RowBegin("Cuánto se puede rebobinar", "Más segundos usan más memoria (unos 3 MB por cada 10 s en NES).");
        changed |= SliderIntStyled("##rws", &settings_.rewind_seconds, 5, 60, "%d s", S(320));
        RowEnd();
    }
    static const int speeds[] = {2, 3, 4, 6};
    int fi = 1;
    for (int i = 0; i < 4; ++i)
        if (settings_.fast_forward == speeds[i]) fi = i;
    RowBegin("Velocidad del avance rápido", "Mientras mantienes Tab o el gatillo derecho.");
    if (Segmented("##ff", &fi, {"2×", "3×", "4×", "6×"}, S(320))) {
        settings_.fast_forward = speeds[fi];
        changed = true;
    }
    RowEnd();
    if (changed) SaveConfig();
}

void App::DrawSettingsMeter() {
    GuideSays(GuideId::Samy, "El medidor muestra en vivo cómo va el juego: cuadros por segundo, velocidad, tirones y uso del equipo. F3 lo cambia mientras juegas.");
    ImGui::Dummy({0, S(16)});
    bool changed = false;
    RowBegin("Modo", nullptr);
    changed |= Segmented("##mm", &settings_.meter, {"Apagado", "Compacto", "Detallado"}, S(320));
    RowEnd();
    RowBegin("Esquina", nullptr);
    changed |= Segmented("##me", &settings_.meter_corner, {"Arriba izq.", "Arriba der.", "Abajo izq.", "Abajo der."}, S(420));
    RowEnd();
    RowBegin("Color", MeterColorName(settings_.meter_color));
    for (int i = 0; i < 5; ++i) {
        ImGui::PushID(i);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##c", {S(36), S(36)})) {
            settings_.meter_color = i;
            changed = true;
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddCircleFilled({p.x + S(18), p.y + S(18)}, S(13), U32(MeterColor(i)));
        if (settings_.meter_color == i) dl->AddCircle({p.x + S(18), p.y + S(18)}, S(17), U32(Color::Text), 0, S(2));
        if (ImGui::IsItemHovered()) HelpTooltip(MeterColorName(i));
        ImGui::SameLine(0, S(6));
        ImGui::PopID();
    }
    ImGui::NewLine();
    RowEnd();
    if (settings_.meter > 0) {
        Heading("Vista previa", 15);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, {p.x + S(420), p.y + (settings_.meter == 2 ? S(290) : S(70))}, U32(0x05040A), S(14));
        DrawMeter(true, {p.x + S(16), p.y + S(16)});
        ImGui::Dummy({S(420), settings_.meter == 2 ? S(290) : S(70)});
    }
    if (changed) SaveConfig();
}

void App::DrawSettingsCore() {
    // Elegir de qué emulador se ven las opciones.
    std::vector<const char*> names;
    int cur = 0;
    for (size_t i = 0; i < Systems().size(); ++i) {
        names.push_back(Systems()[i].name.c_str());
        if (Systems()[i].id == core_tab_system_) cur = static_cast<int>(i);
    }
    if (in_game_) {
        for (size_t i = 0; i < Systems().size(); ++i)
            if (&Systems()[i] == sys_) cur = static_cast<int>(i);
    }
    ImGui::BeginDisabled(in_game_);
    if (Segmented("##nucleo_sis", &cur, names, S(420))) core_tab_system_ = Systems()[cur].id;
    ImGui::EndDisabled();
    ImGui::Dummy({0, S(10)});
    const SystemDef& tsys = Systems()[cur];
    std::string err;
    if (!in_game_ && !EnsureCore(tsys, &err)) {
        IconText(Icon::Info, ("No se pudo cargar el núcleo de " + tsys.full + ".").c_str(), U32(Color::Red));
        return;
    }
    if (!core_ok_) return;
    const auto& si = core_.SystemInfo();
    GuideSays(GuideId::Samy,
              Fmt("El corazón de %s es %s %s. Estas opciones son suyas; la mayoría se nota al momento y otras al reiniciar el juego.%s",
                  tsys.full.c_str(), si.library_name, si.library_version,
                  tsys.core == "mgba" ? " Bolsillo y Bolsillo Plus comparten el mismo núcleo y sus opciones." : "").c_str());
    ImGui::Dummy({0, S(16)});
    bool changed = false;
    auto& opts = core_.Options();
    auto draw_option = [&](CoreOption& o) {
        if (!o.visible || o.values.size() < 2) return;
        ImGui::PushID(o.key.c_str());
        RowBegin(o.label.c_str(), o.info.c_str());
        std::vector<std::string> labels;
        int cur = 0;
        for (size_t i = 0; i < o.values.size(); ++i) {
            labels.push_back(NiceValue(o.values[i]));
            if (o.values[i].value == o.current) cur = static_cast<int>(i);
        }
        if (ComboStyled("##v", &cur, labels, S(320))) {
            core_.SetOption(o.key, o.values[cur].value);
            changed = true;
        }
        RowEnd();
        ImGui::PopID();
    };
    if (core_.Categories().empty()) {
        for (auto& o : opts) draw_option(o);
    } else {
        for (const auto& cat : core_.Categories()) {
            bool any = false;
            for (const auto& o : opts) any |= o.category == cat.key && o.visible;
            if (!any) continue;
            Heading(cat.label.c_str(), 16);
            ImGui::Dummy({0, S(4)});
            for (auto& o : opts)
                if (o.category == cat.key) draw_option(o);
        }
        bool header = false;
        for (auto& o : opts) {
            if (!o.category.empty()) continue;
            if (!header) {
                Heading("General", 16);
                header = true;
            }
            draw_option(o);
        }
    }
    if (Button("Restablecer opciones del núcleo", ButtonKind::Ghost)) {
        for (auto& o : opts) core_.SetOption(o.key, o.def);
        changed = true;
    }
    if (changed) SaveConfig();
}

void App::DrawSettingsLibrary() {
    Heading("Carpetas de juegos", 16);
    Muted("Se busca en cada carpeta y sus subcarpetas.", 13);
    ImGui::Dummy({0, S(4)});
    int remove = -1;
    for (size_t i = 0; i < settings_.folders.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, {p.x + w, p.y + S(46)}, U32(Color::Surface2), S(12));
        DrawIcon(dl, Icon::Folder, {p.x + S(24), p.y + S(23)}, S(18), U32(Color::Amber));
        {
            FontScope f(GetFonts().medium, 14);
            Ellipsize(dl, GetFonts().medium, S(14), {p.x + S(46), p.y + S(13)}, U32(Color::Text), settings_.folders[i], w - S(110), 1);
        }
        ImGui::SetCursorScreenPos({p.x + w - S(42), p.y + S(5)});
        if (IconButton("##quitar", Icon::Close, S(36), U32(Color::Muted), "Quitar esta carpeta")) remove = static_cast<int>(i);
        ImGui::SetCursorScreenPos({p.x, p.y + S(54)});
        ImGui::Dummy({0, 0});
        ImGui::PopID();
    }
    if (remove >= 0) {
        settings_.folders.erase(settings_.folders.begin() + remove);
        SaveConfig();
        Rescan();
    }
    if (settings_.folders.empty()) Muted("Aún no hay carpetas.", 14);
    ImGui::Dummy({0, S(6)});
    if (Button("Agregar carpeta##aj", ButtonKind::Primary, {0, 0}, Icon::Plus, true)) AddFolderDialog();
    ImGui::SameLine();
    if (Button("Volver a buscar", ButtonKind::Secondary, {0, 0}, Icon::Refresh, true)) Rescan();
    ImGui::Dummy({0, S(20)});

    RowBegin("Carpeta de datos", Paths::Portable() ? "Modo portátil: todo se guarda junto al programa." : Paths::ToUtf8(Paths::User()).c_str());
    if (Button("Abrir carpeta", ButtonKind::Secondary, {0, 0}, Icon::Folder, true)) OpenFolder(Paths::User());
    RowEnd();
    RowBegin("Capturas de pantalla", Paths::ToUtf8(Paths::Screenshots()).c_str());
    if (Button("Abrir capturas", ButtonKind::Secondary, {0, 0}, Icon::Camera, true)) OpenFolder(Paths::Screenshots());
    RowEnd();
    RowBegin("BIOS y archivos del sistema", "Para juegos de Famicom Disk System se necesita «disksys.rom» en esta carpeta.");
    if (Button("Abrir carpeta del sistema", ButtonKind::Secondary, {0, 0}, Icon::Folder, true)) OpenFolder(Paths::System());
    RowEnd();
}

void App::DrawSettingsGuides() {
    bool changed = false;
    RowBegin("Mostrar a los guías", "Samy, Troya, Ñoño, Juana y MechaRidds te explican las cosas por el camino.");
    changed |= Toggle("##g", &settings_.guides);
    RowEnd();
    RowBegin("Burbuja de consejos", "Un consejo en la esquina de la biblioteca de vez en cuando.");
    if (Toggle("##b", &settings_.bubble)) {
        changed = true;
        bubble_closed_ = false;
    }
    RowEnd();
    ImGui::Dummy({0, S(6)});
    const float w = ImGui::GetContentRegionAvail().x;
    const float cw = (w - S(16)) * 0.5f;
    static const char* bios[] = {
        "El protagonista. Conoce cada rincón de Satronho y te acompaña en todo.",
        "Pitbull terrier café con blanco. Te explica los juegos, los controles, los atajos y los estados.",
        "Pitbull malinois de cejitas amarillas y gafitas. Sabe de filtros, efectos y todo lo que se ve.",
        "Labrabull blanco con negro, el más grande. Revisa tu equipo y te dice qué configuración le conviene.",
        "La viejita criolla negra de canas y moñito, la más pequeña. Te cuenta las novedades de cada build.",
    };
    for (int i = 0; i < static_cast<int>(GuideId::Count); ++i) {
        if (i % 2) ImGui::SameLine(0, S(16));
        const GuideInfo& g = Guide(static_cast<GuideId>(i));
        GuideBanner(guides_[i], g.name, g.role, g.color, bios[i], cw);
    }
    if (changed) SaveConfig();
}


// ---------------------------------------------------------------- controles

int App::BindingChip(const char* id, bool keyboard, const std::string& text, bool capturing, bool pressed, float width) {
    const float h = S(32);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, {width, h});
    const bool hovered = ImGui::IsItemHovered();
    int result = 0;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) result = 1;
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) result = 2;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(ImGui::GetTime() * 7));
    dl->AddRectFilled(p, {p.x + width, p.y + h}, U32(hovered ? Color::Surface3 : Color::Surface2), S(9));
    if (pressed) dl->AddRectFilled(p, {p.x + width, p.y + h}, U32(Color::Gold, 0.22f), S(9));
    if (capturing) dl->AddRect(p, {p.x + width, p.y + h}, U32(Color::Accent, 0.4f + 0.6f * pulse), S(9), S(2));
    DrawIcon(dl, keyboard ? Icon::Keyboard : Icon::Gamepad, {p.x + S(16), p.y + h * 0.5f}, S(15), U32(capturing ? Color::Accent : Color::Dim));
    const std::string t = capturing ? std::string("Presiona…") : text;
    Ellipsize(dl, GetFonts().medium, S(14), {p.x + S(32), p.y + (h - S(14)) * 0.5f - S(1)},
              U32(capturing ? Color::Accent : (t == "—" ? Color::Dim : Color::Text)), t, width - S(38), 1);
    if (hovered && !capturing) HelpTooltip("Clic: asignar (tecla o botón del mando, lo que presiones) · Clic derecho: dejar vacío");
    return result;
}

void App::DrawControllerEditor(const SystemDef& sys, int player, bool compact) {
    ImGui::PushID(sys.id.c_str());
    ImGui::PushID(player);
    PlayerConfig& pc = input_.players[player];
    const auto pads = input_.Gamepads();
    const PadFamily family = input_.FamilyOfSlot(pc.pad);
    bool changed = false;

    // Barra: dispositivo y perfil.
    {
        FontScope f(GetFonts().medium, 14.5f);
        const float y0 = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(y0 + S(5));
        if (Toggle("##teclado", &pc.keyboard)) changed = true;
        ImGui::SameLine(0, S(8));
        ImGui::SetCursorPosY(y0 + S(8));
        ImGui::TextUnformatted("Teclado");
        ImGui::SameLine(0, S(18));
        ImGui::SetCursorPosY(y0);
        std::vector<std::string> pad_names{"Sin mando"};
        for (int i = 0; i < 4; ++i) {
            if (i < static_cast<int>(pads.size())) pad_names.push_back(Fmt("Mando %d · %s", i + 1, pads[i].name.c_str()));
            else pad_names.push_back(Fmt("Mando %d (no conectado)", i + 1));
        }
        int pad_sel = pc.pad + 1;
        if (ComboStyled("##mando", &pad_sel, pad_names, compact ? S(250) : S(300))) {
            pc.pad = pad_sel - 1;
            changed = true;
        }
        ImGui::SameLine(0, S(14));
        const std::vector<std::string> profs = input_.Profiles(sys.id);
        const std::string& current = input_.PlayerProfile(player, sys);
        int prof_sel = 0;
        for (size_t i = 0; i < profs.size(); ++i)
            if (profs[i] == current) prof_sel = static_cast<int>(i);
        std::vector<std::string> prof_labels;
        for (const auto& n : profs) prof_labels.push_back("Perfil: " + n);
        // El combo del perfil cede espacio para que los tres íconos de al lado siempre quepan.
        const float prof_w = std::clamp(ImGui::GetContentRegionAvail().x - S(38) * 3 - S(14), S(150), S(240));
        if (ComboStyled("##perfil", &prof_sel, prof_labels, prof_w)) {
            input_.SetPlayerProfile(player, sys, profs[prof_sel]);
            changed = true;
        }
        ImGui::SameLine(0, S(6));
        if (IconButton("##nuevo", Icon::Plus, S(38), U32(Color::Muted), "Nuevo perfil (copia del actual)")) {
            profile_popup_ = 1;
            profile_player_ = player;
            ctrl_system_ = sys.id;
            std::snprintf(profile_name_, sizeof profile_name_, "Mi perfil");
        }
        ImGui::SameLine(0, S(2));
        ImGui::BeginDisabled(current == Input::kDefaultProfile);
        if (IconButton("##renombrar", Icon::File, S(38), U32(Color::Muted), "Renombrar perfil")) {
            profile_popup_ = 2;
            profile_player_ = player;
            ctrl_system_ = sys.id;
            std::snprintf(profile_name_, sizeof profile_name_, "%s", current.c_str());
        }
        ImGui::SameLine(0, S(2));
        if (IconButton("##borrar", Icon::Close, S(38), U32(Color::Muted), "Borrar perfil")) {
            input_.DeleteProfile(sys, current);
            changed = true;
        }
        ImGui::EndDisabled();
    }
    ImGui::Dummy({0, S(4)});
    {
        FontScope f(GetFonts().medium, 14);
        if (Button("Asignación automática", ButtonKind::Secondary, {0, S(38)}, Icon::Sparkle, true)) {
            input_.AutoMapPad(player, sys);
            changed = true;
            Toast("Mando asignado con la distribución de " + sys.name, GuideId::Samy, 2.5);
        }
        if (ImGui::IsItemHovered()) HelpTooltip("Pone los botones del mando en su lugar natural para esta consola.");
        ImGui::SameLine(0, S(8));
        if (Button("Uno por uno", ButtonKind::Secondary, {0, S(38)}, Icon::ChevronRight, true)) input_.StartWizard(player, sys, true);
        if (ImGui::IsItemHovered()) HelpTooltip("Te pide cada botón en orden. Presiona la tecla o el botón del mando que quieras para cada uno.");
        ImGui::SameLine(0, S(8));
        if (Button("Restablecer", ButtonKind::Ghost, {0, S(38)}, Icon::Refresh, true)) {
            input_.ResetProfile(player, sys);
            changed = true;
        }
        ImGui::SameLine(0, S(4));
        if (Button("Borrar todo", ButtonKind::Ghost, {0, S(38)}, Icon::Close, true)) {
            input_.ClearProfile(player, sys);
            changed = true;
        }
    }
    ImGui::Dummy({0, S(6)});

    // Aviso mientras se espera una tecla o botón, o el último botón que llegó.
    const bool cap_here = input_.Capturing() && !input_.capture.hotkey && input_.capture.sys == sys.id && input_.capture.player == player;
    auto label_of = [&](int id) {
        for (const auto& [bid, label] : sys.buttons)
            if (bid == id) return label;
        return "?";
    };
    {
        FontScope f(GetFonts().semibold, 15);
        if (cap_here) {
            const float pulse = 0.6f + 0.4f * static_cast<float>(std::sin(ImGui::GetTime() * 7));
            ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Accent, pulse));
            ImGui::TextUnformatted(Fmt("Presiona la tecla o el botón del mando para «%s»   ·   Esc cancela", label_of(input_.capture.button)).c_str());
            ImGui::PopStyleColor();
        } else {
            const double age = SDL_GetTicks() / 1000.0 - input_.last_input_time;
            ImGui::PushStyleColor(ImGuiCol_Text, V4(age < 1.5 ? Color::Gold : Color::Dim));
            ImGui::TextUnformatted(input_.last_input.empty() ? "Presiona cualquier botón para verlo en vivo." : ("Último botón: " + input_.last_input).c_str());
            ImGui::PopStyleColor();
        }
    }
    ImGui::Dummy({0, S(6)});

    Mapping& m = input_.Edit(player, sys);
    const uint16_t live = input_.LiveButtons(player, sys);

    // Casilla de un botón: nombre, su tecla y su botón de mando.
    const float gap = S(10);
    const float W = ImGui::GetContentRegionAvail().x;
    float cell_w = S(160);
    const float cell_h = S(112);
    auto cell = [&](int id, ImVec2 pos) {
        ImGui::PushID(id);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const bool down = (live >> id) & 1;
        const bool cap = cap_here && input_.capture.button == id;
        dl->AddRectFilled(pos, {pos.x + cell_w, pos.y + cell_h}, U32(Color::Bg, 0.65f), S(14));
        if (down) dl->AddRectFilled(pos, {pos.x + cell_w, pos.y + cell_h}, U32(Color::Gold, 0.12f), S(14));
        dl->AddRect(pos, {pos.x + cell_w, pos.y + cell_h}, U32(down ? Color::Gold : cap ? Color::Accent : Color::Border), S(14), down || cap ? S(2) : 1.0f);
        {
            FontScope f(GetFonts().semibold, 15);
            dl->AddText({pos.x + S(12), pos.y + S(8)}, U32(down ? Color::Gold : Color::Text), label_of(id));
        }
        const float cw = cell_w - S(20);
        ImGui::SetCursorScreenPos({pos.x + S(10), pos.y + S(36)});
        const int rk = BindingChip("##k", true, Input::KeyLabel(m.keys[id]), cap, down, cw);
        ImGui::SetCursorScreenPos({pos.x + S(10), pos.y + S(72)});
        const int rp = BindingChip("##p", false, Input::PadCodeLabel(m.pad[id], family), cap, down, cw);
        if (rk == 1 || rp == 1) input_.StartCapture(player, sys, id, rp == 1, true);
        if (rk == 2) { m.keys[id] = SDL_SCANCODE_UNKNOWN; changed = true; }
        if (rp == 2) { m.pad[id] = kPadNone; changed = true; }
        ImGui::PopID();
    };
    auto group_title = [&](const char* t, ImVec2 pos) {
        FontScope f(GetFonts().semibold, 13);
        ImGui::GetWindowDrawList()->AddText(pos, U32(Color::Dim), t);
    };
    const float title_h = S(24);
    const bool shoulders = !sys.shoulders.empty();
    const float aspect = ControllerAspect(sys.layout);
    const int cap_btn = cap_here ? input_.capture.button : -1;
    int hov = -1;
    bool clicked = false;
    auto drawing = [&](ImVec2 dp, ImVec2 ds) {
        ImGui::SetCursorScreenPos(dp);
        ImGui::InvisibleButton("##dibujo", ds);
        clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        hov = DrawController(ImGui::GetWindowDrawList(), sys.layout, dp, ds, live, cap_btn,
                             ImGui::IsItemHovered() ? ImGui::GetIO().MousePos : ImVec2(-1e5f, -1e5f));
    };
    // Grupos: cruceta en cruz; Select/Start (y gatillos); B, A y turbos.
    auto dpad_group = [&](ImVec2 p) {
        group_title("CRUCETA", p);
        const float gw = cell_w * 2 + gap;
        cell(RETRO_DEVICE_ID_JOYPAD_UP, {p.x + (gw - cell_w) * 0.5f, p.y + title_h});
        cell(RETRO_DEVICE_ID_JOYPAD_LEFT, {p.x, p.y + title_h + cell_h + gap});
        cell(RETRO_DEVICE_ID_JOYPAD_RIGHT, {p.x + cell_w + gap, p.y + title_h + cell_h + gap});
        cell(RETRO_DEVICE_ID_JOYPAD_DOWN, {p.x + (gw - cell_w) * 0.5f, p.y + title_h + 2 * (cell_h + gap)});
        return title_h + 3 * cell_h + 2 * gap;
    };
    // Botones de acción en filas centradas (rombo en SNES, 3 + 3 en Mega, B/A y turbos en NES).
    size_t face_cols = 2;
    for (const auto& row : sys.face) face_cols = std::max(face_cols, row.size());
    auto buttons_group = [&](ImVec2 p, float width) {
        group_title("BOTONES", p);
        const float saved = cell_w;
        cell_w = (width - gap * (face_cols - 1)) / face_cols;
        float y = p.y + title_h;
        for (const auto& row : sys.face) {
            const float row_w = row.size() * cell_w + (row.size() - 1) * gap;
            float x = p.x + (width - row_w) * 0.5f;
            for (int id : row) {
                cell(id, {x, y});
                x += cell_w + gap;
            }
            y += cell_h + gap;
        }
        cell_w = saved;
        return y - gap - p.y;
    };
    auto center_group = [&](ImVec2 p) {
        group_title(sys.center_title, p);
        cell(RETRO_DEVICE_ID_JOYPAD_SELECT, {p.x, p.y + title_h});
        cell(RETRO_DEVICE_ID_JOYPAD_START, {p.x + cell_w + gap, p.y + title_h});
        float h = title_h + cell_h;
        if (shoulders) {
            const float y = p.y + h + gap * 1.5f;
            group_title("GATILLOS", {p.x, y});
            cell(sys.shoulders[0], {p.x, y + title_h});
            if (sys.shoulders.size() > 1) cell(sys.shoulders[1], {p.x + cell_w + gap, y + title_h});
            h = y - p.y + title_h + cell_h;
        }
        return h;
    };

    const ImVec2 o = ImGui::GetCursorScreenPos();
    float total_h;
    const float col_gap = S(24);
    const float wide_cell = S(160);
    const float wide_side = wide_cell * 2 + gap;
    const float right_side = wide_cell * face_cols + gap * (face_cols - 1);
    if (!compact && W >= wide_side + right_side + col_gap * 2 + S(420)) {
        // Pantalla ancha: el control al centro y los grupos alrededor, como en DuckStation.
        cell_w = wide_cell;
        const float lh = dpad_group(o);
        const float rx = o.x + W - right_side;
        const float rh = buttons_group({rx, o.y}, right_side);
        const float cx0 = o.x + wide_side + col_gap, cx1 = rx - col_gap;
        const float ch = center_group({(cx0 + cx1) * 0.5f - cell_w - gap * 0.5f, o.y});
        const float draw_y = o.y + ch + S(20);
        float draw_h = std::clamp((cx1 - cx0) / aspect, S(200), S(440));
        total_h = std::max({lh, rh, draw_y - o.y + draw_h});
        draw_h = total_h - (draw_y - o.y);
        drawing({cx0, draw_y}, {cx1 - cx0, draw_h});
    } else {
        // Más angosto: el control grande arriba y los tres grupos debajo.
        // El dibujo crece hasta donde quepan debajo las casillas sin tener que bajar.
        const int rows = std::max<int>(3, static_cast<int>(sys.face.size()));
        const float groups_h = S(18) + title_h + rows * cell_h + (rows - 1) * gap;
        const float fit_h = ImGui::GetContentRegionAvail().y - groups_h - S(48);
        const float draw_h = std::clamp(std::min(W / aspect, fit_h), S(200), compact ? S(240) : S(320));
        drawing({o.x, o.y}, {W, draw_h});
        const float gy = o.y + draw_h + S(18);
        const float col_w = (W - col_gap * 2) / 3.0f;
        if (col_w / face_cols >= S(150)) {
            // Tres columnas: cruceta, centro y botones.
            cell_w = (col_w - gap) * 0.5f;
            const float h1 = dpad_group({o.x, gy});
            const float h2 = center_group({o.x + col_w + col_gap, gy});
            const float h3 = buttons_group({o.x + 2 * (col_w + col_gap), gy}, col_w);
            total_h = gy - o.y + std::max({h1, h2, h3});
        } else {
            // Angosto (menú de pausa): cruceta y centro lado a lado, y los botones debajo a todo lo ancho.
            const float half = (W - col_gap) * 0.5f;
            cell_w = (half - gap) * 0.5f;
            const float h1 = dpad_group({o.x, gy});
            const float h2 = center_group({o.x + half + col_gap, gy});
            const float by = gy + std::max(h1, h2) + S(18);
            const float h3 = buttons_group({o.x, by}, W);
            total_h = by - o.y + h3;
        }
    }
    if (clicked && hov >= 0) input_.StartCapture(player, sys, hov, pc.pad >= 0, true);
    if (hov >= 0 && !cap_here) HelpTooltip(Fmt("%s · clic para asignar", label_of(hov)).c_str());

    ImGui::SetCursorScreenPos({o.x, o.y + total_h + S(14)});

    // Inclinación para los juegos con sensor en el cartucho (solo jugador 1).
    if (sys.tilt && player == 0) {
        PanelBegin("##inclinacion", {0, 0}, Color::Bg, 14, {16, 14});
        float tx, ty, gz;
        input_.TiltNow(&tx, &ty, &gz);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float box = S(84);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, {p.x + box, p.y + box}, U32(Color::Surface2), S(12));
        dl->AddLine({p.x + box * 0.5f, p.y + S(8)}, {p.x + box * 0.5f, p.y + box - S(8)}, U32(Color::Border));
        dl->AddLine({p.x + S(8), p.y + box * 0.5f}, {p.x + box - S(8), p.y + box * 0.5f}, U32(Color::Border));
        const bool moving = std::fabs(tx) > 0.05f || std::fabs(ty) > 0.05f;
        dl->AddCircleFilled({p.x + box * 0.5f + tx * box * 0.38f, p.y + box * 0.5f + ty * box * 0.38f}, S(8), U32(moving ? Color::Gold : Color::Accent));
        ImGui::SetCursorScreenPos({p.x + box + S(18), p.y});
        ImGui::BeginGroup();
        Heading("Inclinación", 16);
        Muted("Para juegos con sensor en el cartucho, como Kirby Tilt 'n' Tumble o WarioWare Twisted. "
              "Con el teclado: I, K, J y L.", 13.5f);
        int src = input_.tilt_source;
        const bool motion = input_.PadHasMotion(pc.pad);
        if (Segmented("##fuente", &src, {"Stick derecho", "Sensor del mando"}, S(330))) {
            input_.tilt_source = src;
            changed = true;
        }
        if (src == 1 && !motion) {
            ImGui::SameLine(0, S(12));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(9));
            Muted("Tu mando no tiene sensor de movimiento (los de PS4, PS5 y Switch sí).", 13);
        }
        ImGui::SameLine(0, S(16));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(5));
        changed |= Toggle("##invx", &input_.tilt_invert_x);
        ImGui::SameLine(0, S(6));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(3));
        Muted("Invertir horizontal", 13.5f);
        ImGui::SameLine(0, S(14));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(3));
        changed |= Toggle("##invy", &input_.tilt_invert_y);
        ImGui::SameLine(0, S(6));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(3));
        Muted("Invertir vertical", 13.5f);
        ImGui::EndGroup();
        PanelEnd();
        ImGui::Dummy({0, S(8)});
    }

    Muted(Fmt("Nombres de botones como en un mando %s. Clic derecho en una casilla la deja vacía.",
              family == PadFamily::PlayStation ? "PlayStation" : family == PadFamily::Switch ? "de Switch" : "Xbox")
              .c_str(),
          13.5f);
    static bool was_capturing = false;
    if (was_capturing && !input_.Capturing()) changed = true; // se acaba de asignar algo
    was_capturing = input_.Capturing();
    if (changed) SaveConfig();
    ImGui::PopID();
    ImGui::PopID();
}

void App::DrawHotkeys() {
    GuideSays(GuideId::Samy, "Los atajos sirven en cualquier consola. Haz clic en una casilla y presiona la tecla o el botón del mando. "
                             "Select+Start en el mando siempre abre el menú, y Alt+Enter cambia a pantalla completa.");
    ImGui::Dummy({0, S(12)});
    const PadFamily family = input_.FamilyOfSlot(0);
    bool changed = false;
    for (int i = 0; i < static_cast<int>(Hotkey::Count); ++i) {
        ImGui::PushID(i);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        {
            FontScope f(GetFonts().semibold, 14.5f);
            ImGui::SetCursorScreenPos({p.x, p.y + S(4)});
            ImGui::TextUnformatted(HotkeyInfo(static_cast<Hotkey>(i)).name);
        }
        const bool cap = input_.Capturing() && input_.capture.hotkey && input_.capture.button == i;
        const float avail_w = ImGui::GetContentRegionAvail().x;
        const float chip_w = std::min(S(200), (avail_w - S(250)) * 0.5f - S(10));
        ImGui::SetCursorScreenPos({p.x + avail_w - chip_w * 2 - S(10), p.y});
        const int rk = BindingChip("##k", true, Input::KeyLabel(input_.hk_keys[i]), cap, false, chip_w);
        if (rk == 1) input_.StartHotkeyCapture(static_cast<Hotkey>(i), false);
        if (rk == 2) { input_.hk_keys[i] = SDL_SCANCODE_UNKNOWN; changed = true; }
        ImGui::SetCursorScreenPos({p.x + avail_w - chip_w, p.y});
        const int rp = BindingChip("##p", false, Input::PadCodeLabel(input_.hk_pad[i], family), cap, false, chip_w);
        if (rp == 1) input_.StartHotkeyCapture(static_cast<Hotkey>(i), true);
        if (rp == 2) { input_.hk_pad[i] = kPadNone; changed = true; }
        ImGui::SetCursorScreenPos({p.x, p.y + S(42)});
        ImGui::Dummy({0, 0});
        ImGui::PopID();
    }
    ImGui::Dummy({0, S(8)});
    if (Button("Restablecer atajos", ButtonKind::Ghost, {0, 0}, Icon::Refresh, true)) {
        input_.ResetHotkeys();
        changed = true;
    }
    static bool was_capturing = false;
    if (was_capturing && !input_.Capturing()) changed = true;
    was_capturing = input_.Capturing();
    if (changed) SaveConfig();
}

void App::DrawProfilePopup() {
    if (profile_popup_ == 0) return;
    ImGui::OpenPopup("##perfil_nombre");
    ImGui::SetNextWindowSize({S(420), 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(24), S(22)});
    if (ImGui::BeginPopupModal("##perfil_nombre", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize)) {
        Heading(profile_popup_ == 1 ? "Nuevo perfil" : "Renombrar perfil", 18);
        Muted(profile_popup_ == 1 ? "Se crea como copia de las asignaciones actuales." : "El perfil Predeterminado no se puede renombrar.", 13);
        ImGui::Dummy({0, S(6)});
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);
        const bool enter = ImGui::InputText("##nombre", profile_name_, sizeof profile_name_, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Dummy({0, S(8)});
        const SystemDef* sys = SystemById(ctrl_system_);
        if ((Button("Guardar", ButtonKind::Primary) || enter) && sys) {
            std::string name = profile_name_;
            while (!name.empty() && name.back() == ' ') name.pop_back();
            bool ok;
            if (profile_popup_ == 1) ok = input_.NewProfile(*sys, name, profile_player_);
            else ok = input_.RenameProfile(*sys, input_.PlayerProfile(profile_player_, *sys), name);
            if (ok) {
                SaveConfig();
                profile_popup_ = 0;
                ImGui::CloseCurrentPopup();
            } else {
                Toast("Ese nombre ya existe o no es válido (sin «.» ni «|»).", GuideId::Samy, 3);
            }
        }
        ImGui::SameLine();
        if (Button("Cancelar", ButtonKind::Ghost)) {
            profile_popup_ = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

void App::DrawControls() {
    Title("Controles", "Asigna los botones de cada consola y míralos en vivo");
    ImGui::Dummy({0, S(6)});
    std::vector<const char*> names;
    int cur = 0;
    for (size_t i = 0; i < Systems().size(); ++i) {
        names.push_back(Systems()[i].name.c_str());
        if (Systems()[i].id == ctrl_system_) cur = static_cast<int>(i);
    }
    if (Segmented("##ctrl_sis", &cur, names, S(420))) ctrl_system_ = Systems()[cur].id;
    const SystemDef& sys = Systems()[cur];
    ImGui::SameLine(0, S(16));
    Segmented("##ctrl_tab", &ctrl_tab_, {"General", "Jugador 1", "Jugador 2", "Atajos"}, S(480));
    ImGui::Dummy({0, S(10)});
    const auto pads = input_.Gamepads();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(24), S(20)});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V4(Color::Surface));
    ImGui::BeginChild("##ctrl_panel", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (ctrl_tab_ == 0) {
        bool changed = false;
        RowBegin("Zona muerta del stick", "Cuánto hay que mover el stick para que cuente.");
        changed |= SliderPercent("##dz", &input_.deadzone, S(320));
        RowEnd();
        RowBegin("El stick izquierdo también mueve la cruceta", nullptr);
        changed |= Toggle("##stick", &input_.stick_dpad);
        RowEnd();
        RowBegin("Registro de botones", "Anota en satronho.log cada botón que llega al juego. Sirve para revisar si algún botón se pierde.");
        changed |= Toggle("##registro", &input_.log_buttons);
        RowEnd();
        Muted(input_.last_input.empty() ? "Presiona cualquier botón para verlo aquí." : ("Último botón: " + input_.last_input).c_str(), 14);
        ImGui::Dummy({0, S(10)});
        Heading("Mandos conectados", 16);
        if (pads.empty()) Muted("Ninguno por ahora. Puedes conectar uno en cualquier momento.", 14);
        for (const auto& pd : pads) {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            DrawIcon(ImGui::GetWindowDrawList(), Icon::Gamepad, {p.x + S(12), p.y + S(12)}, S(20), U32(Color::Green));
            ImGui::SetCursorScreenPos({p.x + S(34), p.y});
            FontScope f(GetFonts().medium, 15);
            ImGui::TextUnformatted(Fmt("Mando %d · %s (%s)", pd.slot + 1, pd.name.c_str(), pd.family_name.c_str()).c_str());
        }
        if (changed) SaveConfig();
    } else if (ctrl_tab_ == 3) {
        DrawHotkeys();
    } else {
        DrawControllerEditor(sys, ctrl_tab_ - 1, false);
    }
    ImGui::EndChild();
    DrawProfilePopup();
}

// ---------------------------------------------------------------- acerca de

void App::DrawAbout() {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    if (logo_) ImGui::GetWindowDrawList()->AddImage(logo_.Ref(), p, {p.x + S(150), p.y + S(150)});
    ImGui::SetCursorScreenPos({p.x + S(176), p.y + S(16)});
    ImGui::BeginGroup();
    {
        FontScope f(GetFonts().bold, 34);
        ImGui::TextUnformatted("Satronho Emulation");
    }
    {
        FontScope f(GetFonts().semibold, 16);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(10));
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Accent));
        ImGui::TextUnformatted("por Ridds");
        ImGui::PopStyleColor();
    }
    Muted(build_ > 0 ? Fmt("Build %d", build_).c_str() : "Versión de desarrollo", 14);
    ImGui::EndGroup();
    ImGui::SetCursorScreenPos({p.x, p.y + S(170)});

    {
        FontScope f(GetFonts().regular, 15);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::min(ImGui::GetContentRegionAvail().x, S(820)));
        ImGui::TextWrapped("%s", ("La casa de los emuladores de la familia Satronho. Un solo lugar para tus consolas, con una imagen cuidada, "
                                  "controles que responden al instante, un medidor en vivo y configuraciones sugeridas para tu equipo. Ya están " +
                                  SystemsSentence() + "; pronto llegan más consolas.").c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::Dummy({0, S(14)});

    Muted("Logo, guías (Samy, Troya, Ñoño, Juana y MechaRidds), interfaz y diseño: Ridds. "
          "Software libre bajo GPL 3.0; las licencias de los componentes van en la carpeta «licencias».", 13);
    Muted("NES, Game Boy y Game Boy Advance son marcas de Nintendo, usadas solo para describir. Este proyecto no está afiliado a Nintendo y no incluye juegos.", 12.5f);
    ImGui::Dummy({0, S(14)});
    if (Button("Código fuente", ButtonKind::Secondary, {0, 0}, Icon::File, true)) SDL_OpenURL("https://github.com/Riddsc/Satronho-Emulation");
    ImGui::SameLine();
    if (Button("Licencias", ButtonKind::Secondary, {0, 0}, Icon::File, true)) OpenFolder(Paths::Base() / "licencias");
    ImGui::SameLine();
    if (Button("Abrir carpeta de datos", ButtonKind::Secondary, {0, 0}, Icon::Folder, true)) OpenFolder(Paths::User());
    ImGui::SameLine();
    if (Button("Ver el registro", ButtonKind::Ghost, {0, 0}, Icon::File, true)) {
        std::string url = "file:///" + Paths::ToUtf8(Paths::LogFile());
        for (auto& c : url)
            if (c == '\\') c = '/';
        SDL_OpenURL(url.c_str());
    }
}

// ---------------------------------------------------------------- capa del juego

void App::DrawGameLayer() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if (menu_open_) {
        DrawPauseMenu();
        return;
    }
    if (settings_.meter > 0) DrawMeter();

    // Indicadores de estado arriba al centro.
    std::string chip;
    Icon icon = Icon::Forward;
    unsigned col = Color::Accent;
    if (rewinding_) { chip = rewind_.empty() ? "Sin más historial" : "Rebobinando"; icon = Icon::Rewind; col = Color::Pink; }
    else if (fast_forward_) { chip = Fmt("Avance rápido ×%d", settings_.fast_forward); }
    else if (paused_) { chip = "En pausa"; icon = Icon::Pause; col = Color::Amber; }
    if (!chip.empty()) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        FontScope f(GetFonts().semibold, 14);
        const ImVec2 ts = ImGui::CalcTextSize(chip.c_str());
        const float w = ts.x + S(52), h = S(38);
        const ImVec2 p(vp->Pos.x + (vp->Size.x - w) * 0.5f, vp->Pos.y + S(20));
        dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(Color::Surface, 0.92f), h * 0.5f);
        dl->AddRect(p, {p.x + w, p.y + h}, U32(col, 0.6f), h * 0.5f);
        DrawIcon(dl, icon, {p.x + S(22), p.y + h * 0.5f}, S(15), U32(col));
        dl->AddText({p.x + S(38), p.y + (h - ts.y) * 0.5f}, U32(Color::Text), chip.c_str());
    }
}

void App::DrawPauseMenu() {
    DrawProfilePopup();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos, {vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y}, IM_COL32(8, 6, 14, 190));

    const float W = std::min(S(1000), vp->Size.x - S(60));
    const float H = std::min(S(620), vp->Size.y - S(60));
    ImGui::SetNextWindowPos({vp->Pos.x + (vp->Size.x - W) * 0.5f, vp->Pos.y + (vp->Size.y - H) * 0.5f});
    ImGui::SetNextWindowSize({W, H});
    ImGui::SetNextWindowFocus();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, V4(Color::Bg, 0.97f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(24));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, V4(Color::Accent, 0.35f));
    ImGui::Begin("##menu_juego", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    // Columna izquierda.
    const float left_w = S(280);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(20), S(22)});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V4(Color::Side));
    ImGui::BeginChild("##menu_izq", {left_w, 0}, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        DrawCartridge(ImGui::GetWindowDrawList(), p, {p.x + w, p.y + S(150)}, current_.hue, current_.title, S(14), false, sys_->layout,
                      sys_->label.c_str());
        ImGui::Dummy({w, S(156)});
        Ellipsize(ImGui::GetWindowDrawList(), GetFonts().bold, S(18), ImGui::GetCursorScreenPos(), U32(Color::Text), current_.title, w, 2);
        ImGui::Dummy({w, S(24) * (ImGui::CalcTextSize(current_.title.c_str()).x > w ? 2 : 1)});
        Muted(Fmt("Esta sesión: %s", FormatPlaytime(static_cast<long long>(session_s_)).c_str()).c_str(), 13);
        ImGui::Dummy({0, S(8)});
    }
    if (NavItem("Continuar", Icon::Play, false, "Esc", false, Color::Green)) SetMenuOpen(false);
    if (NavItem("Estados", Icon::Save, menu_section_ == 0)) menu_section_ = 0;
    if (NavItem("Ajustes rápidos", Icon::Gear, menu_section_ == 1, nullptr, false, Color::Accent)) menu_section_ = 1;
    if (NavItem("Controles", Icon::Gamepad, menu_section_ == 2, nullptr, false, Color::Amber)) menu_section_ = 2;
    if (NavItem("Reiniciar", Icon::Restart, false)) {
        core_.Reset();
        SetMenuOpen(false);
        Toast("Juego reiniciado", GuideId::Samy, 2);
    }
    if (NavItem("Captura", Icon::Camera, false)) Screenshot();
    if (NavItem("Salir a la biblioteca", Icon::Exit, false, nullptr, false, Color::Red)) {
        ImGui::EndChild();
        ImGui::End();
        StopGame();
        return;
    }
    ImGui::EndChild();
    ImGui::SameLine(0, 0);

    // Contenido.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(28), S(24)});
    ImGui::BeginChild("##menu_der", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    if (menu_section_ == 0) {
        Title("Estados", "Guarda en cualquier momento y vuelve justo ahí");
        ImGui::Dummy({0, S(4)});
        const float avail = ImGui::GetContentRegionAvail().x;
        const float gap = S(14);
        const float cw = std::floor((avail - gap * 3) / 4);
        const float thumb_h = cw * 240.0f / 256.0f * 0.82f;
        const float ch = thumb_h + S(92);
        for (int s = 1; s <= 8; ++s) {
            const int col = (s - 1) % 4;
            if (col) ImGui::SameLine(0, gap);
            ImGui::PushID(s);
            ImGui::BeginGroup();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const bool current = s == slot_;
            dl->AddRectFilled(p, {p.x + cw, p.y + ch}, U32(Color::Surface), S(14));
            if (current) dl->AddRect(p, {p.x + cw, p.y + ch}, U32(Color::Accent, 0.8f), S(14), S(1.5f));
            const ImVec2 t0(p.x + S(8), p.y + S(8)), t1(p.x + cw - S(8), p.y + S(8) + thumb_h);
            auto it = slot_thumbs_.find(s);
            if (it != slot_thumbs_.end() && it->second) {
                dl->AddImageRounded(it->second.Ref(), t0, t1, {0, 0}, {1, 1}, IM_COL32_WHITE, S(10));
            } else {
                dl->AddRectFilled(t0, t1, U32(Color::Surface2), S(10));
                DrawIcon(dl, Icon::Save, {(t0.x + t1.x) * 0.5f, (t0.y + t1.y) * 0.5f}, S(22), U32(Color::Dim));
            }
            {
                FontScope f(GetFonts().semibold, 13.5f);
                dl->AddText({p.x + S(12), t1.y + S(8)}, U32(current ? Color::Accent : Color::Text), Fmt("Ranura %d", s).c_str());
            }
            {
                FontScope f(GetFonts().regular, 11.5f);
                const auto d = slot_dates_.find(s);
                dl->AddText({p.x + S(12), t1.y + S(28)}, U32(Color::Dim), d != slot_dates_.end() ? d->second.c_str() : "Vacía");
            }
            const float bw = (cw - S(24) - S(6)) * 0.5f;
            ImGui::SetCursorScreenPos({p.x + S(12), p.y + ch - S(36)});
            FontScope f(GetFonts().semibold, 12.5f);
            if (Button("Guardar", ButtonKind::Secondary, {bw, S(28)})) {
                slot_ = s;
                SaveSlot(s);
            }
            ImGui::SetCursorScreenPos({p.x + S(12) + bw + S(6), p.y + ch - S(36)});
            ImGui::BeginDisabled(it == slot_thumbs_.end());
            if (Button("Cargar", ButtonKind::Primary, {bw, S(28)})) {
                slot_ = s;
                LoadSlot(s);
                SetMenuOpen(false);
            }
            ImGui::EndDisabled();
            ImGui::SetCursorScreenPos(p);
            ImGui::Dummy({cw, ch});
            ImGui::EndGroup();
            ImGui::PopID();
            if (col == 3) ImGui::Dummy({0, S(2)});
        }
        ImGui::Dummy({0, S(4)});
        Muted("F5 guarda y F9 carga en la ranura marcada. F6 y F7 cambian de ranura.", 13);
    } else if (menu_section_ == 1) {
        Title("Ajustes rápidos", "Cambia la imagen sin salir del juego");
        ImGui::Dummy({0, S(8)});
        bool changed = false;
        RowBegin("Efecto de pantalla", nullptr);
        changed |= Segmented("##qe", &settings_.effect, {"Ninguno", "Líneas", "CRT", "LCD", "Auto"}, S(320));
        RowEnd();
        RowBegin("Filtro", nullptr);
        changed |= Segmented("##qf", &settings_.filter, {"Píxel", "Nítido", "Suave"}, S(300));
        RowEnd();
        RowBegin("Tamaño", nullptr);
        changed |= Segmented("##qs", &settings_.scale_mode, {"Ajustar", "Entero", "Estirar"}, S(300));
        RowEnd();
        RowBegin("Medidor", nullptr);
        changed |= Segmented("##qm", &settings_.meter, {"Apagado", "Compacto", "Detallado"}, S(300));
        RowEnd();
        RowBegin("Volumen", nullptr);
        if (SliderPercent("##qv", &settings_.volume, S(300))) {
            ApplyAudioSettings();
            changed = true;
        }
        RowEnd();
        RowBegin("Adelanto de cuadros", nullptr);
        changed |= Segmented("##qr", &settings_.runahead, {"Apagado", "1", "2"}, S(300));
        RowEnd();
        if (changed) SaveConfig();
    } else {
        Title("Controles", "Toca un botón del dibujo y presiona la tecla o el botón que quieras");
        ImGui::Dummy({0, S(4)});
        static int player = 0;
        Segmented("##jug_menu", &player, {"Jugador 1", "Jugador 2"}, S(260));
        ImGui::Dummy({0, S(6)});
        DrawControllerEditor(*sys_, player, true);
    }
    ImGui::EndChild();
    ImGui::End();
}

void App::DrawMeter(bool preview, ImVec2 preview_pos) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = preview ? ImGui::GetWindowDrawList() : ImGui::GetForegroundDrawList();
    const unsigned accent = MeterColor(settings_.meter_color);
    const double fps_target = core_.GameLoaded() ? core_.Fps() : 60.0988;
    const double fps = preview && !in_game_ ? 60.0 : perf_.fps;
    const double speed = fps_target > 0 ? fps / fps_target * 100.0 : 0;
    float core_avg = 0;
    float host_max = 0;
    for (int i = 0; i < 180; ++i) {
        core_avg += perf_.core_ms[i];
        host_max = std::max(host_max, perf_.host_ms[i]);
    }
    core_avg /= 180.0f;
    if (preview && !in_game_) core_avg = 0.42f;

    const float pad = S(14);
    const bool detailed = settings_.meter == 2;
    const float w = detailed ? S(272) : S(0);
    const unsigned speed_col = speed < 95 && !paused_ ? Color::Orange : Color::Text;

    if (!detailed) {
        FontScope f(GetFonts().mono, 13.5f);
        const std::string s = Fmt("%.0f FPS  %3.0f %%  %.2f ms", fps, speed, core_avg);
        const ImVec2 ts = ImGui::CalcTextSize(s.c_str());
        const ImVec2 size(ts.x + S(34), ts.y + S(16));
        ImVec2 p = preview_pos;
        if (!preview) {
            const float m = S(16);
            p.x = (settings_.meter_corner % 2) ? vp->Pos.x + vp->Size.x - size.x - m : vp->Pos.x + m;
            p.y = (settings_.meter_corner >= 2) ? vp->Pos.y + vp->Size.y - size.y - m : vp->Pos.y + m;
        }
        dl->AddRectFilled(p, {p.x + size.x, p.y + size.y}, U32(0x1B1829, 0.88f), S(10));
        dl->AddCircleFilled({p.x + S(14), p.y + size.y * 0.5f}, S(4), U32(accent));
        dl->AddText({p.x + S(26), p.y + S(8)}, U32(speed_col), s.c_str());
        return;
    }

    const float graph_h = S(54);
    const float row_h = S(22);
    const int rows = 6;
    const float h = pad * 2 + S(44) + graph_h + S(10) + rows * row_h;
    ImVec2 p = preview_pos;
    if (!preview) {
        const float m = S(16);
        p.x = (settings_.meter_corner % 2) ? vp->Pos.x + vp->Size.x - w - m : vp->Pos.x + m;
        p.y = (settings_.meter_corner >= 2) ? vp->Pos.y + vp->Size.y - h - m : vp->Pos.y + m;
    }
    dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(0x1B1829, 0.90f), S(16));
    dl->AddRectFilled({p.x, p.y + S(16)}, {p.x + S(3), p.y + h - S(16)}, U32(accent), S(2));

    // Cabecera: FPS grande y velocidad.
    {
        FontScope f(GetFonts().bold, 30);
        const std::string s = Fmt("%.0f", fps);
        dl->AddText({p.x + pad + S(4), p.y + pad - S(4)}, U32(Color::Text), s.c_str());
        const float sw = ImGui::CalcTextSize(s.c_str()).x;
        FontScope f2(GetFonts().semibold, 12);
        dl->AddText({p.x + pad + S(10) + sw, p.y + pad + S(14)}, U32(Color::Muted), "FPS");
    }
    {
        FontScope f(GetFonts().semibold, 15);
        const std::string s = Fmt("%.0f %%", speed);
        const float sw = ImGui::CalcTextSize(s.c_str()).x;
        dl->AddText({p.x + w - pad - sw, p.y + pad + S(2)}, U32(speed_col), s.c_str());
        FontScope f2(GetFonts().regular, 11);
        const char* lbl = "velocidad";
        dl->AddText({p.x + w - pad - ImGui::CalcTextSize(lbl).x, p.y + pad + S(22)}, U32(Color::Dim), lbl);
    }

    // Gráfica del tiempo entre cuadros.
    const ImVec2 g0(p.x + pad + S(4), p.y + pad + S(44));
    const ImVec2 g1(p.x + w - pad, g0.y + graph_h);
    dl->AddRectFilled(g0, g1, U32(0x0F0D17, 0.8f), S(8));
    const float max_ms = std::max(40.0f, host_max * 1.1f);
    const float target_ms = static_cast<float>(1000.0 / std::max(30.0f, hw_.refresh));
    const float ty = g1.y - (target_ms / max_ms) * graph_h;
    dl->AddLine({g0.x, ty}, {g1.x, ty}, U32(Color::Dim, 0.6f));
    ImVec2 prev;
    for (int i = 0; i < 180; ++i) {
        const int idx = (perf_.head + i) % 180;
        float v = perf_.host_ms[idx];
        if (preview && !in_game_) v = target_ms + std::sin(i * 0.3f) * 0.6f + (i == 120 ? 18.0f : 0.0f);
        const ImVec2 pt(g0.x + (g1.x - g0.x) * i / 179.0f, g1.y - std::min(v / max_ms, 1.0f) * graph_h);
        if (i) dl->AddLine(prev, pt, U32(v > target_ms * 2.2f ? Color::Red : accent), S(1.6f));
        prev = pt;
    }

    // Filas.
    const ProcessUsage pu = proc_;
    const char* labels[rows] = {"Núcleo", "Tirones", "CPU", "RAM", "Sonido", "Motor"};
    std::string values[rows] = {
        Fmt("%.2f ms por cuadro", core_avg),
        Fmt("%d  ·  %d este minuto", perf_.stutters, perf_.stutters_minute),
        Fmt("%.0f %% del equipo", pu.cpu_percent),
        Fmt("%.0f MB", pu.ram_mb),
        audio_.IsOpen() ? Fmt("%.0f ms  ·  ×%.4f", audio_.QueuedMs(), audio_.Ratio()) : std::string("sin salida"),
        RendererName(hw_.renderer) + (settings_.vsync ? "  ·  vsync" : ""),
    };
    float y = g1.y + S(10);
    for (int i = 0; i < rows; ++i) {
        FontScope f(GetFonts().medium, 12.5f);
        dl->AddText({p.x + pad + S(4), y}, U32(Color::Muted), labels[i]);
        FontScope f2(GetFonts().mono, 12.5f);
        const unsigned col = i == 1 && perf_.stutters_minute > 0 ? Color::Orange : Color::Text;
        dl->AddText({p.x + pad + S(76), y}, U32(col), values[i].c_str());
        y += row_h;
    }
}

// ---------------------------------------------------------------- avisos, burbuja y bienvenida

void App::DrawToasts() {
    const double now = SDL_GetTicks() / 1000.0;
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [&](const ToastItem& t) { return now > t.until + 0.3; }), toasts_.end());
    if (toasts_.empty()) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float y = vp->Pos.y + (in_game_ ? S(70) : S(24));
    for (const auto& t : toasts_) {
        const float in = static_cast<float>(std::clamp((now - t.born) / 0.18, 0.0, 1.0));
        const float out = static_cast<float>(std::clamp((t.until + 0.3 - now) / 0.3, 0.0, 1.0));
        const float a = std::min(in, out);
        const GuideInfo& g = Guide(t.guide);
        FontScope f(GetFonts().medium, 14.5f);
        const ImVec2 ts = ImGui::CalcTextSize(t.text.c_str(), nullptr, false, S(520));
        const float h = std::max(S(46), ts.y + S(22));
        const float w = ts.x + S(70);
        const ImVec2 p(vp->Pos.x + (vp->Size.x - w) * 0.5f, y - (1 - a) * S(10));
        dl->AddRectFilled({p.x, p.y + S(4)}, {p.x + w, p.y + h + S(4)}, IM_COL32(0, 0, 0, static_cast<int>(60 * a)), h * 0.5f);
        dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(Color::Surface, 0.97f * a), std::min(h * 0.5f, S(22)));
        dl->AddRect(p, {p.x + w, p.y + h}, U32(g.color, 0.55f * a), std::min(h * 0.5f, S(22)));
        const Image& av = guides_[static_cast<int>(t.guide)];
        const ImVec2 ac(p.x + S(26), p.y + h * 0.5f);
        if (av && settings_.guides)
            dl->AddImageRounded(av.Ref(), {ac.x - S(16), ac.y - S(16)}, {ac.x + S(16), ac.y + S(16)}, {0, 0}, {1, 1}, IM_COL32(255, 255, 255, static_cast<int>(255 * a)), S(16));
        else
            dl->AddCircleFilled(ac, S(6), U32(g.color, a));
        dl->AddText(nullptr, 0.0f, {p.x + S(52), p.y + (h - ts.y) * 0.5f}, U32(Color::Text, a), t.text.c_str(), nullptr, S(520));
        y += h + S(10);
    }
}

void App::DrawGuideBubble() {
    if (!settings_.guides || !settings_.bubble || bubble_closed_ || tips_.empty() || show_welcome_) return;
    tip_timer_ += ImGui::GetIO().DeltaTime;
    if (tip_timer_ > 25.0) {
        tip_timer_ = 0;
        tip_index_ = (tip_index_ + 1) % static_cast<int>(tips_.size());
    }
    const Tip& tip = tips_[tip_index_];
    const GuideInfo& g = Guide(tip.guide);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float w = S(380);
    float text_h;
    {
        FontScope f(GetFonts().regular, 13.5f);
        text_h = ImGui::CalcTextSize(tip.text.c_str(), nullptr, false, w - S(36)).y;
    }
    const float h = text_h + S(74);
    const float av = S(60);
    const ImVec2 pos(vp->Pos.x + vp->Size.x - w - av - S(40), vp->Pos.y + vp->Size.y - h - S(26));
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize({w + av + S(14), h});
    ImGui::Begin("##burbuja", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 b0 = pos, b1(pos.x + w, pos.y + h);
    dl->AddRectFilled({b0.x, b0.y + S(4)}, {b1.x, b1.y + S(4)}, IM_COL32(0, 0, 0, 70), S(18));
    dl->AddRectFilled(b0, b1, U32(Color::Surface, 0.98f), S(18));
    dl->AddRect(b0, b1, U32(g.color, 0.5f), S(18));
    dl->AddTriangleFilled({b1.x - 1, b1.y - S(34)}, {b1.x + S(12), b1.y - S(24)}, {b1.x - 1, b1.y - S(16)}, U32(Color::Surface, 0.98f));
    {
        FontScope f(GetFonts().bold, 14);
        dl->AddText({b0.x + S(18), b0.y + S(14)}, U32(g.color), g.name);
    }
    {
        FontScope f(GetFonts().regular, 13.5f);
        dl->AddText(nullptr, 0.0f, {b0.x + S(18), b0.y + S(38)}, U32(Color::Text), tip.text.c_str(), nullptr, w - S(36));
    }
    const ImVec2 ac(b1.x + S(14) + av * 0.5f, b1.y - av * 0.5f);
    const Image& img = guides_[static_cast<int>(tip.guide)];
    if (img) dl->AddImageRounded(img.Ref(), {ac.x - av * 0.5f, ac.y - av * 0.5f}, {ac.x + av * 0.5f, ac.y + av * 0.5f}, {0, 0}, {1, 1}, IM_COL32_WHITE, av * 0.5f);

    ImGui::SetCursorScreenPos({b1.x - S(70), b0.y + S(8)});
    if (IconButton("##sig", Icon::ChevronRight, S(28), U32(Color::Muted), "Otro consejo")) {
        tip_index_ = (tip_index_ + 1) % static_cast<int>(tips_.size());
        tip_timer_ = 0;
    }
    ImGui::SameLine(0, S(4));
    if (IconButton("##cerrar", Icon::Close, S(28), U32(Color::Muted), "Ocultar por ahora")) bubble_closed_ = true;
    ImGui::End();
}

void App::DrawWelcome() {
    ImGui::OpenPopup("##bienvenida");
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float w = std::min(S(640), vp->Size.x - S(40));
    ImGui::SetNextWindowPos({vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(30), S(28)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(24));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, V4(Color::Bg));
    ImGui::PushStyleColor(ImGuiCol_Border, V4(Color::Accent, 0.4f));
    if (ImGui::BeginPopupModal("##bienvenida", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize)) {
        // Pasos arriba.
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            for (int i = 0; i < 3; ++i)
                ImGui::GetWindowDrawList()->AddRectFilled({p.x + i * S(34), p.y}, {p.x + i * S(34) + S(26), p.y + S(5)},
                                                          U32(i <= welcome_step_ ? Color::Accent : Color::Surface3), S(3));
            ImGui::Dummy({0, S(14)});
        }
        const float inner = w - S(60);
        if (welcome_step_ == 0) {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            if (logo_) ImGui::GetWindowDrawList()->AddImage(logo_.Ref(), {p.x + (inner - S(120)) * 0.5f, p.y}, {p.x + (inner + S(120)) * 0.5f, p.y + S(120)});
            ImGui::Dummy({inner, S(128)});
            {
                FontScope f(GetFonts().bold, 26);
                const char* t = "¡Bienvenido a Satronho Emulation!";
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (inner - ImGui::CalcTextSize(t).x) * 0.5f);
                ImGui::TextUnformatted(t);
            }
            ImGui::Dummy({0, S(6)});
            GuideSays(GuideId::MechaRidds,
                      ("¡Hola! Soy MechaRidds. Esta es la casa de los emuladores de la familia Satronho. Ya están " + SystemsSentence() +
                       ", y pronto llegan más consolas. Te presento a mi equipo en dos pasos.")
                          .c_str(),
                      inner);
            ImGui::Dummy({0, S(14)});
            if (Button("Empezar", ButtonKind::Primary, {inner, S(48)}, Icon::ChevronRight, true)) welcome_step_ = 1;
        } else if (welcome_step_ == 1) {
            const Suggestion s = Suggest(hw_, settings_);
            Heading("Tu equipo", 22);
            ImGui::Dummy({0, S(2)});
            GuideSays(GuideId::Nono, (s.summary + Fmt(" (Detecté %s, %.0f GB de RAM y %s.)", hw_.cpu.c_str(), hw_.ram_mb / 1024.0, hw_.gpu.c_str())).c_str(), inner);
            ImGui::Dummy({0, S(10)});
            for (const auto& c : s.changes) {
                const ImVec2 p = ImGui::GetCursorScreenPos();
                DrawIcon(ImGui::GetWindowDrawList(), Icon::Check, {p.x + S(9), p.y + S(11)}, S(14), U32(Color::Teal));
                ImGui::SetCursorScreenPos({p.x + S(26), p.y});
                FontScope f(GetFonts().medium, 14);
                ImGui::TextUnformatted(c.c_str());
            }
            ImGui::Dummy({0, S(14)});
            const float bw = (inner - S(12)) * 0.5f;
            if (Button("Aplícalo por mí", ButtonKind::Primary, {bw, S(48)}, Icon::Sparkle, true)) {
                ApplySuggestion();
                welcome_step_ = 2;
            }
            ImGui::SameLine(0, S(12));
            if (Button("Lo reviso después", ButtonKind::Secondary, {bw, S(48)})) welcome_step_ = 2;
        } else {
            Heading("Tus juegos", 22);
            ImGui::Dummy({0, S(2)});
            GuideSays(GuideId::Samy,
                      "Dime en qué carpeta tienes tus juegos (NES, Game Boy, Game Boy Color o Game Boy Advance) y los acomodo en la biblioteca. Mientras juegas: Esc abre el menú, "
                      "F5 guarda, F9 carga y mantener Tab acelera.",
                      inner);
            ImGui::Dummy({0, S(14)});
            const float bw = (inner - S(12)) * 0.5f;
            auto finish = [&] {
                show_welcome_ = false;
                settings_.first_run_done = true;
                SaveConfig();
                ImGui::CloseCurrentPopup();
            };
            if (Button("Agregar carpeta", ButtonKind::Primary, {bw, S(48)}, Icon::Folder, true)) {
                finish();
                AddFolderDialog();
            }
            ImGui::SameLine(0, S(12));
            if (Button("Más tarde", ButtonKind::Secondary, {bw, S(48)})) finish();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

} // namespace Satronho
