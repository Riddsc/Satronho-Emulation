// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "libretro.h"
#include "util/paths.h"

namespace Satronho {

// Una opción del núcleo (por ejemplo, la paleta o el recorte de bordes de Nestopia).
struct CoreOption {
    std::string key;
    std::string label;
    std::string info;
    std::string category;
    struct Value {
        std::string value;
        std::string label;
    };
    std::vector<Value> values;
    std::string def;       // por defecto (el de Satronho si lo cambia para esa consola)
    std::string core_def;  // el que trae el núcleo
    std::string current;
    bool visible = true;
};

// Ayudas para listas de opciones (las del núcleo cargado o una copia guardada).
void ResetToCoreDefaults(std::vector<CoreOption>& opts);
bool OverrideDefault(std::vector<CoreOption>& opts, const std::string& key, const std::string& value);
void ApplySaved(std::vector<CoreOption>& opts, const std::function<std::string(const std::string&)>& lookup);

struct CoreCategory {
    std::string key;
    std::string label;
};

// Estado de los mandos que el núcleo lee en cada cuadro (RetroPad).
struct PadState {
    uint16_t buttons = 0; // bits RETRO_DEVICE_ID_JOYPAD_*
    int16_t analog[2][2] = {};
    // Pantalla táctil (DS): posición en el rango de libretro (-0x7fff..0x7fff) y si se toca.
    int16_t pointer_x = 0, pointer_y = 0;
    bool pointer_down = false;
};

enum class PixelFormat { XRGB1555, XRGB8888, RGB565 };

// Dibujo por tarjeta gráfica que piden algunos núcleos (CD con Beetle PSX HW, 64 con GLideN64 o
// ParaLLEl-RDP). La aplicación lo implementa con OpenGL o Vulkan (gfx/hw_render).
class HwRenderHost {
public:
    virtual ~HwRenderHost() = default;
    virtual bool Accept(retro_hw_render_callback* cb) = 0;   // RETRO_ENVIRONMENT_SET_HW_RENDER
    virtual unsigned Preferred() const = 0;                  // RETRO_HW_CONTEXT_* que conviene en este equipo
    virtual bool Interface(const retro_hw_render_interface** out) = 0;
    virtual bool Negotiation(const retro_hw_render_context_negotiation_interface* iface) = 0;
    virtual bool Start(const retro_system_av_info& av) = 0;  // tras cargar el juego: crea el contexto
    virtual void Stop() = 0;                                 // al cerrar el juego
    virtual void Enter() = 0;                                // antes de llamar al núcleo
    virtual void Leave() = 0;                                // después
    virtual void Frame(unsigned w, unsigned h) = 0;          // el núcleo entregó un cuadro
    virtual void Geometry(const retro_system_av_info& av) = 0;
    virtual uintptr_t Framebuffer() = 0;
    virtual retro_proc_address_t ProcAddress(const char* sym) = 0;
};

// Carga un núcleo libretro (.dll/.so), le habla al juego y guarda lo que el núcleo entrega:
// la imagen, el sonido y los mensajes.
class CoreHost {
public:
    CoreHost();
    ~CoreHost();

    bool LoadCore(const fs::path& path, std::string* error);
    void UnloadCore();
    bool CoreLoaded() const { return lib_ != nullptr; }

    // data: el juego ya en memoria (sacado de un .zip/.7z); rom queda como nombre del juego.
    bool LoadGame(const fs::path& rom, std::string* error, std::vector<uint8_t>* data = nullptr);
    void UnloadGame();
    bool GameLoaded() const { return game_loaded_; }

    void RunFrame();
    void Reset();

    // Estados y partidas.
    size_t StateSize() const;
    bool SaveState(std::vector<uint8_t>& out) const;
    bool LoadState(const std::vector<uint8_t>& in);
    bool SaveStateFile(const fs::path& file) const;
    bool LoadStateFile(const fs::path& file);
    void SaveSram();

    // Opciones del núcleo.
    std::vector<CoreOption>& Options() { return options_; }
    const std::vector<CoreCategory>& Categories() const { return categories_; }
    void SetOption(const std::string& key, const std::string& value);
    void MarkOptionsChanged() { options_updated_ = true; }

    // Información.
    const retro_system_info& SystemInfo() const { return sys_info_; }
    const retro_system_av_info& AvInfo() const { return av_info_; }
    double Fps() const { return av_info_.timing.fps > 1 ? av_info_.timing.fps : 60.0; }
    double SampleRate() const { return av_info_.timing.sample_rate > 1000 ? av_info_.timing.sample_rate : 48000.0; }
    float AspectRatio() const;
    std::vector<std::string> Extensions() const;
    const fs::path& RomPath() const { return rom_path_; }
    std::string GameKey() const; // nombre base del juego para partidas y estados

    // Carpeta de partidas de la consola actual (partidas/<consola>). Los núcleos también
    // guardan ahí lo suyo (tarjetas de memoria, .bkr de Saturn). La ruta queda fija en memoria
    // porque algunos núcleos guardan el puntero que se les da.
    void SetSaveDir(const fs::path& dir);
    fs::path SramPath() const;

    // Imagen del último cuadro.
    const std::vector<uint8_t>& Frame() const { return frame_; }
    unsigned FrameWidth() const { return frame_w_; }
    unsigned FrameHeight() const { return frame_h_; }
    size_t FramePitch() const { return frame_pitch_; }
    PixelFormat Format() const { return pixel_format_; }
    uint64_t FrameSerial() const { return frame_serial_; }
    bool GeometryChanged() { bool c = geometry_changed_; geometry_changed_ = false; return c; }
    // Copia el cuadro actual en RGBA de 32 bits (para capturas y miniaturas).
    std::vector<uint8_t> FrameRGBA(unsigned* w, unsigned* h) const;

    // Entradas y salidas que conecta la aplicación.
    PadState pads[4];
    std::function<void(const int16_t*, size_t)> on_audio;     // frames estéreo intercalados
    std::function<void(const std::string&, int)> on_message; // texto, duración en ms
    std::function<float(unsigned)> on_sensor;                // RETRO_SENSOR_* -> valor
    bool video_enabled = true;
    bool audio_enabled = true;
    bool fast_forwarding = false;
    float target_refresh = 60.0f;
    unsigned port_device = RETRO_DEVICE_JOYPAD; // tipo de mando que se conecta (DualShock en CD)
    // Cambia el tipo de mando con el juego abierto (como desenchufar uno y enchufar otro).
    void SetPortDevice(unsigned device) {
        port_device = device;
        if (retro_set_controller_port_device_ && GameLoaded())
            for (unsigned port = 0; port < 2; ++port) retro_set_controller_port_device_(port, device);
    }

    // Vibración que pide el núcleo (Rumble Pak, DualShock): 0..65535 por puerto, fuerte y débil.
    uint16_t rumble[4][2] = {};
    // true una vez si el núcleo cambió la frecuencia del sonido a mitad del juego.
    bool TakeAudioRateChanged() { bool c = audio_rate_changed_; audio_rate_changed_ = false; return c; }

    // Discos (juegos de varios CD).
    unsigned DiskCount() const;
    int DiskIndex() const;
    std::string DiskLabel(unsigned i) const;
    bool SetDisk(unsigned index);      // todo de una vez (abrir, cambiar y cerrar)
    bool EjectDisk(bool open);         // abrir o cerrar la tapa
    bool InsertDisk(unsigned index);   // con la tapa abierta

    // Información para el medidor.
    double last_run_ms = 0.0;

    // Tarjeta gráfica: quién atiende a los núcleos que la piden (nullptr = solo software).
    HwRenderHost* hw_host = nullptr;
    bool HwActive() const { return hw_active_; }
    const retro_hw_render_callback& HwCallback() const { return hw_cb_; }

private:
    friend struct CoreCallbacks;

    bool Environment(unsigned cmd, void* data);
    void Video(const void* data, unsigned w, unsigned h, size_t pitch);
    void SetCoreOptionsV1(const retro_core_option_definition* defs, const retro_core_option_definition* local);
    void SetCoreOptionsV2(const retro_core_options_v2* opts, const retro_core_options_v2* local);
    void LoadSram();
    void HwEnter() const { if (hw_active_ && hw_host) hw_host->Enter(); }
    void HwLeave() const { if (hw_active_ && hw_host) hw_host->Leave(); }

    retro_hw_render_callback hw_cb_{};
    bool hw_requested_ = false;
    bool hw_active_ = false;

    void* lib_ = nullptr;
    bool game_loaded_ = false;
    retro_disk_control_ext_callback disk_{};
    bool has_disk_ = false;
    std::string last_msg_;
    fs::path rom_path_;
    std::vector<uint8_t> rom_data_;
    std::string system_dir_, save_dir_;
    fs::path save_path_;
    std::map<std::string, std::string> save_dirs_; // textos estables para GET_SAVE_DIRECTORY
    bool sram_backed_up_ = false;
    bool audio_rate_changed_ = false;
    retro_system_info sys_info_{};
    retro_system_av_info av_info_{};
    bool support_no_game_ = false;
    PixelFormat pixel_format_ = PixelFormat::XRGB1555;

    std::vector<uint8_t> frame_;
    unsigned frame_w_ = 0, frame_h_ = 0;
    size_t frame_pitch_ = 0;
    uint64_t frame_serial_ = 0;
    bool geometry_changed_ = true;

    std::vector<CoreOption> options_;
    std::vector<CoreCategory> categories_;
    bool options_updated_ = false;
    std::vector<uint8_t> sram_snapshot_;

    // Funciones del núcleo.
    void (*retro_init_)() = nullptr;
    void (*retro_deinit_)() = nullptr;
    unsigned (*retro_api_version_)() = nullptr;
    void (*retro_get_system_info_)(retro_system_info*) = nullptr;
    void (*retro_get_system_av_info_)(retro_system_av_info*) = nullptr;
    void (*retro_set_environment_)(retro_environment_t) = nullptr;
    void (*retro_set_video_refresh_)(retro_video_refresh_t) = nullptr;
    void (*retro_set_audio_sample_)(retro_audio_sample_t) = nullptr;
    void (*retro_set_audio_sample_batch_)(retro_audio_sample_batch_t) = nullptr;
    void (*retro_set_input_poll_)(retro_input_poll_t) = nullptr;
    void (*retro_set_input_state_)(retro_input_state_t) = nullptr;
    void (*retro_set_controller_port_device_)(unsigned, unsigned) = nullptr;
    void (*retro_reset_)() = nullptr;
    void (*retro_run_)() = nullptr;
    size_t (*retro_serialize_size_)() = nullptr;
    bool (*retro_serialize_)(void*, size_t) = nullptr;
    bool (*retro_unserialize_)(const void*, size_t) = nullptr;
    bool (*retro_load_game_)(const retro_game_info*) = nullptr;
    void (*retro_unload_game_)() = nullptr;
    void* (*retro_get_memory_data_)(unsigned) = nullptr;
    size_t (*retro_get_memory_size_)(unsigned) = nullptr;
};

} // namespace Satronho
