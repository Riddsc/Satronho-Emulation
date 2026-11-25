// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Interfaz nativa de Android (build 25). En el celular la biblioteca, la tarjeta de carga, el control
// en pantalla, el cajón del juego y las tarjetas de los guías los dibuja Android (Kotlin), como en
// Satronho NX. Aquí se atienden sus órdenes y se le pasa el estado en JSON. Lo que todavía no es
// nativo (Ajustes, Controles, el explorador de carpetas…) se sigue mostrando con la interfaz de
// siempre: Kotlin se esconde mientras tanto (NativeNeedsImGui).
#include <algorithm>
#include <cstdio>
#include <sstream>

#include "app.h"
#include "imgui.h"
#include "sys/android.h"
#include "sys/native_ui.h"
#include "util/i18n.h"
#include "util/log.h"

namespace Satronho {

using NativeUi::Quote;

namespace {
std::string Hex(unsigned rgb) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "\"#%06X\"", rgb & 0xFFFFFF);
    return buf;
}
const char* B(bool v) { return v ? "true" : "false"; }
std::string F(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4f", v);
    return buf;
}
std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}
} // namespace

bool App::NativeMode() const {
#ifdef __ANDROID__
    return NativeUi::Active();
#else
    return false;
#endif
}

bool App::NativeNeedsImGui() const {
    if (!NativeMode()) return true;
    if (confirm_ != 0 || !game_settings_.key.empty() || touch_edit_ || browser_open_) return true;
    if (in_game_) return menu_open_ || overlay_page_ >= 0;
    return legacy_page_ >= 0 || show_welcome_ || show_whats_new_ || archive_pick_.Archived() || unpack_state_ == 1 ||
           move_state_ == 1 || import_.active;
}

const Game* App::FindGame(const std::string& key) {
    if (games_dirty_ || games_.empty()) games_ = library_.Snapshot();
    for (const auto& g : games_)
        if (g.key == key) return &g;
    return nullptr;
}

void App::SetNativeMenu(bool open) {
    if (!in_game_) open = false;
    if (native_menu_ == open) return;
    native_menu_ = open;
    audio_.SetPaused(open || paused_);
    NativeUi::ResetTouch();
    if (open) core_.SaveSram();
}

// ---------------------------------------------------------------- órdenes de Kotlin

void App::NativeCommand(const std::string& line) {
    const auto a = Split(line, '|');
    const std::string& cmd = a[0];
    auto arg = [&](size_t i) -> std::string { return i < a.size() ? a[i] : std::string(); };
    Log::Info("Interfaz nativa: %s", line.c_str());

    if (cmd == "jugar" || cmd == "continuar") {
        if (const Game* g = FindGame(arg(1))) {
            start_entry_ = g->entry;
            RequestStart(g->path, cmd == "continuar");
        }
    } else if (cmd == "favorito") {
        library_.ToggleFavorite(arg(1));
        games_dirty_ = true;
        native_lib_t_ = -10;
    } else if (cmd == "ocultar") {
        library_.SetHidden(arg(1), arg(2) != "0");
        games_dirty_ = true;
        native_lib_t_ = -10;
    } else if (cmd == "ajustes_juego") {
        if (const Game* g = FindGame(arg(1))) game_settings_ = *g;
    } else if (cmd == "carpeta") {
        OpenBrowser(true);
    } else if (cmd == "buscar") {
        Rescan();
    } else if (cmd == "pagina") {
        // Pantalla de siempre (mientras no sea nativa). «atrás» vuelve a la nativa.
        const std::string p = arg(1);
        const Page pg = p == "controles" ? Page::Controls : p == "sugerida" ? Page::Suggested : p == "novedades" ? Page::News
                      : p == "acerca" ? Page::About : p == "guias" ? Page::Guides : p == "herramientas" ? Page::Tools : Page::Settings;
        if (in_game_) {
            OpenOverlay(pg);
        } else {
            page_ = pg;
            legacy_page_ = static_cast<int>(pg);
        }
        if (pg == Page::Settings && !arg(2).empty())
            tab_ = static_cast<SettingsTab>(std::clamp(std::atoi(arg(2).c_str()), 0, static_cast<int>(SettingsTab::Count) - 1));
    } else if (cmd == "volver") {
        legacy_page_ = -1;
        page_ = Page::Library;
    } else if (cmd == "menu") {
        SetNativeMenu(arg(1) == "1");
    } else if (cmd == "pausa") {
        if (in_game_) {
            paused_ = !paused_;
            audio_.SetPaused(paused_ || native_menu_);
        }
    } else if (cmd == "guardar") {
        if (in_game_) SaveSlot(std::max(1, std::atoi(arg(1).c_str())));
    } else if (cmd == "cargar") {
        if (in_game_) LoadSlot(std::max(1, std::atoi(arg(1).c_str())));
    } else if (cmd == "reiniciar") {
        if (in_game_) {
            core_.Reset();
            Toast("Juego reiniciado.", GuideId::Samy, 2.5);
        }
    } else if (cmd == "salir_juego") {
        SetNativeMenu(false);
        StopGame();
    } else if (cmd == "tactil") {
        // 0 automático, 1 siempre, 2 nunca (lo mismo que Ajustes » Controles).
        settings_.touch_show = std::clamp(std::atoi(arg(1).c_str()), 0, 2);
        SaveConfig();
    } else if (cmd == "tactil_editar") {
        if (in_game_) {
            SetNativeMenu(false);
            touch_edit_ = true;
        }
    } else if (cmd == "tarjeta") {
        if (arg(1) == "lento") {
            if (arg(2) == "bajar") SlowLower();
            else SlowKeep();
        }
    } else if (cmd == "registro") {
        SendLog();
    }
}

// ---------------------------------------------------------------- estado para Kotlin

std::string App::NativeTouchJson() {
    // El control de cada consola, en proporciones de la pantalla (0..1). Se arma con TouchLayout,
    // el mismo de siempre (respeta lo que la persona movió en el editor).
    int W = 0, H = 0;
    SDL_GetWindowSizeInPixels(window_, &W, &H);
    if (W < H) std::swap(W, H); // el juego siempre va acostado
    if (W <= 0 || H <= 0) return "null";
    if (W == native_touch_w_ && H == native_touch_h_ && sys_ && native_touch_sys_ == sys_->id && !native_touch_cache_.empty())
        return native_touch_cache_;
    const auto layout = TouchLayout(static_cast<float>(W), static_cast<float>(H));
    std::string s = "[";
    bool first = true;
    for (const auto& b : layout) {
        if (!first) s += ',';
        first = false;
        s += "{\"id\":" + std::to_string(b.id) + ",\"etiqueta\":" + Quote(b.label ? b.label : "") + ",\"grupo\":" + Quote(b.group ? b.group : "") +
             ",\"x\":" + F(b.c.x / W) + ",\"y\":" + F(b.c.y / H) + ",\"r\":" + F(b.r / H) + "}";
    }
    s += "]";
    native_touch_cache_ = s;
    native_touch_w_ = W;
    native_touch_h_ = H;
    native_touch_sys_ = sys_ ? sys_->id : "";
    return s;
}

std::string App::NativeStateJson() {
    std::string s = "{";
    s += "\"modo\":" + Quote(in_game_ ? "juego" : "biblioteca");
    s += ",\"imgui\":" + std::string(B(NativeNeedsImGui()));
    s += ",\"build\":" + std::to_string(build_);
    s += ",\"guias\":" + std::string(B(settings_.guides));
    s += ",\"idioma\":" + Quote(I18n::Current());
    // Colores del tema elegido (Ajustes » Apariencia), para que lo nativo combine.
    s += ",\"tema\":{\"fondo\":" + Hex(Ui::Color::Bg) + ",\"superficie\":" + Hex(Ui::Color::Surface) + ",\"superficie2\":" + Hex(Ui::Color::Surface2) +
         ",\"borde\":" + Hex(Ui::Color::Border) + ",\"texto\":" + Hex(Ui::Color::Text) + ",\"tenue\":" + Hex(Ui::Color::Muted) +
         ",\"acento\":" + Hex(Ui::Color::Accent) + ",\"claro\":" + B(Ui::LightTheme()) + "}";

    // Abriendo un juego: la tarjeta de carga.
    if (!pending_start_.empty() || unpack_state_ == 1) {
        const Game g = unpack_state_ == 1 ? unpack_game_ : start_entry_.empty() ? Library::MakeGame(pending_start_) : Library::MakeGameEntry(pending_start_, start_entry_);
        const SystemDef* gs = SystemById(g.system);
        std::error_code ec;
        const fs::path cover = covers_.PathFor(g);
        s += ",\"cargando\":{\"titulo\":" + Quote(g.title) + ",\"consola\":" + Quote(gs ? Tr(gs->full) : "") +
             ",\"color\":" + Hex(gs ? gs->color : Ui::Color::Accent) +
             ",\"portada\":" + Quote(!cover.empty() && fs::exists(cover, ec) ? Paths::ToUtf8(cover) : "") +
             ",\"avance\":" + F(unpack_state_ == 1 ? unpack_progress_.load() : -1.0) + "}";
    }

    if (in_game_ && sys_) {
        const bool touch = settings_.touch_show == 1 || (settings_.touch_show == 0 && input_.Gamepads().empty());
        s += ",\"juego\":{\"titulo\":" + Quote(current_.title) + ",\"clave\":" + Quote(current_.key) + ",\"consola\":" + Quote(Tr(sys_->full)) +
             ",\"sistema\":" + Quote(sys_->id) + ",\"color\":" + Hex(sys_->color) + ",\"pausa\":" + B(paused_) + ",\"menu\":" + B(native_menu_) +
             ",\"nucleo\":" + Quote(core_name_) + ",\"ranura\":" + std::to_string(slot_) + "}";
        s += ",\"tactil\":{\"visible\":" + std::string(B(touch && !touch_edit_)) + ",\"modo\":" + std::to_string(settings_.touch_show) +
             ",\"opacidad\":" + F(settings_.touch_opacity) + ",\"vibrar\":" + B(settings_.touch_haptics) +
             ",\"solo4\":" + B(settings_.touch_dpad4) + ",\"botones\":" + NativeTouchJson() + "}";
    }
    s += "}";
    return s;
}

std::string App::NativeLibraryJson() {
    const std::vector<Game> games = library_.Snapshot();
    std::map<std::string, int> counts;
    std::string g_json = "[";
    bool first = true;
    for (const auto& g : games) {
        ++counts[g.system];
        if (!first) g_json += ',';
        first = false;
        std::error_code ec;
        const fs::path cover = covers_.PathFor(g);
        const bool has_cover = !cover.empty() && fs::exists(cover, ec);
        g_json += "{\"clave\":" + Quote(g.key) + ",\"titulo\":" + Quote(g.title) + ",\"sistema\":" + Quote(g.system) +
                  ",\"etiquetas\":" + Quote([&] {
                      std::string t;
                      for (const auto& x : g.tags) t += (t.empty() ? "" : " ") + x;
                      return t;
                  }()) +
                  ",\"portada\":" + Quote(has_cover ? Paths::ToUtf8(cover) : "") + ",\"favorito\":" + B(g.favorite) + ",\"oculto\":" + B(g.hidden) +
                  ",\"tiempo\":" + std::to_string(g.playtime_s) + ",\"tiempo_texto\":" + Quote(g.playtime_s > 0 ? FormatPlaytime(g.playtime_s) : "") +
                  ",\"ultima\":" + std::to_string(g.last_played) + ",\"ultima_texto\":" + Quote(g.last_played > 0 ? FormatLastPlayed(g.last_played) : "") +
                  ",\"continuar\":" + B(HasAutoState(g)) + ",\"tono\":" + std::to_string(g.hue) + "}";
    }
    g_json += "]";

    std::string sys_json = "[";
    first = true;
    for (const auto& sys : Systems()) {
        if (!first) sys_json += ',';
        first = false;
        sys_json += "{\"id\":" + Quote(sys.id) + ",\"nombre\":" + Quote(Tr(sys.name)) + ",\"completo\":" + Quote(Tr(sys.full)) +
                    ",\"pista\":" + Quote(Tr(sys.hint)) + ",\"juegos_de\":" + Quote(Tr(sys.games)) + ",\"color\":" + Hex(sys.color) +
                    ",\"cuantos\":" + std::to_string(counts[sys.id]) + "}";
    }
    sys_json += "]";

    std::string tips = "[";
    first = true;
    for (const auto& t : tips_) {
        if (!first) tips += ',';
        first = false;
        tips += "{\"guia\":" + Quote(Guide(t.guide).key) + ",\"texto\":" + Quote(Tr(t.text)) + "}";
    }
    tips += "]";

    return "{\"juegos\":" + g_json + ",\"consolas\":" + sys_json + ",\"consejos\":" + tips + ",\"buscando\":" + B(library_.Scanning()) +
           ",\"carpetas\":" + std::to_string(settings_.folders.size()) + "}";
}

void App::NativeTick() {
    if (!NativeMode()) {
        legacy_page_ = -1;
        native_menu_ = false;
        return;
    }
    for (const auto& c : NativeUi::TakeCommands()) NativeCommand(c);
    // Con la interfaz de siempre encima (editor del control, Ajustes) el control puede cambiar.
    if (NativeNeedsImGui()) native_touch_cache_.clear();
    // Desde la barra de abajo de la interfaz de siempre se volvió a la biblioteca: vuelve la nativa.
    if (legacy_page_ >= 0 && page_ == Page::Library) legacy_page_ = -1;
    if (!in_game_ && native_menu_) native_menu_ = false;
    const double now = SDL_GetTicks() / 1000.0;
    // La biblioteca cambia poco: se arma cada segundo y medio (solo se manda si cambió).
    if (now - native_lib_t_ > 1.5) {
        native_lib_t_ = now;
        NativeUi::PublishLibrary(NativeLibraryJson());
    }
    NativeUi::PublishState(NativeStateJson());
}

// El control en pantalla nativo escribe aquí lo que está apretado.
void App::NativeApplyTouch() {
    if (native_menu_ || touch_edit_) return;
    const NativeUi::Touch t = NativeUi::TouchState();
    if (t.buttons == 0 && t.lx == 0 && t.ly == 0 && t.rx == 0 && t.ry == 0) {
        native_touch_prev_ = 0;
        return;
    }
    core_.pads[0].buttons |= static_cast<uint16_t>(t.buttons & 0xFFFF);
    if (t.lx != 0 || t.ly != 0) {
        core_.pads[0].analog[0][0] = static_cast<int16_t>(std::clamp(t.lx, -1.0f, 1.0f) * 32767);
        core_.pads[0].analog[0][1] = static_cast<int16_t>(std::clamp(t.ly, -1.0f, 1.0f) * 32767);
    }
    if (t.rx != 0 || t.ry != 0) {
        core_.pads[0].analog[1][0] = static_cast<int16_t>(std::clamp(t.rx, -1.0f, 1.0f) * 32767);
        core_.pads[0].analog[1][1] = static_cast<int16_t>(std::clamp(t.ry, -1.0f, 1.0f) * 32767);
    }
    native_touch_prev_ = t.buttons;
}

} // namespace Satronho
