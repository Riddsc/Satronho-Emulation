// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <thread>

#include "app.h"
#include "util/i18n.h"
#include "imgui.h"
#include "miniz.h"
#include "util/shortcut.h"
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
    std::vsnprintf(buf, sizeof buf, Tr(fmt), args);
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
    text = Tr(text);
    FontScope f(GetFonts().regular, size);
    ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
    ImGui::TextWrapped("%s", Tr(text));
    ImGui::PopStyleColor();
}

void Heading(const char* text, float size = 18) {
    text = Tr(text);
    FontScope f(GetFonts().semibold, size);
    ImGui::TextUnformatted(Tr(text));
}

unsigned TierColor(const std::string& tier) {
    if (tier == "Alto") return Color::Green;
    if (tier == "Equilibrado") return Color::Teal;
    return Color::Amber;
}


std::string RendererName(const std::string& d) {
    if (d.empty() || d == "auto") return "Automático";
    if (d.rfind("OpenGL ", 0) == 0) return d;
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
        if (!pending_start_.empty()) DrawLoading();
        DrawTouchPad();
        DrawConfirm();
        return;
    }

    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, V4(Color::Bg));
    ImGui::Begin("##principal", nullptr, kFull);
    ImGui::PopStyleColor();

    const float side_w = compact_ ? S(80) : S(252);
    // Fondo con la carátula del juego que se mira (borrosa) y, encima, el logo de Satronho.
    if (page_ == Page::Library) DrawBackdrop({vp->Pos.x + side_w, vp->Pos.y}, {vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y});
    DrawWatermark({vp->Pos.x + side_w, vp->Pos.y}, {vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y});
    DrawSidebar(side_w);
    ImGui::SameLine(0, 0);

    // Al cambiar de página el contenido entra con un fundido corto y sube un poco.
    if (page_ != shown_page_) {
        static const char* kPageNames[] = {"Biblioteca", "Controles", "Configuración sugerida", "Novedades", "Ajustes", "Acerca de", "Guías", "Herramientas"};
        const int pi = static_cast<int>(page_);
        Log::Info("Pantalla: %s", pi >= 0 && pi < 8 ? kPageNames[pi] : "?");
        shown_page_ = page_;
        BeginPageTransition();
    }
    const float tr = std::clamp(static_cast<float>((ImGui::GetTime() - page_anim_t_) / 0.22), 0.0f, 1.0f);
    const float ease = 1.0f - (1.0f - tr) * (1.0f - tr);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.35f + 0.65f * ease);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, compact_ ? ImVec2(S(18), S(14) + (1.0f - ease) * S(10)) : ImVec2(S(38), S(30) + (1.0f - ease) * S(10)));
    ImGui::BeginChild("##contenido", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar(2);
    switch (page_) {
    case Page::Library: DrawLibrary(); break;
    case Page::Controls: DrawControls(); break;
    case Page::Suggested: DrawSuggested(); break;
    case Page::News: DrawNews(); break;
    case Page::Settings: DrawSettings(); break;
    case Page::About: DrawAbout(); break;
    case Page::Guides: DrawGuideChat(); break;
    case Page::Tools: DrawTools(); break;
    }
    ImGui::Dummy({0, S(24)});
    ImGui::EndChild();
    ImGui::End();

    if (page_ == Page::Library && !compact_) DrawGuideBubble();
    DrawToasts();
    if (show_welcome_) DrawWelcome();
    else if (show_whats_new_) DrawWhatsNew();
    if (!pending_start_.empty()) DrawLoading();
    DrawArchivePicker();
    DrawBrowser();
    DrawConfirm();
    DrawSplash();
}

void App::DrawLoading() {
    // «Cargando…» sobre todo lo demás, con la portada del juego que se abre.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(vp->Pos, {vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y}, IM_COL32(8, 6, 14, 215));
    const Game g = start_entry_.empty() ? Library::MakeGame(pending_start_) : Library::MakeGameEntry(pending_start_, start_entry_);
    const SystemDef* gs = SystemById(g.system);
    const float cw = S(150), chh = S(170);
    const ImVec2 c(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f);
    DrawCover(dl, g, {c.x - cw * 0.5f, c.y - chh * 0.5f - S(40)}, {c.x + cw * 0.5f, c.y + chh * 0.5f - S(40)}, S(16), false);
    {
        FontScope f(GetFonts().semibold, 18);
        const std::string t = "Cargando " + g.title + "…";
        const ImVec2 ts = ImGui::CalcTextSize(Tr(t.c_str()));
        dl->AddText({c.x - ts.x * 0.5f, c.y + chh * 0.5f - S(20)}, U32(Color::Text), Tr(t.c_str()));
    }
    if (gs) {
        FontScope f(GetFonts().medium, 14);
        const ImVec2 ts = ImGui::CalcTextSize(Tr(gs->full.c_str()));
        dl->AddText({c.x - ts.x * 0.5f, c.y + chh * 0.5f + S(10)}, U32(gs->color), Tr(gs->full.c_str()));
    }
    // Puntitos que avanzan (cada cuadro dibujado mueve uno).
    const int dot = static_cast<int>(ImGui::GetTime() * 6) % 3;
    for (int i = 0; i < 3; ++i)
        dl->AddCircleFilled({c.x - S(16) + i * S(16), c.y + chh * 0.5f + S(44)}, S(4), U32(Color::Accent, i == dot ? 1.0f : 0.3f));
}

void App::DrawSidebar(float width) {
    if (compact_) {
        DrawSidebarRail(width);
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V4(Color::Side));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(16), S(22)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {S(6), S(4)});
    // Con ventanas bajas no cabe todo (son 9 consolas): la barra se puede desplazar y muestra una
    // barrita fina; si cabe, no se ve ninguna.
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, S(5));
    ImGui::BeginChild("##lateral", {width, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Marca.
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float ls = S(48);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (logo_) dl->AddImageRounded(logo_.Ref(), {p.x + S(4), p.y}, {p.x + S(4) + ls, p.y + ls}, {0, 0}, {1, 1}, IM_COL32_WHITE, S(12));
        {
            FontScope f(GetFonts().bold, 19);
            dl->AddText({p.x + ls + S(16), p.y + S(2)}, U32(Color::Text), Tr("Satronho"));
        }
        {
            FontScope f(GetFonts().semibold, 13);
            dl->AddText({p.x + ls + S(16), p.y + S(26)}, U32(Color::Accent), Tr("Emulation"));
        }
        ImGui::Dummy({width, ls + S(22)});
    }

    char count[16];
    size_t visible = 0;
    for (const auto& g : games_) visible += !g.hidden;
    std::snprintf(count, sizeof count, "%zu", visible);
    SectionLabel("   JUGAR");
    if (NavItem("Biblioteca", Icon::Library, page_ == Page::Library && lib_system_.empty(), visible == 0 ? nullptr : count)) {
        page_ = Page::Library;
        lib_system_.clear();
    }
    ImGui::Dummy({0, S(10)});
    SectionLabel("   CONSOLAS");
    for (const auto& sys : Systems()) {
        size_t n = 0;
        for (const auto& g : games_) n += g.system == sys.id && !g.hidden;
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
    // Las que vienen, una por una y en orden de llegada: la primera es la próxima.
    ImGui::Dummy({0, S(8)});
    SectionLabel("   EN CAMINO");
    ImGui::Dummy({0, S(2)});
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float w = ImGui::GetContentRegionAvail().x, h = S(30);
        // Si la ventana es bajita y no cabe todo, la lista se resume en una sola fila.
        const bool collapsed = sidebar_full_h_ > ImGui::GetWindowHeight();
        if (collapsed) {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##en_camino", {w, h});
            const bool hov = ImGui::IsItemHovered();
            if (hov) dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(Color::Surface2, 0.6f), S(10));
            dl->AddCircleFilled({p.x + S(24), p.y + h * 0.5f}, S(4), U32(Color::Accent));
            FontScope f(GetFonts().medium, 14);
            const std::string t = Fmt("Sigue %s  ·  y %zu más", SoonSystems().front().name, SoonSystems().size() - 1);
            dl->AddText({p.x + S(44), p.y + (h - ImGui::GetFontSize()) * 0.5f}, U32(Color::Muted), Tr(t.c_str()));
            if (hov) {
                std::string names;
                for (const auto& c : SoonSystems()) names += std::string(names.empty() ? "" : ", ") + c.name + " (" + c.hint + ")";
                HelpTooltip(("Vienen en camino, en este orden: " + names + ".").c_str());
            }
        }
        int i = 0;
        for (const auto& c : SoonSystems()) {
            if (collapsed) break;
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::PushID(c.name);
            ImGui::InvisibleButton("##pronto", {w, h});
            const bool hov = ImGui::IsItemHovered();
            ImGui::PopID();
            if (hov) dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(Color::Surface2, 0.6f), S(10));
            const bool next = i == 0;
            // Puntito: lila si es la próxima, gris las demás.
            dl->AddCircleFilled({p.x + S(24), p.y + h * 0.5f}, S(4), U32(next ? Color::Accent : Color::Border));
            if (next) dl->AddCircle({p.x + S(24), p.y + h * 0.5f}, S(7), U32(Color::Accent, 0.35f), 0, S(1.5f));
            {
                FontScope f(GetFonts().medium, 14);
                const std::string name = std::string("Satronho ") + c.name;
                dl->AddText({p.x + S(44), p.y + (h - ImGui::GetFontSize()) * 0.5f}, U32(next ? Color::Muted : Color::Dim), Tr(c.name));
                const float lw = ImGui::CalcTextSize(Tr(c.name)).x;
                FontScope f2(GetFonts().regular, 12);
                dl->AddText({p.x + S(44) + lw + S(6), p.y + (h - ImGui::GetFontSize()) * 0.5f}, U32(Color::Dim), Tr(c.hint));
                if (hov) HelpTooltip((name + ": para " + c.games + ". " + (next ? "Es la que sigue." : "Viene en camino.")).c_str());
            }
            {
                FontScope f(GetFonts().semibold, 11);
                const char* badge = next ? "Próxima" : "Pronto";
                const ImVec2 bs = ImGui::CalcTextSize(Tr(badge));
                const ImVec2 b1(p.x + w - S(12), p.y + h * 0.5f + bs.y * 0.5f + S(3));
                const ImVec2 b0(b1.x - bs.x - S(14), b1.y - bs.y - S(6));
                dl->AddRectFilled(b0, b1, next ? U32(Color::Accent, 0.2f) : U32(Color::Surface2), S(20));
                dl->AddText({b0.x + S(7), b0.y + S(2.5f)}, U32(next ? Color::Accent : Color::Dim), Tr(badge));
            }
            ++i;
        }
    }
    ImGui::Dummy({0, S(10)});
    SectionLabel("   SATRONHO");
    if (NavItem("Pregúntale a los guías", Icon::Paw, page_ == Page::Guides, nullptr, false, Color::Green)) page_ = Page::Guides;
    if (NavItem("Configuración sugerida", Icon::Sparkle, page_ == Page::Suggested, nullptr, false, Color::Teal)) page_ = Page::Suggested;
    const bool update = updater_.GetState() == Updater::State::Available;
    if (NavItem("Novedades", Icon::Bell, page_ == Page::News, update ? "Nueva" : nullptr, false, Color::Pink)) page_ = Page::News;
    if (NavItem("Controles", Icon::Gamepad, page_ == Page::Controls, nullptr, false, Color::Amber)) page_ = Page::Controls;
    if (NavItem("Herramientas", Icon::Tools, page_ == Page::Tools, nullptr, false, Color::Orange)) page_ = Page::Tools;
    if (NavItem("Ajustes", Icon::Gear, page_ == Page::Settings)) page_ = Page::Settings;
    if (NavItem("Acerca de", Icon::Info, page_ == Page::About)) page_ = Page::About;

    // Pie: autor y build.
    const float foot_h = S(58);
    {
        // Alto que ocuparía todo con la lista completa de «En camino»; si no cabe, se resume.
        const bool was_collapsed = sidebar_full_h_ > ImGui::GetWindowHeight();
        const float extra = was_collapsed ? S(30) * static_cast<float>(SoonSystems().size() - 1) : 0.0f;
        sidebar_full_h_ = ImGui::GetCursorPosY() + extra + foot_h + S(16);
    }
    const float y = ImGui::GetWindowHeight() - foot_h - S(16);
    if (ImGui::GetCursorPosY() < y) ImGui::SetCursorPosY(y);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float w = ImGui::GetContentRegionAvail().x;
    dl->AddRectFilled(p, {p.x + w, p.y + foot_h}, U32(Color::Surface), S(14));
    if (guides_[0]) dl->AddImageRounded(guides_[0].Ref(), {p.x + S(10), p.y + S(10)}, {p.x + S(48), p.y + S(48)}, {0, 0}, {1, 1}, IM_COL32_WHITE, S(19));
    {
        FontScope f(GetFonts().semibold, 13.5f);
        dl->AddText({p.x + S(58), p.y + S(10)}, U32(Color::Text), Tr("por Ridds"));
    }
    {
        FontScope f(GetFonts().regular, 12);
        const std::string b = build_ > 0 ? Fmt("Build %d", build_) : std::string("Versión de desarrollo");
        dl->AddText({p.x + S(58), p.y + S(30)}, U32(Color::Dim), Tr(b.c_str()));
    }
    ImGui::Dummy({w, foot_h});
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------- biblioteca

void App::DrawCoreMissing() {
    PanelBegin("##sin_nucleo", {0, 0}, LightTheme() ? 0xFBE3E8 : 0x2A1622);
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
        if (!g.hidden && (!lib_sys || g.system == lib_sys->id)) shown.push_back(&g);

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
        const float add_w = compact_ ? S(42) : ImGui::CalcTextSize(Tr("Agregar carpeta")).x + ImGui::GetFontSize() + S(44);
        const float open_w = S(42);
        float x = top.x + avail - add_w;
        ImGui::SetCursorScreenPos({x, top.y + S(6)});
        if (compact_ ? IconButton("##agregar", Icon::Plus, S(42), U32(Color::Accent), "Agregar carpeta")
                     : Button("Agregar carpeta", ButtonKind::Primary, {add_w, S(42)}, Icon::Plus, true))
            AddFolderDialog();
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
        ImGui::InputTextWithHint("##buscar", Tr("Buscar en la biblioteca"), search_, sizeof search_);
        ImGui::PopStyleVar(2);
        DrawIcon(ImGui::GetWindowDrawList(), Icon::Search, {sp.x + S(20), sp.y + S(21)}, S(15), U32(Color::Dim));
    }
    ImGui::SetCursorScreenPos({top.x, std::max(after_title, top.y + S(56)) + S(14)});

    if (shown.empty()) {
        if (library_.Scanning()) {
            Spinner(S(12), Color::Accent);
            ImGui::SameLine();
            Muted("Buscando juegos…");
            ImGui::SameLine(0, S(12));
            if (Button("Cancelar##busqueda", ButtonKind::Ghost, {0, S(30)})) library_.CancelScan();
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
        // Vista (como el menú Vista de DuckStation): cuadrícula, carrusel o lista (cada consola
        // recuerda la suya), tamaño de las carátulas y orden.
        {
            const std::string vk = lib_system_.empty() ? "todas" : lib_system_;
            int& view = settings_.lib_view[vk];
            static const char* kViews[] = {"Cuadrícula", "Carrusel", "En lista"};
            const Icon kIcons[] = {Icon::Grid, Icon::Carousel, Icon::List};
            view = std::clamp(view, 0, 2);
            ImGui::SameLine(0, S(10));
            const ImVec2 bp = ImGui::GetCursorScreenPos();
            const std::string lbl = std::string(Tr("Vista: ")) + Tr(kViews[view]);
            FontScope f(GetFonts().medium, 13.5f);
            const float bw = ImGui::CalcTextSize(Tr(lbl.c_str())).x + S(62);
            if (ImGui::InvisibleButton("##vista", {bw, S(36)})) ImGui::OpenPopup("##vista_menu");
            const bool vh = ImGui::IsItemHovered();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(bp, {bp.x + bw, bp.y + S(36)}, U32(vh ? Color::Surface3 : Color::Surface2), S(10));
            DrawIcon(dl, kIcons[view], {bp.x + S(18), bp.y + S(18)}, S(15), U32(Color::Accent));
            dl->AddText({bp.x + S(32), bp.y + (S(36) - ImGui::GetFontSize()) * 0.5f}, U32(Color::Text), Tr(lbl.c_str()));
            DrawIcon(dl, Icon::ChevronDown, {bp.x + bw - S(16), bp.y + S(18)}, S(11), U32(Color::Muted));
            ImGui::SetNextWindowPos({bp.x, bp.y + S(42)});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(14), S(12)});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {S(10), S(8)});
            if (ImGui::BeginPopup("##vista_menu")) {
                SectionLabel("VISTA");
                for (int i = 0; i < 3; ++i)
                    if (ImGui::MenuItem(Tr(kViews[i]), nullptr, view == i)) {
                        view = i;
                        SaveConfig();
                    }
                ImGui::Separator();
                SectionLabel("TAMAÑO DE LAS CARÁTULAS");
                const char* sizes[] = {"Pequeñas", "Medianas", "Grandes"};
                for (int i = 0; i < 3; ++i)
                    if (ImGui::MenuItem(Tr(sizes[i]), nullptr, settings_.cover_size == i)) {
                        settings_.cover_size = i;
                        SaveConfig();
                    }
                ImGui::Separator();
                SectionLabel("ORDEN");
                const char* sorts[] = {"Nombre (A-Z)", "Jugados hace poco", "Más jugados"};
                for (int i = 0; i < 3; ++i)
                    if (ImGui::MenuItem(Tr(sorts[i]), nullptr, settings_.sort == i)) {
                        settings_.sort = i;
                        SaveConfig();
                    }
                ImGui::Separator();
                if (ImGui::MenuItem(Tr("Solo favoritos"), nullptr, settings_.favorites_only)) {
                    settings_.favorites_only = !settings_.favorites_only;
                    SaveConfig();
                }
                if (ImGui::MenuItem(Tr("Usar esta vista en todas las consolas"))) {
                    const int v = view;
                    settings_.lib_view.clear();
                    settings_.lib_view["todas"] = v;
                    for (const auto& sd : Systems()) settings_.lib_view[sd.id] = v;
                    SaveConfig();
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar(2);
        }
        if (library_.Scanning()) {
            ImGui::SameLine(0, S(16));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(8));
            Spinner(S(9), Color::Accent);
            ImGui::SameLine();
            Muted("Actualizando…", 13);
            ImGui::SameLine(0, S(8));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(6));
            if (Button("Cancelar##busqueda2", ButtonKind::Ghost, {0, S(28)})) library_.CancelScan();
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

    if (settings_.LibView(lib_system_) == 1) {
        DrawLibraryCarousel(list, lib_sys);
        return;
    }
    if (settings_.LibView(lib_system_) == 2) {
        const float rh = settings_.cover_size == 0 ? S(58) : settings_.cover_size == 2 ? S(96) : S(74);
        const float rw = ImGui::GetContentRegionAvail().x;
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(list.size()), rh + S(8));
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const Game& g = *list[i];
                if (DrawGameRow(g, rw, rh, g.key == selected_key_)) {
                    RequestStart(g.path);
                    return;
                }
                ImGui::Dummy({0, S(8) - ImGui::GetStyle().ItemSpacing.y});
            }
        return;
    }

    // Cuadrícula. En «todas las consolas» (orden A-Z, sin buscar) los juegos se separan por
    // consola, cada grupo con su control en miniatura y su nombre.
    const float gap = S(18);
    const float min_w = (compact_ ? S(150) : S(188)) * (settings_.cover_size == 0 ? 0.78f : settings_.cover_size == 2 ? 1.32f : 1.0f);
    const float width = ImGui::GetContentRegionAvail().x;
    const int cols = std::max(1, static_cast<int>((width + gap) / (min_w + gap)));
    const float cw = std::floor((width - gap * (cols - 1)) / cols);
    const float ch = cw * 0.95f + S(78);

    auto draw_grid = [&](const std::vector<const Game*>& items) -> bool {
        const int rows = (static_cast<int>(items.size()) + cols - 1) / cols;
        ImGuiListClipper clipper;
        clipper.Begin(rows, ch + gap);
        while (clipper.Step()) {
            for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r) {
                const ImVec2 row0 = ImGui::GetCursorScreenPos();
                for (int c = 0; c < cols; ++c) {
                    const int i = r * cols + c;
                    if (i >= static_cast<int>(items.size())) break;
                    ImGui::SetCursorScreenPos({row0.x + c * (cw + gap), row0.y});
                    const Game& g = *items[i];
                    if (DrawGameCard(g, cw, ch, g.key == selected_key_)) {
                        RequestStart(g.path);
                        return true;
                    }
                }
                ImGui::SetCursorScreenPos({row0.x, row0.y + ch + gap});
                ImGui::Dummy({0, 0});
            }
        }
        return false;
    };

    const bool grouped = !lib_sys && query.empty() && settings_.sort == 0;
    if (!grouped) {
        draw_grid(list);
        return;
    }
    for (const auto& sys : Systems()) {
        std::vector<const Game*> part;
        for (const Game* g : list)
            if (g->system == sys.id) part.push_back(g);
        if (part.empty()) continue;
        // Encabezado del grupo: control en miniatura, nombre, cantidad y «Ver solo estos».
        ImGui::Dummy({0, S(6)});
        const ImVec2 hp = ImGui::GetCursorScreenPos();
        const float hh = S(40);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawControllerMini(dl, sys.layout, {hp.x + S(22), hp.y + hh * 0.5f}, sys.handheld ? S(28) : S(40), 1.0f);
        {
            FontScope f(GetFonts().bold, 19);
            dl->AddText({hp.x + S(54), hp.y + (hh - ImGui::GetFontSize()) * 0.5f}, U32(Color::Text), Tr(sys.full.c_str()));
            const float nw = ImGui::CalcTextSize(Tr(sys.full.c_str())).x;
            FontScope f2(GetFonts().medium, 13);
            const std::string sub = Fmt("%s  ·  %zu %s", sys.hint.c_str(), part.size(), part.size() == 1 ? "juego" : "juegos");
            dl->AddText({hp.x + S(54) + nw + S(12), hp.y + (hh - ImGui::GetFontSize()) * 0.5f + S(1)}, U32(sys.color), Tr(sub.c_str()));
        }
        // Línea fina con el color de la consola.
        dl->AddRectFilled({hp.x, hp.y + hh + S(6)}, {hp.x + width, hp.y + hh + S(7.5f)}, U32(Color::Border));
        dl->AddRectFilled({hp.x, hp.y + hh + S(5.5f)}, {hp.x + S(54), hp.y + hh + S(8)}, U32(sys.color), S(2));
        {
            FontScope f(GetFonts().medium, 13.5f);
            const char* more = "Ver solo estos";
            const ImVec2 ms = ImGui::CalcTextSize(Tr(more));
            ImGui::SetCursorScreenPos({hp.x + width - ms.x - S(26), hp.y + (hh - ms.y) * 0.5f});
            ImGui::PushID(sys.id.c_str());
            if (ImGui::InvisibleButton("##ver", {ms.x + S(24), ms.y})) lib_system_ = sys.id;
            const bool mh = ImGui::IsItemHovered();
            ImGui::PopID();
            dl->AddText({hp.x + width - ms.x - S(26), hp.y + (hh - ms.y) * 0.5f}, U32(mh ? Color::Text : Color::Muted), Tr(more));
            DrawIcon(dl, Icon::ChevronRight, {hp.x + width - S(10), hp.y + hh * 0.5f}, S(12), U32(mh ? Color::Text : Color::Muted));
        }
        ImGui::SetCursorScreenPos({hp.x, hp.y + hh + S(22)});
        if (draw_grid(part)) return;
        ImGui::Dummy({0, S(8)});
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
    (void)gs;
    DrawCover(dl, g, {p.x + S(14), p.y + S(12)}, {p.x + S(144), p.y + h - S(12)}, S(14), false);

    const float tx = p.x + S(170);
    {
        FontScope f(GetFonts().semibold, 12);
        dl->AddText({tx, p.y + S(24)}, U32(Color::Gold), Tr("SEGUIR JUGANDO"));
    }
    Ellipsize(dl, GetFonts().bold, S(26), {tx, p.y + S(44)}, U32(Color::Text), g.title, width - S(420), 1);
    {
        FontScope f(GetFonts().regular, 14);
        std::string meta = FormatLastPlayed(g.last_played) + "  ·  " + FormatPlaytime(g.playtime_s) + " jugado";
        if (HasAutoState(g)) meta += "  ·  sigues donde quedaste";
        dl->AddText({tx, p.y + S(86)}, U32(Color::Muted), Tr(meta.c_str()));
    }

    // Botones.
    const float bw = S(150);
    ImGui::SetCursorScreenPos({p.x + width - bw - S(26), p.y + (h - S(48)) * 0.5f});
    {
        FontScope f(GetFonts().semibold, 15);
        // Con guardado automático, «Continuar» sigue justo donde quedaste.
        if (Button("Continuar##hero", ButtonKind::Primary, {bw, S(48)}, Icon::Play, true)) RequestStart(g.path, HasAutoState(g));
    }
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({width, h});
}

bool App::GameContextMenu(const Game& g) {
    // Clic derecho en un juego (como en Satronho NX): todo lo que se puede hacer con él.
    bool play = false;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(10), S(10)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {S(10), S(7)});
    if (ImGui::BeginPopupContextItem("##menu")) {
        ImGui::PopStyleVar(2);
        {
            FontScope f(GetFonts().bold, 14);
            ImGui::TextUnformatted(Tr(g.title.c_str()));
        }
        ImGui::Separator();
        if (ImGui::MenuItem(Tr("Favorito"), nullptr, g.favorite)) {
            library_.ToggleFavorite(g.key);
            games_dirty_ = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(Tr("Jugar"))) play = true;
        if (HasAutoState(g) && ImGui::MenuItem(Tr("Continuar donde quedé"))) RequestStart(g.path, true);
        ImGui::Separator();
        if (ImGui::MenuItem(Tr("Abrir la carpeta del juego"))) OpenFolder(g.path.parent_path());
        if (ImGui::MenuItem(Tr("Abrir la carpeta de partidas"))) OpenFolder(Paths::Saves() / g.system);
        if (ImGui::MenuItem(Tr("Abrir la carpeta de estados"))) OpenFolder(StatePathFor(g, 1).parent_path());
        ImGui::Separator();
        if (ImGui::BeginMenu(Tr("Carátula"))) {
            if (ImGui::MenuItem(Tr("Elegir una imagen…"))) PickCoverDialog(g);
            if (ImGui::MenuItem(Tr("Quitar mi carátula"), nullptr, false, covers_.HasCustom(g))) covers_.RemoveCustom(g);
            if (ImGui::MenuItem(Tr("Buscarla de nuevo en internet"))) covers_.QueueDownloads({g});
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(Tr("Tiempo de juego"))) {
            ImGui::TextDisabled("%s", g.playtime_s > 0 ? FormatPlaytime(g.playtime_s).c_str() : "Sin jugar");
            if (ImGui::MenuItem(Tr("Volver a cero"), nullptr, false, g.playtime_s > 0)) {
                library_.ResetPlaytime(g.key);
                games_dirty_ = true;
            }
            ImGui::EndMenu();
        }
        const std::string ext = Lower(Paths::ToUtf8(g.path.extension()));
        if (ext == ".cue" && ImGui::MenuItem(Tr("Convertir a .chd"))) {
            StartChdJob({g.path}, false);
            Toast("Convirtiendo «" + g.title + "» a .chd. Tu original se queda.", GuideId::Nono, 4);
        }
        if (ImGui::MenuItem(Tr("Verificar el archivo"))) VerifyGame(g);
        if (ImGui::BeginMenu(Tr("Copiar"))) {
            if (ImGui::MenuItem(Tr("El nombre"))) SDL_SetClipboardText(g.title.c_str());
            if (ImGui::MenuItem(Tr("La ruta del archivo"))) SDL_SetClipboardText(Paths::ToUtf8(g.path).c_str());
            ImGui::EndMenu();
        }
#ifdef _WIN32
        if (ImGui::BeginMenu(Tr("Crear acceso directo"))) {
            if (ImGui::MenuItem(Tr("En el escritorio"))) {
                std::string err;
                if (Shortcut::CreateOnDesktop(g.title, g.path, {}, &err)) Toast("Listo: «" + g.title + "» está en tu escritorio.", GuideId::Samy, 4);
                else Toast(err, GuideId::MechaRidds, 4);
            }
            ImGui::EndMenu();
        }
#endif
        ImGui::Separator();
        if (ImGui::MenuItem(Tr("Ocultar de la biblioteca"))) {
            library_.SetHidden(g.key, true);
            games_dirty_ = true;
            Toast("«" + g.title + "» quedó oculto. Lo vuelves a mostrar en Ajustes » Biblioteca.", GuideId::Samy, 4.5);
        }
        ImGui::EndPopup();
    } else {
        ImGui::PopStyleVar(2);
    }
    return play;
}

// Calcula el CRC32 y el tamaño del juego (como los de las listas No-Intro / Redump) para saber si
// el archivo está completo. Los muy grandes se miden en un hilo aparte.
void App::VerifyGame(const Game& g) {
    std::error_code ec;
    const uintmax_t size = fs::file_size(g.path, ec);
    const fs::path path = g.path;
    const std::string title = g.title;
    std::thread([this, path, title, size]() {
        std::ifstream f(path, std::ios::binary);
        mz_ulong crc = 0;
        if (f) {
            std::vector<unsigned char> buf(1 << 20);
            while (f) {
                f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
                const std::streamsize n = f.gcount();
                if (n <= 0) break;
                crc = mz_crc32(crc, buf.data(), static_cast<size_t>(n));
            }
        }
        const bool ok = static_cast<bool>(std::ifstream(path, std::ios::binary));
        std::lock_guard lock(pending_mutex_);
        pending_toasts_.push_back(ok ? Fmt("«%s»: %.1f MB · CRC32 %08lX. Compáralo con la lista de tu juego (No-Intro o Redump).", title.c_str(),
                                          size / 1048576.0, static_cast<unsigned long>(crc))
                                    : "No pude leer «" + title + "».");
    }).detach();
}

// Fila de la vista en lista: carátula chica, nombre, consola, tiempo jugado y botón de jugar.
bool App::DrawGameRow(const Game& g, float w, float h, bool selected) {
    bool play = false;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(g.key.c_str());
    ImGui::SetNextItemAllowOverlap();
    const bool clicked = ImGui::InvisibleButton("##fila", {w, h});
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) hover_key_next_ = g.key;
    if (clicked) selected_key_ = g.key;
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) play = true;
    if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown))) play = true;
    if (GameContextMenu(g)) play = true;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const SystemDef* sys = SystemById(g.system);
    const unsigned col = sys ? sys->color : Color::Accent;
    dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(selected ? Color::Surface3 : hovered ? Color::Surface2 : Color::Surface, selected || hovered ? 1.0f : 0.7f), S(12));
    if (selected) dl->AddRectFilled({p.x, p.y + S(10)}, {p.x + S(4), p.y + h - S(10)}, U32(col), S(2));
    // Carátula.
    const float ch = h - S(14), cw = ch * 0.78f;
    DrawCover(dl, g, {p.x + S(12), p.y + S(7)}, {p.x + S(12) + cw, p.y + S(7) + ch}, S(6), hovered);
    float x = p.x + S(12) + cw + S(16);
    // Nombre y datos.
    Ellipsize(dl, GetFonts().semibold, S(15.5f), {x, p.y + h * 0.5f - S(19)}, U32(Color::Text), g.title, w * 0.42f, 1);
    {
        std::string meta = (sys ? sys->full : g.system);
        if (!g.tags.empty()) meta += "  ·  " + g.tags.front();
        Ellipsize(dl, GetFonts().medium, S(12.5f), {x, p.y + h * 0.5f + S(3)}, U32(col), meta, w * 0.42f, 1);
    }
    // Columnas a la derecha: jugado, favorito y jugar.
    const float right = p.x + w - S(16);
    {
        FontScope f(GetFonts().regular, 13);
        const std::string t = g.playtime_s > 0 ? FormatPlaytime(g.playtime_s) : std::string("Sin jugar");
        const float tw = ImGui::CalcTextSize(Tr(t.c_str())).x;
        dl->AddText({right - S(150) - tw, p.y + (h - ImGui::GetFontSize()) * 0.5f}, U32(Color::Muted), Tr(t.c_str()));
    }
    if (g.favorite) DrawIcon(dl, Icon::StarFill, {right - S(120), p.y + h * 0.5f}, S(16), U32(Color::Gold));
    {
        const ImVec2 b0(right - S(96), p.y + (h - S(34)) * 0.5f), b1(right, p.y + (h + S(34)) * 0.5f);
        ImGui::SetCursorScreenPos(b0);
        const bool bh = ImGui::InvisibleButton("##jugar", {b1.x - b0.x, b1.y - b0.y});
        const bool bhov = ImGui::IsItemHovered();
        if (bh) play = true;
        dl->AddRectFilled(b0, b1, U32(Color::Accent, bhov ? 1.0f : hovered ? 0.85f : 0.25f), S(17));
        DrawIcon(dl, Icon::Play, {b0.x + S(22), (b0.y + b1.y) * 0.5f}, S(12), U32(bhov || hovered ? Color::Bg : Color::Accent));
        FontScope f(GetFonts().semibold, 13);
        dl->AddText({b0.x + S(36), (b0.y + b1.y - ImGui::GetFontSize()) * 0.5f}, U32(bhov || hovered ? Color::Bg : Color::Accent), Tr("Jugar"));
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos({p.x, p.y + h});
    return play;
}

bool App::DrawGameCard(const Game& g, float w, float h, bool selected) {
    bool play = false;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(g.key.c_str());
    ImGui::SetNextItemAllowOverlap();
    const bool clicked = ImGui::InvisibleButton("##card", {w, h});
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) hover_key_next_ = g.key;
    const bool focused = ImGui::IsItemFocused();
    if (clicked) selected_key_ = g.key;
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) play = true;
    if (focused && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown))) play = true;
    if (GameContextMenu(g)) play = true;

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
    DrawCover(dl, g, a, {b.x, a.y + cover_h}, rr, hv > 0.5f);
    if (selected || hv > 0) dl->AddRect(a, b, U32(selected ? Color::Accent : Color::Surface3, selected ? 1.0f : hv), rr, S(selected ? 2.0f : 1.5f));
    // Distintivo de la consola arriba a la izquierda: su control en miniatura y su nombre.
    if (gs) {
        FontScope f(GetFonts().semibold, 11.5f);
        const ImVec2 ts = ImGui::CalcTextSize(Tr(gs->name.c_str()));
        const float ph = S(26), icon_w = gs->handheld ? S(16) : S(24);
        const ImVec2 b0(a.x + S(10), a.y + S(10)), b1(b0.x + S(10) + icon_w + S(6) + ts.x + S(10), b0.y + ph);
        dl->AddRectFilled(b0, b1, IM_COL32(15, 13, 23, 190), ph * 0.5f);
        dl->AddRect(b0, b1, U32(gs->color, 0.45f), ph * 0.5f, 1.0f);
        DrawControllerMini(dl, gs->layout, {b0.x + S(10) + icon_w * 0.5f, b0.y + ph * 0.5f}, icon_w, 1.0f);
        dl->AddText({b0.x + S(10) + icon_w + S(6), b0.y + (ph - ts.y) * 0.5f}, U32(gs->color), Tr(gs->name.c_str()));
    }

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
        if (!g.tags.empty()) {
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
        ImGui::SetCursorPosX(x0 + (w - ImGui::CalcTextSize(Tr(t)).x) * 0.5f);
        ImGui::TextUnformatted(Tr(t));
    }
    ImGui::Dummy({0, S(6)});
    ImGui::SetCursorPosX(x0);
    GuideSays(GuideId::Samy,
              "Agrega la carpeta donde guardas tus juegos (de cualquiera de las nueve consolas) y yo busco también en las subcarpetas; "
              "cada juego queda en su consola. "
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
        ImGui::SetCursorPosX(x0 + (w - ImGui::CalcTextSize(Tr(t)).x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Dim));
        ImGui::TextUnformatted(Tr(t));
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
            ImGui::TextUnformatted(Tr(label));
            ImGui::PopStyleColor();
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(10));
        {
            FontScope f(GetFonts().semibold, 14.5f);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + col_w - S(110));
            ImGui::TextWrapped("%s", Tr(value.c_str()));
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
        ImGui::TextUnformatted(Tr(c.c_str()));
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

    // Cómo le va a cada consola en este equipo.
    ImGui::Dummy({0, S(18)});
    PanelBegin("##por_consola", {0, 0});
    Heading("Cómo le va a cada consola");
    Muted("Según los hilos de tu procesador. Es una guía: un procesador viejo puede ir más lento aunque tenga muchos hilos.", 13);
    ImGui::Dummy({0, S(6)});
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const int per_row = avail_w > S(900) ? 3 : avail_w > S(560) ? 2 : 1;
    const float cell_w = (avail_w - S(12) * (per_row - 1)) / per_row;
    int col = 0;
    for (const auto& v : s.systems) {
        const SystemDef* sd = SystemById(v.system);
        if (!sd) continue;
        if (col) ImGui::SameLine(0, S(12));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float h = S(62);
        static const unsigned level_col[] = {Color::Green, Color::Teal, Color::Amber, Color::Red};
        const unsigned lc = level_col[std::clamp(v.level, 0, 3)];
        dl->AddRectFilled(p, {p.x + cell_w, p.y + h}, U32(Color::Surface2), S(12));
        dl->AddRectFilled(p, {p.x + S(5), p.y + h}, U32(lc), S(12), ImDrawFlags_RoundLeft);
        DrawControllerMini(dl, sd->layout, {p.x + S(32), p.y + h * 0.5f}, sd->handheld ? S(22) : S(30), 1.0f);
        {
            FontScope f(GetFonts().semibold, 14.5f);
            dl->AddText({p.x + S(58), p.y + S(10)}, U32(Color::Text), Tr(sd->full.c_str()));
        }
        Ellipsize(dl, GetFonts().regular, S(12.5f), {p.x + S(58), p.y + S(33)}, U32(lc), v.text, cell_w - S(66), 1);
        ImGui::Dummy({cell_w, h});
        if (ImGui::IsItemHovered()) HelpTooltip(v.text.c_str());
        col = (col + 1) % per_row;
        if (!col) ImGui::Dummy({0, S(2)});
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
            ImGui::TextUnformatted(Tr((b["version"].String() + "  ·  " + b["fecha"].String()).c_str()));
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
            ImGui::TextWrapped("%s", Tr(c["texto"].String().c_str()));
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
    PanelBegin("##actualizar", {0, 0}, st == Updater::State::Available ? (LightTheme() ? 0xFCE7F1 : 0x2A1B2C) : Color::Surface);
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
        ImGui::TextUnformatted(Tr("Buscar al abrir"));
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
        {SettingsTab::Video, "Gráficos", Icon::Monitor},       {SettingsTab::Audio, "Sonido", Icon::Volume},
        {SettingsTab::Controls, "Controles", Icon::Gamepad}, {SettingsTab::Performance, "Rendimiento", Icon::Forward},
        {SettingsTab::Meter, "Medidor", Icon::Meter},        {SettingsTab::Core, "Núcleo", Icon::Chip},
        {SettingsTab::Library, "Biblioteca", Icon::Folder},  {SettingsTab::Guides, "Guías", Icon::Paw},
        {SettingsTab::Appearance, "Apariencia", Icon::Palette},
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
    case SettingsTab::Appearance: DrawSettingsAppearance(); break;
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

    RowBegin("Efecto de pantalla", "«Auto» usa televisor CRT en las consolas de casa en 2D, pantalla LCD en las portátiles y la imagen limpia en las 3D (CD, 64, Anillo y Dual).");
    changed |= Segmented("##efecto", &settings_.effect, {"Ninguno", "Líneas", "CRT", "LCD", "Auto"}, S(340));
    RowEnd();

    if (settings_.effect != 0) {
        RowBegin("Intensidad del efecto", nullptr);
        changed |= SliderPercent("##intensidad", &settings_.effect_strength, S(320));
        RowEnd();
    }

    {
        int own = 0;
        for (const auto& [id, v] : settings_.video_for) own += v.on;
        if (own > 0) {
            RowBegin(Fmt("%d %s su propia imagen", own, own == 1 ? "consola tiene" : "consolas tienen").c_str(),
                     "Se elige desde el menú del juego » Ajustes rápidos » «Imagen propia». Ahí manda sobre lo de esta página.");
            if (Button("Usar la general en todas", ButtonKind::Ghost, {0, 0}, Icon::Refresh, true)) {
                for (auto& [id, v] : settings_.video_for) v.on = false;
                changed = true;
            }
            RowEnd();
        } else {
            Muted("Cada consola puede tener su propia imagen: menú del juego » Ajustes rápidos » «Imagen propia».", 13);
            ImGui::Dummy({0, S(6)});
        }
    }

    RowBegin("Fondo ambiente", "Llena los bordes con los colores del juego, suaves y desenfocados.");
    changed |= Toggle("##amb", &settings_.ambient);
    RowEnd();

    RowBegin("Sincronización vertical", "Evita que la imagen se corte. Déjala activada salvo que sepas lo que haces.");
    if (Toggle("##vsync", &settings_.vsync)) {
        ApplyVsync();
        changed = true;
    }
    RowEnd();

    DrawGpuSettings(changed);

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
              "Por ahora funciona en Satronho Octa (NES); en las demás consolas se queda apagado solo, porque su sonido "
              "no vive dentro del estado y sonaría raro.");
    ImGui::Dummy({0, S(16)});
    bool changed = false;
    RowBegin("Adelanto de cuadros", "Reduce el retraso entre presionar un botón y verlo en pantalla. Solo en Satronho Octa.");
    changed |= Segmented("##ra", &settings_.runahead, {"Apagado", "1 cuadro", "2 cuadros"}, S(320));
    RowEnd();

    Heading("Rebobinar", 16);
    ImGui::Dummy({0, S(4)});
    RowBegin("Rebobinar", "Mantén Retroceso (o el gatillo izquierdo) para volver atrás en el tiempo.");
    changed |= Toggle("##rw", &settings_.rewind);
    RowEnd();
    if (settings_.rewind) {
        RowBegin("También en las consolas pesadas",
                 "PS1, DS, Saturn y 64 guardan varios MB por paso y en equipos modestos pueden dar tirones. Apagado de fábrica.");
        changed |= Toggle("##rwh", &settings_.rewind_heavy);
        RowEnd();
        RowBegin("Cuánto se puede rebobinar", "Segundos hacia atrás como máximo.");
        changed |= SliderIntStyled("##rws", &settings_.rewind_seconds, 5, 60, "%d s", S(320));
        RowEnd();
        RowBegin("Memoria para el historial",
                 "Se guarda solo lo que cambia entre un momento y otro, comprimido. Si se llena, se borra lo más viejo primero.");
        changed |= SliderIntStyled("##rwm", &settings_.rewind_mb, 64, 1024, "%d MB", S(320));
        RowEnd();
        if (in_game_ && RewindActive()) {
            const double secs = static_cast<double>(rewind_.Steps()) * 2.0 / std::max(1.0, core_.Fps());
            Muted(Fmt("Ahora mismo: %.0f s guardados en %.0f MB.", secs, static_cast<double>(rewind_.Bytes()) / (1024.0 * 1024.0)).c_str(), 13);
        }
    }

    ImGui::Dummy({0, S(8)});
    Heading("Avance rápido", 16);
    ImGui::Dummy({0, S(4)});
    RowBegin("Avance rápido", "Apagado de fábrica: el gatillo choca con botones de varias consolas. Al activarlo, se usa manteniendo Tab "
                              "(y el botón del mando que le asignes en Controles » Atajos).");
    changed |= Toggle("##ffon", &settings_.fast_forward_enabled);
    RowEnd();
    if (settings_.fast_forward_enabled) {
        static const int speeds[] = {2, 3, 4, 6};
        int fi = 1;
        for (int i = 0; i < 4; ++i)
            if (settings_.fast_forward == speeds[i]) fi = i;
        RowBegin("Velocidad del avance rápido", nullptr);
        if (Segmented("##ff", &fi, {"2×", "3×", "4×", "6×"}, S(320))) {
            settings_.fast_forward = speeds[fi];
            changed = true;
        }
        RowEnd();
    }

    ImGui::Dummy({0, S(8)});
    Heading("Al jugar", 16);
    ImGui::Dummy({0, S(4)});
    RowBegin("Guardar dónde quedé al salir", "Al volver a la biblioteca se guarda una ranura automática; «Continuar» sigue justo ahí.");
    changed |= Toggle("##autosave", &settings_.autosave_exit);
    RowEnd();
    RowBegin("Pausar al cambiar de ventana", "Si haces clic en otro programa, el juego se detiene hasta que vuelvas.");
    changed |= Toggle("##foco", &settings_.pause_on_focus_loss);
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
    if (Segmented("##nucleo_sis", &cur, names, std::min(ImGui::GetContentRegionAvail().x, S(110) * names.size()))) core_tab_system_ = Systems()[cur].id;
    ImGui::EndDisabled();
    ImGui::Dummy({0, S(10)});
    const SystemDef& tsys = Systems()[cur];
    // Opciones «en vivo» si el núcleo cargado es el de esta consola; si no, se muestran desde la
    // copia guardada (sin cargar y descargar el .dll solo por mirar) y se guardan en satronho.ini.
    bool live = core_.CoreLoaded() && core_file_ == CoreFile(tsys) && options_sys_ == tsys.id;
    auto cached = core_cache_.find(CoreFile(tsys));
    if (!live && !in_game_ && cached == core_cache_.end()) {
        std::string err;
        if (!EnsureCore(tsys, &err)) {
            IconText(Icon::Info, ("No se pudo cargar el núcleo de " + tsys.full + ".").c_str(), U32(Color::Red));
            return;
        }
        live = true;
        cached = core_cache_.find(CoreFile(tsys));
    }
    if (!live) {
        if (cached == core_cache_.end()) return;
        if (tab_options_sys_ != tsys.id) {
            tab_options_ = OptionsFor(tsys, cached->second.options);
            tab_options_sys_ = tsys.id;
        }
    }
    const std::string core_label = live ? std::string(core_.SystemInfo().library_name ? core_.SystemInfo().library_name : "")
                                        : cached->second.name;
    const std::string core_version = live ? std::string(core_.SystemInfo().library_version ? core_.SystemInfo().library_version : "")
                                          : cached->second.version;
    GuideSays(GuideId::Samy,
              Fmt("El corazón de %s es %s %s. Estas opciones son suyas; la mayoría se nota al momento y otras al reiniciar el juego.%s",
                  tsys.full.c_str(), core_label.c_str(), core_version.c_str(),
                  tsys.core == "mgba" ? " Bolsillo y Bolsillo Plus comparten el núcleo, pero cada una guarda sus propias opciones." : "").c_str());
    ImGui::Dummy({0, S(16)});
    if (!tsys.bios.empty()) {
        // BIOS de la consola: la pone cada quien (no se incluye). Se muestra cuál hay y cuál falta.
        const std::vector<std::string> have = BiosFound(tsys);
        std::string status;
        if (have.empty()) {
            status = tsys.bios_required ? "Falta. " : "No hay (es opcional). ";
        } else {
            status = "Tienes: ";
            for (size_t i = 0; i < have.size(); ++i) status += (i ? ", " : "") + have[i];
            status += ". ";
        }
        std::string names;
        for (size_t i = 0; i < tsys.bios.size(); ++i) names += std::string(i ? ", " : "") + tsys.bios[i].name;
        RowBegin(("BIOS de " + tsys.full).c_str(), (status + tsys.bios_note + " Nombres que reconoce: " + names + ".").c_str());
        if (Button(have.empty() ? "Agregar BIOS…" : "Agregar otra…", tsys.bios_required && have.empty() ? ButtonKind::Primary : ButtonKind::Secondary,
                   {0, 0}, Icon::Chip, true))
            PickBios(tsys);
        ImGui::SameLine(0, S(8));
        if (Button("Abrir carpeta", ButtonKind::Ghost, {0, 0}, Icon::Folder, true)) OpenFolder(Paths::System());
        RowEnd();
        ImGui::Dummy({0, S(6)});
    }
    bool changed = false;
    std::vector<CoreOption>& opts = live ? core_.Options() : tab_options_;
    const std::vector<CoreCategory>& categories = live ? core_.Categories() : cached->second.categories;
    auto set_option = [&](CoreOption& o, const std::string& v) {
        if (live) core_.SetOption(o.key, v);
        else o.current = v;
    };
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
            set_option(o, o.values[cur].value);
            changed = true;
        }
        RowEnd();
        ImGui::PopID();
    };
    if (categories.empty()) {
        for (auto& o : opts) draw_option(o);
    } else {
        for (const auto& cat : categories) {
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
        for (auto& o : opts) set_option(o, o.def);
        changed = true;
    }
    if (changed) {
        if (!live) SaveCoreOptions(tsys.id, tab_options_);
        SaveConfig();
    }
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
            Ellipsize(dl, GetFonts().medium, S(14), {p.x + S(46), p.y + S(13)}, U32(Color::Text),
                      Paths::ToUtf8(Paths::ResolveFolder(settings_.folders[i])), w - S(110), 1);
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
    if (library_.Scanning()) {
        ImGui::SameLine();
        if (Button("Cancelar búsqueda", ButtonKind::Ghost, {0, 0})) library_.CancelScan();
    }
    ImGui::Dummy({0, S(14)});
    RowBegin("Carátulas reales", "Se descargan solas de libretro-thumbnails (las de RetroArch). Clic derecho en un juego para poner la tuya.");
    if (Toggle("##caratulas", &settings_.covers)) {
        SaveConfig();
        if (settings_.covers) covers_.QueueDownloads(library_.Snapshot());
        else covers_.CancelDownloads();
    }
    RowEnd();
    if (settings_.covers) {
        if (covers_.Downloading()) {
            Spinner(S(9), Color::Accent);
            ImGui::SameLine();
            Muted(Fmt("Buscando carátulas… faltan %d", covers_.Remaining()).c_str(), 13);
        } else if (Button("Buscar carátulas ahora", ButtonKind::Secondary, {0, 0}, Icon::Refresh, true)) {
            covers_.QueueDownloads(library_.Snapshot());
        }
    }
    ImGui::Dummy({0, S(14)});
    DrawChdPanel();
    RowBegin("Juegos comprimidos (.zip y .7z)", "Se juegan sin descomprimir. Los de disco se sacan una vez a una caché de hasta 4 GB que se limpia sola.");
    if (Button("Vaciar la caché", ButtonKind::Secondary, {0, 0}, Icon::Close, true)) {
        std::error_code ec;
        fs::remove_all(Paths::User() / "cache" / "descomprimidos", ec);
        Toast("Caché de juegos descomprimidos vacía.", GuideId::Samy, 3);
    }
    RowEnd();
    ImGui::Dummy({0, S(14)});
    DrawHiddenGames();
    ImGui::Dummy({0, S(14)});

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

void App::DrawHiddenGames() {
    if (games_dirty_) {
        games_ = library_.Snapshot();
        games_dirty_ = false;
    }
    std::vector<const Game*> hidden;
    for (const auto& g : games_)
        if (g.hidden) hidden.push_back(&g);
    Heading(Fmt("Juegos ocultos (%zu)", hidden.size()).c_str(), 16);
    if (hidden.empty()) {
        Muted("Ninguno. Para ocultar un juego, clic derecho sobre su portada » «Ocultar de la biblioteca».", 13);
        return;
    }
    for (const Game* g : hidden) {
        ImGui::PushID(g->key.c_str());
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, {p.x + w, p.y + S(42)}, U32(Color::Surface2), S(12));
        const SystemDef* gs = SystemById(g->system);
        if (gs) DrawControllerMini(dl, gs->layout, {p.x + S(24), p.y + S(21)}, gs->handheld ? S(18) : S(26), 1.0f);
        Ellipsize(dl, GetFonts().medium, S(14), {p.x + S(48), p.y + S(12)}, U32(Color::Text),
                  g->title + (gs ? "  ·  " + gs->name : std::string()), w - S(170), 1);
        ImGui::SetCursorScreenPos({p.x + w - S(110), p.y + S(5)});
        if (Button("Mostrar", ButtonKind::Secondary, {S(100), S(32)})) {
            library_.SetHidden(g->key, false);
            games_dirty_ = true;
        }
        ImGui::SetCursorScreenPos({p.x, p.y + S(48)});
        ImGui::Dummy({0, 0});
        ImGui::PopID();
    }
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
    const float h = S(27);
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
    DrawIcon(dl, keyboard ? Icon::Keyboard : Icon::Gamepad, {p.x + S(14), p.y + h * 0.5f}, S(13), U32(capturing ? Color::Accent : Color::Dim));
    // Los nombres de teclas y botones («Cruceta arriba», «Flecha izquierda»…) también se traducen.
    const std::string t = capturing ? std::string(Tr("Presiona…")) : Tr(text);
    Ellipsize(dl, GetFonts().medium, S(13), {p.x + S(28), p.y + (h - S(13)) * 0.5f - S(1)},
              U32(capturing ? Color::Accent : (t == "—" ? Color::Dim : Color::Text)), t, width - S(34), 1);
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
        ImGui::TextUnformatted(Tr("Teclado"));
        ImGui::SameLine(0, S(18));
        ImGui::SetCursorPosY(y0);
        std::vector<std::string> pad_names{"Sin mando"};
        for (int i = 0; i < 4; ++i) {
            if (i < static_cast<int>(pads.size())) pad_names.push_back(Fmt("Mando %d · %s", i + 1, pads[i].name.c_str()));
            else pad_names.push_back(Fmt("Mando %d (no conectado)", i + 1));
        }
        // Su mando está desconectado: se recuerda y vuelve solo al conectarlo.
        const bool disconnected = pc.Disconnected();
        if (disconnected) pad_names.push_back("Su mando (desconectado)");
        int pad_sel = disconnected ? 5 : pc.pad >= 0 ? pc.pad + 1 : std::clamp(pc.want_pos, -1, 3) + 1;
        if (ComboStyled("##mando", &pad_sel, pad_names, compact ? S(250) : S(300)) && pad_sel <= 4) {
            input_.SetPlayerPad(player, pad_sel - 1);
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
    // Asignación automática, uno por uno, restablecer y borrar (arriba en la vista compacta, abajo en la de NX).
    const bool nx = !compact && ImGui::GetContentRegionAvail().x >= S(900);
    auto toolbar = [&]() {
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
    };
    if (!nx) {
        ImGui::Dummy({0, S(4)});
        toolbar();
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
            ImGui::TextUnformatted(Tr(Fmt("Presiona la tecla o el botón del mando para «%s»   ·   Esc cancela", label_of(input_.capture.button)).c_str()));
            ImGui::PopStyleColor();
        } else {
            const double age = SDL_GetTicks() / 1000.0 - input_.last_input_time;
            ImGui::PushStyleColor(ImGuiCol_Text, V4(age < 1.5 ? Color::Gold : Color::Dim));
            ImGui::TextUnformatted(Tr(input_.last_input.empty() ? "Presiona cualquier botón para verlo en vivo." : ("Último botón: " + input_.last_input).c_str()));
            ImGui::PopStyleColor();
        }
    }
    ImGui::Dummy({0, S(6)});

    Mapping& m = input_.Edit(player, sys);
    uint16_t live = input_.LiveButtons(player, sys);
    if (qa_.count("presionar")) live |= static_cast<uint16_t>(std::atoi(qa_["presionar"].c_str()));
    {
        float sticks[4];
        input_.LiveSticks(player, sticks);
        SetLiveSticks(sticks);
    }

    // Casilla de un botón: nombre, su tecla y su botón de mando.
    const float gap = S(10);
    const float W = ImGui::GetContentRegionAvail().x;
    float cell_w = S(150);
    const float cell_h = S(88);
    auto cell = [&](int id, ImVec2 pos) {
        ImGui::PushID(id);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const bool down = (live >> id) & 1;
        const bool cap = cap_here && input_.capture.button == id;
        dl->AddRectFilled(pos, {pos.x + cell_w, pos.y + cell_h}, U32(Color::Surface, 0.9f), S(12));
        if (down) dl->AddRectFilled(pos, {pos.x + cell_w, pos.y + cell_h}, U32(Color::Gold, 0.14f), S(12));
        dl->AddRect(pos, {pos.x + cell_w, pos.y + cell_h}, U32(down ? Color::Gold : cap ? Color::Accent : Color::Border), S(12), down || cap ? S(2) : 1.0f);
        {
            FontScope f(GetFonts().semibold, 13.5f);
            dl->AddText({pos.x + S(10), pos.y + S(6)}, U32(down ? Color::Gold : Color::Text), Tr(label_of(id)));
        }
        const float cw = cell_w - S(16);
        ImGui::SetCursorScreenPos({pos.x + S(8), pos.y + S(27)});
        const int rk = BindingChip("##k", true, Input::KeyLabel(m.keys[id]), cap, down, cw);
        ImGui::SetCursorScreenPos({pos.x + S(8), pos.y + S(57)});
        const int rp = BindingChip("##p", false, Input::PadCodeLabel(m.pad[id], family), cap, down, cw);
        if (rk == 1 || rp == 1) input_.StartCapture(player, sys, id, rp == 1, true);
        if (rk == 2) { m.keys[id] = SDL_SCANCODE_UNKNOWN; changed = true; }
        if (rp == 2) { m.pad[id] = kPadNone; changed = true; }
        ImGui::PopID();
    };
    auto group_title = [&](const char* t, ImVec2 pos) {
        FontScope f(GetFonts().semibold, 13);
        ImGui::GetWindowDrawList()->AddText(pos, U32(Color::Dim), Tr(t));
    };
    const float title_h = S(22);
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
        for (size_t i = 0; i < sys.center.size(); ++i) cell(sys.center[i], {p.x + i * (cell_w + gap), p.y + title_h});
        float h = title_h + cell_h;
        if (shoulders) {
            const float y = p.y + h + gap * 1.5f;
            group_title(sys.shoulders.size() > 4 ? "GATILLOS Y STICKS" : "GATILLOS", {p.x, y});
            // De a dos por fila: L1 R1, L2 R2, L3 R3.
            float ry = y + title_h;
            for (size_t i = 0; i < sys.shoulders.size(); i += 2) {
                cell(sys.shoulders[i], {p.x, ry});
                if (i + 1 < sys.shoulders.size()) cell(sys.shoulders[i + 1], {p.x + cell_w + gap, ry});
                ry += cell_h + gap;
            }
            h = ry - gap - p.y;
        }
        return h;
    };

    const ImVec2 o = ImGui::GetCursorScreenPos();
    float total_h;
    const float col_gap = S(24);
    const float wide_cell = S(150);
    const float wide_side = wide_cell * 2 + gap;
    const float right_side = wide_cell * face_cols + gap * (face_cols - 1);
    if (nx) {
        // Como en Satronho NX: el mando grande al centro y sus botones alrededor, cada grupo en su
        // recuadro. Se asigna un dispositivo a la vez (mando o teclado), con casillas pequeñas.
        const bool dev_key = ctrl_device_ == 1;
        const float colw = std::clamp(W * 0.27f, S(260), S(340));
        const float pad = S(12), title_box = S(30), cgap = S(8);
        const float item_h = S(50);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        auto box = [&](const char* title, ImVec2 p, float w, float h) {
            dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(Color::Surface, 0.75f), S(12));
            dl->AddRect(p, {p.x + w, p.y + h}, U32(Color::Border), S(12));
            FontScope f(GetFonts().semibold, 12.5f);
            const float tw = ImGui::CalcTextSize(Tr(title)).x;
            dl->AddText({p.x + (w - tw) * 0.5f, p.y + S(8)}, U32(Color::Muted), Tr(title));
        };
        auto nx_cell = [&](int id, ImVec2 pos, float w) {
            ImGui::PushID(id);
            const bool down = (live >> id) & 1;
            const bool cap = cap_here && input_.capture.button == id;
            {
                FontScope f(GetFonts().medium, 12.5f);
                const char* l = label_of(id);
                const float tw = ImGui::CalcTextSize(Tr(l)).x;
                dl->AddText({pos.x + (w - tw) * 0.5f, pos.y}, U32(down ? Color::Gold : Color::Text), Tr(l));
            }
            ImGui::SetCursorScreenPos({pos.x, pos.y + S(18)});
            const int r = BindingChip("##b", dev_key, dev_key ? Input::KeyLabel(m.keys[id]) : Input::PadCodeLabel(m.pad[id], family), cap, down, w);
            if (r == 1) input_.StartCapture(player, sys, id, !dev_key, false);
            if (r == 2) {
                if (dev_key) m.keys[id] = SDL_SCANCODE_UNKNOWN;
                else m.pad[id] = kPadNone;
                changed = true;
            }
            ImGui::PopID();
        };
        // Recuadro con filas de casillas (cada fila centrada, como en el mando); devuelve su alto.
        auto rows_box = [&](const char* title, const std::vector<std::vector<int>>& rows, ImVec2 p, float w) {
            int cols = 1;
            for (const auto& r : rows) cols = std::max(cols, static_cast<int>(r.size()));
            const int nr = static_cast<int>(rows.size());
            const float h = title_box + nr * item_h + (nr - 1) * cgap + pad;
            box(title, p, w, h);
            const float cw = (w - pad * 2 - cgap * (cols - 1)) / cols;
            for (int r = 0; r < nr; ++r) {
                const int in_row = static_cast<int>(rows[r].size());
                const float row_w = in_row * cw + (in_row - 1) * cgap;
                float x = p.x + (w - row_w) * 0.5f;
                for (int id : rows[r]) {
                    nx_cell(id, {x, p.y + title_box + r * (item_h + cgap)}, cw);
                    x += cw + cgap;
                }
            }
            return h;
        };
        auto grid_box = [&](const char* title, const std::vector<int>& ids, ImVec2 p, float w, int per_row) {
            std::vector<std::vector<int>> rows;
            for (size_t i = 0; i < ids.size(); i += per_row)
                rows.emplace_back(ids.begin() + i, ids.begin() + std::min(ids.size(), i + per_row));
            return rows_box(title, rows, p, w);
        };
        // Palanca en vivo (círculo con su punto) y, si hay, el clic de la palanca.
        float sticks[4];
        input_.LiveSticks(player, sticks);
        auto stick_box = [&](const char* title, int side, int click_id, ImVec2 p, float w, const char* note) {
            const float r = S(36);
            const float h = title_box + r * 2 + S(14) + (click_id >= 0 ? item_h + cgap : 0.0f) + (note ? S(20) : 0.0f) + pad;
            box(title, p, w, h);
            const ImVec2 c(p.x + w * 0.5f, p.y + title_box + r + S(4));
            dl->AddCircleFilled(c, r, U32(Color::Bg));
            dl->AddCircle(c, r, U32(Color::Border), 0, S(1.5f));
            dl->AddCircle(c, r * input_.deadzone, U32(Color::Accent, 0.35f), 0, S(1));
            const float x = sticks[side * 2], y = sticks[side * 2 + 1];
            const bool moved = x * x + y * y > 0.04f;
            dl->AddCircleFilled({c.x + x * r, c.y + y * r}, S(6), U32(moved ? Color::Gold : Color::Accent));
            float yy = p.y + title_box + r * 2 + S(14);
            if (note) {
                FontScope f(GetFonts().regular, 12);
                const float tw = ImGui::CalcTextSize(Tr(note)).x;
                dl->AddText({p.x + (w - tw) * 0.5f, yy}, U32(Color::Dim), Tr(note));
                yy += S(20);
            }
            if (click_id >= 0) nx_cell(click_id, {p.x + pad + (w - pad * 2) * 0.2f, yy}, (w - pad * 2) * 0.6f);
            return h;
        };
        auto has_btn = [&](int id) {
            return std::any_of(sys.buttons.begin(), sys.buttons.end(), [&](const auto& b) { return b.first == id; });
        };
        // Gatillos de cada lado (L, Z… a la izquierda; R a la derecha). L3/R3 van con su palanca.
        std::vector<int> left_sh, right_sh;
        for (int id : sys.shoulders) {
            if (sys.analog && (id == RETRO_DEVICE_ID_JOYPAD_L3 || id == RETRO_DEVICE_ID_JOYPAD_R3)) continue;
            const char c0 = label_of(id)[0];
            (c0 == 'R' ? right_sh : left_sh).push_back(id);
        }
        std::vector<int> face;
        for (const auto& row : sys.face) face.insert(face.end(), row.begin(), row.end());
        int face_per_row = 2;
        for (const auto& row : sys.face) face_per_row = std::max(face_per_row, static_cast<int>(row.size()));

        // Fila de arriba: qué se asigna (mando o teclado).
        {
            FontScope f(GetFonts().semibold, 13.5f);
            ImGui::SetCursorScreenPos(o);
            ImGui::TextUnformatted(Tr("Asignar con"));
            ImGui::SameLine(0, S(10));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(8));
            int dev = ctrl_device_;
            if (Segmented("##dispositivo", &dev, {"Mando", "Teclado"}, S(240))) ctrl_device_ = dev;
        }
        const float top = o.y + S(46);
        // Columna izquierda.
        float ly = top;
        if (!left_sh.empty()) ly += grid_box("Gatillos izquierdos", left_sh, {o.x, ly}, colw, 2) + col_gap * 0.5f;
        ly += grid_box("Cruceta", {RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_RIGHT},
                       {o.x, ly}, colw, 2) + col_gap * 0.5f;
        if (sys.analog) ly += stick_box("Palanca izquierda", 0, has_btn(RETRO_DEVICE_ID_JOYPAD_L3) ? RETRO_DEVICE_ID_JOYPAD_L3 : -1, {o.x, ly}, colw, nullptr);
        // Columna derecha.
        const float rx = o.x + W - colw;
        float ry = top;
        if (!right_sh.empty()) ry += grid_box("Gatillos derechos", right_sh, {rx, ry}, colw, 2) + col_gap * 0.5f;
        (void)face_per_row;
        ry += rows_box(sys.layout == Layout::N64 ? "Botones (C en la palanca derecha)" : "Botones", sys.face, {rx, ry}, colw) + col_gap * 0.5f;
        if (sys.analog)
            ry += stick_box("Palanca derecha", 1, has_btn(RETRO_DEVICE_ID_JOYPAD_R3) ? RETRO_DEVICE_ID_JOYPAD_R3 : -1, {rx, ry}, colw,
                            sys.layout == Layout::N64 ? "Mueve los botones C" : nullptr);
        // Centro: Select/Start arriba y el mando grande.
        const float cx0 = o.x + colw + col_gap, cx1 = rx - col_gap;
        float cy = top;
        if (!sys.center.empty()) {
            const float cw = std::min(S(170), (cx1 - cx0) * 0.4f);
            const float bw = cw * static_cast<float>(sys.center.size()) + cgap * (sys.center.size() - 1) + pad * 2;
            cy += grid_box(sys.center_title, sys.center, {(cx0 + cx1 - bw) * 0.5f, cy}, bw, static_cast<int>(sys.center.size())) + S(14);
        }
        const float bottom = std::max({ly, ry, cy + S(300)});
        const float dh = bottom - cy;
        const float dw = std::min(cx1 - cx0, dh * aspect);
        drawing({(cx0 + cx1 - dw) * 0.5f, cy}, {dw, dh});
        total_h = bottom - o.y;
        // Abajo, como en NX: vibración y los botones de asignar, restablecer y borrar.
        ImGui::SetCursorScreenPos({o.x, o.y + total_h + S(16)});
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            if (Toggle("##vibracion_nx", &settings_.rumble)) changed = true;
            ImGui::SameLine(0, S(8));
            ImGui::SetCursorScreenPos({ImGui::GetCursorScreenPos().x, p.y + S(3)});
            Muted("Vibración", 13.5f);
            ImGui::SameLine(0, S(22));
            ImGui::SetCursorScreenPos({ImGui::GetCursorScreenPos().x, p.y - S(4)});
            FontScope f(GetFonts().medium, 13);
            ImGui::TextDisabled("%s", Tr("Zona muerta"));
            ImGui::SameLine(0, S(8));
            changed |= SliderPercent("##zona_nx", &input_.deadzone, S(200));
            ImGui::SameLine(0, S(22));
            ImGui::SetCursorScreenPos({ImGui::GetCursorScreenPos().x, p.y - S(6)});
            toolbar();
        }
        total_h += S(60);
    } else if (!compact && W >= wide_side + right_side + col_gap * 2 + S(420)) {
        // Pantalla ancha: el control al centro y los grupos alrededor, como en DuckStation.
        cell_w = wide_cell;
        const float lh = dpad_group(o);
        const float rx = o.x + W - right_side;
        const float rh = buttons_group({rx, o.y}, right_side);
        const float cx0 = o.x + wide_side + col_gap, cx1 = rx - col_gap;
        const float ch = center_group({(cx0 + cx1) * 0.5f - cell_w - gap * 0.5f, o.y});
        const float draw_y = o.y + ch + S(20);
        float draw_h = std::clamp((cx1 - cx0) / aspect, S(200), S(380));
        total_h = std::max({lh, rh, draw_y - o.y + draw_h});
        draw_h = total_h - (draw_y - o.y);
        drawing({cx0, draw_y}, {cx1 - cx0, draw_h});
    } else {
        // Más angosto: el control grande arriba y los tres grupos debajo.
        // El dibujo crece hasta donde quepan debajo las casillas sin tener que bajar.
        // Alto real de cada grupo, para que el dibujo use justo lo que sobra y no haya que bajar.
        const int sh_rows = static_cast<int>((sys.shoulders.size() + 1) / 2);
        const float center_h = title_h + cell_h + (shoulders ? gap * 1.5f + title_h + sh_rows * (cell_h + gap) - gap : 0.0f);
        const float face_h = title_h + static_cast<float>(sys.face.size()) * (cell_h + gap) - gap;
        const float dpad_h = title_h + 3 * cell_h + 2 * gap;
        const float groups_h = S(18) + std::max({center_h, face_h, dpad_h});
        const float fit_h = ImGui::GetContentRegionAvail().y - groups_h - S(12);
        const float draw_h = std::clamp(std::min(W / aspect, fit_h), S(150), compact ? S(240) : S(340));
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
                             "Select+Start en el mando siempre abre el menú, y Alt+Enter cambia a pantalla completa. Un botón del "
                             "mando que use el juego no dispara su atajo. El avance rápido viene apagado: se enciende en "
                             "Ajustes » Rendimiento.");
    ImGui::Dummy({0, S(12)});
    const PadFamily family = input_.FamilyOfSlot(0);
    bool changed = false;
    for (int i = 0; i < static_cast<int>(Hotkey::Count); ++i) {
        ImGui::PushID(i);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        {
            FontScope f(GetFonts().semibold, 14.5f);
            ImGui::SetCursorScreenPos({p.x, p.y + S(4)});
            ImGui::TextUnformatted(Tr(HotkeyInfo(static_cast<Hotkey>(i)).name));
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
                Toast("Ese nombre ya existe o no es válido (sin «.», «|», «=», «#» ni «;», y sin espacios al principio o al final).", GuideId::Samy, 4);
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
    std::vector<std::string> names;
    int cur = 0;
    for (size_t i = 0; i < Systems().size(); ++i) {
        names.push_back(Systems()[i].full + "  ·  " + Systems()[i].hint);
        if (Systems()[i].id == ctrl_system_) cur = static_cast<int>(i);
    }
    const SystemDef& sys = Systems()[cur];
    const auto pads = input_.Gamepads();

    // Como DuckStation: a la izquierda los puertos (jugadores), los atajos y lo general; a la derecha,
    // el contenido. Todo dentro de un panel de ancho cómodo (no se desparrama en pantallas anchas).
    const float avail = ImGui::GetContentRegionAvail().x;
    const float panel_w = compact_ ? avail : std::min(avail, S(1560));
    const float nav_w = compact_ ? 0.0f : S(236);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - panel_w) * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(14), S(14)});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V4(Color::Surface));
    ImGui::BeginChild("##ctrl_marco", {panel_w, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (compact_) {
        Segmented("##ctrl_tab", &ctrl_tab_, {"General", "Jugador 1", "Jugador 2", "Atajos"}, ImGui::GetContentRegionAvail().x);
        ImGui::Dummy({0, S(6)});
    } else {
        ImGui::BeginChild("##ctrl_nav", {nav_w, 0});
        SectionLabel("PUERTOS");
        for (int pl = 0; pl < 2; ++pl) {
            const PlayerConfig& pc = input_.players[pl];
            std::string sub = pc.pad >= 0 && pc.pad < static_cast<int>(pads.size()) ? pads[pc.pad].name : pc.keyboard ? "Teclado" : "Sin mando";
            if (sub.size() > 22) sub = sub.substr(0, 21) + "…";
            if (NavItem(Fmt("Jugador %d", pl + 1).c_str(), Icon::Gamepad, ctrl_tab_ == pl + 1, sub.c_str())) ctrl_tab_ = pl + 1;
        }
        ImGui::Dummy({0, S(8)});
        SectionLabel("AJUSTES");
        if (NavItem("Atajos", Icon::Forward, ctrl_tab_ == 3, nullptr, false, Color::Amber)) ctrl_tab_ = 3;
        if (NavItem("General", Icon::Gear, ctrl_tab_ == 0, nullptr, false, Color::Teal)) ctrl_tab_ = 0;
        ImGui::Dummy({0, S(14)});
        SectionLabel("MANDOS CONECTADOS");
        if (pads.empty()) Muted("Ninguno. Conecta uno cuando quieras.", 13);
        for (const auto& pd : pads) {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            DrawIcon(ImGui::GetWindowDrawList(), Icon::Gamepad, {p.x + S(10), p.y + S(10)}, S(16), U32(Color::Green));
            ImGui::SetCursorScreenPos({p.x + S(26), p.y});
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + nav_w - S(30));
            Muted(Fmt("%d · %s", pd.slot + 1, pd.name.c_str()).c_str(), 13);
            ImGui::PopTextWrapPos();
        }
        ImGui::EndChild();
        ImGui::SameLine(0, S(14));
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(22), S(18)});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V4(Color::Bg, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, S(14));
    ImGui::BeginChild("##ctrl_panel", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    // Puertos: la consola se elige en una lista, como el «Tipo de mando» de DuckStation.
    if (ctrl_tab_ == 1 || ctrl_tab_ == 2) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        DrawControllerMini(ImGui::GetWindowDrawList(), sys.layout, {p.x + S(22), p.y + S(19)}, sys.handheld ? S(22) : S(36), 1.0f);
        ImGui::SetCursorScreenPos({p.x + S(52), p.y + S(9)});
        {
            FontScope f(GetFonts().semibold, 14);
            ImGui::TextUnformatted(Tr("Consola"));
        }
        ImGui::SameLine(0, S(12));
        ImGui::SetCursorScreenPos({ImGui::GetCursorScreenPos().x, p.y});
        if (ComboStyled("##ctrl_sis", &cur, names, std::min(S(360), ImGui::GetContentRegionAvail().x))) ctrl_system_ = Systems()[cur].id;
        ImGui::Dummy({0, S(10)});
    }
    if (ctrl_tab_ == 0) {
        bool changed = false;
        RowBegin("Zona muerta del stick", "Cuánto hay que mover el stick para que cuente.");
        changed |= SliderPercent("##dz", &input_.deadzone, S(320));
        RowEnd();
        RowBegin("El stick izquierdo también mueve la cruceta", nullptr);
        changed |= Toggle("##stick", &input_.stick_dpad);
        RowEnd();
        RowBegin("Vibración", "Los juegos que la usan (Rumble Pak en 64, DualShock en CD) hacen vibrar tu mando.");
        if (Toggle("##vibra", &settings_.rumble)) {
            changed = true;
            if (!settings_.rumble) input_.ApplyRumble(core_, false);
        }
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
            std::string owner;
            for (int pl = 0; pl < Input::kPlayers; ++pl)
                if (input_.players[pl].pad == pd.slot) owner = Fmt("  ·  Jugador %d", pl + 1);
            ImGui::TextUnformatted(Tr(Fmt("Mando %d · %s (%s)%s", pd.slot + 1, pd.name.c_str(), pd.family_name.c_str(), owner.c_str()).c_str()));
        }
        Muted("Cada jugador recuerda su mando: si uno se desconecta, los demás siguen con el suyo.", 13);
        if (changed) SaveConfig();
    } else if (ctrl_tab_ == 3) {
        DrawHotkeys();
    } else {
        DrawControllerEditor(Systems()[cur], ctrl_tab_ - 1, compact_);
    }
    ImGui::EndChild();
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
        ImGui::TextUnformatted(Tr("Satronho Emulation"));
    }
    {
        FontScope f(GetFonts().semibold, 16);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(10));
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Accent));
        ImGui::TextUnformatted(Tr("por Ridds"));
        ImGui::PopStyleColor();
    }
    Muted(build_ > 0 ? Fmt("Build %d", build_).c_str() : "Versión de desarrollo", 14);
    ImGui::EndGroup();
    ImGui::SetCursorScreenPos({p.x, p.y + S(170)});

    {
        FontScope f(GetFonts().regular, 15);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::min(ImGui::GetContentRegionAvail().x, S(820)));
        ImGui::TextWrapped("%s", Tr(("La casa de los emuladores de la familia Satronho. Un solo lugar para tus consolas, con una imagen cuidada, "
                                  "controles que responden al instante, un medidor en vivo y configuraciones sugeridas para tu equipo. Ya están " +
                                  SystemsSentence() + "; pronto llegan más consolas.").c_str()));
        ImGui::PopTextWrapPos();
    }
    ImGui::Dummy({0, S(14)});

    Muted("Logo, guías (Samy, Troya, Ñoño, Juana y MechaRidds), interfaz y diseño: Ridds. "
          "Software libre bajo GPL 3.0; las licencias de los componentes van en la carpeta «licencias».", 13);
    Muted("NES, Super Nintendo, Game Boy, Nintendo 64 y DS son marcas de Nintendo; Genesis, Mega Drive y Saturn, de Sega; PlayStation, de Sony. "
          "Se usan solo para describir con qué juegos funciona cada emulador. Este proyecto no está afiliado a ninguna de ellas y no incluye juegos.",
          12.5f);
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
    // Fundido al entrar al juego.
    if (const float f = 1.0f - static_cast<float>((ImGui::GetTime() - game_enter_t_) / 0.6); f > 0)
        ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos, {vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y}, IM_COL32(0, 0, 0, static_cast<int>(255 * f * f)));
    if (menu_open_) {
        DrawPauseMenu();
        return;
    }
    DrawGameBar();
    if (settings_.meter > 0) DrawMeter();

    // Indicadores de estado arriba al centro.
    std::string chip;
    Icon icon = Icon::Forward;
    unsigned col = Color::Accent;
    if (rewinding_) { chip = rewind_.Empty() ? "Sin más historial" : "Rebobinando"; icon = Icon::Rewind; col = Color::Pink; }
    else if (fast_forward_) { chip = Fmt("Avance rápido ×%d", settings_.fast_forward); }
    else if (disk_swap_target_ >= 0) { chip = "Cambiando de disco"; icon = Icon::Refresh; col = Color::Teal; }
    else if (paused_) { chip = "En pausa"; icon = Icon::Pause; col = Color::Amber; }
    if (!chip.empty()) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        FontScope f(GetFonts().semibold, 14);
        const ImVec2 ts = ImGui::CalcTextSize(Tr(chip.c_str()));
        const float w = ts.x + S(52), h = S(38);
        const ImVec2 p(vp->Pos.x + (vp->Size.x - w) * 0.5f, vp->Pos.y + GameBarTop() + S(20));
        dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(Color::Surface, 0.92f), h * 0.5f);
        dl->AddRect(p, {p.x + w, p.y + h}, U32(col, 0.6f), h * 0.5f);
        DrawIcon(dl, icon, {p.x + S(22), p.y + h * 0.5f}, S(15), U32(col));
        dl->AddText({p.x + S(38), p.y + (h - ts.y) * 0.5f}, U32(Color::Text), Tr(chip.c_str()));
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
    // Sin robarle el foco a una ventana encima (la pregunta de salir): si no, nunca se vería.
    if (confirm_ == 0 && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) ImGui::SetNextWindowFocus();
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
        DrawCover(ImGui::GetWindowDrawList(), current_, p, {p.x + w, p.y + S(150)}, S(14), false);
        ImGui::Dummy({w, S(156)});
        Ellipsize(ImGui::GetWindowDrawList(), GetFonts().bold, S(18), ImGui::GetCursorScreenPos(), U32(Color::Text), current_.title, w, 2);
        ImGui::Dummy({w, S(24) * (ImGui::CalcTextSize(Tr(current_.title.c_str())).x > w ? 2 : 1)});
        Muted(Fmt("Esta sesión: %s", FormatPlaytime(static_cast<long long>(session_s_)).c_str()).c_str(), 13);
        ImGui::Dummy({0, S(8)});
    }
    if (NavItem("Continuar", Icon::Play, false, "Esc", false, Color::Green)) SetMenuOpen(false);
    if (NavItem("Estados", Icon::Save, menu_section_ == 0)) menu_section_ = 0;
    if (NavItem("Ajustes rápidos", Icon::Gear, menu_section_ == 1, nullptr, false, Color::Accent)) menu_section_ = 1;
    if (NavItem("Controles", Icon::Gamepad, menu_section_ == 2, nullptr, false, Color::Amber)) menu_section_ = 2;
    // Juegos de varios discos: elegir el disco como en la consola.
    if (const unsigned discs = core_.DiskCount(); discs > 1) {
        const int idx = core_.DiskIndex();
        const std::string badge = Fmt("%d de %u", idx + 1, discs);
        if (NavItem("Discos", Icon::Refresh, menu_section_ == 3, badge.c_str(), false, Color::Teal)) menu_section_ = 3;
    }
    if (NavItem("Reiniciar", Icon::Restart, false)) {
        core_.Reset();
        SetMenuOpen(false);
        Toast("Juego reiniciado", GuideId::Samy, 2);
    }
    if (NavItem("Captura", Icon::Camera, false)) Screenshot();
    if (touch_mode_ && NavItem("Control en pantalla", Icon::Gamepad, false, nullptr, false, Color::Amber)) {
        touch_edit_ = true;
        SetMenuOpen(false);
    }
    if (NavItem("Salir a la biblioteca", Icon::Exit, false, nullptr, false, Color::Red)) RequestExitGame();
    ImGui::EndChild();
    ImGui::SameLine(0, 0);

    // Contenido.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(28), S(24)});
    ImGui::BeginChild("##menu_der", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    if (menu_section_ == 0) {
        Title("Estados", "Guarda en cualquier momento y vuelve justo ahí");
        ImGui::Dummy({0, S(4)});
        // Ranura automática (se guarda al salir) y deshacer la última carga.
        {
            const auto auto_it = slot_thumbs_.find(0);
            const bool has_auto = auto_it != slot_thumbs_.end();
            const auto d = slot_dates_.find(0);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x, h = S(64);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(Color::Surface), S(14));
            const float tw = h * 1.2f;
            if (has_auto && auto_it->second)
                dl->AddImageRounded(auto_it->second.Ref(), {p.x + S(8), p.y + S(8)}, {p.x + S(8) + tw, p.y + h - S(8)}, {0, 0}, {1, 1}, IM_COL32_WHITE,
                                    S(8));
            else
                dl->AddRectFilled({p.x + S(8), p.y + S(8)}, {p.x + S(8) + tw, p.y + h - S(8)}, U32(Color::Surface2), S(8));
            {
                FontScope f(GetFonts().semibold, 14);
                dl->AddText({p.x + tw + S(22), p.y + S(12)}, U32(Color::Text), Tr("Guardado automático"));
            }
            {
                FontScope f(GetFonts().regular, 12.5f);
                const std::string sub = !settings_.autosave_exit ? std::string("Apagado (Ajustes » Rendimiento)")
                                        : has_auto && d != slot_dates_.end() ? "Al salir · " + d->second
                                                                              : std::string("Se guarda al salir a la biblioteca");
                dl->AddText({p.x + tw + S(22), p.y + S(34)}, U32(Color::Dim), Tr(sub.c_str()));
            }
            FontScope f(GetFonts().semibold, 12.5f);
            float x = p.x + w - S(12);
            if (!undo_state_.empty()) {
                const float bw2 = S(170);
                x -= bw2;
                ImGui::SetCursorScreenPos({x, p.y + (h - S(32)) * 0.5f});
                if (Button("Deshacer la carga", ButtonKind::Secondary, {bw2, S(32)}, Icon::Rewind, true)) {
                    UndoLoad();
                    SetMenuOpen(false);
                }
                x -= S(8);
            }
            const float bw1 = S(100);
            x -= bw1;
            ImGui::SetCursorScreenPos({x, p.y + (h - S(32)) * 0.5f});
            ImGui::BeginDisabled(!has_auto);
            if (Button("Cargar##auto", ButtonKind::Primary, {bw1, S(32)})) {
                LoadSlot(0);
                SetMenuOpen(false);
            }
            ImGui::EndDisabled();
            ImGui::SetCursorScreenPos({p.x, p.y + h + S(12)});
            ImGui::Dummy({0, 0});
        }
        const float avail = ImGui::GetContentRegionAvail().x;
        const float gap = S(14);
        const float cw = std::floor((avail - gap * 3) / 4);
        const float thumb_h = cw * 240.0f / 256.0f * 0.64f;
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
                dl->AddText({p.x + S(12), t1.y + S(8)}, U32(current ? Color::Accent : Color::Text), Tr(Fmt("Ranura %d", s).c_str()));
            }
            {
                FontScope f(GetFonts().regular, 11.5f);
                const auto d = slot_dates_.find(s);
                dl->AddText({p.x + S(12), t1.y + S(28)}, U32(Color::Dim), Tr(d != slot_dates_.end() ? d->second.c_str() : "Vacía"));
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
        Muted(Fmt("%s guarda y %s carga en la ranura marcada. %s y %s cambian de ranura.",
                  Input::KeyLabel(input_.hk_keys[static_cast<int>(Hotkey::SaveState)]).c_str(),
                  Input::KeyLabel(input_.hk_keys[static_cast<int>(Hotkey::LoadState)]).c_str(),
                  Input::KeyLabel(input_.hk_keys[static_cast<int>(Hotkey::PrevSlot)]).c_str(),
                  Input::KeyLabel(input_.hk_keys[static_cast<int>(Hotkey::NextSlot)]).c_str()).c_str(),
              13);
    } else if (menu_section_ == 1) {
        Title("Ajustes rápidos", "Cambia la imagen sin salir del juego");
        ImGui::Dummy({0, S(8)});
        bool changed = false;
        // Imagen propia de esta consola: se guarda aparte y manda sobre la general.
        auto vit = settings_.video_for.find(sys_->id);
        bool own = vit != settings_.video_for.end() && vit->second.on;
        RowBegin(("Imagen propia para " + sys_->full).c_str(),
                 own ? "Lo que cambies aquí vale solo para esta consola." : "Apagado: se usa la imagen general de Ajustes » Gráficos.");
        if (Toggle("##propio", &own)) {
            VideoOverride& v = settings_.video_for[sys_->id];
            if (own && !v.on) {
                // Empieza con lo que se está viendo ahora.
                v.scale_mode = settings_.scale_mode;
                v.filter = settings_.filter;
                v.effect = settings_.effect;
                v.effect_strength = settings_.effect_strength;
            }
            v.on = own;
            changed = true;
        }
        RowEnd();
        int* q_effect = own ? &settings_.video_for[sys_->id].effect : &settings_.effect;
        int* q_filter = own ? &settings_.video_for[sys_->id].filter : &settings_.filter;
        int* q_scale = own ? &settings_.video_for[sys_->id].scale_mode : &settings_.scale_mode;
        float* q_strength = own ? &settings_.video_for[sys_->id].effect_strength : &settings_.effect_strength;
        RowBegin("Efecto de pantalla", nullptr);
        changed |= Segmented("##qe", q_effect, {"Ninguno", "Líneas", "CRT", "LCD", "Auto"}, S(320));
        RowEnd();
        if (*q_effect != 0) {
            RowBegin("Intensidad del efecto", nullptr);
            changed |= SliderPercent("##qi", q_strength, S(300));
            RowEnd();
        }
        RowBegin("Filtro", nullptr);
        changed |= Segmented("##qf", q_filter, {"Píxel", "Nítido", "Suave"}, S(300));
        RowEnd();
        RowBegin("Tamaño", nullptr);
        changed |= Segmented("##qs", q_scale, {"Ajustar", "Entero", "Estirar"}, S(300));
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
        if (sys_->runahead_ok) {
            RowBegin("Adelanto de cuadros", nullptr);
            changed |= Segmented("##qr", &settings_.runahead, {"Apagado", "1", "2"}, S(300));
            RowEnd();
        }
        if (changed) SaveConfig();
    } else if (menu_section_ == 3) {
        Title("Discos", "Elige el disco como en la consola: se abre la tapa, se cambia y se cierra");
        ImGui::Dummy({0, S(8)});
        const unsigned discs = core_.DiskCount();
        const int idx = core_.DiskIndex();
        for (unsigned i = 0; i < discs; ++i) {
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x, h = S(52);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const bool in = static_cast<int>(i) == idx;
            dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(in ? Color::Surface2 : Color::Surface), S(12));
            if (in) dl->AddRect(p, {p.x + w, p.y + h}, U32(Color::Teal, 0.7f), S(12), S(1.5f));
            dl->AddCircle({p.x + S(28), p.y + h * 0.5f}, S(14), U32(in ? Color::Teal : Color::Dim), 0, S(2));
            dl->AddCircleFilled({p.x + S(28), p.y + h * 0.5f}, S(4), U32(in ? Color::Teal : Color::Dim));
            {
                FontScope f(GetFonts().semibold, 14.5f);
                dl->AddText({p.x + S(54), p.y + (h - ImGui::GetFontSize()) * 0.5f}, U32(Color::Text), Tr(core_.DiskLabel(i).c_str()));
            }
            ImGui::SetCursorScreenPos({p.x + w - S(130), p.y + (h - S(34)) * 0.5f});
            if (in) {
                Chip("Puesto", Color::Teal);
            } else {
                ImGui::BeginDisabled(disk_swap_target_ >= 0);
                if (Button("Poner este", ButtonKind::Primary, {S(120), S(34)})) {
                    RequestDisk(i);
                    SetMenuOpen(false);
                }
                ImGui::EndDisabled();
            }
            ImGui::SetCursorScreenPos({p.x, p.y + h + S(8)});
            ImGui::Dummy({0, 0});
            ImGui::PopID();
        }
        if (disk_swap_target_ >= 0) Muted("Cambiando de disco…", 13);
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
        const ImVec2 ts = ImGui::CalcTextSize(Tr(s.c_str()));
        const ImVec2 size(ts.x + S(34), ts.y + S(16));
        ImVec2 p = preview_pos;
        if (!preview) {
            const float m = S(16);
            p.x = (settings_.meter_corner % 2) ? vp->Pos.x + vp->Size.x - size.x - m : vp->Pos.x + m;
            p.y = (settings_.meter_corner >= 2) ? vp->Pos.y + vp->Size.y - size.y - m - GameBarBottom() : vp->Pos.y + m + GameBarTop();
        }
        dl->AddRectFilled(p, {p.x + size.x, p.y + size.y}, U32(0x1B1829, 0.88f), S(10));
        dl->AddCircleFilled({p.x + S(14), p.y + size.y * 0.5f}, S(4), U32(accent));
        dl->AddText({p.x + S(26), p.y + S(8)}, U32(speed_col), Tr(s.c_str()));
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
        dl->AddText({p.x + pad + S(4), p.y + pad - S(4)}, U32(Color::Text), Tr(s.c_str()));
        const float sw = ImGui::CalcTextSize(Tr(s.c_str())).x;
        FontScope f2(GetFonts().semibold, 12);
        dl->AddText({p.x + pad + S(10) + sw, p.y + pad + S(14)}, U32(Color::Muted), Tr("FPS"));
    }
    {
        FontScope f(GetFonts().semibold, 15);
        const std::string s = Fmt("%.0f %%", speed);
        const float sw = ImGui::CalcTextSize(Tr(s.c_str())).x;
        dl->AddText({p.x + w - pad - sw, p.y + pad + S(2)}, U32(speed_col), Tr(s.c_str()));
        FontScope f2(GetFonts().regular, 11);
        const char* lbl = "velocidad";
        dl->AddText({p.x + w - pad - ImGui::CalcTextSize(lbl).x, p.y + pad + S(22)}, U32(Color::Dim), Tr(lbl));
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
        dl->AddText({p.x + pad + S(4), y}, U32(Color::Muted), Tr(labels[i]));
        FontScope f2(GetFonts().mono, 12.5f);
        const unsigned col = i == 1 && perf_.stutters_minute > 0 ? Color::Orange : Color::Text;
        dl->AddText({p.x + pad + S(76), y}, U32(col), Tr(values[i].c_str()));
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
        const ImVec2 ts = ImGui::CalcTextSize(Tr(t.text.c_str()), nullptr, false, S(520));
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
        dl->AddText(nullptr, 0.0f, {p.x + S(52), p.y + (h - ts.y) * 0.5f}, U32(Color::Text, a), Tr(t.text.c_str()), nullptr, S(520));
        y += h + S(10);
    }
}

void App::DrawGuideBubble() {
    if (!settings_.guides || !settings_.bubble || bubble_closed_ || tips_.empty() || show_welcome_) return;
    tip_timer_ += ImGui::GetIO().DeltaTime;
    if (tip_timer_ > 25.0) {
        tip_timer_ = 0;
        tip_index_ = NextTip();
    }
    const Tip& tip = tips_[tip_index_];
    const GuideInfo& g = Guide(tip.guide);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float w = S(380);
    float text_h;
    {
        FontScope f(GetFonts().regular, 13.5f);
        text_h = ImGui::CalcTextSize(Tr(tip.text.c_str()), nullptr, false, w - S(36)).y;
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
        dl->AddText({b0.x + S(18), b0.y + S(14)}, U32(g.color), Tr(g.name));
    }
    {
        FontScope f(GetFonts().regular, 13.5f);
        dl->AddText(nullptr, 0.0f, {b0.x + S(18), b0.y + S(38)}, U32(Color::Text), Tr(tip.text.c_str()), nullptr, w - S(36));
    }
    const ImVec2 ac(b1.x + S(14) + av * 0.5f, b1.y - av * 0.5f + GuideBob(g.name));
    const Image& img = guides_[static_cast<int>(tip.guide)];
    if (img) dl->AddImageRounded(img.Ref(), {ac.x - av * 0.5f, ac.y - av * 0.5f}, {ac.x + av * 0.5f, ac.y + av * 0.5f}, {0, 0}, {1, 1}, IM_COL32_WHITE, av * 0.5f);

    ImGui::SetCursorScreenPos({b1.x - S(102), b0.y + S(8)});
    if (IconButton("##preguntar", Icon::Paw, S(28), U32(Color::Muted), "Pregúntale a los guías")) page_ = Page::Guides;
    ImGui::SameLine(0, S(4));
    if (IconButton("##sig", Icon::ChevronRight, S(28), U32(Color::Muted), "Otro consejo")) {
        tip_index_ = NextTip();
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
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (inner - ImGui::CalcTextSize(Tr(t)).x) * 0.5f);
                ImGui::TextUnformatted(Tr(t));
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
                ImGui::TextUnformatted(Tr(c.c_str()));
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
                      "Dime en qué carpeta tienes tus juegos y los acomodo en la biblioteca, cada uno en su consola. Mientras juegas: Esc abre el menú, "
                      "F5 guarda, F9 carga y mantener Retroceso rebobina. Al salir guardo dónde quedaste.",
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
