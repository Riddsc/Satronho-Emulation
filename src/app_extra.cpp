// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Acabados de la etapa 1: temas y logo de fondo, carrusel, carátulas, preguntas al salir,
// novedades de Juana y la conversación con los guías.
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <random>

#include "app.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/controllers.h"
#include "util/log.h"

namespace Satronho {

using namespace Ui;

namespace {
std::string Fmt(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    return buf;
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

unsigned Rng() {
    static std::mt19937 rng(static_cast<unsigned>(SDL_GetTicksNS()));
    return rng();
}

float EaseOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }
constexpr float kPi() { return 3.14159265f; }
} // namespace

// ---------------------------------------------------------------- temas y logo de fondo

void App::ApplyTheme() {
    Ui::ApplyTheme(settings_.theme);
    Ui::ApplyStyle();
}

void App::BeginPageTransition() { page_anim_t_ = ImGui::GetTime(); }

void App::DrawWatermark(ImVec2 p0, ImVec2 p1) {
    if (!logo_) return;
    // Nunca se apaga: como mínimo 10 %.
    const float a = std::clamp(settings_.watermark, 0.10f, 1.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float w = p1.x - p0.x, h = p1.y - p0.y;
    const float size = std::min(w * 0.42f, h * 0.48f);
    const ImVec2 c(p0.x + w * 0.5f, p0.y + h * 0.47f);
    const float fs = size * 0.30f;
    const char* word = "Satronho";
    const ImVec2 ts = GetFonts().bold->CalcTextSizeA(fs, FLT_MAX, 0, word);
    const float total = size + fs * 0.9f;
    const float top = c.y - total * 0.5f;
    dl->AddImage(logo_.Ref(), {c.x - size * 0.5f, top}, {c.x + size * 0.5f, top + size}, {0, 0}, {1, 1}, IM_COL32(255, 255, 255, static_cast<int>(255 * a)));
    dl->AddText(GetFonts().bold, fs, {c.x - ts.x * 0.5f, top + size - fs * 0.05f}, U32(Color::Text, a), word);
}

void App::DrawSettingsAppearance() {
    GuideSays(GuideId::Troya, "Elige el tema que más te guste. El logo de Satronho siempre queda de fondo: súbele o bájale la opacidad, "
                              "pero nunca menos del 10 %.");
    ImGui::Dummy({0, S(14)});
    Heading("Tema", 16);
    ImGui::Dummy({0, S(2)});
    const auto& themes = Themes();
    const float avail = ImGui::GetContentRegionAvail().x;
    const int cols = avail > S(640) ? 3 : 2;
    const float gap = S(12);
    const float cw = (avail - gap * (cols - 1)) / cols, ch = S(118);
    for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
        const Theme& t = themes[i];
        if (i % cols) ImGui::SameLine(0, gap);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushID(i);
        const bool clicked = ImGui::InvisibleButton("##tema", {cw, ch});
        const bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool sel = settings_.theme == i;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float r = S(14);
        // Vista previa en miniatura: barra lateral, fondo, una tarjeta y el acento.
        dl->AddRectFilled(p, {p.x + cw, p.y + ch}, U32(t.bg), r);
        dl->AddRectFilled(p, {p.x + cw * 0.26f, p.y + ch}, U32(t.side), r, ImDrawFlags_RoundLeft);
        for (int k = 0; k < 3; ++k)
            dl->AddRectFilled({p.x + S(10), p.y + S(14) + k * S(14)}, {p.x + cw * 0.26f - S(10), p.y + S(22) + k * S(14)},
                              U32(k == 0 ? t.accent : t.surface3), S(3));
        const float cx = p.x + cw * 0.26f + S(12);
        dl->AddRectFilled({cx, p.y + S(12)}, {cx + S(56), p.y + S(64)}, U32(t.surface), S(8));
        dl->AddRectFilled({cx + S(64), p.y + S(12)}, {cx + S(120), p.y + S(64)}, U32(t.surface2), S(8));
        dl->AddRectFilled({cx, p.y + S(70)}, {cx + S(44), p.y + S(78)}, U32(t.accent), S(4));
        {
            FontScope f(GetFonts().semibold, 14);
            dl->AddText({cx, p.y + ch - S(36)}, U32(t.text), t.name);
        }
        {
            FontScope f(GetFonts().regular, 11.5f);
            Ellipsize(dl, GetFonts().regular, S(11.5f), {cx, p.y + ch - S(18)}, U32(t.muted), t.blurb, cw - (cx - p.x) - S(8), 1);
        }
        dl->AddRect(p, {p.x + cw, p.y + ch}, sel ? U32(Color::Accent) : U32(Color::Border, hov ? 1.0f : 0.6f), r, S(sel ? 2.5f : 1.0f));
        if (sel) {
            const ImVec2 c(p.x + cw - S(18), p.y + S(18));
            dl->AddCircleFilled(c, S(11), U32(Color::Accent));
            DrawIcon(dl, Icon::Check, c, S(12), U32(0xFFFFFF));
        }
        if (clicked && !sel) {
            settings_.theme = i;
            ApplyTheme();
            SaveConfig();
        }
    }
    ImGui::Dummy({0, S(18)});

    RowBegin("Logo de Satronho de fondo", "Su opacidad. Va de 10 % a 100 %; no se puede apagar.");
    int pct = static_cast<int>(std::lround(settings_.watermark * 100));
    if (SliderIntStyled("##marca", &pct, 10, 100, "%d %%", ImGui::GetContentRegionAvail().x)) {
        settings_.watermark = std::clamp(pct / 100.0f, 0.10f, 1.0f);
        SaveConfig();
    }
    RowEnd();
    ImGui::Dummy({0, S(10)});
    Heading("Biblioteca y juego", 16);
    {
        bool changed = false;
        RowBegin("Vista de la biblioteca", "Cómo se ven tus juegos. Cada consola puede tener la suya desde el botón «Vista» de la biblioteca; esto la pone en todas.");
        int v = std::clamp(settings_.LibView(""), 0, 2);
        if (Segmented("##vista_todas", &v, {"Cuadrícula", "Carrusel", "Lista"}, S(360))) {
            settings_.lib_view.clear();
            settings_.lib_view["todas"] = v;
            for (const auto& sd : Systems()) settings_.lib_view[sd.id] = v;
            changed = true;
        }
        RowEnd();
        RowBegin("Tamaño de las carátulas", nullptr);
        changed |= Segmented("##tam_caratulas", &settings_.cover_size, {"Pequeñas", "Medianas", "Grandes"}, S(360));
        RowEnd();
        RowBegin("Barra de menús en el juego", "Arriba del juego en ventana: Juego, Imagen, Sonido, Controles… En pantalla completa aparece con Esc.");
        changed |= Toggle("##barra_juego", &settings_.game_bar);
        RowEnd();
        if (changed) SaveConfig();
    }
    ImGui::Dummy({0, S(10)});
    Heading("Control en pantalla", 16);
    ImGui::Dummy({0, S(2)});
    RowBegin("Mostrar el control en pantalla", "Automático: con pantalla táctil y sin mando conectado. Se acomoda en el menú del juego » «Control en pantalla».");
    if (Segmented("##tactil_ver", &settings_.touch_show, {"Automático", "Siempre", "Nunca"}, ImGui::GetContentRegionAvail().x)) SaveConfig();
    RowEnd();
    RowBegin("Tamaño de los botones", "");
    {
        int v = static_cast<int>(std::lround(settings_.touch_scale * 100));
        if (SliderIntStyled("##tactil_tam", &v, 60, 160, "%d %%", ImGui::GetContentRegionAvail().x)) {
            settings_.touch_scale = v / 100.0f;
            SaveConfig();
        }
    }
    RowEnd();
    RowBegin("Opacidad de los botones", "");
    {
        int v = static_cast<int>(std::lround(settings_.touch_opacity * 100));
        if (SliderIntStyled("##tactil_op", &v, 15, 100, "%d %%", ImGui::GetContentRegionAvail().x)) {
            settings_.touch_opacity = v / 100.0f;
            SaveConfig();
        }
    }
    RowEnd();
    RowBegin("Vibrar al tocar", "Una vibración cortita cada vez que aprietas un botón en pantalla.");
    if (Toggle("##tactil_vib", &settings_.touch_haptics)) SaveConfig();
    RowEnd();
    ImGui::Dummy({0, S(10)});
    Heading("Al salir", 16);
    ImGui::Dummy({0, S(2)});
    RowBegin("Preguntar antes de cerrar la app", "Así un clic de más en la X no te saca de golpe.");
    if (Toggle("##conf_app", &settings_.confirm_exit_app)) SaveConfig();
    RowEnd();
    RowBegin("Preguntar antes de salir de un juego", "Desde el menú del juego, antes de volver a la biblioteca.");
    if (Toggle("##conf_juego", &settings_.confirm_exit_game)) SaveConfig();
    RowEnd();
}

// ---------------------------------------------------------------- carátulas

bool App::DrawCover(ImDrawList* dl, const Game& g, ImVec2 a, ImVec2 b, float rounding, bool hovered) {
    const Ui::Image* img = (settings_.covers || covers_.HasCustom(g)) ? covers_.Get(g) : nullptr;
    if (!img || !*img) {
        const SystemDef* gs = SystemById(g.system);
        DrawCartridge(dl, a, b, g.hue, g.title, rounding, hovered, gs ? gs->layout : Layout::Nes, gs ? gs->label.c_str() : "OCTA");
        return false;
    }
    const float w = b.x - a.x, h = b.y - a.y;
    const float ia = static_cast<float>(img->w) / std::max(1, img->h);
    // Fondo: la misma carátula agrandada y oscurecida (llena el espacio sin deformar).
    {
        float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
        const float area = w / h;
        if (ia > area) {
            const float k = area / ia;
            u0 = (1 - k) * 0.5f;
            u1 = 1 - u0;
        } else {
            const float k = ia / area;
            v0 = (1 - k) * 0.5f;
            v1 = 1 - v0;
        }
        dl->AddImageRounded(img->Ref(), a, b, {u0, v0}, {u1, v1}, IM_COL32(70, 66, 82, 255), rounding, ImDrawFlags_RoundTop);
        dl->AddRectFilled(a, b, Hsv(static_cast<float>(g.hue), 0.5f, 0.12f, 0.45f), rounding, ImDrawFlags_RoundTop);
    }
    // La carátula entera, centrada, con sombra.
    const float pad = std::min(w, h) * 0.07f;
    float cw = w - pad * 2, ch = cw / ia;
    if (ch > h - pad * 2) {
        ch = h - pad * 2;
        cw = ch * ia;
    }
    const float lift = hovered ? S(3) : 0;
    const ImVec2 c0(a.x + (w - cw) * 0.5f, a.y + (h - ch) * 0.5f - lift), c1(c0.x + cw, c0.y + ch);
    dl->AddRectFilled({c0.x + S(2), c0.y + S(6)}, {c1.x + S(2), c1.y + S(6)}, IM_COL32(0, 0, 0, 110), S(6));
    dl->AddImageRounded(img->Ref(), c0, c1, {0, 0}, {1, 1}, IM_COL32_WHITE, S(5));
    return true;
}

void App::PickCoverDialog(const Game& g) {
    cover_target_ = g.key;
    static const SDL_DialogFileFilter filters[] = {{"Imágenes", "png;jpg;jpeg"}};
    SDL_ShowOpenFileDialog(&CoverDialogCallback, this, window_, filters, 1, nullptr, false);
}

// ---------------------------------------------------------------- carrusel

void App::DrawLibraryCarousel(const std::vector<const Game*>& list, const SystemDef* lib_sys) {
    if (list.empty()) return;
    const int n = static_cast<int>(list.size());
    // Al cambiar de consola se empieza por el juego elegido antes (si está) o el primero.
    if (carousel_list_ != lib_system_) {
        carousel_list_ = lib_system_;
        carousel_index_ = 0;
        for (int i = 0; i < n; ++i)
            if (list[i]->key == carousel_key_) carousel_index_ = i;
        carousel_pos_ = static_cast<float>(carousel_index_);
    }
    for (int i = 0; i < n; ++i)
        if (list[i]->key == carousel_key_) carousel_index_ = i;
    carousel_index_ = std::clamp(carousel_index_, 0, n - 1);

    // En «todas las consolas»: una fila de consolas arriba (con cuántos juegos tiene cada una). La
    // del juego elegido se ilumina con su color y un clic salta a su primer juego.
    if (!lib_sys) {
        std::vector<std::pair<const SystemDef*, int>> present; // consola y su primer índice
        std::map<std::string, int> counts;
        for (int i = 0; i < n; ++i) {
            if (!counts.count(list[i]->system))
                if (const SystemDef* sd = SystemById(list[i]->system)) present.push_back({sd, i});
            ++counts[list[i]->system];
        }
        ImDrawList* tdl = ImGui::GetWindowDrawList();
        ImVec2 tp = ImGui::GetCursorScreenPos();
        const float th = S(40), x_end = tp.x + ImGui::GetContentRegionAvail().x;
        float x = tp.x;
        for (const auto& [sd, first] : present) {
            const bool cur = sd->id == list[carousel_index_]->system;
            FontScope f(GetFonts().semibold, 13.5f);
            const std::string t = sd->name + Fmt("  %d", counts[sd->id]);
            const float w = ImGui::CalcTextSize(t.c_str()).x + S(58);
            if (x + w > x_end) break;
            ImGui::SetCursorScreenPos({x, tp.y});
            ImGui::PushID(sd->id.c_str());
            if (ImGui::InvisibleButton("##sis", {w, th})) {
                carousel_index_ = first;
                carousel_key_ = list[first]->key;
            }
            const bool h = ImGui::IsItemHovered();
            ImGui::PopID();
            tdl->AddRectFilled({x, tp.y}, {x + w, tp.y + th}, U32(cur ? sd->color : Color::Surface2, cur ? 0.22f : h ? 1.0f : 0.8f), th * 0.5f);
            if (cur) tdl->AddRect({x, tp.y}, {x + w, tp.y + th}, U32(sd->color, 0.9f), th * 0.5f, S(1.5f));
            DrawControllerMini(tdl, sd->layout, {x + S(22), tp.y + th * 0.5f}, sd->handheld ? S(16) : S(24), cur || h ? 1.0f : 0.7f);
            tdl->AddText({x + S(40), tp.y + (th - ImGui::GetFontSize()) * 0.5f}, U32(cur ? Color::Text : Color::Muted), t.c_str());
            x += w + S(8);
        }
        ImGui::SetCursorScreenPos({tp.x, tp.y + th + S(12)});
        ImGui::Dummy({0, 0});
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float W = ImGui::GetContentRegionAvail().x;
    const float avail_h = std::max(ImGui::GetContentRegionAvail().y, S(420));
    const float H = std::clamp(avail_h - S(150), S(260), S(460));
    const float cover_h = H - S(40);

    // Teclado y mando: izquierda/derecha cambian de juego; Enter o A juegan.
    const bool typing = ImGui::GetIO().WantTextInput;
    const bool focus_ok = !typing && !show_welcome_ && !show_whats_new_ && confirm_ == 0 && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    int move = 0;
    if (focus_ok) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft)) move = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight)) move = 1;
    }
    const ImRect area(origin, {origin.x + W, origin.y + H});
    if (ImGui::IsMouseHoveringRect(area.Min, area.Max) && ImGui::GetIO().MouseWheel != 0 && confirm_ == 0) move = ImGui::GetIO().MouseWheel > 0 ? -1 : 1;
    if (move) carousel_index_ = std::clamp(carousel_index_ + move, 0, n - 1);

    const float dt = ImGui::GetIO().DeltaTime;
    carousel_pos_ += (carousel_index_ - carousel_pos_) * std::min(1.0f, dt * 11.0f);
    if (std::fabs(carousel_pos_ - carousel_index_) < 0.001f) carousel_pos_ = static_cast<float>(carousel_index_);

    const Game& sel = *list[carousel_index_];
    carousel_key_ = sel.key;
    selected_key_ = sel.key;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Escenario: degradado con el color del juego elegido.
    const float hue = static_cast<float>(sel.hue);
    dl->AddRectFilled(area.Min, area.Max, U32(Color::Surface, 0.92f), S(24));
    dl->AddRectFilledMultiColor({area.Min.x + 1, area.Min.y + area.GetHeight() * 0.35f}, {area.Max.x - 1, area.Max.y - S(20)}, Hsv(hue, 0.6f, 0.35f, 0.0f),
                                Hsv(hue, 0.6f, 0.35f, 0.0f), Hsv(hue, 0.6f, 0.40f, 0.35f), Hsv(hue, 0.6f, 0.40f, 0.35f));
    dl->AddRect(area.Min, area.Max, Hsv(hue, 0.5f, 0.7f, 0.25f), S(24));
    // Reflejo en el «piso».
    dl->AddRectFilled({area.Min.x + S(40), area.Max.y - S(26)}, {area.Max.x - S(40), area.Max.y - S(24)}, U32(Color::Border, 0.6f));

    dl->PushClipRect(area.Min, area.Max, true);
    const float cx = area.Min.x + W * 0.5f;
    const float base_y = area.Min.y + S(18);
    // Cada carátula con su forma: las cajas de SNES y 64 son apaisadas; las de NES, Genesis o
    // Saturn, altas. Si todavía no hay carátula, se usa la forma típica de la consola.
    auto aspect_of = [&](const Game& g) {
        if (settings_.covers || covers_.HasCustom(g))
            if (const Ui::Image* img = covers_.Get(g); img && *img) return std::clamp(static_cast<float>(img->w) / std::max(1, img->h), 0.62f, 1.45f);
        if (g.system == "hexa" || g.system == "n64") return 1.38f;
        if (g.system == "cd" || g.system == "bolsillo_plus" || g.system == "bolsillo") return 1.0f;
        return 0.74f;
    };
    const int lo = std::max(0, carousel_index_ - 6), hi = std::min(n - 1, carousel_index_ + 6);
    // Posición de cada una en una tira: el centro de la i-ésima, sumando los anchos (se enciman un poco).
    std::vector<float> asp(hi - lo + 1), center(hi - lo + 1);
    for (int i = lo; i <= hi; ++i) asp[i - lo] = aspect_of(*list[i]);
    center[0] = 0;
    for (int i = lo + 1; i <= hi; ++i) center[i - lo] = center[i - lo - 1] + (asp[i - lo - 1] + asp[i - lo]) * 0.5f * cover_h * 0.70f;
    auto strip_at = [&](float pos) {
        const float p = std::clamp(pos, static_cast<float>(lo), static_cast<float>(hi));
        const int i0 = static_cast<int>(std::floor(p)), i1 = std::min(hi, i0 + 1);
        const float t = p - i0;
        return center[i0 - lo] + (center[i1 - lo] - center[i0 - lo]) * t;
    };
    const float origin_x = strip_at(carousel_pos_);
    // Se dibujan de atrás hacia adelante: primero las más lejanas.
    std::vector<int> order;
    for (int i = lo; i <= hi; ++i) order.push_back(i);
    std::sort(order.begin(), order.end(), [&](int x, int y) { return std::fabs(x - carousel_pos_) > std::fabs(y - carousel_pos_); });
    int clicked = -1;
    for (int i : order) {
        const float d = i - carousel_pos_;
        const float ad = std::min(std::fabs(d), 4.0f);
        const float k = 1.0f - ad * 0.14f;
        const float chh = cover_h * k, cw = chh * asp[i - lo];
        const float x = cx + (center[i - lo] - origin_x) * (1.0f - ad * 0.05f);
        const ImVec2 a(x - cw * 0.5f, base_y + (cover_h - chh) * 0.55f), b(x + cw * 0.5f, base_y + (cover_h - chh) * 0.55f + chh);
        if (b.x < area.Min.x || a.x > area.Max.x) continue;
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(list[i]->key.c_str());
        ImGui::SetNextItemAllowOverlap();
        if (ImGui::InvisibleButton("##car", {cw, chh})) clicked = i;
        const bool hov = ImGui::IsItemHovered() && ad < 0.5f;
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) clicked = i + 100000;
        ImGui::PopID();
        // Sombra y carátula.
        dl->AddRectFilled({a.x + S(4), a.y + S(12)}, {b.x + S(4), b.y + S(10)}, IM_COL32(0, 0, 0, static_cast<int>(90 * k)), S(18));
        DrawCover(dl, *list[i], a, b, S(16), hov);
        // Las de los lados se oscurecen; la del centro lleva borde del color de su consola.
        if (ad > 0.05f) dl->AddRectFilled(a, b, IM_COL32(8, 6, 14, static_cast<int>(std::min(0.72f, ad * 0.34f) * 255)), S(16));
        else if (const SystemDef* gs = SystemById(list[i]->system)) dl->AddRect(a, b, U32(gs->color, 0.85f), S(16), S(2.5f));
        // Con todas las consolas juntas, cada carátula lleva arriba la etiqueta de su consola.
        if (!lib_sys)
            if (const SystemDef* gs = SystemById(list[i]->system)) {
                FontScope f(GetFonts().bold, 11.5f * std::max(0.7f, k));
                const ImVec2 ts = ImGui::CalcTextSize(gs->name.c_str());
                const ImVec2 c0(a.x + S(10), a.y + S(10)), c1(c0.x + ts.x + S(16), c0.y + ts.y + S(8));
                dl->AddRectFilled(c0, c1, U32(gs->color, 0.92f * std::max(0.4f, k)), (c1.y - c0.y) * 0.5f);
                dl->AddText({c0.x + S(8), c0.y + S(4)}, IM_COL32(16, 12, 28, static_cast<int>(255 * std::max(0.5f, k))), gs->name.c_str());
            }
    }
    dl->PopClipRect();
    if (clicked >= 100000) {
        RequestStart(list[clicked - 100000]->path);
    } else if (clicked >= 0) {
        if (clicked == carousel_index_) RequestStart(list[clicked]->path);
        else carousel_index_ = clicked;
    }
    carousel_key_ = list[carousel_index_]->key;

    // Flechas a los lados.
    auto arrow = [&](const char* id, Icon icon, float x, int dir) {
        ImGui::SetCursorScreenPos({x, area.Min.y + H * 0.5f - S(24)});
        const bool enabled = dir < 0 ? carousel_index_ > 0 : carousel_index_ < n - 1;
        if (IconButton(id, icon, S(48), U32(enabled ? Color::Text : Color::Dim), dir < 0 ? "Anterior" : "Siguiente") && enabled) {
            carousel_index_ += dir;
            carousel_key_ = list[carousel_index_]->key;
        }
    };
    arrow("##car_izq", Icon::ChevronLeft, area.Min.x + S(14), -1);
    arrow("##car_der", Icon::ChevronRight, area.Max.x - S(62), 1);
    if (focus_ok && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown))) RequestStart(sel.path);

    // Información del juego elegido.
    ImGui::SetCursorScreenPos({origin.x, area.Max.y + S(18)});
    const ImVec2 ip = ImGui::GetCursorScreenPos();
    const SystemDef* gs = SystemById(sel.system);
    float info_w = W - S(360);
    Ellipsize(dl, GetFonts().bold, S(26), ip, U32(Color::Text), sel.title, info_w, 1);
    {
        FontScope f(GetFonts().regular, 14);
        std::string meta = gs ? gs->full + "  ·  " : std::string();
        meta += sel.playtime_s > 0 ? FormatPlaytime(sel.playtime_s) + " jugado" : std::string("Sin jugar");
        if (sel.last_played > 0) meta += "  ·  " + FormatLastPlayed(sel.last_played);
        if (!sel.tags.empty()) meta += "  ·  " + sel.tags.front();
        Ellipsize(dl, GetFonts().regular, S(14), {ip.x, ip.y + S(40)}, U32(Color::Muted), meta, info_w, 1);
    }
    {
        FontScope f(GetFonts().medium, 12.5f);
        const std::string pos = Fmt("%d de %d", carousel_index_ + 1, n);
        dl->AddText({ip.x, ip.y + S(66)}, U32(Color::Dim), pos.c_str());
        // Barrita de avance.
        const float bw = S(160), bx = ip.x + ImGui::CalcTextSize(pos.c_str()).x + S(12), by = ip.y + S(74);
        dl->AddRectFilled({bx, by}, {bx + bw, by + S(4)}, U32(Color::Surface3), S(2));
        const float t = n > 1 ? carousel_pos_ / (n - 1) : 1.0f;
        dl->AddRectFilled({bx, by}, {bx + std::max(S(8), bw * t), by + S(4)}, U32(gs ? gs->color : Color::Accent), S(2));
    }
    // Botones.
    {
        FontScope f(GetFonts().semibold, 15);
        const float bw = S(150), bh = S(48);
        float x = origin.x + W - bw;
        const bool resume = HasAutoState(sel);
        ImGui::SetCursorScreenPos({x, ip.y + S(6)});
        if (Button(resume ? "Continuar##car" : "Jugar##car", ButtonKind::Primary, {bw, bh}, Icon::Play, true)) RequestStart(sel.path, resume);
        if (resume) {
            x -= S(130) + S(10);
            ImGui::SetCursorScreenPos({x, ip.y + S(6)});
            if (Button("Desde cero##car", ButtonKind::Secondary, {S(130), bh})) RequestStart(sel.path, false);
        }
        x -= bh + S(10);
        ImGui::SetCursorScreenPos({x, ip.y + S(6)});
        if (IconButton("##car_fav", sel.favorite ? Icon::StarFill : Icon::Star, bh, U32(sel.favorite ? Color::Gold : Color::Muted),
                       sel.favorite ? "Quitar de favoritos" : "Marcar como favorito", sel.favorite)) {
            library_.ToggleFavorite(sel.key);
            games_dirty_ = true;
        }
    }
    ImGui::SetCursorScreenPos({origin.x, ip.y + S(96)});
    ImGui::Dummy({W, 0});
}

// ---------------------------------------------------------------- preguntas al salir

void App::RequestQuit() {
    if (!settings_.confirm_exit_app || !qa_.empty()) {
        running_ = false;
        return;
    }
    // Segunda vez que se pide cerrar (otra X o Alt+F4) mientras la pregunta está pendiente: se cierra
    // de una vez. Así nunca hace falta el Administrador de tareas.
    // Ojo: una sola X manda dos avisos seguidos (cerrar ventana y salir); solo cuenta como «otra
    // vez» si pasó al menos medio segundo.
    const Uint64 now = SDL_GetTicks();
    if (confirm_ == 1) {
        if (now - quit_request_ms_ > 500) running_ = false;
        return;
    }
    quit_request_ms_ = now;
    confirm_ = 1;
    confirm_frames_ = 0;
    confirm_dont_ask_ = false;
    if (in_game_) audio_.SetPaused(true);
}

void App::RequestExitGame() {
    if (!settings_.confirm_exit_game) {
        confirm_ = 3; // salir ya (fuera del menú del juego)
        return;
    }
    confirm_ = 2;
    confirm_frames_ = 0;
    confirm_dont_ask_ = false;
}

void App::DrawConfirm() {
    if (confirm_ == 3) {
        confirm_ = 0;
        if (in_game_) StopGame();
        return;
    }
    if (confirm_ == 0) return;
    const bool app = confirm_ == 1;
    auto close = [&](bool accept) {
        if (confirm_dont_ask_) {
            (app ? settings_.confirm_exit_app : settings_.confirm_exit_game) = false;
            SaveConfig();
            Toast("Listo, no vuelvo a preguntar. Lo cambias en Ajustes » Apariencia.", GuideId::Samy, 5);
        }
        const int what = confirm_;
        confirm_ = 0;
        ImGui::CloseCurrentPopup();
        if (!accept) {
            if (in_game_) audio_.SetPaused(paused_ || menu_open_);
            return;
        }
        if (what == 1) running_ = false;
        else if (in_game_) StopGame();
    };
    ImGui::OpenPopup("##confirmar");
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float w = std::min(S(500), vp->Size.x - S(40));
    ImGui::SetNextWindowPos({vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(28), S(26)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(22));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, V4(Color::Bg));
    ImGui::PushStyleColor(ImGuiCol_Border, V4(app ? Color::Green : Color::Amber, 0.45f));
    const bool shown = ImGui::BeginPopupModal("##confirmar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
    // Si por algo la pregunta no logra mostrarse en 2 segundos, se hace lo pedido igual (cerrar con
    // guardado automático): la app nunca queda esperando una ventana que no se ve.
    if (!shown && ++confirm_frames_ > 120) {
        Log::Warn("La pregunta de salir no se pudo mostrar; se cierra igual");
        const int what = confirm_;
        confirm_ = 0;
        if (what == 1) running_ = false;
        else if (in_game_) StopGame();
    }
    if (shown) {
        confirm_frames_ = 0;
        const float inner = w - S(56);
        const GuideId gid = app ? GuideId::MechaRidds : GuideId::Samy;
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float av = S(64);
            if (settings_.guides && guides_[static_cast<int>(gid)])
                dl->AddImageRounded(guides_[static_cast<int>(gid)].Ref(), p, {p.x + av, p.y + av}, {0, 0}, {1, 1}, IM_COL32_WHITE, av * 0.5f);
            else
                dl->AddCircleFilled({p.x + av * 0.5f, p.y + av * 0.5f}, av * 0.5f, U32(Guide(gid).color, 0.3f));
            dl->AddCircle({p.x + av * 0.5f, p.y + av * 0.5f}, av * 0.5f + S(2), U32(Guide(gid).color, 0.7f), 0, S(2));
            ImGui::SetCursorScreenPos({p.x + av + S(18), p.y + S(4)});
            ImGui::BeginGroup();
            {
                FontScope f(GetFonts().bold, 20);
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner - av - S(18));
                ImGui::TextWrapped("%s", app ? "¿Cerrar Satronho Emulation?" : ("¿Salir de «" + current_.title + "»?").c_str());
                ImGui::PopTextWrapPos();
            }
            {
                std::string t;
                if (app && in_game_) t = settings_.autosave_exit ? "Estás jugando. Antes de cerrar guardo dónde quedaste." : "Estás jugando; lo que no hayas guardado se pierde.";
                else if (app) t = "¿Seguro? Tus juegos, partidas y ajustes quedan guardados.";
                else t = settings_.autosave_exit ? "Guardo dónde quedaste y con «Continuar» sigues justo ahí." : "Lo que no hayas guardado se pierde.";
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner - av - S(18));
                Muted(t.c_str(), 14);
                ImGui::PopTextWrapPos();
            }
            ImGui::EndGroup();
            ImGui::SetCursorScreenPos({p.x, std::max(ImGui::GetCursorScreenPos().y, p.y + av) + S(14)});
        }
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            Toggle("##no_preguntar", &confirm_dont_ask_);
            ImGui::SameLine(0, S(10));
            ImGui::SetCursorScreenPos({ImGui::GetCursorScreenPos().x, p.y + S(3)});
            FontScope f(GetFonts().medium, 13.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
            ImGui::TextUnformatted("No volver a preguntar");
            ImGui::PopStyleColor();
        }
        ImGui::Dummy({0, S(14)});
        const float bw = (inner - S(12)) * 0.5f;
        bool yes, no;
        {
            FontScope f(GetFonts().semibold, 15);
            no = Button(app ? "Quedarme##conf" : "Seguir jugando##conf", ButtonKind::Secondary, {bw, S(48)});
            ImGui::SameLine(0, S(12));
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            yes = Button(app ? "Cerrar##conf" : "Salir##conf", ButtonKind::Danger, {bw, S(48)}, Icon::Exit, true);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) no = true;
        if (yes) close(true);
        else if (no) close(false);
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------- novedades de Juana

void App::DrawWhatsNew() {
    const Json& builds = news_["builds"];
    if (!builds.IsArray() || builds.size() == 0) {
        show_whats_new_ = false;
        return;
    }
    const Json& b = builds[0];
    ImGui::OpenPopup("##lo_nuevo");
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float w = std::min(S(620), vp->Size.x - S(40));
    ImGui::SetNextWindowPos({vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(30), S(28)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(24));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, V4(Color::Bg));
    ImGui::PushStyleColor(ImGuiCol_Border, V4(Color::Pink, 0.45f));
    if (ImGui::BeginPopupModal("##lo_nuevo", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize)) {
        const float inner = w - S(60);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float av = S(84);
            // Destellos alrededor de Juana.
            const float t = static_cast<float>(ImGui::GetTime());
            for (int i = 0; i < 8; ++i) {
                const float ang = i * 0.785f + t * 0.6f;
                const float rr = av * 0.62f + std::sin(t * 2 + i) * S(3);
                dl->AddCircleFilled({p.x + av * 0.5f + std::cos(ang) * rr, p.y + av * 0.5f + std::sin(ang) * rr}, S(2.2f), U32(i % 2 ? Color::Gold : Color::Pink, 0.8f));
            }
            if (guides_[static_cast<int>(GuideId::Juana)])
                dl->AddImageRounded(guides_[static_cast<int>(GuideId::Juana)].Ref(), p, {p.x + av, p.y + av}, {0, 0}, {1, 1}, IM_COL32_WHITE, av * 0.5f);
            ImGui::SetCursorScreenPos({p.x + av + S(22), p.y + S(6)});
            ImGui::BeginGroup();
            {
                FontScope f(GetFonts().semibold, 12.5f);
                ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Pink));
                ImGui::TextUnformatted(build_ > 0 ? Fmt("BUILD %d INSTALADA", build_).c_str() : "LO NUEVO");
                ImGui::PopStyleColor();
            }
            {
                FontScope f(GetFonts().bold, 22);
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner - av - S(22));
                ImGui::TextWrapped("%s", b["titulo"].String().c_str());
                ImGui::PopTextWrapPos();
            }
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner - av - S(22));
            Muted("Hola, mijo. Soy Juana. Mira lo que trae esta versión:", 14);
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();
            ImGui::SetCursorScreenPos({p.x, std::max(ImGui::GetCursorScreenPos().y, p.y + av) + S(14)});
        }
        const float list_h = std::min(S(320), vp->Size.y * 0.45f);
        ImGui::BeginChild("##lo_nuevo_lista", {inner, list_h}, 0);
        for (const auto& c : b["cambios"].arr) {
            const std::string tipo = c["tipo"].String();
            const unsigned col = tipo == "nuevo" ? Color::Green : tipo == "mejora" ? Color::Accent : Color::Amber;
            const char* label = tipo == "nuevo" ? "Nuevo" : tipo == "mejora" ? "Mejora" : "Arreglo";
            const ImVec2 p = ImGui::GetCursorScreenPos();
            Chip(label, col);
            ImGui::SetCursorScreenPos({p.x + S(78), p.y - S(1)});
            FontScope f(GetFonts().regular, 14);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - S(8));
            ImGui::TextWrapped("%s", c["texto"].String().c_str());
            ImGui::PopTextWrapPos();
            ImGui::Dummy({0, S(2)});
        }
        ImGui::EndChild();
        ImGui::Dummy({0, S(12)});
        const float bw = (inner - S(12)) * 0.5f;
        {
        FontScope f(GetFonts().semibold, 15);
        if (Button("Ver todas las novedades", ButtonKind::Secondary, {bw, S(48)}, Icon::Bell, true)) {
            show_whats_new_ = false;
            page_ = Page::News;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, S(12));
        if (Button("¡A jugar!", ButtonKind::Primary, {bw, S(48)}, Icon::Play, true) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            show_whats_new_ = false;
            ImGui::CloseCurrentPopup();
        }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------- guías: consejos y conversación

int App::NextTip() {
    if (tips_.empty()) return 0;
    const int want = PickGuide(Rng());
    std::vector<int> pool;
    for (int i = 0; i < static_cast<int>(tips_.size()); ++i)
        if (static_cast<int>(tips_[i].guide) == want && i != tip_index_) pool.push_back(i);
    if (pool.empty())
        for (int i = 0; i < static_cast<int>(tips_.size()); ++i)
            if (i != tip_index_) pool.push_back(i);
    if (pool.empty()) return tip_index_;
    return pool[Rng() % pool.size()];
}

ChatContext App::BuildChatContext() const {
    ChatContext c;
    c.hw = &hw_;
    c.settings = &settings_;
    c.games = &games_;
    const Suggestion s = Suggest(hw_, settings_);
    c.verdicts = s.systems;
    c.tier = s.tier;
    for (const auto& p : input_.Gamepads()) c.pads.push_back(p.name + (p.family_name.empty() ? "" : " (" + p.family_name + ")"));
    for (int i = 0; i < static_cast<int>(Hotkey::Count); ++i) {
        const Hotkey h = static_cast<Hotkey>(i);
        std::string key = input_.hk_keys[i] != SDL_SCANCODE_UNKNOWN ? Input::KeyLabel(input_.hk_keys[i]) : std::string();
        c.hotkeys.emplace_back(HotkeyInfo(h).name, key);
    }
    for (const auto& sys : Systems()) {
        if (sys.bios.empty()) continue;
        if (!BiosFound(sys).empty()) continue;
        (sys.bios_required ? c.bios_missing : c.bios_optional).push_back(sys.full);
    }
    c.missing_cores = missing_cores_;
    c.build = build_;
    switch (updater_.GetState()) {
    case Updater::State::Available: c.update_text = Fmt("¡Ya salió la build %d! En Novedades la instalas con un clic.", updater_.LatestBuild()); break;
    case Updater::State::UpToDate: c.update_text = "Tienes la última build."; break;
    case Updater::State::Checking: c.update_text = "Estoy revisando si hay una build nueva."; break;
    default: c.update_text = updater_.Supported() ? "Toca «Buscar actualizaciones» y te digo si hay algo nuevo." : "Las actualizaciones automáticas son para Windows."; break;
    }
    c.gfx = hw_.renderer;
    c.vulkan = gfx_vulkan_available_;
    c.covers = static_cast<int>(covers_.Count());
    return c;
}

void App::AskGuides(const std::string& text) {
    if (text.empty()) return;
    const double now = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    if (chat_.empty()) {
        const ChatReply hello = ChatGreeting(BuildChatContext(), chat_guide_ >= 0 ? chat_guide_ : 0);
        chat_.push_back({hello.guide, hello.text, hello.buttons, now});
    }
    chat_.push_back({-1, text, {}, now});
    ChatReply r = AnswerQuestion(text, BuildChatContext(), chat_guide_);
    // El guía «escribe» un ratito antes de responder.
    const double typing = std::clamp(r.text.size() / 260.0, 0.45, 1.2);
    chat_.push_back({r.guide, r.text, r.buttons, now + typing});
    chat_scroll_ = true;
}

void App::DoChatAction(const ChatButton& b) {
    switch (b.action) {
    case ChatAction::Library: page_ = Page::Library; lib_system_.clear(); break;
    case ChatAction::System: page_ = Page::Library; lib_system_ = b.arg; break;
    case ChatAction::Controls: page_ = Page::Controls; break;
    case ChatAction::Hotkeys: page_ = Page::Controls; ctrl_tab_ = 3; break;
    case ChatAction::Suggested: page_ = Page::Suggested; break;
    case ChatAction::ApplySuggestion:
        ApplySuggestion();
        Toast("¡Listo! Dejé todo configurado para tu equipo.", GuideId::Nono, 4);
        break;
    case ChatAction::News: page_ = Page::News; break;
    case ChatAction::CheckUpdates:
        if (updater_.Supported() && build_ > 0) updater_.Check(build_);
        page_ = Page::News;
        break;
    case ChatAction::AddFolder: AddFolderDialog(); break;
    case ChatAction::SettingsVideo: page_ = Page::Settings; tab_ = SettingsTab::Video; break;
    case ChatAction::SettingsAudio: page_ = Page::Settings; tab_ = SettingsTab::Audio; break;
    case ChatAction::SettingsPerformance: page_ = Page::Settings; tab_ = SettingsTab::Performance; break;
    case ChatAction::SettingsCore:
        page_ = Page::Settings;
        tab_ = SettingsTab::Core;
        if (!b.arg.empty()) core_tab_system_ = b.arg;
        break;
    case ChatAction::SettingsLibrary: page_ = Page::Settings; tab_ = SettingsTab::Library; break;
    case ChatAction::SettingsAppearance: page_ = Page::Settings; tab_ = SettingsTab::Appearance; break;
    case ChatAction::SettingsGuides: page_ = Page::Settings; tab_ = SettingsTab::Guides; break;
    case ChatAction::DownloadCovers:
        settings_.covers = true;
        SaveConfig();
        covers_.QueueDownloads(library_.Snapshot());
        Toast("Buscando carátulas para tus juegos…", GuideId::Samy, 3);
        break;
    case ChatAction::PlayGame:
        for (const auto& g : games_)
            if (g.key == b.arg) RequestStart(g.path, HasAutoState(g));
        break;
    default: break;
    }
}

void App::DrawGuideChat() {
    if (games_dirty_) {
        games_ = library_.Snapshot();
        games_dirty_ = false;
    }
    Title("Pregúntale a los guías", "Escribe lo que quieras saber: responden con lo que saben de tu equipo, tus juegos y tus mandos");
    ImGui::Dummy({0, S(4)});
    const double now = ImGui::GetTime();
    if (chat_.empty()) {
        const ChatReply hello = ChatGreeting(BuildChatContext(), chat_guide_ >= 0 ? chat_guide_ : 0);
        chat_.push_back({hello.guide, hello.text, hello.buttons, now});
    }

    // A quién se le pregunta.
    {
        FontScope f(GetFonts().medium, 13.5f);
        auto chip = [&](int id, const char* name, unsigned color) {
            const float av = S(28);
            const ImVec2 ts = ImGui::CalcTextSize(name);
            const float w = ts.x + (id >= 0 ? av + S(26) : S(28)), h = S(40);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::PushID(id + 10);
            const bool click = ImGui::InvisibleButton("##guia", {w, h});
            const bool hov = ImGui::IsItemHovered();
            ImGui::PopID();
            const bool sel = chat_guide_ == id;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p, {p.x + w, p.y + h}, sel ? U32(color, 0.22f) : U32(Color::Surface2, hov ? 1.0f : 0.7f), h * 0.5f);
            if (sel) dl->AddRect(p, {p.x + w, p.y + h}, U32(color, 0.8f), h * 0.5f, S(1.5f));
            float x = p.x + S(14);
            if (id >= 0 && guides_[id]) {
                dl->AddImageRounded(guides_[id].Ref(), {p.x + S(6), p.y + (h - av) * 0.5f}, {p.x + S(6) + av, p.y + (h + av) * 0.5f}, {0, 0}, {1, 1}, IM_COL32_WHITE, av * 0.5f);
                x = p.x + av + S(14);
            }
            dl->AddText({x, p.y + (h - ts.y) * 0.5f}, U32(sel ? Color::Text : Color::Muted), name);
            if (click && chat_guide_ != id) {
                chat_guide_ = id;
                const ChatReply hello = ChatGreeting(BuildChatContext(), id >= 0 ? id : 0);
                chat_.push_back({hello.guide, hello.text, {}, now + 0.35});
                chat_scroll_ = true;
            }
            ImGui::SameLine(0, S(8));
        };
        chip(-1, "Al que más sepa", Color::Accent);
        for (int i = 0; i < static_cast<int>(GuideId::Count); ++i) chip(i, Guide(static_cast<GuideId>(i)).name, Guide(static_cast<GuideId>(i)).color);
        ImGui::NewLine();
    }
    ImGui::Dummy({0, S(6)});

    // Mensajes.
    const float input_h = S(56), sugg_h = S(44);
    const float list_h = std::max(S(200), ImGui::GetContentRegionAvail().y - input_h - sugg_h - S(70));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V4(Color::Surface, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(20), S(18)});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, S(20));
    ImGui::BeginChild("##chat", {0, list_h}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    const float full_w = ImGui::GetContentRegionAvail().x;
    const float max_w = std::min(full_w * 0.74f, S(620));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool still_typing = false;
    for (size_t i = 0; i < chat_.size(); ++i) {
        ChatMsg& m = chat_[i];
        if (m.guide >= 0 && now < m.born) {
            // «Escribiendo…»: el avatar con tres puntitos que saltan.
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float av = S(36);
            if (guides_[m.guide]) dl->AddImageRounded(guides_[m.guide].Ref(), p, {p.x + av, p.y + av}, {0, 0}, {1, 1}, IM_COL32_WHITE, av * 0.5f);
            const ImVec2 b0(p.x + av + S(10), p.y), b1(b0.x + S(70), b0.y + S(36));
            dl->AddRectFilled(b0, b1, U32(Color::Surface2), S(16));
            for (int k = 0; k < 3; ++k) {
                const float y = std::sin(static_cast<float>(now) * 9.0f - k * 0.9f) * S(3);
                dl->AddCircleFilled({b0.x + S(20) + k * S(15), b0.y + S(18) - std::max(0.0f, y)}, S(4), U32(Guide(static_cast<GuideId>(m.guide)).color, 0.9f));
            }
            ImGui::Dummy({full_w, av + S(10)});
            still_typing = true;
            break; // lo que viene después todavía no se muestra
        }
        const float appear = std::clamp(static_cast<float>((now - m.born) / 0.25), 0.0f, 1.0f);
        const float ease = EaseOut(appear);
        ImGui::PushID(static_cast<int>(i));
        if (m.guide < 0) {
            // Lo que escribió la persona: a la derecha, color del acento.
            FontScope f(GetFonts().medium, 14.5f);
            const ImVec2 ts = ImGui::CalcTextSize(m.text.c_str(), nullptr, false, max_w - S(32));
            const float bw = ts.x + S(32), bh = ts.y + S(22);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const ImVec2 b0(p.x + full_w - bw, p.y + (1 - ease) * S(8)), b1(b0.x + bw, b0.y + bh);
            dl->AddRectFilled(b0, b1, U32(Color::Accent, 0.9f * ease), S(18));
            dl->AddText(nullptr, 0.0f, {b0.x + S(16), b0.y + S(11)}, U32(LightTheme() ? 0xFFFFFF : 0x14101F, ease), m.text.c_str(), nullptr, max_w - S(32));
            ImGui::Dummy({full_w, bh + S(12)});
        } else {
            const GuideInfo& g = Guide(static_cast<GuideId>(m.guide));
            const float av = S(40);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float y0 = p.y + (1 - ease) * S(8);
            if (settings_.guides && guides_[m.guide])
                dl->AddImageRounded(guides_[m.guide].Ref(), {p.x, y0}, {p.x + av, y0 + av}, {0, 0}, {1, 1}, IM_COL32(255, 255, 255, static_cast<int>(255 * ease)), av * 0.5f);
            else
                dl->AddCircleFilled({p.x + av * 0.5f, y0 + av * 0.5f}, av * 0.4f, U32(g.color, ease));
            ImVec2 ts;
            {
                FontScope f(GetFonts().regular, 14.5f);
                ts = ImGui::CalcTextSize(m.text.c_str(), nullptr, false, max_w - S(36));
            }
            float nw;
            {
                FontScope f(GetFonts().bold, 13);
                nw = ImGui::CalcTextSize(g.name).x;
            }
            const float name_h = S(20);
            const float bh = ts.y + name_h + S(26);
            const float bw = std::min(max_w, std::max(ts.x, nw) + S(40));
            const ImVec2 b0(p.x + av + S(12), y0), b1(b0.x + bw, b0.y + bh);
            dl->AddRectFilled(b0, b1, U32(Color::Surface2, ease), S(18));
            dl->AddRectFilled({b0.x, b0.y + S(14)}, {b0.x + S(3), b1.y - S(14)}, U32(g.color, ease), S(2));
            {
                FontScope f(GetFonts().bold, 13);
                dl->AddText({b0.x + S(18), b0.y + S(11)}, U32(g.color, ease), g.name);
            }
            {
                FontScope f(GetFonts().regular, 14.5f);
                dl->AddText(nullptr, 0.0f, {b0.x + S(18), b0.y + S(11) + name_h}, U32(Color::Text, ease), m.text.c_str(), nullptr, max_w - S(36));
            }
            ImGui::Dummy({full_w, bh + S(8)});
            if (!m.buttons.empty()) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + av + S(12));
                FontScope f(GetFonts().medium, 13.5f);
                for (size_t k = 0; k < m.buttons.size(); ++k) {
                    if (k) ImGui::SameLine(0, S(8));
                    ImGui::PushID(static_cast<int>(k));
                    if (Button(m.buttons[k].label.c_str(), ButtonKind::Secondary, {0, S(36)}, Icon::ChevronRight, true)) DoChatAction(m.buttons[k]);
                    ImGui::PopID();
                }
                ImGui::Dummy({0, S(6)});
            }
            ImGui::Dummy({0, S(4)});
        }
        ImGui::PopID();
    }
    if (chat_scroll_ || still_typing) {
        ImGui::SetScrollHereY(1.0f);
        if (!still_typing) chat_scroll_ = false;
    }
    ImGui::EndChild();
    ImGui::Dummy({0, S(4)});

    // Preguntas sugeridas.
    {
        FontScope f(GetFonts().medium, 13);
        const auto sugg = SuggestedQuestions(chat_guide_ >= 0 ? chat_guide_ : 0);
        for (size_t i = 0; i < sugg.size(); ++i) {
            if (i) ImGui::SameLine(0, S(8));
            const ImVec2 ts = ImGui::CalcTextSize(sugg[i].c_str());
            if (ImGui::GetCursorScreenPos().x + ts.x + S(28) > ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - S(30)) break;
            ImGui::PushID(static_cast<int>(i) + 500);
            if (Button(sugg[i].c_str(), ButtonKind::Secondary, {0, S(34)})) AskGuides(sugg[i]);
            ImGui::PopID();
        }
    }
    ImGui::Dummy({0, S(4)});

    // Caja para escribir.
    {
        const float send_w = S(120);
        const float w = ImGui::GetContentRegionAvail().x - send_w - S(10);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {S(18), S(15)});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, S(16));
        ImGui::SetNextItemWidth(w);
        if (chat_focus_ || ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
            chat_focus_ = false;
        }
        const char* hint = chat_guide_ >= 0 ? "Escríbele tu pregunta…" : "Escribe tu pregunta: «¿cómo va el 64 en mi PC?»";
        const bool enter = ImGui::InputTextWithHint("##pregunta", hint, chat_input_, sizeof chat_input_, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleVar(2);
        ImGui::SameLine(0, S(10));
        FontScope f(GetFonts().semibold, 15);
        const bool send = Button("Enviar##chat", ButtonKind::Primary, {send_w, ImGui::GetFrameHeight()}, Icon::Send, true);
        if ((enter || send) && chat_input_[0]) {
            AskGuides(chat_input_);
            chat_input_[0] = 0;
            chat_focus_ = true;
        }
    }
}

} // namespace Satronho

namespace Satronho {

// ---------------------------------------------------------------- tarjeta gráfica en CD y 64

namespace {
// Factores que acepta cada núcleo, de menor a mayor.
int Floor(int want, std::initializer_list<int> allowed) {
    int best = *allowed.begin();
    for (int a : allowed)
        if (a <= want) best = a;
    return best;
}
} // namespace

int App::GpuApiFor(const SystemDef& sys) const {
    // 1 OpenGL, 2 Vulkan, 3 procesador.
    if (!gl_) return 3;
    int api = settings_.GpuApi(sys.id);
    if (api == 0 && hw_.software) return 3; // sin tarjeta gráfica de verdad
    if (api == 0) {
        // Automático: Vulkan en tarjetas NVIDIA y AMD (dedicadas), OpenGL en el resto (Intel,
        // integradas viejas): es lo más compatible.
        std::string g = hw_.gpu;
        for (auto& c : g) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const bool strong = g.find("nvidia") != std::string::npos || g.find("geforce") != std::string::npos ||
                            g.find("rtx") != std::string::npos || g.find("radeon") != std::string::npos || g.find("amd") != std::string::npos;
        api = strong && hw_render_.VulkanAvailable() ? 2 : 1;
        // La 64 con ParaLLEl-RDP pide una tarjeta con bastante memoria de video.
        if (api == 2 && sys.id == "n64" && hw_.vram_mb > 0 && hw_.vram_mb < 2000) api = 1;
    }
    if (api == 2 && !hw_render_.VulkanAvailable()) api = 1;
    return api;
}

// Valores de resolución: 1..16 = multiplicador de la imagen original; más de 16 = alto exacto en
// píxeles (solo 64 con OpenGL, que acepta tamaños exactos como 1440×1080 o 2880×2160).
std::vector<int> App::GpuResSteps(const SystemDef& sys, int api) const {
    if (sys.id == "cd") return api == 3 ? std::vector<int>{1, 2} : std::vector<int>{1, 2, 4, 8, 16};
    if (api == 2) return {1, 2, 4, 8};          // ParaLLEl-RDP
    if (api == 3) return {1};                    // Angrylion: siempre la original
    return {240, 480, 720, 960, 1080, 1200, 1440, 1680, 1920, 2160, 2400, 2640, 2880}; // GLideN64
}

int App::ResHeight(int k) { return k > 16 ? k : k * 240; }

std::string App::ResLabel(int k) const {
    const int h = ResHeight(k);
    const char* tag = h <= 240 ? "original" : h < 700 ? "" : h < 900 ? "HD" : h < 1300 ? "Full HD" : h < 1700 ? "2K" : h < 3000 ? "4K" : "8K";
    std::string s;
    if (k > 16) s = Fmt("%d × %d", h * 4 / 3, h);
    else s = Fmt("%d× · %d × %d", k, 320 * k, h);
    if (*tag) s += Fmt("  (%dp · %s)", h, tag);
    else s += Fmt("  (%dp)", h);
    // La que más se parece al alto de tu pantalla.
    if (hw_.display_h > 0 && std::abs(h - hw_.display_h) <= hw_.display_h / 12) s += "  · como tu pantalla";
    return s;
}

int App::GpuResFor(const SystemDef& sys, int api) const {
    int res = settings_.GpuRes(sys.id);
    const bool exact = sys.id == "n64" && api == 1;
    if (res > 0) {
        // Guardado de otra forma (multiplicador vs. alto exacto): se convierte.
        if (exact && res <= 16) return ResHeight(res);
        if (!exact && res > 16) return std::max(1, res / 240);
        return res;
    }
    if (api == 3) return sys.id == "cd" ? 1 : 1;
    // Automática: según el alto de la pantalla (240 líneas de la consola) y la tarjeta.
    const int lines = std::max(480, hw_.display_h > 0 ? hw_.display_h : 1080);
    res = std::clamp(lines / 240, 1, 8);
    std::string g = hw_.gpu;
    for (auto& c : g) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const bool integrated = g.find("intel") != std::string::npos || g.find("uhd") != std::string::npos || g.find("vega") != std::string::npos ||
                            (hw_.vram_mb > 0 && hw_.vram_mb < 1500);
    if (integrated) res = std::min(res, 2);
    else if (hw_.vram_mb > 0 && hw_.vram_mb < 4000) res = std::min(res, 3);
    else res = std::min(res, 4);
    res = std::max(1, res);
    if (exact) {
        // 64 con OpenGL: el tamaño exacto más cercano al alto de la pantalla (con el mismo tope).
        const int want = std::min(hw_.display_h > 0 ? hw_.display_h : 1080, res * 240 + (res >= 4 ? 480 : 0));
        int best = 240;
        for (int h : GpuResSteps(sys, api))
            if (h <= want) best = h;
        return best;
    }
    return res;
}

void App::ApplyGpuChoice(const SystemDef& sys) {
    const bool gpu_sys = sys.id == "cd" || sys.id == "n64";
    if (!gpu_sys) {
        hw_render_.SetWanted(gl_ ? RETRO_HW_CONTEXT_OPENGL_CORE : RETRO_HW_CONTEXT_NONE);
        return;
    }
    const int api = GpuApiFor(sys);
    const int res = GpuResFor(sys, api);
    hw_render_.SetWanted(api == 1 ? RETRO_HW_CONTEXT_OPENGL_CORE : api == 2 ? RETRO_HW_CONTEXT_VULKAN : RETRO_HW_CONTEXT_NONE);
    const char* api_name = api == 1 ? "OpenGL" : api == 2 ? "Vulkan" : "procesador";
    if (sys.id == "cd") {
        core_.SetOption("beetle_psx_hw_renderer", api == 1 ? "hardware_gl" : api == 2 ? "hardware_vk" : "software");
        // Con el procesador, más de 2× va lento: se limita.
        const int k = Floor(api == 3 ? std::min(res, 2) : res, {1, 2, 4, 8, 16});
        core_.SetOption("beetle_psx_hw_internal_resolution", k == 1 ? "1x(native)" : std::to_string(k) + "x");
        Log::Info("CD: %s a %d×", api_name, k);
    } else {
        if (api == 1) {
            core_.SetOption("mupen64plus-rdp-plugin", "gliden64");
            core_.SetOption("mupen64plus-rsp-plugin", "hle");
            if (res > 16) {
                // Tamaño exacto: factor 0 y el tamaño en las listas 4:3 y 16:9 del núcleo.
                core_.SetOption("mupen64plus-EnableNativeResFactor", "0");
                core_.SetOption("mupen64plus-43screensize", Fmt("%dx%d", res * 4 / 3, res));
                core_.SetOption("mupen64plus-169screensize", Fmt("%dx%d", (res * 16 / 9 + 1) / 2 * 2, res));
                Log::Info("64: GLideN64 (OpenGL) a %dx%d", res * 4 / 3, res);
            } else {
                core_.SetOption("mupen64plus-EnableNativeResFactor", std::to_string(std::clamp(res, 1, 8)));
                Log::Info("64: GLideN64 (OpenGL) a %d×", std::clamp(res, 1, 8));
            }
        } else if (api == 2) {
            const int k = Floor(res, {1, 2, 4, 8});
            core_.SetOption("mupen64plus-rdp-plugin", "parallel");
            core_.SetOption("mupen64plus-rsp-plugin", "parallel");
            core_.SetOption("mupen64plus-parallel-rdp-upscaling", std::to_string(k) + "x");
            Log::Info("64: ParaLLEl-RDP (Vulkan) a %d×", k);
        } else {
            core_.SetOption("mupen64plus-rdp-plugin", "angrylion");
            core_.SetOption("mupen64plus-rsp-plugin", "parallel");
            Log::Info("64: Angrylion (procesador)");
        }
    }
    // Lo elegido en Ajustes » Gráficos (antialiasing, filtros, pantalla ancha…).
    if (auto it = settings_.gfx.find(sys.id); it != settings_.gfx.end())
        for (const auto& [k, v] : it->second) core_.SetOption(k, v);
}

} // namespace Satronho

namespace Satronho {

namespace {
// Nombre de un valor de opción del núcleo, en español cuando es de los comunes.
std::string GfxValueLabel(const std::string& v, const std::string& label) {
    std::string l = label.empty() ? v : label;
    std::string low = l;
    for (auto& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::map<std::string, std::string> names{
        {"enabled", "Activado"}, {"disabled", "Desactivado"}, {"on", "Activado"}, {"off", "Desactivado"}, {"true", "Sí"}, {"false", "No"},
        {"auto", "Automático"}, {"none", "Ninguno"}, {"nearest", "Píxel exacto"}, {"bilinear", "Bilineal (suave)"},
        {"3-point", "3 puntos (como la consola)"}, {"internal resolution", "Resolución interna"}, {"1x", "Apagado (1×)"},
        {"memory card 1", "Memory Card 1"}, {"16 bpp (native)", "16 bits (original)"}, {"32 bpp", "32 bits"},
        {"1x (native)", "Original (1×)"}, {"internal resolution only", "Solo en la resolución interna"}};
    auto it = names.find(low);
    return it != names.end() ? it->second : l;
}
} // namespace

void App::DrawGpuSettings(bool& changed) {
    // Motor de la ventana: OpenGL (el de Satronho) o el clásico de SDL.
    std::vector<std::string> names{"Automático (OpenGL)", "OpenGL"};
    std::vector<std::string> values{"auto", "opengl"};
    for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
        const std::string d = SDL_GetRenderDriver(i);
        if (d == "software" || d == "gpu" || d == "opengl" || d == "opengles2") continue;
        std::string n = d == "direct3d11" ? "Direct3D 11" : d == "direct3d12" ? "Direct3D 12" : d == "direct3d" ? "Direct3D 9" : d == "vulkan" ? "Vulkan" : d;
        names.push_back("Clásico · " + n);
        values.push_back(d);
    }
    int cur = 0;
    for (size_t i = 0; i < values.size(); ++i)
        if (values[i] == settings_.renderer) cur = static_cast<int>(i);
    const std::string now = gl_ ? hw_.renderer : std::string("clásico (") + (renderer_ ? SDL_GetRendererName(renderer_) : "?") + ")";
    RowBegin("Motor gráfico", Fmt("OpenGL dibuja todo con la tarjeta gráfica y deja que CD y 64 usen OpenGL o Vulkan. El clásico es el de "
                                  "respaldo. Se aplica al volver a abrir la app. Ahora: %s.", now.c_str()).c_str());
    if (ComboStyled("##motor", &cur, names, ImGui::GetContentRegionAvail().x)) {
        settings_.renderer = values[cur];
        changed = true;
        Toast("El motor gráfico cambia la próxima vez que abras Satronho Emulation.", GuideId::Troya, 4);
    }
    RowEnd();

    // CD y 64: tarjeta, motor, resolución y las opciones de gráficos de cada núcleo (como la
    // pestaña Graphics de Satronho NX).
    ImGui::Dummy({0, S(8)});
    Heading("Tarjeta gráfica", 16);
    {
        RowBegin("Tarjeta", "La que usan CD y 64 para dibujar.");
        FontScope f(GetFonts().semibold, 14);
        ImGui::TextUnformatted(hw_.gpu.empty() ? "?" : hw_.gpu.c_str());
        RowEnd();
        RowBegin("Vulkan", nullptr);
        Muted(hw_render_.VulkanAvailable() ? ("Disponible · " + hw_render_.VulkanGpu()).c_str()
              : hw_render_.VulkanProbed() ? "Este equipo no tiene Vulkan: se usa OpenGL." : "Revisando…", 14);
        RowEnd();
    }
    if (!gl_) {
        Muted("Con el motor clásico, CD y 64 dibujan con el procesador. Elige OpenGL arriba para usar la tarjeta gráfica.", 13.5f);
        return;
    }
    struct GfxOpt {
        const char* key;
        const char* label;
        const char* help;
    };
    static const GfxOpt kCd[] = {
        {"beetle_psx_hw_msaa", "Antialiasing (MSAA)", "Bordes más suaves en los polígonos. Pide más a la tarjeta."},
        {"beetle_psx_hw_super_sampling", "Supermuestreo", "Dibuja grande y lo achica a la resolución original: sin dientes de sierra."},
        {"beetle_psx_hw_filter", "Filtrado de texturas", "Cómo se suavizan las texturas al agrandarlas."},
        {"beetle_psx_hw_pgxp_mode", "Geometría precisa (PGXP)", "Quita el temblor de los polígonos típico de la PS1."},
        {"beetle_psx_hw_pgxp_texture", "Texturas sin deformar (PGXP)", "Corrige la perspectiva de las texturas. Necesita PGXP."},
        {"beetle_psx_hw_widescreen_hack", "Pantalla ancha (16:9)", "Abre la vista en los juegos 3D. En algunos se ven cosas de más."},
        {"beetle_psx_hw_dither_mode", "Tramado", "El patrón de puntitos de la PS1. Desactivado se ve más limpio."},
        {"beetle_psx_hw_depth", "Profundidad de color", "32 bits quita las bandas de color."},
    };
    static const GfxOpt kN64[] = {
        {"mupen64plus-aspect", "Relación de aspecto", "4:3 como la consola, o 16:9 para pantallas anchas."},
        {"mupen64plus-MultiSampling", "Antialiasing (MSAA)", "Bordes más suaves (OpenGL)."},
        {"mupen64plus-FXAA", "FXAA", "Suaviza bordes con muy poco costo (OpenGL)."},
        {"mupen64plus-txFilterMode", "Filtro de texturas", "Suaviza o afila las texturas (OpenGL)."},
        {"mupen64plus-txEnhancementMode", "Mejora de texturas", "Agranda las texturas con un filtro (OpenGL)."},
        {"mupen64plus-BilinearMode", "Filtro bilineal", "«3point» es como la consola real; «standard» es más suave."},
        {"mupen64plus-parallel-rdp-downscaling", "Supermuestreo (Vulkan)", "Dibuja grande y lo achica: imagen más limpia."},
    };
    for (const char* id : {"cd", "n64"}) {
        const SystemDef* sys = SystemById(id);
        if (!sys) continue;
        ImGui::PushID(id);
        ImGui::Dummy({0, S(14)});
        // Encabezado de la consola, con su control en miniatura.
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p, {p.x + ImGui::GetContentRegionAvail().x, p.y + S(46)}, U32(sys->color, 0.10f), S(12));
            DrawControllerMini(dl, sys->layout, {p.x + S(28), p.y + S(23)}, S(36), 1.0f);
            FontScope f(GetFonts().bold, 17);
            dl->AddText({p.x + S(58), p.y + (S(46) - ImGui::GetFontSize()) * 0.5f}, U32(Color::Text), (sys->full + "  ·  " + sys->hint).c_str());
            ImGui::Dummy({0, S(52)});
        }
        const int api_now = GpuApiFor(*sys);
        const int res_now = GpuResFor(*sys, api_now);
        const char* api_names[] = {"", "OpenGL", "Vulkan", "el procesador"};
        std::string help;
        if (sys->id == "cd") help = api_now == 3 ? "Beetle PSX dibujando con el procesador." : std::string("Beetle PSX HW con ") + api_names[api_now] + ".";
        else help = api_now == 1 ? "GLideN64 con OpenGL: acepta tamaños exactos." : api_now == 2 ? "ParaLLEl-RDP con Vulkan: el más fiel a la consola." : "Angrylion con el procesador: fiel, pero pesado.";
        RowBegin("API", help.c_str());
        int api = settings_.GpuApi(sys->id);
        if (Segmented("##api", &api, {"Automática", "OpenGL", "Vulkan", "Procesador"}, ImGui::GetContentRegionAvail().x)) {
            if (api == 2 && !hw_render_.VulkanAvailable()) Toast("Este equipo no tiene Vulkan; se usará OpenGL.", GuideId::Troya, 4);
            settings_.gpu_api[sys->id] = api;
            changed = true;
            if (in_game_ && sys_ == sys) Toast("Se aplica la próxima vez que abras el juego.", GuideId::Troya, 3);
        }
        RowEnd();
        RowBegin("Resolución", hw_.display_h > 0 ? Fmt("Tu pantalla tiene %d píxeles de alto.", hw_.display_h).c_str() : nullptr);
        {
            const std::vector<int> ks = GpuResSteps(*sys, api_now);
            std::vector<std::string> res_names{"Automática (ahora " + ResLabel(res_now) + ")"};
            int sel = 0;
            const int stored = settings_.GpuRes(sys->id);
            for (size_t i = 0; i < ks.size(); ++i) {
                res_names.push_back(ResLabel(ks[i]));
                if (stored > 0 && ResHeight(ks[i]) <= ResHeight(stored)) sel = static_cast<int>(i) + 1;
            }
            if (ComboStyled("##res", &sel, res_names, ImGui::GetContentRegionAvail().x)) {
                settings_.gpu_res[sys->id] = sel == 0 ? 0 : ks[sel - 1];
                changed = true;
                if (in_game_ && sys_ == sys) Toast("Se aplica la próxima vez que abras el juego.", GuideId::Troya, 3);
            }
        }
        RowEnd();
        // Opciones del núcleo: se leen de él mismo (valores reales) y se guardan por consola.
        const std::vector<CoreOption>* defs = nullptr;
        if (core_.CoreLoaded() && core_file_ == CoreFile(*sys)) defs = &core_.Options();
        else if (auto it = core_cache_.find(CoreFile(*sys)); it != core_cache_.end()) defs = &it->second.options;
        else if (!in_game_ && gfx_probe_tried_.insert(sys->id).second) {
            std::string err;
            if (EnsureCore(*sys, &err)) {
                if (auto it2 = core_cache_.find(CoreFile(*sys)); it2 != core_cache_.end()) defs = &it2->second.options;
            }
        }
        if (!defs) {
            Muted("Las opciones de gráficos aparecen cuando el núcleo de esta consola está instalado.", 13);
        } else {
            const auto& list = sys->id == "cd" ? std::vector<GfxOpt>(std::begin(kCd), std::end(kCd)) : std::vector<GfxOpt>(std::begin(kN64), std::end(kN64));
            for (const auto& g : list) {
                auto def = std::find_if(defs->begin(), defs->end(), [&](const CoreOption& o) { return o.key == g.key; });
                if (def == defs->end() || def->values.empty()) continue;
                std::string cur = def->current.empty() ? def->def : def->current;
                if (auto it = settings_.gfx[sys->id].find(g.key); it != settings_.gfx[sys->id].end()) cur = it->second;
                std::vector<std::string> labels;
                int sel = 0;
                for (size_t i = 0; i < def->values.size(); ++i) {
                    labels.push_back(GfxValueLabel(def->values[i].value, def->values[i].label));
                    if (def->values[i].value == cur) sel = static_cast<int>(i);
                }
                RowBegin(g.label, g.help);
                ImGui::PushID(g.key);
                if (ComboStyled("##o", &sel, labels, ImGui::GetContentRegionAvail().x)) {
                    settings_.gfx[sys->id][g.key] = def->values[sel].value;
                    if (core_.CoreLoaded() && core_file_ == CoreFile(*sys)) core_.SetOption(g.key, def->values[sel].value);
                    changed = true;
                }
                ImGui::PopID();
                RowEnd();
            }
        }
        ImGui::PopID();
    }
}

} // namespace Satronho

#include "util/archive.h"

namespace Satronho {

// ---------------------------------------------------------------- juegos en .zip y .7z

namespace {
fs::path ArchiveCacheDir() { return Paths::User() / "cache" / "descomprimidos"; }
constexpr uintmax_t kArchiveCacheMax = 4ull * 1024 * 1024 * 1024; // 4 GB
} // namespace

bool App::PrepareArchived(const Game& g, fs::path& load_path, std::vector<uint8_t>& mem) {
    const SystemDef* sys = SystemById(g.system);
    const bool full_path = core_.SystemInfo().need_fullpath || (sys && sys->disc);
    // El nombre del juego (partidas y estados) sale del archivo comprimido, con la extensión real.
    fs::path virt = g.path;
    virt.replace_extension(Paths::FromUtf8("." + g.ext));
    if (!full_path) {
        if (!Archive::ExtractToMemory(g.path, g.entry, mem) || mem.empty()) {
            Toast("No pude descomprimir «" + g.title + "». ¿El archivo está completo?", GuideId::Samy, 6);
            return false;
        }
        load_path = virt;
        Log::Info("Juego sacado del archivo comprimido a la memoria: %s (%zu KB)", g.entry.c_str(), mem.size() / 1024);
        return true;
    }
    // De disco: se saca todo una vez a la caché y se reutiliza.
    const fs::path dir = ArchiveCacheDir() / g.key;
    const fs::path done = dir / ".listo";
    std::error_code ec;
    if (!fs::exists(done, ec)) {
        fs::remove_all(dir, ec);
        TrimArchiveCache();
        Log::Info("Descomprimiendo %s…", Paths::ToUtf8(g.path).c_str());
        if (!Archive::ExtractAll(g.path, dir)) {
            fs::remove_all(dir, ec);
            Toast("No pude descomprimir «" + g.title + "». Revisa que haya espacio en el disco.", GuideId::Samy, 6);
            return false;
        }
        std::ofstream(done) << Paths::ToUtf8(g.path) << '\n';
    } else {
        // Se marca como usado (la caché suelta primero lo que hace más tiempo no se juega).
        fs::last_write_time(done, fs::file_time_type::clock::now(), ec);
    }
    load_path = dir / Paths::FromUtf8(g.entry);
    return true;
}

void App::TrimArchiveCache() {
    std::error_code ec;
    const fs::path root = ArchiveCacheDir();
    if (!fs::exists(root, ec)) return;
    struct Item {
        fs::path dir;
        fs::file_time_type used;
        uintmax_t size;
    };
    std::vector<Item> items;
    uintmax_t total = 0;
    for (const auto& d : fs::directory_iterator(root, ec)) {
        if (!d.is_directory(ec)) continue;
        uintmax_t size = 0;
        for (fs::recursive_directory_iterator it(d.path(), ec), end; it != end && !ec; it.increment(ec))
            if (it->is_regular_file(ec)) size += it->file_size(ec);
        auto used = fs::last_write_time(d.path() / ".listo", ec);
        if (ec) used = fs::file_time_type::min();
        items.push_back({d.path(), used, size});
        total += size;
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.used < b.used; });
    for (const auto& it : items) {
        if (total <= kArchiveCacheMax) break;
        fs::remove_all(it.dir, ec);
        total -= it.size;
        Log::Info("Caché de descomprimidos: se soltó %s", Paths::ToUtf8(it.dir.filename()).c_str());
    }
}

void App::DrawArchivePicker() {
    if (!archive_pick_.Archived()) return;
    ImGui::OpenPopup("##elegir_juego");
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float w = std::min(S(560), vp->Size.x - S(40));
    ImGui::SetNextWindowPos({vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(26), S(24)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(22));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, V4(Color::Bg));
    ImGui::PushStyleColor(ImGuiCol_Border, V4(Color::Amber, 0.45f));
    if (ImGui::BeginPopupModal("##elegir_juego", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize)) {
        const float inner = w - S(52);
        GuideSays(GuideId::Samy, ("«" + archive_pick_.title + "» trae varios juegos adentro. ¿Cuál abrimos?").c_str(), inner);
        ImGui::Dummy({0, S(10)});
        ImGui::BeginChild("##lista_zip", {inner, std::min(S(330), S(62) * archive_pick_.choices.size() + S(6))});
        std::string chosen;
        for (const auto& c : archive_pick_.choices) {
            const std::string base = c.substr(c.find_last_of("/\\") == std::string::npos ? 0 : c.find_last_of("/\\") + 1);
            const Game g = Library::MakeGameEntry(archive_pick_.path, c);
            const SystemDef* sys = SystemById(g.system);
            ImGui::PushID(c.c_str());
            const ImVec2 p = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##op", {inner - S(12), S(46)})) chosen = c;
            const bool hov = ImGui::IsItemHovered() || ImGui::IsItemFocused();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p, {p.x + inner - S(12), p.y + S(46)}, U32(hov ? Color::Surface3 : Color::Surface2), S(12));
            if (sys) DrawControllerMini(dl, sys->layout, {p.x + S(26), p.y + S(23)}, sys->handheld ? S(22) : S(30), 1.0f);
            Ellipsize(dl, GetFonts().medium, S(14), {p.x + S(52), p.y + S(8)}, U32(Color::Text), base, inner - S(80), 1);
            {
                FontScope f(GetFonts().regular, 12);
                dl->AddText({p.x + S(52), p.y + S(27)}, U32(sys ? sys->color : Color::Muted), sys ? sys->full.c_str() : "");
            }
            ImGui::PopID();
            ImGui::Dummy({0, S(2)});
        }
        ImGui::EndChild();
        ImGui::Dummy({0, S(8)});
        const bool cancel = Button("Cancelar##zip", ButtonKind::Secondary, {inner, S(44)}) || ImGui::IsKeyPressed(ImGuiKey_Escape);
        if (!chosen.empty()) {
            start_entry_ = chosen;
            RequestStart(archive_pick_.path);
            archive_pick_ = Game{};
            ImGui::CloseCurrentPopup();
        } else if (cancel) {
            archive_pick_ = Game{};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

} // namespace Satronho

namespace Satronho {

// ---------------------------------------------------------------- fondo con la carátula

void App::DrawBackdrop(ImVec2 p0, ImVec2 p1) {
    hover_key_ = hover_key_next_;
    hover_key_next_.clear();
    std::string want = !hover_key_.empty() ? hover_key_ : !selected_key_.empty() ? selected_key_ : carousel_key_;
    if (want.empty()) {
        // Sin nada elegido: el último que se jugó.
        long long best = 0;
        for (const auto& g : games_)
            if (g.last_played > best && !g.hidden) best = g.last_played, want = g.key;
    }
    if (want != backdrop_key_) {
        backdrop_prev_ = backdrop_key_;
        backdrop_key_ = want;
        backdrop_t_ = ImGui::GetTime();
    }
    const float t = std::clamp(static_cast<float>((ImGui::GetTime() - backdrop_t_) / 0.45), 0.0f, 1.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto draw = [&](const std::string& key, float alpha) {
        if (key.empty() || alpha <= 0.01f) return;
        const Game* g = nullptr;
        for (const auto& x : games_)
            if (x.key == key) g = &x;
        if (!g) return;
        const Ui::Image* img = covers_.GetBlur(*g);
        if (!img) return;
        // Llenando el área sin deformar, un poco agrandada (se mueve lento hacia arriba).
        const float W = p1.x - p0.x, H = p1.y - p0.y;
        const float ia = static_cast<float>(img->w) / std::max(1, img->h);
        float w = W * 1.12f, h = w / ia;
        if (h < H * 1.12f) {
            h = H * 1.12f;
            w = h * ia;
        }
        const float drift = std::sin(static_cast<float>(ImGui::GetTime()) * 0.15f) * H * 0.02f;
        const ImVec2 a(p0.x + (W - w) * 0.5f, p0.y + (H - h) * 0.5f + drift);
        dl->AddImage(img->Ref(), a, {a.x + w, a.y + h}, {0, 0}, {1, 1}, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha * (LightTheme() ? 0.20f : 0.30f))));
    };
    draw(backdrop_prev_, 1.0f - t);
    draw(backdrop_key_, t);
    // Se funde con el fondo hacia abajo para que el texto se lea bien.
    const float mid = p0.y + (p1.y - p0.y) * 0.45f;
    dl->AddRectFilledMultiColor({p0.x, mid}, p1, U32(Color::Bg, 0.0f), U32(Color::Bg, 0.0f), U32(Color::Bg, 0.92f), U32(Color::Bg, 0.92f));
}

// ---------------------------------------------------------------- presentación al abrir

void App::DrawSplash() {
    if (splash_done_) return;
    const double now = ImGui::GetTime();
    if (splash_t0_ < 0) splash_t0_ = now;
    const float t = static_cast<float>(now - splash_t0_);
    const float total = 2.3f;
    // Se salta con un clic, una tecla o un botón del mando.
    if (t > 0.3f && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                     ImGui::IsKeyPressed(ImGuiKey_Space) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown)))
        splash_t0_ = now - total + 0.35;
    if (t >= total) {
        splash_done_ = true;
        return;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float out = std::clamp((total - t) / 0.45f, 0.0f, 1.0f); // fundido de salida
    const ImVec2 c(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.45f);
    dl->AddRectFilled(vp->Pos, {vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y}, U32(Color::Bg, out));
    // Estrellas que titilan.
    for (int i = 0; i < 70; ++i) {
        const float sx = std::fmod(i * 97.13f, 100.0f) / 100.0f, sy = std::fmod(i * 57.71f + 13, 100.0f) / 100.0f;
        const float tw = 0.5f + 0.5f * std::sin(t * (1.5f + (i % 5) * 0.4f) + i);
        dl->AddCircleFilled({vp->Pos.x + sx * vp->Size.x, vp->Pos.y + sy * vp->Size.y}, S(0.8f + (i % 3) * 0.5f), U32(0xFFFFFF, 0.35f * tw * out));
    }
    // El logo entra con un rebote suave.
    const float k = std::clamp(t / 0.7f, 0.0f, 1.0f);
    const float spring = 1.0f - std::exp(-6.0f * k) * std::cos(9.0f * k);
    const float size = std::min(vp->Size.x, vp->Size.y) * 0.28f * spring;
    // Anillo dorado que gira.
    const float ring = size * 0.95f;
    dl->PathArcTo(c, ring, t * 1.6f, t * 1.6f + 4.4f, 48);
    dl->PathStroke(U32(Color::Gold, 0.55f * out), S(2.5f), 0);
    if (logo_) dl->AddImageRounded(logo_.Ref(), {c.x - size * 0.5f, c.y - size * 0.5f}, {c.x + size * 0.5f, c.y + size * 0.5f}, {0, 0}, {1, 1},
                                   IM_COL32(255, 255, 255, static_cast<int>(255 * out)), size * 0.22f);
    // Los cinco guías aparecen uno por uno y orbitan alrededor.
    for (int i = 0; i < static_cast<int>(GuideId::Count); ++i) {
        const float appear = std::clamp((t - 0.45f - i * 0.12f) / 0.35f, 0.0f, 1.0f);
        if (appear <= 0 || !guides_[i]) continue;
        const float ang = -kPi() * 0.5f + i * (2 * kPi() / 5) + t * 0.5f;
        const float av = size * 0.30f * (0.6f + 0.4f * appear);
        const ImVec2 gc(c.x + std::cos(ang) * ring, c.y + std::sin(ang) * ring);
        const GuideInfo& gi = Guide(static_cast<GuideId>(i));
        dl->AddCircleFilled(gc, av * 0.5f + S(3), U32(Color::Bg, appear * out));
        dl->AddCircle(gc, av * 0.5f + S(3), U32(gi.color, appear * out), 0, S(2));
        dl->AddImageRounded(guides_[i].Ref(), {gc.x - av * 0.5f, gc.y - av * 0.5f}, {gc.x + av * 0.5f, gc.y + av * 0.5f}, {0, 0}, {1, 1},
                            IM_COL32(255, 255, 255, static_cast<int>(255 * appear * out)), av * 0.5f);
    }
    // Nombre y crédito.
    const float tx = std::clamp((t - 0.55f) / 0.5f, 0.0f, 1.0f);
    {
        const char* name = "Satronho Emulation";
        const float fs = std::max(S(28), size * 0.2f);
        const ImVec2 ts = GetFonts().bold->CalcTextSizeA(fs, FLT_MAX, 0, name);
        dl->AddText(GetFonts().bold, fs, {c.x - ts.x * 0.5f, c.y + ring + S(34) + (1 - tx) * S(14)}, U32(Color::Text, tx * out), name);
        const char* by = "por Ridds";
        const ImVec2 bs = GetFonts().semibold->CalcTextSizeA(S(15), FLT_MAX, 0, by);
        dl->AddText(GetFonts().semibold, S(15), {c.x - bs.x * 0.5f, c.y + ring + S(34) + ts.y + S(6) + (1 - tx) * S(14)}, U32(Color::Accent, tx * out), by);
    }
}

} // namespace Satronho

#include "util/chd.h"

namespace Satronho {

// ---------------------------------------------------------------- discos a .chd (opcional)

void App::StartChdJob(const std::vector<fs::path>& cues, bool delete_originals) {
    if (chd_running_ || cues.empty()) return;
    if (chd_thread_.joinable()) chd_thread_.join();
    chd_running_ = true;
    chd_cancel_ = false;
    chd_total_ = static_cast<int>(cues.size());
    chd_index_ = 0;
    chd_thread_ = std::thread([this, cues, delete_originals] {
        uint64_t before = 0, after = 0;
        int ok = 0;
        std::string last_error;
        for (size_t i = 0; i < cues.size() && !chd_cancel_; ++i) {
            chd_index_ = static_cast<int>(i);
            {
                std::lock_guard lock(chd_mutex_);
                chd_current_ = Paths::ToUtf8(cues[i].stem());
            }
            fs::path out = cues[i];
            out.replace_extension(".chd");
            const uint64_t size = Chd::DiscBytes(cues[i]);
            std::string err;
            if (Chd::FromCue(cues[i], out, &err, [this](float p) {
                    chd_progress_ = p;
                    return !chd_cancel_.load();
                })) {
                std::error_code ec;
                before += size;
                after += fs::file_size(out, ec);
                ++ok;
                if (delete_originals) {
                    // Se borran el .cue y sus pistas solo si el .chd quedó bien.
                    std::ifstream in(cues[i]);
                    std::string line;
                    std::vector<fs::path> parts;
                    while (std::getline(in, line)) {
                        const size_t a = line.find('"'), b = line.rfind('"');
                        if (line.find("FILE") != std::string::npos && a != std::string::npos && b > a)
                            parts.push_back(cues[i].parent_path() / Paths::FromUtf8(line.substr(a + 1, b - a - 1)));
                    }
                    in.close();
                    for (const auto& p : parts) fs::remove(p, ec);
                    fs::remove(cues[i], ec);
                }
            } else {
                last_error = err;
                Log::Warn("CHD: %s: %s", Paths::ToUtf8(cues[i]).c_str(), err.c_str());
            }
        }
        std::lock_guard lock(chd_mutex_);
        if (ok > 0)
            chd_result_ = Fmt("Listo: %d disco%s en .chd. Ahorraste %.0f MB (%.0f %%).", ok, ok == 1 ? "" : "s", (before - after) / 1048576.0,
                              before ? 100.0 * (before - after) / before : 0.0);
        else
            chd_result_ = last_error.empty() ? "No se convirtió ningún disco." : "No se pudo convertir: " + last_error;
        chd_running_ = false;
    });
}

void App::DrawChdPanel() {
    if (!chd_result_.empty() && !chd_running_) {
        std::lock_guard lock(chd_mutex_);
        Toast(chd_result_, GuideId::Nono, 7);
        chd_result_.clear();
        if (chd_thread_.joinable()) chd_thread_.join();
        Rescan();
    }
    // Discos .cue de CD y Anillo que todavía no tienen su .chd.
    std::vector<const Game*> discs;
    uint64_t total = 0;
    for (const auto& g : games_) {
        if (g.ext != "cue" || g.Archived() || (g.system != "cd" && g.system != "anillo")) continue;
        discs.push_back(&g);
    }
    RowBegin("Discos a .chd (opcional)",
             "El .chd es el mismo disco comprimido: ocupa menos y abre igual. Tus originales se quedan, salvo que marques borrarlos.");
    if (chd_running_) {
        std::string cur;
        {
            std::lock_guard lock(chd_mutex_);
            cur = chd_current_;
        }
        Muted(Fmt("Convirtiendo %d de %d: %s", chd_index_.load() + 1, chd_total_.load(), cur.c_str()).c_str(), 13);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, {p.x + w, p.y + S(8)}, U32(Color::Surface3), S(4));
        dl->AddRectFilled(p, {p.x + w * chd_progress_.load(), p.y + S(8)}, U32(Color::Teal), S(4));
        ImGui::Dummy({w, S(14)});
        if (Button("Detener##chd", ButtonKind::Ghost, {0, 0})) chd_cancel_ = true;
    } else if (discs.empty()) {
        Muted("No hay discos .cue para convertir.", 13);
    } else {
        // El tamaño se calcula una vez (leer cada .cue en cada cuadro sería un desperdicio).
        if (chd_cache_count_ != discs.size()) {
            chd_cache_count_ = discs.size();
            chd_cache_bytes_ = 0;
            for (const Game* g : discs) chd_cache_bytes_ += Chd::DiscBytes(g->path);
        }
        total = chd_cache_bytes_;
        Muted(Fmt("%zu disco%s (%.0f MB).", discs.size(), discs.size() == 1 ? "" : "s", total / 1048576.0).c_str(), 13);
        if (Button("Convertir todos", ButtonKind::Secondary, {0, 0}, Icon::Sparkle, true)) {
            std::vector<fs::path> cues;
            for (const Game* g : discs) cues.push_back(g->path);
            StartChdJob(cues, chd_delete_);
        }
        ImGui::Dummy({0, S(2)});
        Toggle("##chd_borrar", &chd_delete_);
        ImGui::SameLine(0, S(8));
        Muted("Borrar los originales al terminar", 13);
    }
    RowEnd();
}

} // namespace Satronho
