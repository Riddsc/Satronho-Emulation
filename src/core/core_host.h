// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <functional>
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
    std::string def;
    std::string current;
    bool visible = true;
};

struct CoreCategory {
    std::string key;
    std::string label;
};

// Estado de los mandos que el núcleo lee en cada cuadro (RetroPad).
struct PadState {
    uint16_t buttons = 0; // bits RETRO_DEVICE_ID_JOYPAD_*
    int16_t analog[2][2] = {};
};

enum class PixelFormat { XRGB1555, XRGB8888, RGB565 };

// Carga un núcleo libretro (.dll/.so), le habla al juego y guarda lo que el núcleo entrega:
// la imagen, el sonido y los mensajes.
class CoreHost {
public:
    CoreHost();
    ~CoreHost();

    bool LoadCore(const fs::path& path, std::string* error);
    void UnloadCore();
    bool CoreLoaded() const { return lib_ != nullptr; }

    bool LoadGame(const fs::path& rom, std::string* error);
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
    void OverrideDefault(const std::string& key, const std::string& value); // valor por defecto de Satronho
    void ApplySavedOptions(const std::function<std::string(const std::string&)>& lookup);

    // Información.
    const retro_system_info& SystemInfo() const { return sys_info_; }
    const retro_system_av_info& AvInfo() const { return av_info_; }
    double Fps() const { return av_info_.timing.fps > 1 ? av_info_.timing.fps : 60.0; }
    double SampleRate() const { return av_info_.timing.sample_rate > 1000 ? av_info_.timing.sample_rate : 48000.0; }
    float AspectRatio() const;
    std::vector<std::string> Extensions() const;
    const fs::path& RomPath() const { return rom_path_; }
    std::string GameKey() const; // nombre base del juego para partidas y estados

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

    // Discos (juegos de varios CD).
    unsigned DiskCount() const;
    int DiskIndex() const;
    std::string DiskLabel(unsigned i) const;
    bool SetDisk(unsigned index);

    // Información para el medidor.
    double last_run_ms = 0.0;

private:
    friend struct CoreCallbacks;

    bool Environment(unsigned cmd, void* data);
    void Video(const void* data, unsigned w, unsigned h, size_t pitch);
    void SetCoreOptionsV1(const retro_core_option_definition* defs);
    void SetCoreOptionsV2(const retro_core_options_v2* opts, const retro_core_options_v2* local);
    void LoadSram();
    fs::path SramPath() const;

    void* lib_ = nullptr;
    bool game_loaded_ = false;
    retro_disk_control_ext_callback disk_{};
    bool has_disk_ = false;
    fs::path rom_path_;
    std::vector<uint8_t> rom_data_;
    std::string system_dir_, save_dir_;
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
