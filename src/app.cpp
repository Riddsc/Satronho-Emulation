// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <sstream>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include "util/log.h"

#ifndef SATRONHO_BUILD
#define SATRONHO_BUILD 0
#endif

namespace Satronho {

const GuideInfo& Guide(GuideId id) {
    static const GuideInfo guides[] = {
        {"mecharidds", "MechaRidds", "Te acompaña en todo", Ui::Color::Green},
        {"samy", "Samy", "Funciones y controles", Ui::Color::Amber},
        {"troya", "Troya", "Gráficos y efectos", Ui::Color::Accent},
        {"nono", "Ñoño", "Configuración para tu equipo", Ui::Color::Teal},
        {"juana", "Juana", "Novedades", Ui::Color::Pink},
    };
    return guides[static_cast<int>(id)];
}

void FolderDialogCallback(void* user, const char* const* list, int) {
    auto* app = static_cast<App*>(user);
    if (!list) return;
    std::lock_guard lock(app->pending_mutex_);
    for (; *list; ++list) app->pending_folders_.push_back(*list);
}

void FileDialogCallback(void* user, const char* const* list, int) {
    auto* app = static_cast<App*>(user);
    if (!list) return;
    std::lock_guard lock(app->pending_mutex_);
    for (; *list; ++list) app->pending_roms_.push_back(*list);
}

// ---------------------------------------------------------------- inicio y cierre

int App::Run(int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (!Init()) return 1;
    while (running_) Frame();
    Shutdown();
    return 0;
}

bool App::Init() {
    build_ = SATRONHO_BUILD;
    SDL_SetAppMetadata("Satronho Emulation", "1.0", "com.ridds.satronho.emulation");
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_STREAM_NAME, "Satronho Emulation");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        // Sin sonido o sin mandos se puede seguir; sin video no.
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Satronho Emulation", SDL_GetError(), nullptr);
            return false;
        }
        SDL_InitSubSystem(SDL_INIT_AUDIO);
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    }
    Paths::Init();
    Log::Open();
    Log::Info("Satronho Emulation · build %d · %s", build_, SDL_GetPlatform());
    Log::Info("Datos en %s%s", Paths::ToUtf8(Paths::User()).c_str(), Paths::Portable() ? " (portátil)" : "");

    // Revisión automática: SATRONHO_QA="page=settings;tab=video;shot=a.png;frames=60"
    if (const char* qa = SDL_getenv("SATRONHO_QA")) {
        std::stringstream ss(qa);
        std::string item;
        while (std::getline(ss, item, ';')) {
            const size_t eq = item.find('=');
            if (eq != std::string::npos) qa_[item.substr(0, eq)] = item.substr(eq + 1);
        }
    }

    ini_.Load(Paths::ConfigFile());
    settings_.Load(ini_);
    if (qa_.count("fresh")) settings_ = Settings{};

    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    int ww = 1320, wh = 820;
    if (const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay())) {
        if (dm->h >= 1350) {
            ww = 1760;
            wh = 1080;
        }
    }
    ww = std::max(960, ini_.GetInt("ventana.ancho", ww));
    wh = std::max(620, ini_.GetInt("ventana.alto", wh));
    window_ = SDL_CreateWindow("Satronho Emulation", ww, wh, flags);
    if (!window_) {
        Log::Error("No se pudo crear la ventana: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(window_, 960, 600);
    if (ini_.GetBool("ventana.maximizada", false)) SDL_MaximizeWindow(window_);

    // Ícono de la ventana.
    if (SDL_Surface* icon = Ui::LoadSurface(Paths::ToUtf8(Paths::Assets() / "logo.png"))) {
        SDL_SetWindowIcon(window_, icon);
        SDL_DestroySurface(icon);
    }

    const char* wanted = settings_.renderer == "auto" ? nullptr : settings_.renderer.c_str();
    renderer_ = SDL_CreateRenderer(window_, wanted);
    if (!renderer_ && wanted) renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        Log::Error("No se pudo crear el dibujador: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(renderer_, settings_.vsync ? 1 : 0);
    Log::Info("Motor gráfico: %s", SDL_GetRendererName(renderer_));

    hw_ = DetectHardware(window_, renderer_);
    Log::Info("Equipo: %s · %d hilos · %d MB RAM · %s (%d MB) · %.0f Hz", hw_.cpu.c_str(), hw_.threads, hw_.ram_mb,
              hw_.gpu.c_str(), hw_.vram_mb, hw_.refresh);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
    ImGui_ImplSDLRenderer3_Init(renderer_);
    Ui::LoadFonts(Paths::ToUtf8(Paths::Assets() / "fonts"));
    UpdateScale();

    video_.Init(renderer_);
    audio_.Open();
    ApplyAudioSettings();
    input_.Init();
    input_.Load(ini_);
    LoadAssets();

    core_.on_audio = [this](const int16_t* data, size_t frames) { audio_.Push(data, frames); };
    core_.on_sensor = [this](unsigned id) { return input_.Sensor(id); };
    core_.on_message = [this](const std::string& text, int ms) { Toast(text, GuideId::MechaRidds, std::clamp(ms / 1000.0, 1.5, 6.0)); };
    for (const auto& sys : Systems()) {
        std::error_code ec;
        if (!fs::exists(Paths::Cores() / CoreFile(sys), ec)) {
            missing_cores_.push_back(sys.full);
            Log::Error("Falta el núcleo %s de %s", CoreFile(sys).c_str(), sys.full.c_str());
        }
    }

    library_.Load();
    Rescan();

    if (!settings_.first_run_done || qa_.count("welcome")) show_welcome_ = true;
    if (settings_.fullscreen) SetFullscreen(true);

    // Juana avisa si hay una build nueva desde la última vez.
    if (build_ > 0 && settings_.last_seen_build > 0 && build_ > settings_.last_seen_build)
        Toast("¡Build " + std::to_string(build_) + " instalada! Mira las novedades.", GuideId::Juana, 6.0);
    if (build_ > 0) settings_.last_seen_build = build_;

    if (settings_.auto_update && build_ > 0 && !qa_.count("fresh")) updater_.Check(build_);

    SDL_ShowWindow(window_);
    last_ns_ = SDL_GetTicksNS();

    if (qa_.count("page")) {
        const std::string pg = qa_["page"];
        page_ = pg == "controles" ? Page::Controls : pg == "sugerida" ? Page::Suggested : pg == "novedades" ? Page::News : pg == "ajustes" ? Page::Settings
              : pg == "acerca" ? Page::About : Page::Library;
    }
    if (qa_.count("sistema")) {
        lib_system_ = qa_["sistema"];
        ctrl_system_ = qa_["sistema"];
    }
    if (qa_.count("ctrl")) ctrl_tab_ = std::atoi(qa_["ctrl"].c_str());
    if (qa_.count("tab")) tab_ = static_cast<SettingsTab>(std::clamp(std::atoi(qa_["tab"].c_str()), 0, 7));
    if (qa_.count("rom")) StartGame(Paths::FromUtf8(qa_["rom"]));
    return true;
}

void App::LoadAssets() {
    const fs::path a = Paths::Assets();
    logo_ = Ui::LoadImage(renderer_, Paths::ToUtf8(a / "logo.png"));
    for (int i = 0; i < static_cast<int>(GuideId::Count); ++i)
        guides_[i] = Ui::LoadImage(renderer_, Paths::ToUtf8(a / "guias" / (std::string(Guide(static_cast<GuideId>(i)).key) + ".png")));

    auto read = [](const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    };
    news_ = Json::Parse(read(a / "novedades.json"));
    const Json tips = Json::Parse(read(a / "consejos.json"));
    for (const auto& t : tips["consejos"].arr) {
        const std::string g = t["guia"].String();
        GuideId id = GuideId::MechaRidds;
        for (int i = 0; i < static_cast<int>(GuideId::Count); ++i)
            if (g == Guide(static_cast<GuideId>(i)).key) id = static_cast<GuideId>(i);
        tips_.push_back({id, t["texto"].String()});
    }
    if (!tips_.empty()) tip_index_ = static_cast<int>(SDL_GetTicks() / 1000) % static_cast<int>(tips_.size());
}

float App::AutoScale() const {
    // Windows al 100 % en una pantalla 1440p o 4K deja todo muy pequeño: se agranda solo.
    float k = 1.0f;
    if (const SDL_DisplayMode* m = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window_))) {
        const float logical_h = m->h / std::max(1.0f, display_scale_);
        if (logical_h >= 2000) k = 1.5f;
        else if (logical_h >= 1350) k = 1.25f;
    }
    return k;
}

void App::UpdateScale() {
    display_scale_ = SDL_GetWindowDisplayScale(window_);
    if (display_scale_ <= 0.1f) display_scale_ = 1.0f;
    const float user = settings_.ui_scale > 0.0f ? settings_.ui_scale : AutoScale();
    Ui::SetScale(display_scale_ * user);
    Ui::ApplyStyle();
    Log::Info("Escala de la interfaz: %.2f (pantalla %.2f × %s %.2f)", display_scale_ * user, display_scale_,
              settings_.ui_scale > 0.0f ? "elegida" : "automática", user);
}

void App::Shutdown() {
    if (in_game_) StopGame();
    int w, h;
    const bool maximized = SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED;
    if (!maximized && !(SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN)) {
        SDL_GetWindowSize(window_, &w, &h);
        ini_.SetInt("ventana.ancho", w);
        ini_.SetInt("ventana.alto", h);
    }
    ini_.SetBool("ventana.maximizada", maximized);
    SaveConfig();
    library_.Save();
    core_.UnloadCore();
    for (auto& [k, img] : slot_thumbs_) Ui::FreeImage(img);
    Ui::FreeImage(logo_);
    for (auto& g : guides_) Ui::FreeImage(g);
    video_.Shutdown();
    audio_.Close();
    input_.Shutdown();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    Log::Info("Hasta pronto.");
    Log::Close();
    SDL_Quit();
}

void App::SaveConfig() {
    settings_.Save(ini_);
    input_.Save(ini_);
    if (core_.CoreLoaded()) {
        const std::string prefix = "nucleo." + Paths::SafeName(core_name_) + ".";
        for (const auto& o : core_.Options()) {
            if (o.current != o.def) ini_.Set(prefix + o.key, o.current);
            else ini_.Erase(prefix + o.key);
        }
    }
    if (!qa_.count("fresh")) ini_.Save(Paths::ConfigFile());
}

void App::ApplyAudioSettings() {
    audio_.SetVolume(settings_.volume);
    audio_.SetLatencyMs(settings_.latency_ms);
}

void App::ApplySuggestion() {
    const Suggestion s = Suggest(hw_, settings_);
    settings_ = s.result;
    settings_.preset = s.tier;
    ApplyAudioSettings();
    SDL_SetRenderVSync(renderer_, settings_.vsync ? 1 : 0);
    SaveConfig();
    Toast("Configuración " + s.tier + " aplicada. ¡Listo para jugar!", GuideId::Nono, 4.5);
}

void App::Rescan() { library_.Rescan(settings_.folders); }

bool App::EnsureCore(const SystemDef& sys, std::string* error) {
    const std::string file = CoreFile(sys);
    if (core_.CoreLoaded() && core_file_ == file) return true;
    if (core_.CoreLoaded()) SaveConfig(); // guarda las opciones del núcleo anterior
    core_ok_ = core_.LoadCore(Paths::Cores() / file, error);
    core_file_ = core_ok_ ? file : std::string();
    if (!core_ok_) return false;
    core_name_ = core_.SystemInfo().library_name ? core_.SystemInfo().library_name : "núcleo";
    for (const auto& [key, value] : sys.core_defaults) core_.OverrideDefault(key, value);
    const std::string prefix = "nucleo." + Paths::SafeName(core_name_) + ".";
    core_.ApplySavedOptions([&](const std::string& key) { return ini_.Get(prefix + key); });
    return true;
}

void App::UpdateRefresh() {
    float hz = 0;
    if (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) {
        if (const SDL_DisplayMode* m = SDL_GetWindowFullscreenMode(window_)) hz = m->refresh_rate;
    }
    if (hz <= 1) {
        if (const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window_))) hz = m->refresh_rate;
    }
    if (hz > 1 && std::fabs(hz - hw_.refresh) > 0.5f) Log::Info("La pantalla va ahora a %.2f Hz", hz);
    if (hz > 1) hw_.refresh = hz;
    core_.target_refresh = hw_.refresh;
}

void App::SetFullscreen(bool on) {
    settings_.fullscreen = on;
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
    if (on && settings_.fullscreen_60hz) {
        // En pantallas de 120/144 Hz, la NES (60 Hz) se ve perfectamente pareja si la pantalla
        // pasa a 60 Hz mientras se juega en pantalla completa.
        const SDL_DisplayMode* desk = SDL_GetDesktopDisplayMode(display);
        SDL_DisplayMode mode{};
        if (desk && desk->refresh_rate > 75 && SDL_GetClosestFullscreenDisplayMode(display, desk->w, desk->h, 60.0f, true, &mode) &&
            mode.refresh_rate < 75) {
            SDL_SetWindowFullscreenMode(window_, &mode);
            Log::Info("Pantalla completa a %dx%d a %.2f Hz", mode.w, mode.h, mode.refresh_rate);
        } else {
            SDL_SetWindowFullscreenMode(window_, nullptr);
        }
    } else {
        SDL_SetWindowFullscreenMode(window_, nullptr);
    }
    SDL_SetWindowFullscreen(window_, on);
}

void App::Toast(const std::string& text, GuideId guide, double seconds) {
    const double now = SDL_GetTicks() / 1000.0;
    // Si el mismo aviso sigue en pantalla, solo se alarga.
    for (auto& t : toasts_) {
        if (t.text == text) {
            t.until = now + seconds;
            return;
        }
    }
    toasts_.push_back({text, guide, now, now + seconds});
    if (toasts_.size() > 4) toasts_.erase(toasts_.begin());
}

// ---------------------------------------------------------------- ciclo principal

void App::Frame() {
    const Uint64 now = SDL_GetTicksNS();
    double dt = static_cast<double>(now - last_ns_) / 1e9;
    last_ns_ = now;
    dt = std::min(dt, 0.25);
    loop_ema_ = loop_ema_ * 0.95 + dt * 0.05;

    SDL_Event e;
    while (SDL_PollEvent(&e)) HandleEvent(e);
    ProcessPendingPaths();

    if (library_.TakeChanged()) games_dirty_ = true;
    PollUpdater();

    // El cursor se esconde mientras se juega.
    const bool want_hidden = in_game_ && !menu_open_ && SDL_GetTicks() - last_mouse_move_ > 2000;
    if (want_hidden != cursor_hidden_) {
        cursor_hidden_ = want_hidden;
        if (want_hidden) SDL_HideCursor();
        else SDL_ShowCursor();
    }

    // Atajos (teclado y mando). Select+Start en el mando también abre el menú.
    {
        const bool typing = ImGui::GetIO().WantTextInput;
        for (Hotkey h : input_.TakeHotkeyPresses())
            if (!typing) DoHotkey(h);
        if (in_game_ && input_.MenuComboPressed()) SetMenuOpen(!menu_open_);
    }

    ImGuiIO& io = ImGui::GetIO();
    // Mientras se juega, el mando es del juego; la interfaz solo lo usa con el menú abierto.
    if ((in_game_ && !menu_open_) || input_.Capturing()) io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    else io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    if (in_game_ && !paused_ && !menu_open_) EmulateTick(dt);
    if (in_game_) video_.Upload(core_);

    usage_timer_ += dt;
    if (usage_timer_ > 0.5) {
        usage_timer_ = 0;
        proc_ = usage_.Sample(hw_.threads);
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    DrawUi();
    ImGui::Render();

    int pw, ph;
    SDL_GetRenderOutputSize(renderer_, &pw, &ph);
    SDL_SetRenderDrawColor(renderer_, 15, 13, 23, 255);
    SDL_RenderClear(renderer_);
    if (in_game_) {
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        video_.Draw(core_, settings_, pw, ph, sys_->handheld);
    }
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
    QaTick();
    SDL_RenderPresent(renderer_);

    if (!settings_.vsync || (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED)) {
        // Sin sincronización vertical se descansa un poco para no gastar CPU de más.
        if (!in_game_ || (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED)) SDL_Delay(8);
        else SDL_DelayPrecise(1000000);
    }
}

void App::PollUpdater() {
    const Updater::State st = updater_.GetState();
    if (st == last_update_state_) return;
    last_update_state_ = st;
    switch (st) {
    case Updater::State::Available:
        Toast("¡Salió la build " + std::to_string(updater_.LatestBuild()) + "! Ve a Novedades para actualizar.", GuideId::Juana, 7);
        break;
    case Updater::State::Ready:
        if (updater_.LaunchInstaller()) {
            Log::Info("Instalando la build %d…", updater_.LatestBuild());
            running_ = false;
        } else {
            Toast("No se pudo abrir el instalador. Descárgala desde GitHub.", GuideId::Juana, 6);
        }
        break;
    case Updater::State::Failed:
        Log::Warn("Actualizaciones: %s", updater_.Error().c_str());
        break;
    default:
        break;
    }
}

void App::QaTick() {
    if (qa_.empty()) return;
    ++qa_frames_;
    const int wanted = qa_.count("frames") ? std::atoi(qa_["frames"].c_str()) : 30;
    if (qa_frames_ == wanted / 2 && qa_.count("menu")) {
        SetMenuOpen(true);
        if (qa_.count("seccion")) menu_section_ = std::atoi(qa_["seccion"].c_str());
    }
    if (qa_.count("estados") && in_game_) {
        if (qa_frames_ == wanted / 3) SaveSlot(2);
        if (qa_frames_ == wanted * 2 / 3) LoadSlot(2);
    }
    if (qa_frames_ < wanted) return;
    if (qa_.count("shot")) {
        if (SDL_Surface* s = SDL_RenderReadPixels(renderer_, nullptr)) {
            SDL_Surface* c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
            if (c) {
                Ui::SavePng(qa_["shot"], static_cast<unsigned char*>(c->pixels), c->w, c->h);
                SDL_DestroySurface(c);
            }
            SDL_DestroySurface(s);
        }
    }
    running_ = false;
}

void App::ProcessPendingPaths() {
    std::vector<std::string> folders, roms;
    {
        std::lock_guard lock(pending_mutex_);
        folders.swap(pending_folders_);
        roms.swap(pending_roms_);
    }
    if (!folders.empty()) {
        for (const auto& f : folders) {
            if (std::find(settings_.folders.begin(), settings_.folders.end(), f) == settings_.folders.end())
                settings_.folders.push_back(f);
        }
        SaveConfig();
        Rescan();
        Toast("Buscando juegos en la carpeta nueva…", GuideId::Samy);
    }
    if (!roms.empty()) StartGame(Paths::FromUtf8(roms.front()));
}

void App::AddFolderDialog() {
    SDL_ShowOpenFolderDialog(&FolderDialogCallback, this, window_, nullptr, false);
}

void App::OpenRomDialog() {
    static const SDL_DialogFileFilter filters[] = {
        {"Juegos de todas las consolas", "nes;fds;unf;unif;gb;gbc;sgb;gba"},
        {"Satronho Octa (NES)", "nes;fds;unf;unif"},
        {"Satronho Bolsillo (Game Boy y Game Boy Color)", "gb;gbc;sgb"},
        {"Satronho Bolsillo Plus (Game Boy Advance)", "gba"},
        {"Todos los archivos", "*"}};
    SDL_ShowOpenFileDialog(&FileDialogCallback, this, window_, filters, 5, nullptr, false);
}

void App::HandleEvent(const SDL_Event& e) {
    // Si se está asignando un botón, esa tecla no cuenta como atajo.
    const bool was_capturing = input_.Capturing();
    ImGui_ImplSDL3_ProcessEvent(&e);
    input_.HandleEvent(e);
    switch (e.type) {
    case SDL_EVENT_QUIT:
        running_ = false;
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (e.window.windowID == SDL_GetWindowID(window_)) running_ = false;
        break;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        UpdateScale();
        break;
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        UpdateRefresh();
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (in_game_ && !menu_open_) audio_.Clear();
        break;
    case SDL_EVENT_MOUSE_MOTION:
        last_mouse_move_ = SDL_GetTicks();
        break;
    case SDL_EVENT_DROP_FILE:
        if (e.drop.data) {
            const fs::path p = Paths::FromUtf8(e.drop.data);
            std::error_code ec;
            if (fs::is_directory(p, ec)) {
                std::lock_guard lock(pending_mutex_);
                pending_folders_.push_back(e.drop.data);
            } else {
                StartGame(p);
            }
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        // Alt+Enter siempre cambia a pantalla completa.
        if (!e.key.repeat && !was_capturing && (e.key.mod & SDL_KMOD_ALT) && e.key.scancode == SDL_SCANCODE_RETURN) {
            SetFullscreen(!settings_.fullscreen);
            SaveConfig();
        }
        break;
    default:
        break;
    }
}

void App::DoHotkey(Hotkey h) {
    if (h == Hotkey::Fullscreen) {
        SetFullscreen(!settings_.fullscreen);
        SaveConfig();
        return;
    }
    if (!in_game_) {
        if (h == Hotkey::Menu && page_ != Page::Library && !show_welcome_ && !input_.Capturing()) page_ = Page::Library;
        return;
    }
    switch (h) {
    case Hotkey::Menu: SetMenuOpen(!menu_open_); break;
    case Hotkey::SaveState: SaveSlot(slot_); break;
    case Hotkey::LoadState: LoadSlot(slot_); break;
    case Hotkey::PrevSlot:
        slot_ = slot_ <= 1 ? 8 : slot_ - 1;
        Toast("Ranura " + std::to_string(slot_), GuideId::Samy, 1.5);
        break;
    case Hotkey::NextSlot:
        slot_ = slot_ >= 8 ? 1 : slot_ + 1;
        Toast("Ranura " + std::to_string(slot_), GuideId::Samy, 1.5);
        break;
    case Hotkey::Screenshot: Screenshot(); break;
    case Hotkey::Meter:
        settings_.meter = (settings_.meter + 1) % 3;
        SaveConfig();
        break;
    case Hotkey::Pause:
        if (!menu_open_) {
            paused_ = !paused_;
            audio_.SetPaused(paused_);
            Toast(paused_ ? "En pausa" : "¡Seguimos!", GuideId::Samy, 1.5);
        }
        break;
    case Hotkey::Reset:
        core_.Reset();
        Toast("Juego reiniciado", GuideId::Samy, 2.0);
        break;
    default: break;
    }
}

// ---------------------------------------------------------------- juego

bool App::StartGame(const fs::path& rom) {
    if (in_game_) StopGame();
    Game probe = Library::MakeGame(rom);
    const SystemDef* sys = SystemById(probe.system);
    if (!sys) {
        Toast("Este archivo no es de ninguna consola de Satronho Emulation.", GuideId::MechaRidds, 5);
        return false;
    }
    std::string err;
    if (!EnsureCore(*sys, &err)) {
        Toast("Falta el núcleo de " + sys->full + ". Vuelve a descargar Satronho Emulation.", GuideId::MechaRidds, 6);
        Log::Error("%s", err.c_str());
        return false;
    }
    sys_ = sys;
    if (!core_.LoadGame(rom, &err)) {
        Toast(err, GuideId::MechaRidds, 6);
        return false;
    }
    current_ = Library::MakeGame(rom);
    library_.MarkPlayed(current_.key);
    games_dirty_ = true;
    core_.target_refresh = hw_.refresh;
    audio_.SetPaused(false);
    audio_.Configure(core_.SampleRate());
    ApplyAudioSettings();
    video_.Invalidate();
    rewind_.clear();
    emu_frames_ = 0;
    acc_ = 0;
    session_s_ = 0;
    uncommitted_s_ = 0;
    sram_timer_ = 0;
    paused_ = false;
    menu_open_ = false;
    in_game_ = true;
    perf_ = Perf{};
    RefreshSlotThumbs();
    SDL_SetWindowTitle(window_, (current_.title + " — Satronho Emulation").c_str());
    Toast("¡A jugar! Esc abre el menú.", GuideId::Samy, 3.0);
    return true;
}

void App::CommitPlaytime() {
    const long long secs = static_cast<long long>(uncommitted_s_);
    if (secs > 0) {
        library_.AddPlaytime(current_.key, secs);
        uncommitted_s_ -= static_cast<double>(secs);
        games_dirty_ = true;
    }
}

void App::StopGame() {
    if (!in_game_) return;
    CommitPlaytime();
    core_.UnloadGame();
    audio_.Clear();
    audio_.SetPaused(true);
    in_game_ = false;
    menu_open_ = false;
    paused_ = false;
    rewind_.clear();
    rewind_.shrink_to_fit();
    for (auto& [k, img] : slot_thumbs_) Ui::FreeImage(img);
    slot_thumbs_.clear();
    SDL_SetWindowTitle(window_, "Satronho Emulation");
    if (cursor_hidden_) {
        SDL_ShowCursor();
        cursor_hidden_ = false;
    }
}

void App::SetMenuOpen(bool open) {
    if (!in_game_) return;
    menu_open_ = open;
    audio_.SetPaused(open || paused_);
    if (open) {
        menu_section_ = 0;
        RefreshSlotThumbs();
        core_.SaveSram();
    }
}

void App::EmulateTick(double dt) {
    const double fps = core_.Fps();
    const bool* ks = SDL_GetKeyboardState(nullptr);
    const bool typing = ImGui::GetIO().WantTextInput;
    (void)ks;
    fast_forward_ = !typing && input_.HotkeyHeld(Hotkey::FastForward);
    rewinding_ = settings_.rewind && !typing && input_.HotkeyHeld(Hotkey::Rewind);
    core_.fast_forwarding = fast_forward_;

    // Si la pantalla va casi a la misma velocidad que la consola, se corre un cuadro por
    // refresco y el sonido se ajusta solo (más suave). Si no, se lleva la cuenta del tiempo.
    const bool vblank_sync = settings_.vsync && std::fabs(hw_.refresh - fps) < fps * 0.015 && loop_ema_ > 0.85 / hw_.refresh;
    int frames = 0;
    if (vblank_sync) {
        frames = 1;
        acc_ = 0;
    } else {
        acc_ += dt;
        const double period = 1.0 / fps;
        while (acc_ >= period && frames < 4) {
            ++frames;
            acc_ -= period;
        }
        if (acc_ > 0.1) acc_ = 0;
    }
    if (fast_forward_) frames *= settings_.fast_forward;
    core_.audio_enabled = !(fast_forward_ && settings_.mute_fast_forward);
    for (int i = 0; i < frames; ++i) Step(rewinding_, fast_forward_);
    core_.audio_enabled = true;
    audio_.Update();

    // Medidor.
    const double now = SDL_GetTicks() / 1000.0;
    perf_.host_ms[perf_.head] = static_cast<float>(dt * 1000.0);
    perf_.core_ms[perf_.head] = static_cast<float>(core_.last_run_ms);
    perf_.head = (perf_.head + 1) % 180;
    if (dt > 2.5 / std::max(30.0f, hw_.refresh) && emu_frames_ > 30 && !fast_forward_) {
        ++perf_.stutters;
        ++perf_.stutters_minute;
    }
    if (now - perf_.stutter_minute_start > 60) {
        perf_.stutter_minute_start = now;
        perf_.stutters_minute = 0;
    }
    while (!perf_.emu_times.empty() && now - perf_.emu_times.front() > 1.0) perf_.emu_times.pop_front();
    perf_.fps = static_cast<double>(perf_.emu_times.size());

    // Tiempo de juego y partida de la batería.
    session_s_ += dt;
    uncommitted_s_ += dt;
    sram_timer_ += dt;
    if (uncommitted_s_ > 60) CommitPlaytime();
    if (sram_timer_ > 30) {
        sram_timer_ = 0;
        core_.SaveSram();
    }
}

void App::Step(bool rewinding, bool fast_forward) {
    input_.Update(core_, *sys_, !ImGui::GetIO().WantTextInput);
    perf_.emu_times.push_back(SDL_GetTicks() / 1000.0);
    if (rewinding) {
        if (!rewind_.empty()) {
            core_.LoadState(rewind_.back());
            rewind_.pop_back();
            const bool a = core_.audio_enabled;
            core_.audio_enabled = false;
            core_.RunFrame();
            core_.audio_enabled = a;
        }
        return;
    }
    // Con mGBA el adelanto hace sonar raro el audio (guarda el sonido fuera del estado), así que
    // solo se usa en los núcleos que lo manejan bien.
    if (settings_.runahead > 0 && !fast_forward && sys_->runahead_ok) RunAheadFrame(settings_.runahead);
    else core_.RunFrame();
    ++emu_frames_;
    if (settings_.rewind && emu_frames_ % 2 == 0) PushRewind();
}

void App::RunAheadFrame(int frames) {
    // Adelanto de cuadros: se corre el cuadro real sin mostrarlo, se guarda el estado, se
    // corren N cuadros "del futuro" y se muestra el último; luego se vuelve al estado real.
    // Así lo que se ve responde N cuadros antes a los botones.
    core_.video_enabled = false;
    core_.RunFrame();
    if (!core_.SaveState(runahead_state_)) {
        core_.video_enabled = true;
        settings_.runahead = 0;
        Toast("Este juego no admite el adelanto de cuadros; lo apagué.", GuideId::Nono, 4);
        return;
    }
    const bool audio = core_.audio_enabled;
    core_.audio_enabled = false;
    for (int i = 0; i < frames - 1; ++i) core_.RunFrame();
    core_.video_enabled = true;
    core_.RunFrame();
    core_.LoadState(runahead_state_);
    core_.audio_enabled = audio;
}

void App::PushRewind() {
    const size_t max = static_cast<size_t>(settings_.rewind_seconds * core_.Fps() / 2.0);
    std::vector<uint8_t> buf;
    if (rewind_.size() >= max && !rewind_.empty()) {
        buf = std::move(rewind_.front());
        rewind_.pop_front();
    }
    if (core_.SaveState(buf)) rewind_.push_back(std::move(buf));
}

fs::path App::StatePath(int slot) const {
    return Paths::States() / (current_.key + "-" + Paths::SafeName(current_.title) + ".estado" + std::to_string(slot));
}

void App::RefreshSlotThumbs() {
    for (auto& [k, img] : slot_thumbs_) Ui::FreeImage(img);
    slot_thumbs_.clear();
    slot_dates_.clear();
    if (!in_game_) return;
    for (int s = 1; s <= 8; ++s) {
        const fs::path p = StatePath(s);
        std::error_code ec;
        if (!fs::exists(p, ec)) continue;
        fs::path png = p;
        png += ".png";
        slot_thumbs_[s] = Ui::LoadImage(renderer_, Paths::ToUtf8(png));
        const auto ft = fs::last_write_time(p, ec);
        if (!ec) {
            const auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            const std::time_t t = std::chrono::system_clock::to_time_t(sys);
            slot_dates_[s] = FormatLastPlayed(static_cast<long long>(t));
        }
    }
}

void App::SaveSlot(int slot) {
    const fs::path p = StatePath(slot);
    if (!core_.SaveStateFile(p)) {
        Toast("No se pudo guardar el estado.", GuideId::Samy, 3);
        return;
    }
    unsigned w, h;
    const auto rgba = core_.FrameRGBA(&w, &h);
    if (!rgba.empty()) {
        fs::path png = p;
        png += ".png";
        Ui::SavePng(Paths::ToUtf8(png), rgba.data(), static_cast<int>(w), static_cast<int>(h));
    }
    RefreshSlotThumbs();
    Toast("Estado guardado en la ranura " + std::to_string(slot), GuideId::Samy, 2.2);
}

void App::LoadSlot(int slot) {
    if (!core_.LoadStateFile(StatePath(slot))) {
        Toast("La ranura " + std::to_string(slot) + " está vacía.", GuideId::Samy, 2.5);
        return;
    }
    rewind_.clear();
    audio_.Clear();
    Toast("Estado de la ranura " + std::to_string(slot) + " cargado", GuideId::Samy, 2.2);
}

void App::Screenshot() {
    unsigned w, h;
    const auto rgba = core_.FrameRGBA(&w, &h);
    if (rgba.empty()) return;
    char stamp[64];
    const std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H.%M.%S", std::localtime(&t));
    const fs::path p = Paths::Screenshots() / (Paths::SafeName(current_.title) + " " + stamp + ".png");
    if (Ui::SavePng(Paths::ToUtf8(p), rgba.data(), static_cast<int>(w), static_cast<int>(h)))
        Toast("Captura guardada en la carpeta de capturas", GuideId::Troya, 2.5);
}

} // namespace Satronho
