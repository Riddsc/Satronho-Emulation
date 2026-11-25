// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.h"
#include "sys/android.h"
#include "sys/native_ui.h"
#include "util/i18n.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_impl_opengl3.h"

#include "util/chd.h"
#include "util/hash.h"
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

namespace {
// Texturas de la interfaz con el dibujador de SDL.
SDL_Renderer* g_ui_renderer = nullptr;
ImTextureID SdlCreateTexture(const unsigned char* rgba, int w, int h, bool smooth) {
    SDL_Texture* t = SDL_CreateTexture(g_ui_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) return ImTextureID_Invalid;
    SDL_UpdateTexture(t, nullptr, rgba, w * 4);
    SDL_SetTextureScaleMode(t, smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(t));
}
void SdlDestroyTexture(ImTextureID id) { SDL_DestroyTexture(reinterpret_cast<SDL_Texture*>(static_cast<intptr_t>(id))); }

// Texturas de la interfaz con OpenGL.
ImTextureID GlCreateTexture(const unsigned char* rgba, int w, int h, bool smooth) {
    GLuint t = 0;
    Gl::GenTextures(1, &t);
    Gl::BindTexture(GL_TEXTURE_2D, t);
    Gl::PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    Gl::PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    Gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    Gl::BindTexture(GL_TEXTURE_2D, 0);
    return static_cast<ImTextureID>(t);
}
void GlDestroyTexture(ImTextureID id) {
    const GLuint t = static_cast<GLuint>(id);
    Gl::DeleteTextures(1, &t);
}
} // namespace

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

void CoverDialogCallback(void* user, const char* const* list, int) {
    auto* app = static_cast<App*>(user);
    if (!list || !*list) return;
    std::lock_guard lock(app->pending_mutex_);
    app->pending_covers_.push_back(*list);
}

void BiosDialogCallback(void* user, const char* const* list, int) {
    auto* app = static_cast<App*>(user);
    if (!list || !*list) return;
    std::lock_guard lock(app->pending_mutex_);
    for (; *list; ++list) app->pending_bios_.push_back(*list);
}

// ---------------------------------------------------------------- inicio y cierre

int App::Run(int argc, char** argv) {
    // Un juego en la línea de comandos (accesos directos del escritorio): se abre al arrancar.
    if (argc > 1 && argv[1] && argv[1][0] != '-') pending_roms_.push_back(argv[1]);
    if (!Init()) return 1;
    while (running_) Frame();
    Shutdown();
    return qa_failed_ ? 2 : 0; // el modo de revisión avisa al CI si una prueba falló
}

bool App::Init() {
    build_ = SATRONHO_BUILD;
    SDL_SetAppMetadata("Satronho Emulation", "1.0", "com.ridds.satronho.emulation");
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_STREAM_NAME, "Satronho Emulation");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");
    // Android: el botón «atrás» lo maneja la app (menú del juego, volver, cerrar con pregunta).
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
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
    Log::InstallCrashHandler();
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
    // Hasta la build 12, CD usaba Beetle PSX solo por software («beetle_psx_…»); ahora es la
    // versión con tarjeta gráfica («beetle_psx_hw_…»): los cambios de la persona se pasan solos.
    {
        std::vector<std::pair<std::string, std::string>> moved;
        for (const auto& [k, v] : ini_.Values())
            if (k.rfind("nucleo.cd.beetle_psx_", 0) == 0 && k.rfind("nucleo.cd.beetle_psx_hw_", 0) != 0) moved.emplace_back(k, v);
        for (const auto& [k, v] : moved) {
            const std::string nk = "nucleo.cd.beetle_psx_hw_" + k.substr(std::strlen("nucleo.cd.beetle_psx_"));
            if (!ini_.Values().count(nk)) ini_.Set(nk, v);
            ini_.Erase(k);
        }
        if (!moved.empty()) Log::Info("Opciones de CD pasadas a Beetle PSX HW: %zu", moved.size());
    }
    settings_.Load(ini_);
    MigrateGfxOptions();
    if (qa_.count("fresh")) settings_ = Settings{};
    if (qa_.count("gpu")) settings_.gpu_api["cd"] = settings_.gpu_api["n64"] = std::atoi(qa_["gpu"].c_str());
    if (qa_.count("res")) settings_.gpu_res["cd"] = settings_.gpu_res["n64"] = std::atoi(qa_["res"].c_str());
    if (qa_.count("escala")) settings_.ui_scale = static_cast<float>(std::atof(qa_["escala"].c_str()));
    if (qa_.count("tema")) settings_.theme = std::atoi(qa_["tema"].c_str());
    if (qa_.count("vista")) settings_.lib_view[qa_.count("sistema") ? qa_["sistema"] : "todas"] = std::atoi(qa_["vista"].c_str());
    if (qa_.count("idioma")) settings_.language = qa_["idioma"];
    I18n::Load(Paths::Assets() / "idiomas", settings_.language);
    Ui::ApplyTheme(settings_.theme);

    // Motor gráfico: OpenGL (el de Satronho, con sombreadores y dibujo por tarjeta gráfica para
    // CD y 64) o el clásico de SDL (Direct3D…), que queda de respaldo si OpenGL no arranca.
    std::string engine = settings_.renderer;
    if (const char* env = SDL_getenv("SATRONHO_GFX")) engine = env;
    bool want_gl = engine == "auto" || engine == "opengl";
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (want_gl) {
        flags |= SDL_WINDOW_OPENGL;
#ifdef __ANDROID__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    }
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
    if (!window_ && want_gl) {
        Log::Warn("No se pudo crear la ventana con OpenGL (%s); sigo con el motor clásico", SDL_GetError());
        want_gl = false;
        window_ = SDL_CreateWindow("Satronho Emulation", ww, wh, flags & ~SDL_WINDOW_OPENGL);
    }
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

    if (want_gl) {
        gl_ctx_ = SDL_GL_CreateContext(window_);
        if (gl_ctx_ && SDL_GL_MakeCurrent(window_, gl_ctx_) && Gl::Load() && video_gl_.Init()) {
            gl_ = true;
            const char* ver = reinterpret_cast<const char*>(Gl::GetString(GL_VERSION));
            const char* gpu = reinterpret_cast<const char*>(Gl::GetString(GL_RENDERER));
            Log::Info("Motor gráfico: OpenGL %s · %s", ver ? ver : "?", gpu ? gpu : "?");
        } else {
            Log::Warn("OpenGL 3.3 no está disponible (%s); sigo con el motor clásico", SDL_GetError());
            if (gl_ctx_) SDL_GL_DestroyContext(gl_ctx_);
            gl_ctx_ = nullptr;
        }
    }
    if (!gl_) {
        const char* wanted = (engine == "auto" || engine == "opengl") ? nullptr : engine.c_str();
        renderer_ = SDL_CreateRenderer(window_, wanted);
        if (!renderer_ && wanted) renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) {
            Log::Error("No se pudo crear el dibujador: %s", SDL_GetError());
            return false;
        }
        Log::Info("Motor gráfico: %s (clásico)", SDL_GetRendererName(renderer_));
    }
    ApplyVsync();

    hw_ = DetectHardware(window_, renderer_);
    if (gl_) {
        const char* ver = reinterpret_cast<const char*>(Gl::GetString(GL_VERSION));
        std::string v = ver ? ver : "";
        if (const size_t sp = v.find(' '); sp != std::string::npos) v = v.substr(0, sp);
        hw_.renderer = "OpenGL " + v;
        const char* gpu = reinterpret_cast<const char*>(Gl::GetString(GL_RENDERER));
        const std::string g = gpu ? gpu : "";
        hw_.software = g.find("llvmpipe") != std::string::npos || g.find("softpipe") != std::string::npos || g.find("GDI Generic") != std::string::npos;
        if ((hw_.gpu.empty() || hw_.gpu == "Desconocida") && !g.empty()) hw_.gpu = g;
        hw_.gpu_render = !hw_.software;
        hw_render_.Init(window_, gl_ctx_);
        core_.hw_host = &hw_render_;
        hw_render_.ProbeVulkan();
    }
    Log::Info("Equipo: %s · %d hilos · %d MB RAM · %s (%d MB) · %.0f Hz", hw_.cpu.c_str(), hw_.threads, hw_.ram_mb,
              hw_.gpu.c_str(), hw_.vram_mb, hw_.refresh);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    if (gl_) {
        ImGui_ImplSDL3_InitForOpenGL(window_, gl_ctx_);
        ImGui_ImplOpenGL3_Init(Gl::Es() ? "#version 300 es" : "#version 330 core");
        Ui::SetTextureBackend({&GlCreateTexture, &GlDestroyTexture});
    } else {
        ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
        ImGui_ImplSDLRenderer3_Init(renderer_);
        g_ui_renderer = renderer_;
        Ui::SetTextureBackend({&SdlCreateTexture, &SdlDestroyTexture});
    }
    Ui::LoadFonts(Paths::ToUtf8(Paths::Assets() / "fonts"));
    UpdateScale();

    if (!gl_) video_.Init(renderer_);
    covers_.Init();
    audio_.Open();
    ApplyAudioSettings();
    input_.Init();
    input_.Load(ini_);
    LoadAssets();

    core_.on_audio = [this](const int16_t* data, size_t frames) { audio_.Push(data, frames); };
    core_.on_sensor = [this](unsigned id) { return input_.Sensor(id); };
    core_.on_message = [this](const std::string& text, int ms) {
        // El mismo aviso repetido seguido (algunos núcleos lo mandan en cada cuadro) sale una vez.
        const double now = SDL_GetTicks() / 1000.0;
        if (text == last_core_msg_ && now - last_core_msg_t_ < 8.0) return;
        last_core_msg_ = text;
        last_core_msg_t_ = now;
        Toast(text, GuideId::MechaRidds, std::clamp(ms / 1000.0, 1.5, 6.0));
    };
    for (const auto& sys : Systems()) {
        std::error_code ec;
        if (!fs::exists(Paths::Cores() / CoreFile(sys), ec)) {
            missing_cores_.push_back(sys.full);
            Log::Error("Falta el núcleo %s de %s", CoreFile(sys).c_str(), sys.full.c_str());
        }
    }

    library_.Load();
    Rescan();

    SetupPublicFolder();
    if (!settings_.first_run_done || qa_.count("welcome")) show_welcome_ = true;
#ifdef __ANDROID__
    SetFullscreen(true);
    Android::SetOrientation(settings_.orientation);
#else
    if (settings_.fullscreen) SetFullscreen(true);
#endif

    // Juana muestra lo nuevo si hay una build nueva desde la última vez.
    if ((build_ > 0 && settings_.last_seen_build > 0 && build_ > settings_.last_seen_build) || qa_.count("novedades_ventana"))
        show_whats_new_ = true;
    if (build_ > 0) settings_.last_seen_build = build_;

    if (settings_.auto_update && build_ > 0 && !qa_.count("fresh")) updater_.Check(build_);

    SDL_ShowWindow(window_);
    last_ns_ = SDL_GetTicksNS();

    if (qa_.count("page")) {
        const std::string pg = qa_["page"];
        page_ = pg == "controles" ? Page::Controls : pg == "sugerida" ? Page::Suggested : pg == "novedades" ? Page::News : pg == "ajustes" ? Page::Settings
              : pg == "acerca" ? Page::About : pg == "guias" ? Page::Guides : pg == "herramientas" ? Page::Tools : Page::Library;
        shown_page_ = page_;
    }
    if (qa_.count("preguntar")) {
        for (std::string q : {std::string("hola"), qa_["preguntar"]}) AskGuides(q);
    }
    if (qa_.count("salir")) confirm_ = std::atoi(qa_["salir"].c_str());
    if (qa_.count("barra")) bar_open_ = true;
    if (qa_.count("recibir")) QueueImport(Paths::FromUtf8(qa_["recibir"]));
    if (qa_.count("encima")) { overlay_page_ = std::atoi(qa_["encima"].c_str()); overlay_focus_ = true; }
    if (qa_.count("ventana")) {
        SDL_SetWindowMinimumSize(window_, 200, 200); // modo de revisión: tamaños de celular
        int w = 0, h = 0;
        if (std::sscanf(qa_["ventana"].c_str(), "%dx%d", &w, &h) == 2) SDL_SetWindowSize(window_, w, h);
    }
    if (qa_.count("escala_ui")) { settings_.ui_scale = static_cast<float>(std::atof(qa_["escala_ui"].c_str())); UpdateScale(); }
    if (qa_.count("sistema")) {
        lib_system_ = qa_["sistema"];
        ctrl_system_ = qa_["sistema"];
    }
    if (qa_.count("gfx")) gfx_sys_ = qa_["gfx"];
    if (qa_.count("ctrl")) ctrl_tab_ = std::atoi(qa_["ctrl"].c_str());
    if (qa_.count("tab")) tab_ = static_cast<SettingsTab>(std::clamp(std::atoi(qa_["tab"].c_str()), 0, static_cast<int>(SettingsTab::Count) - 1));
    if (qa_.count("rom")) {
        // «continuar=1»: como el botón Continuar (pasa por «Cargando…» y la ranura automática).
        if (qa_.count("continuar")) RequestStart(Paths::FromUtf8(qa_["rom"]), true);
        else StartGame(Paths::FromUtf8(qa_["rom"]));
    }
    if (qa_.count("chd")) {
        std::string err;
        fs::path in = Paths::FromUtf8(qa_["chd"]);
        fs::path out = in;
        out.replace_extension(".chd");
        const bool ok = Chd::FromCue(in, out, &err);
        Log::Info("Revisión: conversión a CHD %s %s", ok ? "bien" : "falló:", err.c_str());
    }
    if (qa_.count("tactil")) touch_mode_ = true;
    // La presentación animada se salta en las revisiones automáticas (salvo que se pida).
    if (!qa_.empty() && !qa_.count("presentacion")) splash_done_ = true;
    if (qa_.count("explorador")) OpenBrowser(qa_["explorador"] == "1");
    if (qa_.count("editar_tactil")) touch_edit_ = true;
    if (qa_.count("abrir")) RequestStart(Paths::FromUtf8(qa_["abrir"]));
    if (qa_.count("ajustes_juego")) game_settings_ = Library::MakeGame(Paths::FromUtf8(qa_["ajustes_juego"]));
    if (qa_.count("publica")) BeginDataMove(Paths::FromUtf8(qa_["publica"])); // prueba de la mudanza de Android
    if (qa_.count("descomprimir")) BeginUnpack(Library::MakeGame(Paths::FromUtf8(qa_["descomprimir"])), Paths::FromUtf8(qa_["descomprimir"]));
    if (qa_.count("carpeta")) {
        std::lock_guard lock(pending_mutex_);
        pending_folders_.push_back(qa_["carpeta"]);
    }
    return true;
}

void App::LoadAssets() {
    const fs::path a = Paths::Assets();
    logo_ = Ui::LoadImageFile(Paths::ToUtf8(a / "logo.png"));
    for (int i = 0; i < static_cast<int>(GuideId::Count); ++i)
        guides_[i] = Ui::LoadImageFile(Paths::ToUtf8(a / "guias" / (std::string(Guide(static_cast<GuideId>(i)).key) + ".png")));

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
    chd_cancel_ = true;
    if (chd_thread_.joinable()) chd_thread_.join();
    unpack_cancel_ = true;
    if (unpack_thread_.joinable()) unpack_thread_.join();
    if (move_thread_.joinable()) move_thread_.join();
    if (storage_thread_.joinable()) storage_thread_.join();
    if (verify_thread_.joinable()) verify_thread_.join();
    if (import_thread_.joinable()) import_thread_.join();
    covers_.Shutdown();
    hw_render_.Shutdown();
    if (gl_) video_gl_.Shutdown();
    else video_.Shutdown();
    audio_.Close();
    input_.Shutdown();
    if (gl_) ImGui_ImplOpenGL3_Shutdown();
    else ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (gl_ctx_) SDL_GL_DestroyContext(gl_ctx_);
    SDL_DestroyWindow(window_);
    Log::Info("Hasta pronto.");
    Log::Close();
    SDL_Quit();
}

void App::SaveConfig() {
    settings_.Save(ini_);
    input_.Save(ini_);
    if (core_.CoreLoaded()) SaveCoreOptions(options_sys_, core_.Options());
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
    ApplyVsync();
    SaveConfig();
    Toast("Configuración " + s.tier + " aplicada. ¡Listo para jugar!", GuideId::Nono, 4.5);
}

void App::Rescan() { library_.Rescan(settings_.folders); }

uint64_t App::FreeBytes(const fs::path& dir) {
    std::error_code ec;
    const fs::space_info si = fs::space(dir, ec);
    return ec ? 0 : static_cast<uint64_t>(si.available);
}

// Lo que siempre se deja libre al copiar o descomprimir juegos: el de Ajustes, o más si la consola lo
// pide (las de juegos enormes, como CD2, pueden pedir su propio margen en SystemDef::margin_gb).
uint64_t App::MarginBytes(const SystemDef* sys) const {
    const float gb = std::max(settings_.free_margin_gb, sys ? sys->margin_gb : 0.0f);
    return static_cast<uint64_t>(gb * 1073741824.0);
}

bool App::EnsureCore(const SystemDef& sys, std::string* error) {
    const std::string file = CoreFile(sys);
    if (core_.CoreLoaded() && core_file_ == file) {
        // El mismo núcleo para otra consola (Bolsillo ↔ Bolsillo Plus): se cambian sus opciones.
        if (options_sys_ != sys.id) {
            SaveConfig();
            ApplySystemOptions(sys);
        }
        return true;
    }
    if (core_.CoreLoaded()) SaveConfig(); // guarda las opciones del núcleo anterior
    core_.SetSaveDir(Paths::Saves() / sys.id);
    options_sys_.clear();
    tab_options_sys_.clear();
    core_ok_ = core_.LoadCore(Paths::Cores() / file, error);
    core_file_ = core_ok_ ? file : std::string();
    if (!core_ok_) return false;
    core_name_ = core_.SystemInfo().library_name ? core_.SystemInfo().library_name : "núcleo";
    CoreSnapshot snap;
    snap.options = core_.Options();
    snap.categories = core_.Categories();
    snap.name = core_name_;
    snap.version = core_.SystemInfo().library_version ? core_.SystemInfo().library_version : "";
    core_cache_[file] = std::move(snap);
    ApplySystemOptions(sys);
    return true;
}

std::vector<CoreOption> App::OptionsFor(const SystemDef& sys, std::vector<CoreOption> opts) const {
    ResetToCoreDefaults(opts);
    for (const auto& [key, value] : sys.core_defaults) OverrideDefault(opts, key, value);
    const std::string prefix = "nucleo." + sys.id + ".";
    const bool migrated = ini_.GetBool(prefix + "@migrado", false);
    // Antes de la build 12 las opciones se guardaban por núcleo («nucleo.mGBA.…»); se siguen
    // leyendo hasta que la consola guarde las suyas.
    auto it = core_cache_.find(CoreFile(sys));
    const std::string legacy = "nucleo." + Paths::SafeName(it != core_cache_.end() ? it->second.name : core_name_) + ".";
    ApplySaved(opts, [&](const std::string& key) {
        std::string v = ini_.Get(prefix + key);
        if (v.empty() && !migrated) v = ini_.Get(legacy + key);
        return v;
    });
    return opts;
}

void App::ApplySystemOptions(const SystemDef& sys) {
    auto& opts = core_.Options();
    opts = OptionsFor(sys, opts);
    core_.MarkOptionsChanged();
    options_sys_ = sys.id;
    tab_options_sys_.clear();
}

void App::SaveCoreOptions(const std::string& sys_id, const std::vector<CoreOption>& opts) {
    if (sys_id.empty() || opts.empty()) return;
    const std::string prefix = "nucleo." + sys_id + ".";
    for (const auto& o : opts) {
        if (o.current != o.def) ini_.Set(prefix + o.key, o.current);
        else ini_.Erase(prefix + o.key);
    }
    ini_.SetBool(prefix + "@migrado", true);
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
#ifdef __ANDROID__
    // En el celular siempre es pantalla completa: sin barra de arriba ni botones de abajo (vuelven
    // un momento deslizando desde el borde).
    (void)on;
    settings_.fullscreen = true;
    SDL_SetWindowFullscreen(window_, true);
    return;
#endif
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

// En el celular: nada debajo de la cámara ni del teclado en pantalla. Se achica la zona donde se
// dibuja la interfaz (la pantalla de ImGui) solo durante este cuadro.
void App::ApplySafeArea() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    full_vp_pos_ = vp->Pos;
    full_vp_size_ = vp->Size;
    float l = 0, t = 0, r = 0, b = 0;
    int ww = 0, wh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
#ifdef __ANDROID__
    const float k = ww > 0 ? vp->Size.x / static_cast<float>(ww) : 1.0f;
    SDL_Rect sa{};
    if (SDL_GetWindowSafeArea(window_, &sa) && sa.w > 0 && sa.h > 0) {
        l = sa.x * k;
        t = sa.y * k;
        r = (ww - sa.x - sa.w) * k;
        b = (wh - sa.y - sa.h) * k;
    }
    // El teclado: Android 11+ dice su alto; antes, se supone casi media pantalla.
    if (ImGui::GetIO().WantTextInput && SDL_ScreenKeyboardShown(window_)) {
        int pw = 0, ph = 0;
        SDL_GetWindowSizeInPixels(window_, &pw, &ph);
        const int kb = Android::KeyboardHeight();
        const float kbh = kb > 0 && ph > 0 ? kb * (vp->Size.y / ph) : vp->Size.y * 0.45f;
        b = std::max(b, kbh);
    }
#endif
    if (qa_.count("recorte")) std::sscanf(qa_["recorte"].c_str(), "%f,%f,%f,%f", &l, &t, &r, &b);
    if (qa_.count("teclado") && ImGui::GetIO().WantTextInput) b = std::max(b, vp->Size.y * 0.45f);
    vp->Pos.x += l;
    vp->Pos.y += t;
    vp->Size.x = std::max(100.0f, vp->Size.x - l - r);
    vp->Size.y = std::max(100.0f, vp->Size.y - t - b);
}

void App::Toast(const std::string& text_es, GuideId guide, double seconds) {
    const std::string text = Tr(text_es);
    if (NativeMode() && !NativeNeedsImGui()) {
        // En el celular los avisos los muestra Android (una tarjeta chica con el guía).
        NativeUi::Emit("{\"t\":\"aviso\",\"guia\":" + NativeUi::Quote(Guide(guide).key) + ",\"texto\":" + NativeUi::Quote(text) +
                       ",\"segundos\":" + std::to_string(seconds) + "}");
        return;
    }
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
    NativeTick();
    PollUnpack();
    PollDataMove();
#ifdef __ANDROID__
    // El permiso de archivos se puede dar después (en la pantalla de Android): se revisa cada tanto.
    if (Paths::PublicRoot().empty() && move_state_ == 0 && SDL_GetTicks() / 1000.0 - public_check_t_ > 1.5) {
        public_check_t_ = SDL_GetTicks() / 1000.0;
        SetupPublicFolder();
    }
#endif

    if (library_.TakeChanged()) {
        games_dirty_ = true;
        if (settings_.covers && !qa_.count("fresh")) covers_.QueueDownloads(library_.Snapshot());
    }
    covers_.Pump();
    PollUpdater();

    // El cursor se esconde mientras se juega.
    // En Satronho Dual el mouse es el lápiz de la pantalla táctil: no se esconde.
    const bool want_hidden = in_game_ && overlay_page_ < 0 && !menu_open_ && !bar_menu_active_ && !bar_open_ && !sys_->touch && SDL_GetTicks() - last_mouse_move_ > 2000;
    if (want_hidden != cursor_hidden_) {
        cursor_hidden_ = want_hidden;
        if (want_hidden) SDL_HideCursor();
        else SDL_ShowCursor();
    }

    // Atajos (teclado y mando). Select+Start en el mando también abre el menú.
    {
        const bool typing = ImGui::GetIO().WantTextInput;
        for (Hotkey h : input_.TakeHotkeyPresses())
            if (!typing && confirm_ == 0) DoHotkey(h);
        if (in_game_ && input_.MenuComboPressed() && confirm_ == 0) {
            if (NativeMode()) SetNativeMenu(!native_menu_);
            else SetMenuOpen(!menu_open_);
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    // Mientras se juega, el mando es del juego; la interfaz solo lo usa con el menú abierto.
    if ((in_game_ && !menu_open_ && confirm_ == 0) || input_.Capturing()) io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    else io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    if (in_game_ && !paused_ && !menu_open_ && !native_menu_ && native_page_.empty() && confirm_ == 0 && !touch_edit_ && !bar_menu_active_ && overlay_page_ < 0 && !(touch_mode_ && bar_open_)) EmulateTick(dt);
    else if (in_game_) input_.ApplyRumble(core_, false); // en pausa o con el menú, el mando no vibra
    if (in_game_) {
        if (gl_) video_gl_.Upload(core_);
        else video_.Upload(core_);
    }
    if (const std::string c = input_.TakeConflict(); !c.empty()) Toast(c, GuideId::Samy, 7);

    usage_timer_ += dt;
    if (usage_timer_ > 0.5) {
        usage_timer_ = 0;
        proc_ = usage_.Sample(hw_.threads);
    }

    if (gl_) ImGui_ImplOpenGL3_NewFrame();
    else ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    PollImports();
    ImGui::NewFrame();
    ApplySafeArea();
    UpdateTouchMode();
    TouchScroll();
    carousel_rect_ = {0, 0, 0, 0}; // el carrusel lo vuelve a poner si está en pantalla
    DrawUi();
    // La pantalla real vuelve a ser la entera para dibujar.
    ImGui::GetMainViewport()->Pos = full_vp_pos_;
    ImGui::GetMainViewport()->Size = full_vp_size_;
    ImGui::Render();

    int pw, ph;
    if (gl_) {
        SDL_GetWindowSizeInPixels(window_, &pw, &ph);
        Gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
        Gl::Viewport(0, 0, pw, ph);
        const unsigned bg = in_game_ ? 0 : Ui::Color::Bg;
        Gl::ClearColor(((bg >> 16) & 0xFF) / 255.0f, ((bg >> 8) & 0xFF) / 255.0f, (bg & 0xFF) / 255.0f, 1.0f);
        Gl::Clear(GL_COLOR_BUFFER_BIT);
        if (in_game_) {
            // En ventana, el juego va entre la barra de menús y la de estado.
            const float fb = ImGui::GetIO().DisplayFramebufferScale.y;
            const int top = GameBarReserves() ? static_cast<int>(GameBarTop() * fb) : 0;
            const int bot = GameBarReserves() ? static_cast<int>(GameBarBottom() * fb) : 0;
            int lx = 0, rx = 0;
            TouchReserve(pw, ph, &lx, &rx);
            game_x_off_ = lx;
            video_gl_.SetArea(lx, bot);
            video_gl_.Draw(core_, &hw_render_, VideoSettings(), std::max(1, pw - lx - rx), std::max(1, ph - top - bot), sys_->handheld);
            video_gl_.SetArea(0, 0);
        }
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        QaTick();
        SDL_GL_SwapWindow(window_);
    } else {
        SDL_GetRenderOutputSize(renderer_, &pw, &ph);
        SDL_SetRenderDrawColor(renderer_, (Ui::Color::Bg >> 16) & 0xFF, (Ui::Color::Bg >> 8) & 0xFF, Ui::Color::Bg & 0xFF, 255);
        SDL_RenderClear(renderer_);
        if (in_game_) {
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
            SDL_RenderClear(renderer_);
            const float fb = ImGui::GetIO().DisplayFramebufferScale.y;
            const int top = GameBarReserves() ? static_cast<int>(GameBarTop() * fb) : 0;
            const int bot = GameBarReserves() ? static_cast<int>(GameBarBottom() * fb) : 0;
            int lx = 0, rx = 0;
            TouchReserve(pw, ph, &lx, &rx);
            game_x_off_ = lx;
            const SDL_Rect area{lx, top, std::max(1, pw - lx - rx), std::max(1, ph - top - bot)};
            SDL_SetRenderViewport(renderer_, &area);
            video_.Draw(core_, VideoSettings(), area.w, area.h, sys_->handheld);
            SDL_SetRenderViewport(renderer_, nullptr);
        }
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
        QaTick();
        SDL_RenderPresent(renderer_);
    }

    // Juego pedido: ya se mostró «Cargando…», ahora se abre.
    if (!pending_start_.empty() && ++pending_frames_ >= 2) {
        const fs::path rom = pending_start_;
        const bool resume = pending_resume_;
        pending_start_.clear();
        start_resume_ = resume;
        if (StartGame(rom) && resume) resume_pending_ = true;
        start_resume_ = false;
    }

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
#ifdef __ANDROID__
            // Android abre su instalador; la app la cierra el sistema al instalar.
            Toast("Confirma la instalación en la ventana de Android.", GuideId::Juana, 8);
#else
            running_ = false;
#endif
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

Settings App::VideoSettings() const {
    Settings v = settings_.ForSystem(sys_->id);
    // «Según la consola»: CRT en las de TV en 2D, LCD en las portátiles y nada en las 3D (CD, 64,
    // Anillo y Dual se ven mejor limpias, sobre todo a alta resolución).
    const bool three_d = sys_->gpu || sys_->id == "anillo" || sys_->id == "dual";
    if (v.effect == static_cast<int>(Effect::Auto) && three_d) v.effect = static_cast<int>(Effect::None);
    return v;
}

void App::QaTick() {
    if (qa_.empty()) return;
    ++qa_frames_;
    const int wanted = qa_.count("frames") ? std::atoi(qa_["frames"].c_str()) : 30;
    if (qa_frames_ == wanted / 2 && qa_.count("salir_tarde")) confirm_ = std::atoi(qa_["salir_tarde"].c_str());
    if (qa_frames_ == wanted / 2 && qa_.count("menu")) {
        SetMenuOpen(true);
        if (qa_.count("seccion")) menu_section_ = std::atoi(qa_["seccion"].c_str());
    }
    if (qa_.count("estados") && in_game_) {
        if (qa_frames_ == wanted / 3) SaveSlot(2);
        if (qa_frames_ == wanted * 2 / 3) LoadSlot(2);
    }
    if (qa_frames_ < wanted) return;
    if (qa_.count("shot") && gl_) {
        int w, h;
        SDL_GetWindowSizeInPixels(window_, &w, &h);
        std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4), flip(px.size());
        Gl::PixelStorei(GL_PACK_ALIGNMENT, 1);
        Gl::ReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        for (int y = 0; y < h; ++y) std::memcpy(&flip[static_cast<size_t>(y) * w * 4], &px[static_cast<size_t>(h - 1 - y) * w * 4], static_cast<size_t>(w) * 4);
        Ui::SavePng(qa_["shot"], flip.data(), w, h);
    } else if (qa_.count("shot")) {
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
            const std::string stored = Paths::StoreFolder(Paths::FromUtf8(f));
            if (std::find(settings_.folders.begin(), settings_.folders.end(), stored) == settings_.folders.end())
                settings_.folders.push_back(stored);
        }
        SaveConfig();
        Rescan();
        Toast("Buscando juegos en la carpeta nueva…", GuideId::Samy);
    }
    if (!roms.empty()) RequestStart(Paths::FromUtf8(roms.front()));
    {
        std::vector<std::string> toasts;
        {
            std::lock_guard lock(pending_mutex_);
            toasts.swap(pending_toasts_);
        }
        for (const auto& t : toasts) Toast(t, GuideId::Nono, 7);
    }
    std::vector<std::string> bios;
    {
        std::lock_guard lock(pending_mutex_);
        bios.swap(pending_bios_);
    }
    for (const auto& b : bios) ImportBios(bios_target_, Paths::FromUtf8(b));
    std::vector<std::string> covers;
    {
        std::lock_guard lock(pending_mutex_);
        covers.swap(pending_covers_);
    }
    if (!covers.empty()) {
        if (Game* g = library_.Find(cover_target_)) {
            covers_.SetCustom(*g, Paths::FromUtf8(covers.front()));
            Toast("Carátula nueva para «" + g->title + "».", GuideId::Samy, 3);
        }
    }
}

// ---------------------------------------------------------------- BIOS (la de cada quien)

std::vector<std::string> App::BiosFound(const SystemDef& sys) const {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& b : sys.bios)
        if (fs::exists(Paths::System() / b.name, ec) || fs::exists(Paths::System() / "melonDS DS" / b.name, ec)) out.push_back(b.name);
    return out;
}

void App::PickBios(const SystemDef& sys) {
    bios_target_ = sys.id;
    static const SDL_DialogFileFilter filters[] = {{"BIOS", "bin;rom;bios"}, {"Todos los archivos", "*"}};
    SDL_ShowOpenFileDialog(&BiosDialogCallback, this, window_, filters, 2, nullptr, true);
}

void App::ImportBios(const std::string& sys_id, const fs::path& file) {
    const SystemDef* sys = SystemById(sys_id);
    if (!sys || sys->bios.empty()) return;
    std::error_code ec;
    const auto size = fs::file_size(file, ec);
    if (ec) {
        Toast("No pude leer ese archivo.", GuideId::Nono, 5);
        return;
    }
    std::string name = Paths::ToUtf8(file.filename());
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    // Se reconoce en este orden: 1) la huella exacta, 2) el nombre que busca el núcleo, 3) el
    // tamaño y una pista del nombre (modelo o región), 4) solo el tamaño (el primero de la lista).
    const BiosFile* pick = nullptr;
    bool by_hash = false;
    std::string sha;
    for (const auto& b : sys->bios) {
        if (b.size != size || !*b.sha1) continue;
        if (sha.empty()) sha = Hash::Sha1File(file);
        if (sha == b.sha1) {
            pick = &b;
            by_hash = true;
            break;
        }
    }
    if (!pick)
        for (const auto& b : sys->bios)
            if (name == fs::path(b.name).filename().string()) pick = &b;
    auto size_ok = [&](const BiosFile& b) {
        return b.size == size || (std::string(b.name) == "firmware.bin" && (size == 131072 || size == 524288));
    };
    if (!pick) {
        for (const auto& b : sys->bios) {
            if (!size_ok(b)) continue;
            std::string kw = b.keywords;
            size_t pos = 0;
            while (!pick && pos <= kw.size()) {
                const size_t bar = kw.find('|', pos);
                const std::string word = kw.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
                if (!word.empty() && name.find(word) != std::string::npos) pick = &b;
                if (bar == std::string::npos) break;
                pos = bar + 1;
            }
            if (pick) break;
        }
    }
    if (!pick)
        for (const auto& b : sys->bios)
            if (size_ok(b)) {
                pick = &b;
                break;
            }
    if (!pick) {
        Toast("Ese archivo no parece una BIOS de " + sys->full + " (no tiene el tamaño de ninguna).", GuideId::Nono, 5);
        return;
    }
    fs::create_directories((Paths::System() / pick->name).parent_path(), ec);
    fs::copy_file(file, Paths::System() / pick->name, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        Toast("No pude copiar la BIOS: " + ec.message(), GuideId::Nono, 5);
        return;
    }
    Log::Info("BIOS de %s agregada: %s%s", sys->full.c_str(), pick->name, by_hash ? " (huella correcta)" : "");
    std::string msg = "¡Listo! Quedó como " + std::string(pick->name) + ". Se usa desde el próximo juego.";
    // Con huella conocida y distinta: el núcleo la usa, pero no es la que recomienda.
    if (!by_hash && *pick->sha1) {
        if (sha.empty()) sha = Hash::Sha1File(file);
        if (sha != pick->sha1) msg += " No es la versión que recomienda el núcleo, pero suele andar bien.";
    }
    Toast(msg, GuideId::Nono, 6);
}

void App::AddFolderDialog() {
#ifdef __ANDROID__
    OpenBrowser(true);
    return;
#endif
    SDL_ShowOpenFolderDialog(&FolderDialogCallback, this, window_, nullptr, false);
}

void App::OpenRomDialog() {
#ifdef __ANDROID__
    OpenBrowser(false);
    return;
#endif
    // Los filtros salen de la lista de consolas; tienen que vivir mientras el diálogo esté abierto.
    static std::vector<std::string> names, patterns;
    static std::vector<SDL_DialogFileFilter> filters;
    if (filters.empty()) {
        std::string all;
        for (const auto& s : Systems()) {
            std::string pat;
            for (const auto& e : s.extensions) pat += (pat.empty() ? "" : ";") + e;
            all += (all.empty() ? "" : ";") + pat;
            if (&s == &Systems().back()) all += ";zip;7z";
            names.push_back(s.full + " (" + s.games + ")");
            patterns.push_back(pat);
        }
        names.insert(names.begin(), "Juegos de todas las consolas");
        patterns.insert(patterns.begin(), all);
        names.push_back("Todos los archivos");
        patterns.push_back("*");
        for (size_t i = 0; i < names.size(); ++i) filters.push_back({names[i].c_str(), patterns[i].c_str()});
    }
    SDL_ShowOpenFileDialog(&FileDialogCallback, this, window_, filters.data(), static_cast<int>(filters.size()), nullptr, false);
}

void App::HandleEvent(const SDL_Event& e) {
    // Si se está asignando un botón, esa tecla no cuenta como atajo.
    const bool was_capturing = input_.Capturing();
    ImGui_ImplSDL3_ProcessEvent(&e);
    // Se desconectó el mando mientras se juega con él: el juego espera con el menú de arriba a la vista.
    if (e.type == SDL_EVENT_GAMEPAD_REMOVED && in_game_ && !bar_open_ && !menu_open_ && !native_menu_) {
        if (NativeMode()) SetNativeMenu(true);
        else bar_open_ = true;
        Toast("Se desconectó el mando. El juego espera: conéctalo o sigue con el control en pantalla.", GuideId::Samy, 5);
    }
    input_.HandleEvent(e);
    TouchEvent(e);
    switch (e.type) {
    case SDL_EVENT_QUIT:
        RequestQuit();
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (e.window.windowID == SDL_GetWindowID(window_)) RequestQuit();
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
        if (in_game_ && !menu_open_) {
            audio_.Clear();
            if (settings_.pause_on_focus_loss && !paused_) {
                paused_ = true;
                focus_paused_ = true;
                audio_.SetPaused(true);
            }
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        if (focus_paused_) {
            focus_paused_ = false;
            paused_ = false;
            audio_.SetPaused(menu_open_);
        }
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
                RequestStart(p);
            }
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (e.key.scancode == SDL_SCANCODE_AC_BACK && !e.key.repeat) {
            // Botón «atrás» de Android: cierra lo que esté abierto o vuelve; en la biblioteca, salir.
            if (browser_open_) browser_open_ = false;
            else if (touch_edit_) { touch_edit_ = false; SaveConfig(); }
            else if (confirm_) confirm_ = 0;
            else if (!game_settings_.key.empty()) game_settings_ = Game{};
            else if (in_game_ && overlay_page_ >= 0) overlay_page_ = -1;
            else if (NativeMode() && in_game_) SetNativeMenu(!native_menu_);
            else if (NativeMode() && legacy_page_ >= 0) { legacy_page_ = -1; page_ = Page::Library; }
            else if (in_game_ && bar_open_) bar_open_ = false;
            else if (in_game_ && menu_open_) SetMenuOpen(false);
            else if (in_game_) ToggleGameBar(); // en el celular, «atrás» despliega el menú de arriba
            else if (page_ != Page::Library || !lib_system_.empty()) { page_ = Page::Library; lib_system_.clear(); }
            else RequestQuit();
            break;
        }
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
    if (h == Hotkey::Menu && overlay_page_ >= 0) { // Esc cierra la ventana de Satronho y vuelve al juego
        overlay_page_ = -1;
        return;
    }
    switch (h) {
    case Hotkey::Menu:
        // Esc del teclado en pantalla completa: despliega (o esconde) la barra de menús de arriba.
        // Con el mando, o en ventana, abre el menú grande del juego como siempre.
        if (input_.menu_from_keyboard && settings_.fullscreen && !menu_open_) ToggleGameBar();
        else {
            bar_open_ = false;
            SetMenuOpen(!menu_open_);
        }
        break;
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
            if (touch_mode_ && paused_) bar_open_ = true; // en pausa, con el dedo: el menú de arriba a mano
            Toast(paused_ ? "En pausa" : "¡Seguimos!", GuideId::Samy, 1.5);
        }
        break;
    case Hotkey::Reset:
        core_.Reset();
        Toast("Juego reiniciado", GuideId::Samy, 2.0);
        break;
    case Hotkey::Screens:
        if (sys_->id == "dual") {
            screens_pulse_ = 3; // R3 del RetroPad durante 3 cuadros: melonDS DS pasa a la siguiente forma
            Toast("Pantallas cambiadas", GuideId::Troya, 1.5);
        }
        break;
    default: break;
    }
}

// ---------------------------------------------------------------- juego

void App::RequestStart(const fs::path& rom, bool resume) {
    pending_start_ = rom;
    pending_resume_ = resume;
    pending_frames_ = 0;
}

bool App::StartGame(const fs::path& rom) {
    if (in_game_) StopGame();
    Game probe = start_entry_.empty() ? Library::MakeGame(rom) : Library::MakeGameEntry(rom, start_entry_);
    start_entry_.clear();
    if (probe.Archived() && !probe.choices.empty() && !qa_.count("rom")) {
        // Un .zip con varios juegos: se pregunta cuál (DrawArchivePicker).
        archive_pick_ = probe;
        return false;
    }
    const SystemDef* sys = SystemById(probe.system);
    if (!sys) {
        Toast("Este archivo no es de ninguna consola de Satronho Emulation.", GuideId::MechaRidds, 5);
        return false;
    }
    if (sys->bios_required && BiosFound(*sys).empty()) {
        // Sin BIOS no arranca: se explica y se abre el lugar donde se agrega.
        Toast(sys->full + " necesita la BIOS original de la consola. Agrégala aquí con «Agregar BIOS…».", GuideId::Nono, 8);
        page_ = Page::Settings;
        tab_ = SettingsTab::Core;
        core_tab_system_ = sys->id;
        return false;
    }
    std::string err;
    if (!EnsureCore(*sys, &err)) {
        std::error_code ec;
        if (!fs::exists(Paths::Cores() / CoreFile(*sys), ec))
            Toast("Falta el núcleo de " + sys->full + ". Vuelve a descargar Satronho Emulation.", GuideId::MechaRidds, 6);
        else
            Toast("No se pudo abrir el núcleo de " + sys->full + ". " + err, GuideId::MechaRidds, 6);
        Log::Error("%s", err.c_str());
        return false;
    }
    sys_ = sys;
    core_.port_device = sys->port_device;
    // Ajustes de este juego (si tiene): tipo de mando, motor gráfico y resolución.
    if (library_.GameOption(probe.key, "mando") == "normal") core_.port_device = RETRO_DEVICE_JOYPAD;
    slow_s_ = 0;
    slow_asked_ = slow_prompt_ = slow_card_sent_ = false;
    analog_pulse_ = 0;
    audio_full_s_ = 0;
    game_api_ = std::atoi(library_.GameOption(probe.key, "motor").c_str());
    game_res_ = std::atoi(library_.GameOption(probe.key, "res").c_str());
    // Cada consola guarda sus partidas en su propia carpeta (partidas/<consola>).
    core_.SetSaveDir(Paths::Saves() / sys->id);
    ApplyGpuChoice(*sys);
    game_api_ = game_res_ = 0; // lo del juego vale solo para abrirlo (las pantallas muestran lo de la consola)
    fs::path load_path = rom;
    std::vector<uint8_t> mem;
    if (probe.Archived() && ArchiveNeedsUnpack(probe)) {
        BeginUnpack(probe, rom); // al terminar se vuelve a pedir este juego
        return false;
    }
    if (probe.Archived() && !PrepareArchived(probe, load_path, mem)) return false;
    if (!core_.LoadGame(load_path, &err, mem.empty() ? nullptr : &mem)) {
        Toast(err, GuideId::MechaRidds, 6);
        return false;
    }
    if (sys->id == "cd" && BiosFound(*sys).empty() && !bios_hint_shown_) {
        bios_hint_shown_ = true;
        Toast("Sin BIOS de PlayStation uso OpenBIOS, que es libre. Si un juego falla, agrega la tuya en Ajustes » Núcleo » CD.",
              GuideId::Nono, 8);
    }
    current_ = probe;
    game_enter_t_ = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    MigrateLegacyStates();
    library_.MarkPlayed(current_.key);
    games_dirty_ = true;
    core_.target_refresh = hw_.refresh;
    // Salida de sonido nueva para cada juego (en Android a veces quedaba muda tras el primero).
    audio_.Reopen();
    audio_.Configure(core_.SampleRate());
    ApplyAudioSettings();
    Log::Info("Sonido: %.0f Hz, volumen %.0f %%%s", core_.SampleRate(), settings_.volume * 100.0f,
              audio_.IsOpen() ? (audio_.DevicePaused() ? ", salida en pausa" : "") : ", sin salida");
    video_.Invalidate();
    video_gl_.Invalidate();
    if (settings_.volume < 0.01f) Toast("El sonido está en silencio. Súbelo en el menú Sonido de la barra de arriba.", GuideId::Samy, 5);
    rewind_.Clear();
    undo_state_.clear();
    resume_pending_ = false;
    disk_swap_target_ = -1;
    disk_swap_wait_ = 0;
    screens_pulse_ = 0;
    focus_paused_ = false;
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
    // El aviso habla de lo que la persona tiene en la mano: el dedo, el mando o el teclado.
    if (NativeMode())
        Toast(input_.Gamepads().empty() ? "¡A jugar! «Atrás» o el botón de pausa abren el menú del juego."
                                         : "¡A jugar! «Atrás» o Select + Start abren el menú del juego.", GuideId::Samy, 3.5);
    else if (TouchPadVisible())
        Toast("¡A jugar! El botón de pausa (a la derecha) o deslizar desde arriba abren el menú.", GuideId::Samy, 3.5);
    else if (!input_.Gamepads().empty())
        Toast("¡A jugar! Select + Start (o el botón Guía) abren el menú.", GuideId::Samy, 3.0);
    else
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
    // Ranura automática: al volver, «Continuar» sigue justo aquí.
    if (settings_.autosave_exit && core_.GameLoaded() && emu_frames_ > 30) SaveSlot(0, true);
    input_.ClearGameCodes();
    CommitPlaytime();
    core_.UnloadGame();
    input_.ApplyRumble(core_, false);
    audio_.Clear();
    audio_.SetPaused(true);
    in_game_ = false;
    menu_open_ = false;
    paused_ = false;
    focus_paused_ = false;
    rewinding_ = false;
    fast_forward_ = false;
    rewind_.Clear();
    undo_state_.clear();
    undo_state_.shrink_to_fit();
    runahead_state_.clear();
    runahead_state_.shrink_to_fit();
    disk_swap_target_ = -1;
    bar_open_ = false;
    bar_menu_active_ = false;
    native_menu_ = false;
    NativeUi::ResetTouch();
    fast_forward_toggle_ = false;
    if (pending_page_ >= 0) {
        page_ = static_cast<Page>(pending_page_);
        pending_page_ = -1;
    }
    for (auto& [k, img] : slot_thumbs_) Ui::FreeImage(img);
    slot_thumbs_.clear();
    SDL_SetWindowTitle(window_, "Satronho Emulation");
    if (cursor_hidden_) {
        SDL_ShowCursor();
        cursor_hidden_ = false;
    }
    games_dirty_ = true;
}

void App::SetMenuOpen(bool open) {
    if (!in_game_) return;
    if (NativeMode()) {
        // En el celular el menú del juego es el cajón nativo.
        SetNativeMenu(open);
        return;
    }
    menu_open_ = open;
    audio_.SetPaused(open || paused_);
    if (open) {
        menu_section_ = 0;
        RefreshSlotThumbs();
        core_.SaveSram();
    }
}

bool App::RewindActive() const { return settings_.rewind && (!sys_->Heavy() || settings_.rewind_heavy); }

void App::EmulateTick(double dt) {
    // «Continuar» desde la biblioteca: se carga la ranura automática antes del primer cuadro (si
    // el núcleo todavía no la acepta, después de correr uno).
    if (resume_pending_) {
        resume_pending_ = false;
        bool ok = core_.LoadStateFile(StatePath(0));
        if (!ok) {
            core_.RunFrame();
            ok = core_.LoadStateFile(StatePath(0));
        }
        Toast(ok ? "Seguimos donde quedaste." : "No pude cargar donde quedaste; empieza desde la partida guardada.", GuideId::Samy, 3);
    }
    const double fps = core_.Fps();
    const bool typing = ImGui::GetIO().WantTextInput;
    fast_forward_ = settings_.fast_forward_enabled && ((!typing && input_.HotkeyHeld(Hotkey::FastForward)) || fast_forward_toggle_);
    rewinding_ = RewindActive() && !typing && input_.HotkeyHeld(Hotkey::Rewind);
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
        // El reloj de la pantalla y el de la tarjeta de sonido nunca coinciden del todo (más en
        // 120/144 Hz). Si el búfer de sonido se está vaciando o llenando de más, se corre un
        // cuadro de más o de menos antes de que se oiga un corte.
        if (!fast_forward_ && !rewinding_ && audio_.IsOpen()) {
            const double short_frames = audio_.FramesShort(fps);
            if (frames == 0 && short_frames > audio_.LatencyMs() * fps / 1000.0 * 0.6) {
                frames = 1;
                acc_ -= period;
            } else if (frames > 0 && short_frames < -audio_.LatencyMs() * fps / 1000.0 * 1.2) {
                --frames;
                acc_ += period;
            }
        }
        // Si la tarjeta de sonido deja de sacar sonido (Android a veces la pausa), el búfer queda lleno y
        // el juego se frenaría para siempre esperándola (0 cuadros por segundo). Se vacía y se sigue.
        audio_full_s_ = frames == 0 && !fast_forward_ ? audio_full_s_ + dt : 0;
        if (audio_full_s_ > 0.4) {
            Log::Warn("El sonido no avanzaba (salida en pausa: %s): se vacía para que el juego siga", audio_.DevicePaused() ? "sí" : "no");
            audio_.Clear();
            audio_full_s_ = 0;
            frames = 1;
        }
    }
    if (fast_forward_) frames *= settings_.fast_forward;
    core_.audio_enabled = !(fast_forward_ && settings_.mute_fast_forward);
    for (int i = 0; i < frames; ++i) Step(rewinding_, fast_forward_);
    core_.audio_enabled = true;
    // Algunos núcleos cambian la frecuencia del sonido a mitad del juego (cambio de región o de modo).
    if (core_.TakeAudioRateChanged()) {
        Log::Info("El núcleo cambió el sonido a %.0f Hz", core_.SampleRate());
        audio_.Configure(core_.SampleRate());
        ApplyAudioSettings();
    }
    audio_.Update();
    input_.ApplyRumble(core_, settings_.rumble);

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
    WatchSlowGame(dt, fps);

    // Cada minuto se anota en el registro cómo va el sonido, para revisar cortes.
    audio_report_s_ += dt;
    if (audio_report_s_ > 60) {
        audio_report_s_ = 0;
        const int cuts = audio_.Underruns() - audio_cuts_logged_, drops = audio_.Drops() - audio_drops_logged_;
        audio_cuts_logged_ = audio_.Underruns();
        audio_drops_logged_ = audio_.Drops();
        Log::Info("Sonido: %d cortes y %d descartes en el último minuto · búfer %.0f ms de %d · velocidad %.4f · %d tirones",
                  cuts, drops, audio_.QueuedMs(), audio_.LatencyMs(), audio_.Ratio(), perf_.stutters_minute);
        if (RewindActive())
            Log::Info("Rebobinar: %zu pasos, %.1f MB", rewind_.Steps(), static_cast<double>(rewind_.Bytes()) / (1024.0 * 1024.0));
    }

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
    ApplyTouchPad();
    if (analog_pulse_ > 0) {
        core_.pads[0].buttons |= (1u << RETRO_DEVICE_ID_JOYPAD_L) | (1u << RETRO_DEVICE_ID_JOYPAD_R) | (1u << RETRO_DEVICE_ID_JOYPAD_SELECT);
        --analog_pulse_;
    }
    if (screens_pulse_ > 0) {
        core_.pads[0].buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_R3;
        --screens_pulse_;
    }
    ServiceDiskSwap();
    if (sys_->touch && !menu_open_) {
        // Pantalla táctil con el mouse: la posición dentro de la imagen del juego, en el rango
        // de libretro. El núcleo decide qué parte de la imagen es la pantalla de abajo.
        float mx, my;
        const SDL_MouseButtonFlags mb = SDL_GetMouseState(&mx, &my);
        float dx = mx, dy = my;
        if (gl_) {
            // Del tamaño de la ventana a píxeles (pantallas con escala de Windows).
            const float k = SDL_GetWindowPixelDensity(window_);
            dx = mx * k;
            dy = my * k;
        } else {
            SDL_RenderCoordinatesFromWindow(renderer_, mx, my, &dx, &dy);
        }
        SDL_FRect r = gl_ ? video_gl_.LastRect() : video_.LastRect();
        r.x += static_cast<float>(game_x_off_); // la imagen se corre si el control táctil va a los costados
        if (r.w > 1 && r.h > 1) {
            const float u = (dx - r.x) / r.w, v = (dy - r.y) / r.h;
            const bool inside = u >= 0 && u <= 1 && v >= 0 && v <= 1;
            core_.pads[0].pointer_x = static_cast<int16_t>(std::clamp(u * 2 - 1, -1.0f, 1.0f) * 0x7fff);
            core_.pads[0].pointer_y = static_cast<int16_t>(std::clamp(v * 2 - 1, -1.0f, 1.0f) * 0x7fff);
            core_.pads[0].pointer_down = inside && (mb & SDL_BUTTON_LMASK) && !ImGui::GetIO().WantCaptureMouse;
        }
    }
    perf_.emu_times.push_back(SDL_GetTicks() / 1000.0);
    if (rewinding) {
        if (rewind_.Pop(rewind_tmp_)) {
            core_.LoadState(rewind_tmp_);
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
    if (RewindActive() && emu_frames_ % 2 == 0) PushRewind();
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
    // Un paso cada 2 cuadros; el tope de memoria corta antes si los estados pesan mucho.
    rewind_.SetMaxSteps(static_cast<size_t>(std::max(30.0, settings_.rewind_seconds * core_.Fps() / 2.0)));
    rewind_.SetBudget(static_cast<size_t>(settings_.rewind_mb) * 1024 * 1024);
    if (core_.SaveState(rewind_tmp_)) rewind_.Push(rewind_tmp_);
}

fs::path App::StatePathFor(const Game& g, int slot) {
    const std::string base = g.key + "-" + Paths::SafeName(g.title);
    return Paths::States() / (base + (slot == 0 ? std::string(".estado-auto") : ".estado" + std::to_string(slot)));
}

fs::path App::StatePath(int slot) const { return StatePathFor(current_, slot); }

fs::path App::LegacyStatePath(int slot) const {
    Game g = current_;
    g.key = current_.legacy_key;
    return StatePathFor(g, slot);
}

void App::MigrateLegacyStates() {
    // Modo portátil: los estados viejos llevan la clave de la ruta completa; se renombran.
    if (current_.legacy_key == current_.key) return;
    for (int s = 0; s <= 8; ++s) {
        std::error_code ec;
        const fs::path now = StatePath(s), old = LegacyStatePath(s);
        if (fs::exists(now, ec) || !fs::exists(old, ec)) continue;
        fs::rename(old, now, ec);
        fs::path png_old = old, png_now = now;
        png_old += ".png";
        png_now += ".png";
        if (fs::exists(png_old, ec)) fs::rename(png_old, png_now, ec);
    }
}

bool App::HasAutoState(const Game& g) const {
    std::error_code ec;
    return fs::exists(StatePathFor(g, 0), ec);
}

void App::RefreshSlotThumbs() {
    for (auto& [k, img] : slot_thumbs_) Ui::FreeImage(img);
    slot_thumbs_.clear();
    slot_dates_.clear();
    if (!in_game_) return;
    for (int s = 0; s <= 8; ++s) {
        const fs::path p = StatePath(s);
        std::error_code ec;
        if (!fs::exists(p, ec)) continue;
        fs::path png = p;
        png += ".png";
        slot_thumbs_[s] = Ui::LoadImageFile(Paths::ToUtf8(png));
        const auto ft = fs::last_write_time(p, ec);
        if (!ec) {
            const auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            const std::time_t t = std::chrono::system_clock::to_time_t(sys);
            slot_dates_[s] = FormatLastPlayed(static_cast<long long>(t));
        }
    }
}

void App::SaveSlot(int slot, bool quiet) {
    const fs::path p = StatePath(slot);
    if (!core_.SaveStateFile(p)) {
        if (!quiet) Toast("No se pudo guardar el estado.", GuideId::Samy, 3);
        return;
    }
    unsigned w, h;
    const auto rgba = CurrentFrameRGBA(&w, &h);
    if (!rgba.empty()) {
        fs::path png = p;
        png += ".png";
        Ui::SavePng(Paths::ToUtf8(png), rgba.data(), static_cast<int>(w), static_cast<int>(h));
    }
    if (quiet) return;
    RefreshSlotThumbs();
    Toast("Estado guardado en la ranura " + std::to_string(slot), GuideId::Samy, 2.2);
}

void App::LoadSlot(int slot) {
    // Antes de cargar se guarda cómo iba el juego, por si la ranura era la equivocada.
    std::vector<uint8_t> before;
    const bool have_before = core_.SaveState(before);
    if (!core_.LoadStateFile(StatePath(slot))) {
        Toast(slot == 0 ? std::string("No hay un guardado automático todavía.") : "La ranura " + std::to_string(slot) + " está vacía.",
              GuideId::Samy, 2.5);
        return;
    }
    if (have_before) undo_state_ = std::move(before);
    rewind_.Clear();
    audio_.Clear();
    Toast((slot == 0 ? std::string("Guardado automático cargado") : "Estado de la ranura " + std::to_string(slot) + " cargado") +
              ". ¿Era otra? En el menú puedes deshacerlo.",
          GuideId::Samy, 3.5);
}

void App::UndoLoad() {
    if (undo_state_.empty()) return;
    if (core_.LoadState(undo_state_)) {
        undo_state_.clear();
        rewind_.Clear();
        audio_.Clear();
        Toast("Listo: volviste a como ibas antes de cargar.", GuideId::Samy, 3);
    }
}

void App::RequestDisk(unsigned index) {
    // Como en la consola: se abre la tapa, se espera un momento (el juego tiene que notarlo) y
    // se pone el otro disco.
    if (disk_swap_target_ >= 0) return;
    if (!core_.EjectDisk(true)) {
        Toast("No se pudo cambiar el disco.", GuideId::Samy, 3);
        return;
    }
    disk_swap_target_ = static_cast<int>(index);
    disk_swap_wait_ = 45;
    Toast("Abriendo la tapa…", GuideId::Samy, 1.5);
}

void App::ServiceDiskSwap() {
    if (disk_swap_target_ < 0 || --disk_swap_wait_ > 0) return;
    const unsigned target = static_cast<unsigned>(disk_swap_target_);
    disk_swap_target_ = -1;
    const bool ok = core_.InsertDisk(target);
    core_.EjectDisk(false);
    Toast(ok ? "Pusiste el " + core_.DiskLabel(target) + "." : std::string("No se pudo poner ese disco."), GuideId::Samy, 3);
}

std::vector<uint8_t> App::CurrentFrameRGBA(unsigned* w, unsigned* h) {
    // Los cuadros de la tarjeta gráfica no pasan por la memoria: se leen de la textura.
    if (core_.HwActive() && core_.Frame().empty()) {
        std::vector<uint8_t> out;
        if (hw_render_.ReadRGBA(out, w, h)) return out;
        return {};
    }
    return core_.FrameRGBA(w, h);
}

void App::ApplyVsync() {
    if (gl_) {
        SDL_GL_SetSwapInterval(settings_.vsync ? 1 : 0);
    } else if (renderer_) {
        SDL_SetRenderVSync(renderer_, settings_.vsync ? 1 : 0);
    }
}

void App::Screenshot() {
    unsigned w, h;
    const auto rgba = CurrentFrameRGBA(&w, &h);
    if (rgba.empty()) return;
    char stamp[64];
    const std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H.%M.%S", std::localtime(&t));
    const fs::path p = Paths::Screenshots() / (Paths::SafeName(current_.title) + " " + stamp + ".png");
    if (Ui::SavePng(Paths::ToUtf8(p), rgba.data(), static_cast<int>(w), static_cast<int>(h)))
        Toast("Captura guardada en la carpeta de capturas", GuideId::Troya, 2.5);
}

} // namespace Satronho
