// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <functional>
#include <deque>
#include <thread>
#include <map>
#include <set>
#include <mutex>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "core/audio.h"
#include "core/core_host.h"
#include "core/input.h"
#include "core/rewind.h"
#include "core/video.h"
#include "gfx/hw_render.h"
#include "gfx/video_gl.h"
#include "sys/covers.h"
#include "sys/guide_chat.h"
#include "sys/hardware.h"
#include "sys/library.h"
#include "sys/phone_scan.h"
#include "sys/settings.h"
#include "sys/systems.h"
#include "sys/updater.h"
#include "ui/kit.h"
#include "util/ini.h"
#include "util/json.h"

namespace Satronho {

class NPage;

enum class Page { Library, Controls, Suggested, News, Settings, About, Guides, Tools };
enum class SettingsTab { Video, Audio, Controls, Performance, Meter, Core, Library, Guides, Appearance, Count };
enum class GuideId { MechaRidds = 0, Samy, Troya, Nono, Juana, Count };

struct GuideInfo {
    const char* key;
    const char* name;
    const char* role;
    unsigned color;
};
const GuideInfo& Guide(GuideId id);
void CoverDialogCallback(void* user, const char* const* list, int filter);

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
    void DrawSidebarRail(float width);
    void DrawBottomNav(float height);   // celular en vertical // pantallas chicas: solo íconos
    void DrawLibrary();
    void DrawContinueHero(const Game& g, float width);
    bool DrawGameCard(const Game& g, float w, float h, bool selected);
    bool DrawGameRow(const Game& g, float w, float h, bool selected);
    bool GameContextMenu(const Game& g);
    void VerifyGame(const Game& g);
    std::vector<std::string> pending_toasts_; // avisos que llegan desde otros hilos (con pending_mutex_) // clic derecho en un juego; devuelve true si eligió «Jugar»
    void DrawEmptyLibrary();
    void DrawEmptySystem(const SystemDef& sys);
    // Ajustes de un solo juego (clic derecho o toque largo › «Ajustes de este juego»).
    Game game_settings_;
    void DrawGameSettings();
    void LongPressMenu(const Game& g);
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
    void DrawTools();
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

    // Acabados de la etapa 1 (app_extra.cpp)
    void ApplyTheme();
    void DrawWatermark(ImVec2 p0, ImVec2 p1);
    void DrawBackdrop(ImVec2 p0, ImVec2 p1);
    void DrawSplash();
    std::string hover_key_, hover_key_next_; // juego bajo el mouse (cuadro anterior)
    std::string backdrop_key_, backdrop_prev_;
    double backdrop_t_ = 0;
    double splash_t0_ = -1;                // inicio de la presentación animada
    bool splash_done_ = false;
    double game_enter_t_ = -10;            // para el fundido al entrar a un juego
    void DrawSettingsAppearance();
    void DrawLibraryCarousel(const std::vector<const Game*>& list, const SystemDef* lib_sys);
    bool DrawCover(ImDrawList* dl, const Game& g, ImVec2 a, ImVec2 b, float rounding, bool hovered);
    void DrawConfirm();
    // Barra de menús del juego (app_bar.cpp).
    void DrawGameBar();
    bool GameBarVisible() const;
    bool GameBarReserves() const;   // en ventana: el juego se dibuja entre las barras
    float GameBarTop() const;
    float GameBarBottom() const;
    void ToggleGameBar();
    bool bar_open_ = false;
    bool edge_swipe_ = false;
    bool page_scroll_reset_ = false;
    int game_x_off_ = 0; // píxeles que el juego se corrió a la derecha (control en pantalla a los costados) // la pantalla cambió: la próxima vez empieza arriba
    std::string shown_lib_system_;   // consola que se mostraba en la biblioteca (para volver arriba al cambiar)
    // Carrusel: arrastrar con el mouse o el dedo.
    bool car_drag_ = false, car_moved_ = false;
    float car_x0_ = 0, car_y0_ = 0, car_pos0_ = 0, car_vel_ = 0;
    ImVec4 carousel_rect_{0, 0, 0, 0}; // dónde está el carrusel (el dedo de lado ahí no mueve la página)        // el dedo empezó en el borde de arriba (para desplegar el menú)          // desplegada a pedido (pantalla completa, Android)
    bool bar_menu_active_ = false;   // un menú de la barra está abierto: el juego espera
    bool fast_forward_toggle_ = false;
    float muted_volume_ = 0.0f;
    int pending_page_ = -1;
    int overlay_page_ = -1;          // pantalla de Satronho abierta encima del juego (el juego espera)
    bool overlay_focus_ = false;
    ImVec2 full_vp_pos_, full_vp_size_; // la pantalla entera (ApplySafeArea la achica durante el cuadro)
    void ApplySafeArea();
    // Juegos que llegan de otras apps (app_import.cpp).
    struct ImportState {
        bool active = false, done = false, rescan = false;
        int phase = 0;            // 2 revisando, 3 descomprimiendo, 4 acomodando
        std::string name, receiving;
        float progress = 0;
        int files_done = 0, files_total = 0, skipped = 0;
        std::map<std::string, int> added; // consola → juegos
        std::vector<std::string> bios, errors;
    };
    std::mutex import_mutex_;
    ImportState import_;
    std::vector<fs::path> import_queue_;
    bool import_busy_ = false;
    std::thread import_thread_;
    std::thread verify_thread_; // «Verificar el archivo» (CRC32)
    fs::path ImportFolder() const;
    void QueueImport(const fs::path& file);
    void PollImports();
    void ImportWorker();
    void DrawImport();
    void DrawGameOverlay();
    void SatronhoModuleMenu(bool in_game); // el menú «Satronho» (barra del juego y de inicio)
    void DrawHomeBar();
    float HomeBarHeight() const;
    void OpenOverlay(Page p);          // pantalla a la que ir al terminar de salir del juego
    Settings VideoSettings() const;   // imagen del juego actual (con «según la consola» resuelto)
    int confirm_frames_ = 0;
    Uint64 quit_request_ms_ = 0;
    int ctrl_device_ = 0;
    std::set<std::string> gfx_probe_tried_; // consolas cuyo núcleo ya se intentó cargar para leer sus opciones             // Controles: 0 se asigna el mando, 1 el teclado     // cuándo se pidió cerrar (para no contar dos veces la misma X)          // cuadros que la pregunta lleva sin poder mostrarse
    void RequestQuit();               // cerrar la app (pregunta si está activado)
    void RequestExitGame();           // salir del juego (pregunta si está activado)
    void DrawWhatsNew();
    void DrawGuideChat();
    void AskGuides(const std::string& text);
    void DoChatAction(const ChatButton& b);
    ChatContext BuildChatContext() const;
    void PickCoverDialog(const Game& g);
    void BeginPageTransition();

    // Pantallas táctiles y pantallas chicas (app_touch.cpp).
    struct Finger {
        float x = 0, y = 0, sx = 0, sy = 0; // ahora y donde empezó (píxeles)
        int owner = -1;                     // botón del control en pantalla que tomó
        double born = 0;
        int dir = -1;                       // dirección de la cruceta que marca (0..7), para no saltar entre vecinas
    };
    struct TouchButton {
        ImVec2 c;
        float r;
        int id;            // botón del RetroPad; -1 menú, -2 cruceta, -3 stick
        const char* label;
        const char* group;
    };
    void UpdateTouchMode();
    void TouchEvent(const SDL_Event& e);
    void TouchScroll();
    void TouchReserve(int W, int H, int* left, int* right) const; // espacio del control a los costados del juego
    ImVec2 MeterSides() const;
    bool TouchPadVisible() const;
    std::vector<TouchButton> TouchLayout(float W, float H) const;
    void ApplyTouchPad();
    void DrawTouchPad();
    void DrawTouchEditor(const std::vector<TouchButton>& layout, float sx, float sy);
    void OpenBrowser(bool folders);
    void DrawBrowser();
    bool touch_mode_ = false;          // se usa con el dedo (Android o un toque en la pantalla)
    bool compact_ = false;             // pantalla chica: barra lateral angosta y menos márgenes
    std::map<SDL_FingerID, Finger> fingers_;
    uint16_t touch_buttons_ = 0, touch_prev_ = 0;
    float touch_stick_[2] = {};
    bool touch_menu_latched_ = false;
    bool touch_edit_ = false;
    std::string touch_drag_group_;
    void* drag_start_win_ = nullptr;
    float drag_dx_ = 0, drag_dy_ = 0;
    int drag_axis_ = 1;
    void* drag_win_ = nullptr;
    float drag_total_ = 0, drag_vel_ = 0;
    bool dragging_ = false;
    bool browser_open_ = false, browser_folders_ = true, browser_items_dirty_ = true, browser_perm_ = true;
    bool browser_for_root_ = false; // el explorador elige la casa de «Satronho Games»
    // Ajustes » Biblioteca: cuánto ocupan los juegos de cada consola (se mide en segundo plano).
    std::thread storage_thread_;
    std::atomic<bool> storage_busy_{false};
    std::mutex storage_mutex_;
    std::vector<std::pair<std::string, uint64_t>> storage_sizes_;
    double storage_t_ = -100;
    void DrawStoragePanel();
    double browser_perm_t_ = -10;
    fs::path browser_dir_;
    std::vector<std::pair<std::string, bool>> browser_items_;
    // Juegos en .zip / .7z: de cartucho van a la memoria; de disco se sacan a una caché.
    bool PrepareArchived(const Game& g, fs::path& load_path, std::vector<uint8_t>& mem);
    // Discos dentro de .zip/.7z: se sacan a la caché en segundo plano, con barra de avance.
    bool ArchiveNeedsUnpack(const Game& g) const;
    void BeginUnpack(const Game& g, const fs::path& rom);
    void PollUnpack();
    void DrawUnpack();
    std::thread unpack_thread_;
    std::atomic<int> unpack_state_{0}; // 0 nada, 1 descomprimiendo, 2 listo, 3 falló, 4 cancelado
    std::atomic<float> unpack_progress_{0};
    std::atomic<bool> unpack_cancel_{false};
    Game unpack_game_;
    fs::path unpack_rom_;
    bool unpack_resume_ = false;
    bool start_resume_ = false; // el juego que se está abriendo debe seguir donde quedó
    // Espacio: lo libre en esa carpeta (0 si no se sabe) y lo que siempre se deja libre.
    static uint64_t FreeBytes(const fs::path& dir);
    uint64_t MarginBytes(const SystemDef* sys = nullptr) const;
    // Android: carpeta pública «Satronho Games» (juegos, partidas y estados fuera de la app).
    void SetupPublicFolder();
    fs::path DefaultPublicRoot() const;
    void BeginDataMove(const fs::path& to);
    void PollDataMove();
    void DrawDataMove();
    std::thread move_thread_;
    std::atomic<int> move_state_{0}; // 0 nada, 1 mudando, 2 terminó
    std::atomic<int> move_done_{0}, move_total_{0};
    std::mutex move_mutex_;
    std::vector<std::pair<std::string, std::string>> move_rekeys_; // claves de juego vieja → nueva
    static void RenameStates(const fs::path& states, const std::vector<std::pair<std::string, std::string>>& rekeys);
    void ApplyRekeys(const std::vector<std::pair<std::string, std::string>>& rekeys, const fs::path& states);
    int move_failed_ = 0;
    fs::path move_to_, move_from_games_;
    double public_check_t_ = 0;
    void TrimArchiveCache();
    void DrawArchivePicker();
    // Conversión opcional de discos .cue/.bin a .chd (Ajustes » Biblioteca).
    void DrawChdPanel();
    void StartChdJob(const std::vector<fs::path>& cues, bool delete_originals);
    std::thread chd_thread_;
    std::atomic<bool> chd_running_{false}, chd_cancel_{false};
    std::atomic<float> chd_progress_{0};
    std::atomic<int> chd_index_{0}, chd_total_{0};
    std::mutex chd_mutex_;
    std::string chd_current_, chd_result_;
    bool chd_delete_ = false;
    size_t chd_cache_count_ = static_cast<size_t>(-1);
    uint64_t chd_cache_bytes_ = 0;
    Game archive_pick_;               // juego con varios archivos adentro (se pregunta cuál)
    std::string start_entry_;         // archivo de adentro elegido para el próximo inicio
    int NextTip();

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
    SDL_Renderer* renderer_ = nullptr;   // motor clásico (SDL)
    SDL_GLContext gl_ctx_ = nullptr;     // motor OpenGL
    bool gl_ = false;
    VideoOutGl video_gl_;
    HwRender hw_render_;
    void ApplyVsync();
    std::vector<uint8_t> CurrentFrameRGBA(unsigned* w, unsigned* h);
    void ApplyGpuChoice(const SystemDef& sys); // motor y resolución de CD y 64
    // Prueba del teléfono (Ñoño, build 27): mide la tarjeta gráfica y guarda la potencia (0 a 3), con la
    // que la resolución automática de las consolas 3D elige lo mejor que aguanta a 60 fps.
    void RunPhoneTest(bool tell);
    int PhonePower() const;
    // Más de 2 jugadores: conecta el adaptador de varios jugadores de la consola (Four Score,
    // Multitap, Team Player, 6Player). Devuelve cuántos jugadores quedaron.
    int ApplyPlayers(const SystemDef& sys, bool in_game);

    // Build 26 (app_phone.cpp): buscar en todo el teléfono, «Satronho Games» por consola y Satronho NX.
    bool EnsureGamesTree(const fs::path& root);
    void StartPhoneScan();
    void PollPhoneScan();
    void AddFoundFolders();
    void BeginOrganize();
    void PollOrganize();
    void NativePagePhoneScan(NPage& p);
    std::string NativeOrganizeJson() const;
    std::string NxFilesJson() const;
    void LaunchExternal(const Game& g);
    // Juegos de Switch con el id en el nombre («… [0100633007D48800]»): se sabe sin preguntarle a NX si
    // son juego, actualización o DLC (en PC es la única forma).
    void ClassifyNxByName();
    // Satronho NX en PC (Windows): se baja de sus Releases a la carpeta de datos y se abre con el juego.
    void LaunchNxDesktop(const Game& g);
    void PollNxDesktop();               // cada cuadro: avisos y tiempo jugado al volver de NX
    std::thread nx_desk_worker_;
    std::atomic<int> nx_desk_state_{0};  // 0 nada, 1 bajando, 2 abriendo, 3 jugando, 4 volvió, 5 error, 6 actualizando
    std::atomic<long long> nx_desk_secs_{0};
    std::string nx_desk_key_;
    int nx_desk_shown_ = 0;
    bool NativeCommandPhone(const std::vector<std::string>& a);
    void SyncBiosFolder();
    // Respaldos de partidas (app_backup.cpp).
    bool BackupSaves(const std::vector<Game>& games, const fs::path& out, int* saved_games);
    fs::path BackupFolder() const;
    void BackupAndShare(const std::vector<Game>& games, const std::string& what);
    std::string RestoreSaves(const fs::path& zip, int* restored, std::vector<std::string>* missing);
    void RestoreAndTell(const fs::path& zip);
    int restore_states_skipped_ = 0; // estados de otro núcleo que no se copiaron al restaurar
    PhoneScan phone_scan_;
    PhoneScan::Result phone_result_;
    bool phone_result_ready_ = false;
    std::atomic<int> organize_state_{0}; // 0 nada, 1 moviendo, 2 terminó
    std::atomic<int> organize_done_{0}, organize_total_{0};
    int organize_failed_ = 0;
    int players_applied_ = 0;
    int game_api_ = 0, game_res_ = 0; // ajustes del juego que se abre (0 = como la consola)
    int GpuApiFor(const SystemDef& sys) const;  // 1 OpenGL, 2 Vulkan, 3 procesador
    int GpuResFor(const SystemDef& sys, int api) const;
    std::vector<int> GpuResSteps(const SystemDef& sys, int api) const; // pasos que admite cada núcleo
    std::string ResLabel(int k, const SystemDef* sys = nullptr) const;  // «4× · 1280 × 960 (960p · Full HD)» o «1440 × 1080 (1080p · Full HD)»
    static int ResHeight(int k, const SystemDef* sys = nullptr); // alto en píxeles de un valor de resolución
    int NearestResStep(const SystemDef& sys, int api) const; // el paso más parecido al alto de la pantalla
    // Ajustes » Gráficos por consola (app_graphics.cpp).
    std::string gfx_sys_;               // consola elegida en la barra de Gráficos
    bool gfx_more_ = false;
    bool gfx_open_[4] = {true, true, true, false}; // secciones de Gráficos desplegadas             // «Más opciones de imagen del emulador» desplegadas
    std::string CoreOptionValue(const SystemDef& sys, const CoreOption& def) const;
    void SetCoreOptionFor(const SystemDef& sys, const std::string& key, const std::string& value);
    const std::vector<CoreOption>* CoreDefsFor(const SystemDef& sys);
    void MigrateGfxOptions();           // build 16 guardaba las opciones de gráficos aparte
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
    static constexpr int kCtrlHotkeys = 9;
    int ctrl_tab_ = 1;              // 0 general, 1..8 jugador 1..8, 9 atajos
    int CtrlPlayersShown() const;
    int ctrl_last_player_ = 1;   // cuántos jugadores se muestran en Controles (al menos 2)
    std::string core_tab_system_ = "octa";
    int profile_popup_ = 0;         // 0 cerrado, 1 nuevo, 2 renombrar
    int profile_player_ = 0;
    char profile_name_[48] = {};

    // Imágenes
    Ui::Image logo_;
    Ui::Image guides_[static_cast<int>(GuideId::Count)];
    Covers covers_;
    bool gfx_vulkan_available_ = false; // hay Vulkan en este equipo

    // Preguntas al salir: 0 ninguna, 1 cerrar la app, 2 salir del juego.
    int confirm_ = 0;
    bool confirm_dont_ask_ = false;
    bool show_whats_new_ = false;     // Juana muestra lo nuevo después de actualizar
    bool update_card_later_ = false;
    bool phone_test_tell_ = false;
    std::string prefetched_key_;       // el último juego precargado desde el carrusel     // Ñoño cuenta el resultado de la prueba del teléfono  // «Después» en la tarjeta de build nueva (hasta cerrar la app)

    // Conversación con los guías.
    struct ChatMsg {
        int guide = -1;               // -1 = la persona
        std::string text;
        std::vector<ChatButton> buttons;
        double born = 0;
    };
    std::vector<ChatMsg> chat_;
    int chat_guide_ = -1;             // a quién se le pregunta (-1 = a quien sepa más)
    char chat_input_[256] = {};
    bool chat_scroll_ = false;
    bool chat_focus_ = false;

    // Transición entre páginas y carrusel.
    Page shown_page_ = Page::Library;
    double page_anim_t_ = 0;
    float carousel_pos_ = 0;          // posición animada (índice con decimales)
    int carousel_index_ = 0;
    std::string carousel_key_;        // juego seleccionado en el carrusel
    std::string carousel_list_;       // consola del carrusel (para reiniciar al cambiar)
    std::string cover_target_;        // juego al que se le elige carátula

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
    // Cómo fue la sesión (para el resumen al salir y la ficha del juego).
    double ses_fps_sum_ = 0, ses_fps_low_ = 1e9, ses_sample_t_ = 0;
    int ses_fps_n_ = 0, ses_stutters0_ = 0;
    void RecordSession();
    void NativeGameRecord(NPage& p, const Game& g); // la ficha: tiempo, sesiones y su gráfica
    std::string LastSessionBadge(const std::string& key) const;
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
    int analog_pulse_ = 0;              // cuadros con L1+R1+Select (botón Analog del DualShock en CD)
    double audio_full_s_ = 0;           // tiempo con el sonido sin avanzar (el juego no corre)
    // Juego lento: Ñoño ofrece bajar la resolución (una vez por juego).
    double slow_s_ = 0;
    bool slow_asked_ = false, slow_prompt_ = false, slow_was_paused_ = false;
    void WatchSlowGame(double dt, double target_fps);
    void SendLog(); // el registro (esta sesión y la anterior) al menú Compartir y a «Satronho Games»
    void DrawSlowPrompt();
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
    std::vector<std::string> pending_restores_; // respaldos de partidas elegidos en el PC
    std::vector<std::string> pending_covers_;
    bool bios_hint_shown_ = false;
    std::string bios_target_;
    float sidebar_full_h_ = 0;
    std::string last_core_msg_;
    double last_core_msg_t_ = 0;

    // Modo de revisión automática (SATRONHO_QA).
    std::map<std::string, std::string> qa_;
    int qa_frames_ = 0;
    bool qa_failed_ = false;   // una prueba del modo de revisión falló (la app sale con código 2)
    float qa_scroll_ref_ = -1; // prueba de desplazamiento: hasta dónde llegaba la página al empezar
    float qa_scroll_mid_ = -1; // …y a mitad de la prueba
    void QaScrollTest();       // «desplazar=1»: baja y mueve de lado; la página no debe crecer

    // Interfaz nativa de Android (app_native.cpp, build 25). Las pantallas las dibuja Kotlin y aquí
    // se atienden sus órdenes y se le pasa el estado. En PC NativeMode() siempre es false.
    bool NativeMode() const;
    bool NativeNeedsImGui() const;   // hay algo de la interfaz de siempre encima (Kotlin se esconde)
    void NativeTick();
    void NativeCommand(const std::string& line);
    std::string NativeStateJson();
    std::string NativeLibraryJson();
    std::string NativeTouchJson();
    void NativeApplyTouch();
    const Game* FindGame(const std::string& key);
    void SetNativeMenu(bool open);
    void SlowLower();                // «Bajar la resolución» (Ñoño, juego lento)
    void SlowKeep();                 // «Seguir así»
    int legacy_page_ = -1;           // pantalla de siempre pedida desde la nativa (-1: ninguna)
    bool native_menu_ = false;       // cajón del juego abierto: el juego espera
    bool slow_card_sent_ = false;    // la tarjeta de Ñoño del juego lento ya se mandó
    double native_lib_t_ = -10;      // última vez que se armó la biblioteca para Kotlin
    std::string native_touch_cache_; // control en pantalla ya armado (cambia poco)
    int native_touch_w_ = 0, native_touch_h_ = 0;
    std::string native_touch_sys_;
    uint32_t native_touch_prev_ = 0;
    // Ajustes nativos (app_native_pages.cpp): la pantalla abierta y qué hace cada elemento.
    std::string native_page_, native_page_arg_, native_page_json_;
    double native_page_t_ = -10;
    std::map<std::string, std::function<void(const std::string&)>> native_setters_;
    std::string NativeSettingsJson();
    void NativePageControls(NPage& p);
    void NativePageGraphics(NPage& p);  // app_graphics.cpp
    void NativePageCore(NPage& p);      // app_graphics.cpp
    void NativePageTools(NPage& p);     // app_tools.cpp
    std::string NativeChatJson();
    std::string NativeExtrasJson();
    std::string NativeImportJson();     // app_import.cpp
    void NativeMeterJson(std::string& s);
    bool NativeCommandExtra(const std::vector<std::string>& a);

    friend void FolderDialogCallback(void*, const char* const*, int);
    friend void FileDialogCallback(void*, const char* const*, int);
    friend void BiosDialogCallback(void*, const char* const*, int);
    friend void CoverDialogCallback(void*, const char* const*, int);
    friend void RestoreDialogCallback(void*, const char* const*, int);
};

// Respaldo de partidas elegido en el PC (diálogo de SDL).
void RestoreDialogCallback(void* user, const char* const* list, int);

} // namespace Satronho
