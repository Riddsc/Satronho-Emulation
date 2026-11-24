// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Barra de menús del juego (estilo Satronho NX / DuckStation): Juego, Imagen, Sonido, Controles,
// Herramientas, Satronho y Ayuda arriba, y abajo una barra de estado con el motor, la resolución,
// los cuadros por segundo, el mando y el volumen.
// - En ventana se ve siempre y el juego se dibuja entre las dos barras.
// - En pantalla completa está escondida: Esc la despliega encima del juego (y Esc la vuelve a esconder).
// - En Android (o con el dedo) aparece con el botón de menú del control en pantalla, con botones grandes.
// Mientras un menú está abierto, el juego se pausa.

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

#include "app.h"
#include "util/i18n.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "util/paths.h"
#include "ui/kit.h"
#include "util/log.h"

#ifdef __ANDROID__
#include "sys/android.h"
#endif

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

void OpenFolder(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
    const std::string url = "file:///" + Paths::ToUtf8(p);
    SDL_OpenURL(url.c_str());
}
} // namespace

bool App::GameBarVisible() const {
    if (!in_game_ || menu_open_ || !pending_start_.empty()) return false;
    if (bar_open_) return true;
    // En ventana (y con mouse y teclado) se ve siempre; en pantalla completa o con el dedo, solo al pedirla.
    return !settings_.fullscreen && !touch_mode_ && settings_.game_bar;
}

float App::GameBarTop() const { return GameBarVisible() ? S(touch_mode_ ? 56 : 38) : 0.0f; }
float App::GameBarBottom() const { return GameBarVisible() && !touch_mode_ && settings_.status_bar ? S(28) : 0.0f; }

bool App::GameBarReserves() const {
    // En ventana la barra no tapa el juego: se le deja su espacio. Desplegada sobre la pantalla
    // completa, va encima (para que la imagen no salte).
    return GameBarVisible() && !bar_open_;
}

void App::ToggleGameBar() {
    bar_open_ = !bar_open_;
    if (!bar_open_) bar_menu_active_ = false;
    last_mouse_move_ = SDL_GetTicks(); // que se vea el cursor para usarla
}

void App::DrawGameBar() {
    bar_menu_active_ = false;
    // Con el dedo: deslizar hacia abajo desde el borde de arriba despliega el menú.
    if (touch_mode_ && !bar_open_ && !menu_open_ && overlay_page_ < 0) {
        const ImGuiViewport* v = ImGui::GetMainViewport();
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) edge_swipe_ = io.MousePos.y < v->Pos.y + S(28);
        if (edge_swipe_ && ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.MousePos.y - io.MouseClickedPos[0].y > S(48)) {
            edge_swipe_ = false;
            ToggleGameBar();
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) edge_swipe_ = false;
    }
    if (!GameBarVisible()) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const bool touch = touch_mode_;
    const float top_h = GameBarTop();

    // Medidas de los menús: con el dedo, todo más grande.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {S(touch ? 18 : 12), S(touch ? 14 : 9)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {S(touch ? 14 : 10), S(touch ? 12 : 8)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(touch ? 16 : 12), S(touch ? 12 : 8)});
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, S(14));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, V4(Color::Surface, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, V4(Color::Accent, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_Header, V4(Color::Accent, 0.22f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, V4(Color::Accent, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, V4(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Separator, V4(Color::Border));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, V4(Color::Dim));

    // Fondo de la barra: degradado oscuro con una línea de color abajo.
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    const ImVec2 b0 = vp->Pos, b1 = {vp->Pos.x + vp->Size.x, vp->Pos.y + top_h};
    ImGui::SetNextWindowPos(b0);
    ImGui::SetNextWindowSize({vp->Size.x, top_h});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, V4(0, 0));
    ImGui::Begin("##barra_juego", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilledMultiColor(b0, b1, U32(Color::Surface, 0.97f), U32(Color::Surface, 0.97f), U32(Color::Bg, 0.97f), U32(Color::Bg, 0.97f));
    dl->AddRectFilledMultiColor({b0.x, b1.y - S(2)}, {b0.x + vp->Size.x * 0.5f, b1.y}, U32(Color::Accent, 0.0f), U32(Color::Accent, 0.7f),
                                U32(Color::Accent, 0.7f), U32(Color::Accent, 0.0f));
    dl->AddRectFilledMultiColor({b0.x + vp->Size.x * 0.5f, b1.y - S(2)}, {b1.x, b1.y}, U32(Color::Accent, 0.7f), U32(Color::Accent2, 0.0f),
                                U32(Color::Accent2, 0.0f), U32(Color::Accent, 0.7f));
    (void)bg;

    if (ImGui::BeginMenuBar()) {
        // Logo y nombre del juego a la izquierda.
        {
            const float lh = top_h - S(12);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float y = b0.y + (top_h - lh) * 0.5f;
            if (logo_) dl->AddImage(logo_.Ref(), {p.x, y}, {p.x + lh, y + lh});
            ImGui::Dummy({lh + S(4), lh});
        }
        bool any = false;
        auto menu = [&](const char* label) {
            FontScope f(GetFonts().semibold, touch ? 16.0f : 14.0f);
            if (qa_.count("barra_menu") && qa_["barra_menu"] == label && qa_frames_ > 20) ImGui::OpenPopup(label); // modo de revisión
            const bool open = ImGui::BeginMenu(Tr(label));
            any |= open;
            return open;
        };
        auto item = [&](const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true) {
            FontScope f(GetFonts().medium, touch ? 16.0f : 14.0f);
            return ImGui::MenuItem(Tr(label), touch ? nullptr : shortcut, selected, enabled);
        };
        auto sub = [&](const char* label, bool enabled = true) {
            FontScope f(GetFonts().medium, touch ? 16.0f : 14.0f);
            return ImGui::BeginMenu(Tr(label), enabled);
        };
        auto note = [&](const char* text) {
            FontScope f(GetFonts().regular, touch ? 14.0f : 12.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Dim));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + S(300));
            ImGui::TextWrapped("%s", Tr(text));
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        };

        // ---------------- Juego
        if (menu("Juego")) {
            if (item(paused_ ? "Seguir jugando" : "Pausa", "P")) DoHotkey(Hotkey::Pause);
            if (item("Reiniciar", "F4")) DoHotkey(Hotkey::Reset);
            ImGui::Separator();
            if (item(Fmt("Guardar estado (ranura %d)", slot_).c_str(), "F5")) SaveSlot(slot_);
            if (item(Fmt("Cargar estado (ranura %d)", slot_).c_str(), "F9")) LoadSlot(slot_);
            if (sub("Ranura")) {
                for (int i = 1; i <= 8; ++i)
                    if (item(Fmt("Ranura %d", i).c_str(), nullptr, slot_ == i)) slot_ = i;
                ImGui::EndMenu();
            }
            if (item("Estados con miniatura…")) {
                SetMenuOpen(true);
                menu_section_ = 0;
            }
            if (item("Deshacer la carga", nullptr, false, !undo_state_.empty())) UndoLoad();
            if (core_.DiskCount() > 1 && sub("Cambiar de disco")) {
                const int now = core_.DiskIndex();
                for (unsigned i = 0; i < core_.DiskCount(); ++i) {
                    std::string l = core_.DiskLabel(i);
                    if (l.empty()) l = Fmt("Disco %u", i + 1);
                    if (item(l.c_str(), nullptr, static_cast<int>(i) == now)) RequestDisk(i);
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (item("Captura de pantalla", "F12")) Screenshot();
            if (item("Menú del juego completo", touch ? nullptr : "Mando: Guía")) SetMenuOpen(true);
            ImGui::Separator();
            if (item("Salir a la biblioteca")) {
                bar_open_ = false;
                RequestExitGame();
            }
            if (item("Cerrar Satronho Emulation", "Alt+F4")) RequestQuit();
            ImGui::EndMenu();
        }

        // ---------------- Vista
        if (menu("Vista")) {
            if (item("Pantalla completa", touch ? nullptr : "F11", settings_.fullscreen)) DoHotkey(Hotkey::Fullscreen);
            if (item("Barra de estado", nullptr, settings_.status_bar)) {
                settings_.status_bar = !settings_.status_bar;
                SaveConfig();
            }
            if (item("Medidor Satronho", touch ? nullptr : "F3", settings_.meter > 0)) {
                settings_.meter = settings_.meter > 0 ? 0 : 1;
                SaveConfig();
            }
            if (!settings_.fullscreen && !touch && sub("Tamaño de la ventana")) {
                const auto& g = core_.AvInfo().geometry;
                const int bw = g.base_width ? static_cast<int>(g.base_width) : 320, bh = g.base_height ? static_cast<int>(g.base_height) : 240;
                for (int k = 2; k <= 6; ++k)
                    if (item(Fmt("%d× · %d × %d", k, bw * k, bh * k).c_str())) {
                        const int extra = static_cast<int>(GameBarTop() + GameBarBottom());
                        SDL_SetWindowSize(window_, bw * k, bh * k + extra);
                    }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (item("Ocultar esta barra", touch ? nullptr : "Esc")) {
                if (settings_.fullscreen || touch) bar_open_ = false;
                else {
                    settings_.game_bar = false;
                    SaveConfig();
                    Toast("Barra escondida. Vuelve en Ajustes » Apariencia, o con Esc en pantalla completa.", GuideId::Troya, 4);
                }
            }
            ImGui::EndMenu();
        }

        // ---------------- Gráficos
        if (menu("Gráficos")) {
            VideoOverride* own = nullptr;
            if (auto it = settings_.video_for.find(sys_->id); it != settings_.video_for.end() && it->second.on) own = &it->second;
            int& scale = own ? own->scale_mode : settings_.scale_mode;
            int& filter = own ? own->filter : settings_.filter;
            int& effect = own ? own->effect : settings_.effect;
            bool changed = false;
            if (sub("Tamaño de la imagen")) {
                const char* n[] = {"Ajustar (sin deformar)", "Entero (píxeles perfectos)", "Estirar"};
                for (int i = 0; i < 3; ++i)
                    if (item(n[i], nullptr, scale == i)) { scale = i; changed = true; }
                ImGui::EndMenu();
            }
            if (sub("Filtro")) {
                const char* n[] = {"Píxel exacto", "Nítido suave", "Suave"};
                for (int i = 0; i < 3; ++i)
                    if (item(n[i], nullptr, filter == i)) { filter = i; changed = true; }
                ImGui::EndMenu();
            }
            if (sub("Efecto de pantalla")) {
                const char* n[] = {"Ninguno", "Líneas de barrido", "Televisor CRT", "Pantalla LCD", "Según la consola"};
                for (int i = 0; i < 5; ++i)
                    if (item(n[i], nullptr, effect == i)) { effect = i; changed = true; }
                ImGui::EndMenu();
            }
            if (item("Fondo ambiente", nullptr, settings_.ambient)) { settings_.ambient = !settings_.ambient; changed = true; }
            if (item(Fmt("Imagen propia para %s", sys_->name.c_str()).c_str(), nullptr, own != nullptr)) {
                VideoOverride& v = settings_.video_for[sys_->id];
                if (!v.on) {
                    v.scale_mode = settings_.scale_mode;
                    v.filter = settings_.filter;
                    v.effect = settings_.effect;
                    v.effect_strength = settings_.effect_strength;
                }
                v.on = !v.on;
                changed = true;
            }
            const bool gpu_sys = sys_->id == "cd" || sys_->id == "n64";
            if (gpu_sys && gl_) {
                ImGui::Separator();
                const int api_now = GpuApiFor(*sys_);
                if (sub("Resolución")) {
                    const int stored = settings_.GpuRes(sys_->id);
                    if (item(Fmt("Automática (%s)", ResLabel(GpuResFor(*sys_, api_now)).c_str()).c_str(), nullptr, stored == 0)) {
                        settings_.gpu_res[sys_->id] = 0;
                        changed = true;
                    }
                    for (int k : GpuResSteps(*sys_, api_now))
                        if (item(ResLabel(k).c_str(), nullptr, stored == k)) {
                            settings_.gpu_res[sys_->id] = k;
                            changed = true;
                        }
                    ImGui::Separator();
                    note("Se aplica al volver a abrir el juego.");
                    ImGui::EndMenu();
                }
                if (sub("Motor del juego")) {
                    const int api = settings_.GpuApi(sys_->id);
                    const char* n[] = {"Automático", "OpenGL", "Vulkan", "Procesador"};
                    for (int i = 0; i < 4; ++i) {
                        if (i == 2 && !hw_render_.VulkanAvailable()) continue;
                        if (item(n[i], nullptr, api == i)) { settings_.gpu_api[sys_->id] = i; changed = true; }
                    }
                    ImGui::Separator();
                    note("Se aplica al volver a abrir el juego.");
                    ImGui::EndMenu();
                }
            }
            ImGui::Separator();
            if (item(Fmt("Gráficos de %s (todas las opciones)…", sys_->name.c_str()).c_str())) {
                gfx_sys_ = sys_->id;
                tab_ = SettingsTab::Video;
                OpenOverlay(Page::Settings);
            }
            if (changed) SaveConfig();
            ImGui::EndMenu();
        }

        // ---------------- Sonido
        if (menu("Sonido")) {
            ImGui::SetNextItemWidth(S(220));
            FontScope f(GetFonts().medium, touch ? 16.0f : 14.0f);
            if (SliderPercent("##vol_barra", &settings_.volume, S(touch ? 280 : 220))) {
                ApplyAudioSettings();
                SaveConfig();
            }
            if (item("Silenciar", nullptr, settings_.volume <= 0.001f)) {
                std::swap(settings_.volume, muted_volume_);
                if (settings_.volume <= 0.001f && muted_volume_ <= 0.001f) settings_.volume = 0.8f;
                ApplyAudioSettings();
                SaveConfig();
            }
            if (item("Silenciar al avanzar rápido", nullptr, settings_.mute_fast_forward)) {
                settings_.mute_fast_forward = !settings_.mute_fast_forward;
                SaveConfig();
            }
            ImGui::EndMenu();
        }

        // ---------------- Controles
        if (menu("Controles")) {
            if (item("Asignar botones…")) {
                SetMenuOpen(true);
                menu_section_ = 2;
            }
            if (item("Vibración del mando", nullptr, settings_.rumble)) {
                settings_.rumble = !settings_.rumble;
                SaveConfig();
            }
            if (touch_mode_ || TouchPadVisible()) {
                ImGui::Separator();
                if (item("Mostrar el control en pantalla", nullptr, settings_.touch_show != 2)) {
                    settings_.touch_show = settings_.touch_show == 2 ? 0 : 2;
                    SaveConfig();
                }
                if (item("Acomodar el control en pantalla…")) {
                    bar_open_ = false;
                    touch_edit_ = true;
                }
            }
            ImGui::Separator();
            const auto pads = input_.Gamepads();
            if (pads.empty()) note("Ningún mando conectado: juegas con el teclado.");
            for (const auto& p : pads) note(Fmt("Jugador %d: %s", p.slot + 1, p.name.c_str()).c_str());
            ImGui::EndMenu();
        }

        // ---------------- Herramientas
        if (menu("Herramientas")) {
            if (item("Abrir la carpeta de capturas")) OpenFolder(Paths::User() / "capturas");
            if (item("Abrir la carpeta de partidas")) OpenFolder(Paths::User() / "partidas" / sys_->id);
            if (item("Abrir la carpeta de datos")) OpenFolder(Paths::User());
            ImGui::Separator();
            if (item("Memory Cards, prueba de mandos y más…")) {
                bar_open_ = false;
                pending_page_ = static_cast<int>(Page::Tools);
                RequestExitGame();
            }
            ImGui::Separator();
            if (item("Avance rápido", touch ? nullptr : "Tab", fast_forward_toggle_, settings_.fast_forward_enabled)) fast_forward_toggle_ = !fast_forward_toggle_;
            if (!settings_.fast_forward_enabled) note("El avance rápido se enciende en Ajustes » Rendimiento.");
            if (item("Cambiar pantallas (Dual)", "F2", false, sys_->id == "dual")) DoHotkey(Hotkey::Screens);
            ImGui::EndMenu();
        }

        // ---------------- Satronho (como el módulo de Satronho NX)
        if (menu("Satronho")) {
            SatronhoModuleMenu(true);
            ImGui::EndMenu();
        }

        // ---------------- Ayuda
        if (menu("Ayuda")) {
            note("Atajos del teclado (se cambian en Controles » Atajos):");
            ImGui::Dummy({0, S(2)});
            for (int i = 0; i < static_cast<int>(Hotkey::Count); ++i) {
                const Hotkey h = static_cast<Hotkey>(i);
                const SDL_Scancode k = input_.hk_keys[i];
                if (k == SDL_SCANCODE_UNKNOWN) continue;
                FontScope f(GetFonts().medium, touch ? 15.0f : 13.5f);
                ImGui::TextUnformatted(Tr(HotkeyInfo(h).name));
                ImGui::SameLine(S(240));
                ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Gold));
                ImGui::TextUnformatted(Tr(SDL_GetScancodeName(k)));
                ImGui::PopStyleColor();
            }
            ImGui::Separator();
            if (item("Abrir el registro (satronho.log)")) OpenFolder(Paths::User());
            if (item("Acerca de Satronho Emulation")) {
                bar_open_ = false;
                pending_page_ = static_cast<int>(Page::About);
                RequestExitGame();
            }
            ImGui::EndMenu();
        }

        // A la derecha: nombre del juego y, en pantalla completa, cómo esconder la barra.
        {
            FontScope f(GetFonts().semibold, touch ? 15.0f : 13.5f);
            std::string right = current_.title;
            if (bar_open_) right += touch ? "   ·   Toca el juego para seguir" : "   ·   Esc para esconder";
            const float tw = ImGui::CalcTextSize(Tr(right.c_str())).x;
            const float x = b1.x - tw - S(18);
            if (x > ImGui::GetCursorScreenPos().x + S(20)) {
                dl->AddText({x, b0.y + (top_h - ImGui::GetFontSize()) * 0.5f}, U32(Color::Muted), Tr(right.c_str()));
            }
        }
        bar_menu_active_ = any;
        ImGui::EndMenuBar();
    }
    // Con el dedo o en pantalla completa: tocar el juego (fuera de la barra) la esconde.
    if (bar_open_ && !bar_menu_active_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered() &&
        ImGui::GetIO().MousePos.y > b1.y && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
        bar_open_ = false;
    ImGui::End();

    // ---------------- Barra de estado (abajo)
    if (const float bh = GameBarBottom(); bh > 0) {
        const ImVec2 s0 = {vp->Pos.x, vp->Pos.y + vp->Size.y - bh}, s1 = {vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y};
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        fg->AddRectFilled(s0, s1, U32(Color::Bg, 0.97f));
        fg->AddLine(s0, {s1.x, s0.y}, U32(Color::Border));
        FontScope f(GetFonts().mono, 12.0f);
        float x = s0.x + S(14);
        auto chip = [&](const std::string& t, unsigned col) {
            fg->AddText({x, s0.y + (bh - ImGui::GetFontSize()) * 0.5f}, U32(col), Tr(t.c_str()));
            x += ImGui::CalcTextSize(Tr(t.c_str())).x + S(18);
        };
        const bool gpu_sys = sys_->id == "cd" || sys_->id == "n64";
        if (gpu_sys && hw_render_.Active()) {
            const int api = GpuApiFor(*sys_);
            chip(api == 2 ? "VULKAN" : api == 1 ? "OPENGL" : "PROCESADOR", Color::Orange);
            const int k = GpuResFor(*sys_, api);
            chip(Fmt("%dp", ResHeight(k)), Color::Teal);
        } else {
            chip(gl_ ? "OPENGL" : "CLÁSICO", Color::Orange);
        }
        const Settings v = VideoSettings();
        const char* filters[] = {"PÍXEL EXACTO", "NÍTIDO SUAVE", "SUAVE"};
        const char* effects[] = {"SIN EFECTO", "LÍNEAS", "CRT", "LCD", "AUTO"};
        chip(filters[std::clamp(v.filter, 0, 2)], Color::Muted);
        chip(effects[std::clamp(v.effect, 0, 4)], Color::Muted);
        if (paused_) chip("PAUSA", Color::Amber);
        if (fast_forward_) chip(Fmt("AVANCE ×%d", settings_.fast_forward), Color::Accent);
        chip(Fmt("RANURA %d", slot_), Color::Muted);
        chip(Fmt("VOLUMEN %d%%", static_cast<int>(settings_.volume * 100 + 0.5f)), Color::Muted);
        // A la derecha: mando y cuadros por segundo.
        const auto pads = input_.Gamepads();
        const std::string rt = Fmt("%s   %.0f FPS", pads.empty() ? "TECLADO" : pads.front().family_name.c_str(), perf_.fps);
        const float tw = ImGui::CalcTextSize(Tr(rt.c_str())).x;
        fg->AddText({s1.x - tw - S(14), s0.y + (bh - ImGui::GetFontSize()) * 0.5f}, U32(Color::Green), Tr(rt.c_str()));
    }

    ImGui::PopStyleColor(7);
    ImGui::PopStyleVar(5);
}

// ---------------------------------------------------------------- pantallas de Satronho encima del juego
// Como el módulo Satronho de Satronho NX: Novedades, Configuración sugerida, los guías y Ajustes se
// abren en una ventana sobre el juego, que espera quieto hasta que vuelvas.

void App::OpenOverlay(Page p) {
    bar_open_ = false;
    overlay_page_ = static_cast<int>(p);
    overlay_focus_ = true;
    Log::Info("Encima del juego: pantalla %d", overlay_page_);
}

void App::DrawGameOverlay() {
    if (overlay_page_ < 0 || menu_open_) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    // Velo oscuro sobre el juego.
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(4, 3, 12, 185));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##velo_encima", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                            ImGuiWindowFlags_NoBringToFrontOnFocus);
    const bool click_out = ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    const bool small = compact_ || touch_mode_;
    const ImVec2 size = small ? vp->Size : ImVec2(std::min(vp->Size.x * 0.92f, S(1320)), vp->Size.y * 0.9f);
    ImGui::SetNextWindowPos({vp->Pos.x + (vp->Size.x - size.x) * 0.5f, vp->Pos.y + (vp->Size.y - size.y) * 0.5f});
    ImGui::SetNextWindowSize(size);
    // Solo al abrirla: tomar el foco en cada cuadro cerraría las listas desplegables de adentro.
    if (overlay_focus_) {
        ImGui::SetNextWindowFocus();
        overlay_focus_ = false;
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, V4(Color::Bg));
    ImGui::PushStyleColor(ImGuiCol_Border, V4(Color::Accent, 0.35f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, small ? 0 : S(18));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, small ? 0 : S(1.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, small ? ImVec2(S(16), S(12)) : ImVec2(S(30), S(22)));
    ImGui::Begin("##encima", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    // Encabezado: «Satronho · Novedades» y el botón para volver al juego.
    const Page p = static_cast<Page>(overlay_page_);
    const char* title = p == Page::News ? "Novedades" : p == Page::Suggested ? "Configuración sugerida" : p == Page::Guides ? "Pregúntale a los guías"
                      : p == Page::Settings ? "Ajustes" : p == Page::Controls ? "Controles" : "Satronho";
    {
        FontScope f(GetFonts().bold, small ? 17.0f : 20.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Accent2));
        ImGui::TextUnformatted("Satronho");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, S(10));
        ImGui::TextUnformatted(Tr(title));
    }
    {
        FontScope f(GetFonts().medium, 12.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
        ImGui::TextUnformatted(Tr("El juego está en pausa mientras miras esto."));
        ImGui::PopStyleColor();
    }
    const float bw = S(small ? 150 : 190);
    ImGui::SetCursorPos({ImGui::GetWindowWidth() - bw - (small ? S(16) : S(30)), small ? S(14) : S(24)});
    bool close = Button("Volver al juego", ButtonKind::Primary, {bw, 0}, Icon::Play, true);
    ImGui::SetCursorPosY((small ? S(12) : S(22)) + S(56));
    ImGui::Separator();
    ImGui::Dummy({0, S(6)});

    const Page saved = page_;
    page_ = p;
    ImGui::BeginChild("##encima_contenido", {0, 0}, 0);
    switch (p) {
    case Page::News: DrawNews(); break;
    case Page::Suggested: DrawSuggested(); break;
    case Page::Guides: DrawGuideChat(); break;
    case Page::Settings: DrawSettings(); break;
    case Page::Controls: DrawControls(); break;
    default: close = true; break;
    }
    ImGui::Dummy({0, S(16)});
    ImGui::EndChild();
    // Si la pantalla pidió ir a otra (Ajustes » Controles), se muestra ahí mismo.
    if (page_ != p && (page_ == Page::Controls || page_ == Page::Settings || page_ == Page::News || page_ == Page::Suggested || page_ == Page::Guides))
        overlay_page_ = static_cast<int>(page_);
    else if (page_ != p) { // otra pantalla (biblioteca…): sale del juego como antes
        pending_page_ = static_cast<int>(page_);
        overlay_page_ = -1;
        RequestExitGame();
    }
    page_ = saved;
    ImGui::End();
    if (close || click_out) overlay_page_ = -1;
}

// ---------------------------------------------------------------- módulo Satronho
// El menú «Satronho» de la barra del juego y de la barra de la pantalla de inicio: lo mismo en las dos,
// con la carita del guía que te atiende en cada cosa.

void App::SatronhoModuleMenu(bool in_game) {
    const bool touch = touch_mode_;
    const float fs = touch ? 16.0f : 14.0f;
    auto open = [&](Page p) {
        if (in_game) OpenOverlay(p);
        else page_ = p;
    };
    // Ítem con la carita del guía a la izquierda.
    auto gitem = [&](GuideId g, const char* label) {
        FontScope f(GetFonts().medium, fs);
        const float ih = S(touch ? 44 : 34), d = ih - S(8);
        const std::string l = std::string("##g_") + label;
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, {0.0f, 0.5f});
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::Dummy({0, 0});
        ImGui::SetCursorScreenPos(p);
        const bool r = ImGui::Selectable(l.c_str(), false, 0, {std::max(S(240), ImGui::GetContentRegionAvail().x), ih});
        const bool hov = ImGui::IsItemHovered();
        ImGui::PopStyleVar();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 c = {p.x + d * 0.5f + S(2), p.y + ih * 0.5f};
        const Ui::Image& img = guides_[static_cast<int>(g)];
        dl->AddCircleFilled(c, d * 0.5f + S(2), U32(Guide(g).color, hov ? 1.0f : 0.75f));
        if (img) dl->AddImageRounded(img.Ref(), {c.x - d * 0.5f, c.y - d * 0.5f}, {c.x + d * 0.5f, c.y + d * 0.5f}, {0, 0}, {1, 1}, IM_COL32_WHITE, d * 0.5f);
        dl->AddText({p.x + d + S(14), p.y + (ih - ImGui::GetFontSize()) * 0.5f}, U32(Color::Text), Tr(label));
        return r;
    };
    auto item = [&](const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true) {
        FontScope f(GetFonts().medium, fs);
        return ImGui::MenuItem(Tr(label), touch ? nullptr : shortcut, selected, enabled);
    };
    auto sub = [&](const char* label) {
        FontScope f(GetFonts().medium, fs);
        return ImGui::BeginMenu(Tr(label));
    };
    auto note = [&](const char* text) {
        FontScope f(GetFonts().regular, touch ? 14.0f : 12.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Dim));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + S(300));
        ImGui::TextWrapped("%s", Tr(text));
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    };

    bool changed = false;
    if (gitem(GuideId::Juana, "Novedades")) open(Page::News);
    if (gitem(GuideId::Nono, "Configuración sugerida")) open(Page::Suggested);
    if (gitem(GuideId::Samy, "Pregúntale a los guías")) open(Page::Guides);
    ImGui::Separator();
    if (sub("Medidor")) {
        const char* n[] = {"Apagado", "Compacto", "Detallado"};
        for (int i = 0; i < 3; ++i)
            if (item(n[i], i == 1 ? "F3" : nullptr, settings_.meter == i)) { settings_.meter = i; changed = true; }
        ImGui::Separator();
        const char* corners[] = {"Arriba a la izquierda", "Arriba a la derecha", "Abajo a la izquierda", "Abajo a la derecha"};
        for (int i = 0; i < 4; ++i)
            if (item(corners[i], nullptr, settings_.meter_corner == i)) { settings_.meter_corner = i; changed = true; }
        ImGui::Separator();
        if (item("Más opciones del medidor…")) {
            tab_ = SettingsTab::Meter;
            open(Page::Settings);
        }
        if (!in_game) note("El medidor se ve mientras juegas.");
        ImGui::EndMenu();
    }
    if (sub("Logo de fondo")) {
        for (int pct : {0, 10, 25, 50, 75, 100})
            if (item(pct == 0 ? Tr("Sin logo") : Fmt("%d %%", pct).c_str(), nullptr, std::lround(settings_.watermark * 100) == pct)) {
                settings_.watermark = pct / 100.0f;
                changed = true;
            }
        ImGui::EndMenu();
    }
    if (sub("Guías")) {
        if (item("Mostrar a los guías", nullptr, settings_.guides)) { settings_.guides = !settings_.guides; changed = true; }
        if (item("Burbuja de consejos", nullptr, settings_.bubble)) { settings_.bubble = !settings_.bubble; changed = true; }
        if (item("Conocer a los guías…")) {
            tab_ = SettingsTab::Guides;
            open(Page::Settings);
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (gitem(GuideId::Troya, "Gráficos")) {
        if (in_game) gfx_sys_ = sys_->id;
        tab_ = SettingsTab::Video;
        open(Page::Settings);
    }
    if (gitem(GuideId::MechaRidds, "Ajustes")) open(Page::Settings);
    if (in_game) {
        ImGui::Separator();
        note("La biblioteca cierra el juego (guardando dónde quedaste).");
        if (item("Volver a la biblioteca")) {
            bar_open_ = false;
            pending_page_ = static_cast<int>(Page::Library);
            RequestExitGame();
        }
    }
    if (changed) SaveConfig();
}

// ---------------------------------------------------------------- barra de la pantalla de inicio
// En PC, arriba de todo, como la de Satronho NX: Archivo, Vista, Herramientas, Satronho y Ayuda.

float App::HomeBarHeight() const { return compact_ || touch_mode_ ? 0.0f : S(36); }

void App::DrawHomeBar() {
    const float h = HomeBarHeight();
    if (h <= 0) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {S(12), S(8)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {S(10), S(8)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(12), S(8)});
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, S(14));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, V4(Color::Surface, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, V4(Color::Accent, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_Header, V4(Color::Accent, 0.22f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, V4(Color::Accent, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, V4(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Separator, V4(Color::Border));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, V4(Color::Dim));
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize({vp->Size.x, h});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, V4(Color::Surface, 0.97f));
    ImGui::Begin("##barra_inicio", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilledMultiColor({vp->Pos.x, vp->Pos.y + h - S(1.5f)}, {vp->Pos.x + vp->Size.x, vp->Pos.y + h}, U32(Color::Accent, 0.0f),
                                U32(Color::Accent2, 0.5f), U32(Color::Accent2, 0.5f), U32(Color::Accent, 0.0f));
    if (ImGui::BeginMenuBar()) {
        auto menu = [&](const char* label) {
            FontScope f(GetFonts().semibold, 14.0f);
            if (qa_.count("inicio_menu") && qa_["inicio_menu"] == label && qa_frames_ > 20) ImGui::OpenPopup(label); // modo de revisión
            return ImGui::BeginMenu(Tr(label));
        };
        auto item = [&](const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true) {
            FontScope f(GetFonts().medium, 14.0f);
            return ImGui::MenuItem(Tr(label), shortcut, selected, enabled);
        };
        if (menu("Archivo")) {
            if (item("Abrir un juego…")) OpenRomDialog();
            if (item("Agregar una carpeta de juegos…")) AddFolderDialog();
            if (item("Volver a buscar juegos")) Rescan();
            ImGui::Separator();
            if (item("Abrir la carpeta de datos")) OpenFolder(Paths::User());
            if (item("Abrir la carpeta de capturas")) OpenFolder(Paths::User() / "capturas");
            ImGui::Separator();
            if (item("Cerrar Satronho Emulation", "Alt+F4")) RequestQuit();
            ImGui::EndMenu();
        }
        if (menu("Vista")) {
            if (item("Pantalla completa", "F11", settings_.fullscreen)) DoHotkey(Hotkey::Fullscreen);
            ImGui::Separator();
            const std::string vk = lib_system_.empty() ? "todas" : lib_system_;
            int& view = settings_.lib_view[vk];
            const char* views[] = {"Cuadrícula", "En lista", "Carrusel"};
            bool changed = false;
            for (int i = 0; i < 3; ++i)
                if (item(views[i], nullptr, view == i)) { view = i; page_ = Page::Library; changed = true; }
            ImGui::Separator();
            {
                FontScope f(GetFonts().medium, 14.0f);
                if (ImGui::BeginMenu(Tr("Tamaño de la interfaz"))) {
                    static const float scales[] = {0.0f, 0.9f, 1.0f, 1.15f, 1.3f, 1.5f};
                    const char* names[] = {"Auto", "90 %", "100 %", "115 %", "130 %", "150 %"};
                    for (int i = 0; i < 6; ++i)
                        if (item(names[i], nullptr, std::fabs(settings_.ui_scale - scales[i]) < 0.02f)) {
                            settings_.ui_scale = scales[i];
                            UpdateScale();
                            changed = true;
                        }
                    ImGui::EndMenu();
                }
            }
            if (changed) SaveConfig();
            ImGui::EndMenu();
        }
        if (menu("Consolas")) {
            if (item("Todas las consolas", nullptr, lib_system_.empty())) { lib_system_.clear(); page_ = Page::Library; }
            ImGui::Separator();
            for (const auto& sd : Systems())
                if (item(Fmt("%s · %s", sd.full.c_str(), sd.hint.c_str()).c_str(), nullptr, lib_system_ == sd.id)) {
                    lib_system_ = sd.id;
                    page_ = Page::Library;
                }
            ImGui::EndMenu();
        }
        if (menu("Herramientas")) {
            if (item("Herramientas (Memory Cards, mandos, .chd…)")) page_ = Page::Tools;
            if (item("Controles")) page_ = Page::Controls;
            if (item("Gráficos")) { tab_ = SettingsTab::Video; page_ = Page::Settings; }
            if (item("Ajustes")) page_ = Page::Settings;
            ImGui::EndMenu();
        }
        if (menu("Satronho")) {
            SatronhoModuleMenu(false);
            ImGui::EndMenu();
        }
        if (menu("Ayuda")) {
            if (item("Acerca de Satronho Emulation")) page_ = Page::About;
            if (item("Novedades de esta build")) page_ = Page::News;
            if (item("Abrir el registro (satronho.log)")) OpenFolder(Paths::User());
            if (item("Ver el proyecto en GitHub")) SDL_OpenURL("https://github.com/Riddsc/Satronho-Emulation");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::End();
    ImGui::PopStyleColor(7);
    ImGui::PopStyleVar(5);
}

} // namespace Satronho
