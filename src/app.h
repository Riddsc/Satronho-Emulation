// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "core/audio.h"
#include "core/core_host.h"
#include "core/input.h"
#include "core/rewind.h"
#include "core/video.h"
#include "sys/hardware.h"
#include "sys/library.h"
#include "sys/settings.h"
#include "sys/systems.h"
#include "sys/updater.h"
#include "ui/kit.h"
#include "util/ini.h"
#include "util/json.h"

namespace Satronho {

enum class Page { Library, Controls, Suggested, News, Settings, About };
enum class SettingsTab { Video, Audio, Controls, Performance, Meter, Core, Library, Guides, Count };
enum class GuideId { MechaRidds = 0, Samy, Troya, Nono, Juana, Count };

struct GuideInfo {
    const char* key;
    const char* name;
    const char* role;
    unsigned color;
};
const GuideInfo& Guide(GuideId id);

class App {
public:
    int Run(int argc, char** argv);

private:
    // Ciclo principal
    bool Init();
    void Shutdown();
    void Frame();
    void HandleEvent(const SDL_Event& e);
    void DoHotkey(Hotkey h);
    void SetFullscreen(bool on);
    void UpdateRefresh();
    bool EnsureCore(const SystemDef& sys, std::string* error);
    // Opciones del núcleo de cada consola: Bolsillo y Bolsillo Plus comparten mGBA, pero cada
    // una tiene sus valores por defecto y sus cambios guardados («nucleo.<consola>.<opción>»).
    void ApplySystemOptions(const SystemDef& sys);
    void SaveCoreOptions(const std::string& sys_id, const std::vector<CoreOption>& opts);
    std::vector<CoreOption> OptionsFor(const SystemDef& sys, std::vector<CoreOption> base) const;
    void UpdateScale();
    float AutoScale() const;
    void ProcessPendingPaths();

    // Juego
    // Pide abrir un juego: primero se dibuja «Cargando…» y en el cuadro siguiente se abre
    // (los juegos de disco tardan un poco y la ventana no debe quedarse congelada sin aviso).
    void RequestStart(const fs::path& rom, bool resume = false);
    bool StartGame(const fs::path& rom);
    void StopGame();
    void EmulateTick(double dt);
    void Step(bool rewinding, bool fast_forward);
    void RunAheadFrame(int frames);
    void PushRewind();
    void SaveSlot(int slot, bool quiet = false);
    void LoadSlot(int slot);
    void UndoLoad();
    void Screenshot();
    fs::path StatePath(int slot) const; // 0 = ranura automática (al salir)
    static fs::path StatePathFor(const Game& g, int slot);
    fs::path LegacyStatePath(int slot) const;
    void MigrateLegacyStates();
    bool HasAutoState(const Game& g) const;
    void ServiceDiskSwap();
    void RequestDisk(unsigned index);
    void RefreshSlotThumbs();
    void SetMenuOpen(bool open);
    void CommitPlaytime();

    // Interfaz
    void DrawUi();
    void DrawSidebar(float width);
    void DrawLibrary();
    void DrawContinueHero(const Game& g, float width);
    bool DrawGameCard(const Game& g, float w, float h, bool selected);
    void DrawEmptyLibrary();
    void DrawSuggested();
    void DrawNews();
    void DrawUpdatePanel();
    void PollUpdater();
    void DrawSettings();
    void DrawSettingsVideo();
    void DrawSettingsAudio();
    void DrawControls();
    void DrawControllerEditor(const SystemDef& sys, int player, bool compact);
    void DrawHotkeys();
    int BindingChip(const char* id, bool keyboard, const std::string& text, bool capturing, bool pressed, float width);
    void DrawProfilePopup();
    void DrawSettingsPerformance();
    void DrawSettingsMeter();
    void DrawSettingsCore();
    void DrawSettingsLibrary();
    void DrawSettingsGuides();
    void DrawAbout();
    void DrawGameLayer();
    void DrawPauseMenu();
    void DrawMeter(bool preview = false, ImVec2 preview_pos = {});
    void DrawToasts();
    void DrawGuideBubble();
    void DrawLoading();
    void DrawHiddenGames();
    void DrawWelcome();
    void DrawCoreMissing();
    void GuideSays(GuideId id, const char* text, float width = 0);

    // Utilidades
    void Toast(const std::string& text, GuideId guide = GuideId::MechaRidds, double seconds = 3.5);
    void SaveConfig();
    void ApplyAudioSettings();
    void ApplySuggestion();
    void AddFolderDialog();
    void OpenRomDialog();
    std::vector<std::string> BiosFound(const SystemDef& sys) const;
    void PickBios(const SystemDef& sys);
    void ImportBios(const std::string& sys_id, const fs::path& file);
    void Rescan();
    void LoadAssets();
    void QaTick();

    // Sistema
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool running_ = true;
    Uint64 last_ns_ = 0;
    double loop_ema_ = 1.0 / 60.0;
    float display_scale_ = 1.0f;
    int build_ = 0;

    Ini ini_;
    Settings settings_;
    CoreHost core_;
    AudioOut audio_;
    Input input_;
    VideoOut video_;
    Library library_;
    Updater updater_;
    Updater::State last_update_state_ = Updater::State::Idle;
    HardwareInfo hw_;
    UsageSampler usage_;
    ProcessUsage proc_;
    double usage_timer_ = 0;
    bool core_ok_ = false;          // hay un núcleo cargado ahora
    std::string core_error_;
    std::string core_name_;         // nombre que reporta el núcleo (Nestopia, mGBA)
    std::string core_file_;         // archivo del núcleo cargado
    std::vector<std::string> missing_cores_; // consolas sin su núcleo instalado
    const SystemDef* sys_ = &Systems().front(); // consola del juego en curso
    std::string options_sys_;       // consola cuyas opciones tiene ahora el núcleo cargado
    // Copia de las opciones de cada núcleo (por archivo), para mostrarlas en Ajustes » Núcleo sin
    // tener que cargar y descargar el .dll cada vez que se mira otra consola.
    struct CoreSnapshot {
        std::vector<CoreOption> options;
        std::vector<CoreCategory> categories;
        std::string name, version;
    };
    std::map<std::string, CoreSnapshot> core_cache_;
    std::vector<CoreOption> tab_options_;   // opciones mostradas de una consola sin su núcleo cargado
    std::string tab_options_sys_;
    std::string lib_system_;        // consola elegida en la biblioteca ("" = todas)
    std::string ctrl_system_ = "octa";
    int ctrl_tab_ = 1;              // 0 general, 1 jugador 1, 2 jugador 2, 3 atajos
    std::string core_tab_system_ = "octa";
    int profile_popup_ = 0;         // 0 cerrado, 1 nuevo, 2 renombrar
    int profile_player_ = 0;
    char profile_name_[48] = {};

    // Imágenes
    Ui::Image logo_;
    Ui::Image guides_[static_cast<int>(GuideId::Count)];

    // Navegación
    Page page_ = Page::Library;
    SettingsTab tab_ = SettingsTab::Video;
    char search_[128] = {};
    std::string selected_key_;
    std::vector<Game> games_;
    bool games_dirty_ = true;
    bool show_welcome_ = false;
    int welcome_step_ = 0;

    // Juego en curso
    bool in_game_ = false;
    bool paused_ = false;
    bool menu_open_ = false;
    int menu_section_ = 0;
    Game current_;
    double acc_ = 0;
    double audio_report_s_ = 0;
    int audio_cuts_logged_ = 0, audio_drops_logged_ = 0;
    uint64_t emu_frames_ = 0;
    double session_s_ = 0;
    double uncommitted_s_ = 0;
    double sram_timer_ = 0;
    std::vector<uint8_t> runahead_state_;
    RewindBuffer rewind_;
    std::vector<uint8_t> rewind_tmp_;
    bool RewindActive() const; // rebobinar encendido para la consola actual
    std::vector<uint8_t> undo_state_;   // estado antes de la última carga de ranura
    bool resume_pending_ = false;       // cargar la ranura automática al empezar
    int disk_swap_target_ = -1;         // disco que se pone cuando termina de «abrirse la tapa»
    int disk_swap_wait_ = 0;            // cuadros que faltan
    int screens_pulse_ = 0;             // cuadros con R3 presionado (cambiar pantallas en DS)
    bool focus_paused_ = false;         // pausado porque la ventana perdió el foco
    fs::path pending_start_;            // juego por abrir (tras dibujar «Cargando…»)
    bool pending_resume_ = false;
    int pending_frames_ = 0;
    bool rewinding_ = false;
    bool fast_forward_ = false;
    int slot_ = 1;
    std::map<int, Ui::Image> slot_thumbs_;
    std::map<int, std::string> slot_dates_;
    Uint64 last_mouse_move_ = 0;
    bool cursor_hidden_ = false;

    // Medidor
    struct Perf {
        float host_ms[180] = {};
        float core_ms[180] = {};
        int head = 0;
        std::deque<double> emu_times;
        double fps = 0;
        int stutters = 0;
        double stutter_minute_start = 0;
        int stutters_minute = 0;
    } perf_;

    // Avisos
    struct ToastItem {
        std::string text;
        GuideId guide;
        double born, until;
    };
    std::vector<ToastItem> toasts_;

    // Guías: consejos y novedades
    struct Tip {
        GuideId guide;
        std::string text;
    };
    std::vector<Tip> tips_;
    int tip_index_ = 0;
    double tip_timer_ = 0;
    bool bubble_closed_ = false;
    Json news_;

    // Carpetas y archivos elegidos en los diálogos del sistema (llegan desde otro hilo).
    std::mutex pending_mutex_;
    std::vector<std::string> pending_folders_;
    std::vector<std::string> pending_roms_;
    std::vector<std::string> pending_bios_;
    bool bios_hint_shown_ = false;
    std::string bios_target_;
    float sidebar_full_h_ = 0;
    std::string last_core_msg_;
    double last_core_msg_t_ = 0;

    // Modo de revisión automática (SATRONHO_QA).
    std::map<std::string, std::string> qa_;
    int qa_frames_ = 0;

    friend void FolderDialogCallback(void*, const char* const*, int);
    friend void FileDialogCallback(void*, const char* const*, int);
    friend void BiosDialogCallback(void*, const char* const*, int);
};

} // namespace Satronho
