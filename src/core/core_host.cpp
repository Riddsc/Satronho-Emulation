// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/core_host.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>

#include <SDL3/SDL.h>

#include "util/log.h"

namespace Satronho {

namespace {
CoreHost* g_host = nullptr;

constexpr unsigned Cmd(unsigned c) { return c & ~static_cast<unsigned>(RETRO_ENVIRONMENT_EXPERIMENTAL); }

bool ReadFile(const fs::path& file, std::vector<uint8_t>& out) {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto size = in.tellg();
    if (size < 0) return false;
    out.resize(static_cast<size_t>(size));
    in.seekg(0);
    return static_cast<bool>(in.read(reinterpret_cast<char*>(out.data()), size));
}

bool WriteFileAtomic(const fs::path& file, const void* data, size_t size) {
    fs::path tmp = file;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!out) return false;
    }
    std::error_code ec;
    fs::rename(tmp, file, ec);
    if (ec) {
        fs::remove(file, ec);
        fs::rename(tmp, file, ec);
    }
    return !ec;
}

void CoreLog(enum retro_log_level level, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    size_t n = std::strlen(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
    switch (level) {
    case RETRO_LOG_ERROR: Log::Error("[núcleo] %s", buf); break;
    case RETRO_LOG_WARN: Log::Warn("[núcleo] %s", buf); break;
    default: Log::Info("[núcleo] %s", buf); break;
    }
}
} // namespace

struct CoreCallbacks {
    static bool Env(unsigned cmd, void* data) { return g_host && g_host->Environment(cmd, data); }
    static void Video(const void* data, unsigned w, unsigned h, size_t pitch) {
        if (g_host) g_host->Video(data, w, h, pitch);
    }
    static void Sample(int16_t l, int16_t r) {
        if (g_host && g_host->audio_enabled && g_host->on_audio) {
            const int16_t s[2] = {l, r};
            g_host->on_audio(s, 1);
        }
    }
    static size_t Batch(const int16_t* data, size_t frames) {
        if (g_host && g_host->audio_enabled && g_host->on_audio) g_host->on_audio(data, frames);
        return frames;
    }
    static void Poll() {}
    static int16_t State(unsigned port, unsigned device, unsigned index, unsigned id) {
        if (!g_host || port >= 4) return 0;
        const PadState& pad = g_host->pads[port];
        switch (device & RETRO_DEVICE_MASK) {
        case RETRO_DEVICE_JOYPAD:
            if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return static_cast<int16_t>(pad.buttons);
            return id < 16 ? ((pad.buttons >> id) & 1) : 0;
        case RETRO_DEVICE_ANALOG:
            if (index < 2 && id < 2) return pad.analog[index][id];
            return 0;
        default:
            return 0;
        }
    }
};

CoreHost::CoreHost() { g_host = this; }

CoreHost::~CoreHost() {
    UnloadCore();
    if (g_host == this) g_host = nullptr;
}

template <typename T>
static bool Sym(void* lib, const char* name, T& out) {
    out = reinterpret_cast<T>(SDL_LoadFunction(static_cast<SDL_SharedObject*>(lib), name));
    return out != nullptr;
}

bool CoreHost::LoadCore(const fs::path& path, std::string* error) {
    UnloadCore();
    const std::string p = Paths::ToUtf8(path);
    lib_ = SDL_LoadObject(p.c_str());
    if (!lib_) {
        if (error) *error = std::string("No se pudo abrir el núcleo: ") + SDL_GetError();
        return false;
    }
    bool ok = true;
    ok &= Sym(lib_, "retro_init", retro_init_);
    ok &= Sym(lib_, "retro_deinit", retro_deinit_);
    ok &= Sym(lib_, "retro_api_version", retro_api_version_);
    ok &= Sym(lib_, "retro_get_system_info", retro_get_system_info_);
    ok &= Sym(lib_, "retro_get_system_av_info", retro_get_system_av_info_);
    ok &= Sym(lib_, "retro_set_environment", retro_set_environment_);
    ok &= Sym(lib_, "retro_set_video_refresh", retro_set_video_refresh_);
    ok &= Sym(lib_, "retro_set_audio_sample", retro_set_audio_sample_);
    ok &= Sym(lib_, "retro_set_audio_sample_batch", retro_set_audio_sample_batch_);
    ok &= Sym(lib_, "retro_set_input_poll", retro_set_input_poll_);
    ok &= Sym(lib_, "retro_set_input_state", retro_set_input_state_);
    ok &= Sym(lib_, "retro_set_controller_port_device", retro_set_controller_port_device_);
    ok &= Sym(lib_, "retro_reset", retro_reset_);
    ok &= Sym(lib_, "retro_run", retro_run_);
    ok &= Sym(lib_, "retro_serialize_size", retro_serialize_size_);
    ok &= Sym(lib_, "retro_serialize", retro_serialize_);
    ok &= Sym(lib_, "retro_unserialize", retro_unserialize_);
    ok &= Sym(lib_, "retro_load_game", retro_load_game_);
    ok &= Sym(lib_, "retro_unload_game", retro_unload_game_);
    ok &= Sym(lib_, "retro_get_memory_data", retro_get_memory_data_);
    ok &= Sym(lib_, "retro_get_memory_size", retro_get_memory_size_);
    if (!ok || retro_api_version_() != RETRO_API_VERSION) {
        if (error) *error = "El archivo no es un núcleo compatible.";
        SDL_UnloadObject(static_cast<SDL_SharedObject*>(lib_));
        lib_ = nullptr;
        return false;
    }

    system_dir_ = Paths::ToUtf8(Paths::System());
    save_dir_ = Paths::ToUtf8(Paths::Saves());
    options_.clear();
    categories_.clear();

    retro_set_environment_(&CoreCallbacks::Env);
    retro_set_video_refresh_(&CoreCallbacks::Video);
    retro_set_audio_sample_(&CoreCallbacks::Sample);
    retro_set_audio_sample_batch_(&CoreCallbacks::Batch);
    retro_set_input_poll_(&CoreCallbacks::Poll);
    retro_set_input_state_(&CoreCallbacks::State);
    retro_init_();
    std::memset(&sys_info_, 0, sizeof sys_info_);
    retro_get_system_info_(&sys_info_);
    Log::Info("Núcleo cargado: %s %s (%s)", sys_info_.library_name ? sys_info_.library_name : "?",
              sys_info_.library_version ? sys_info_.library_version : "",
              sys_info_.valid_extensions ? sys_info_.valid_extensions : "");
    return true;
}

void CoreHost::UnloadCore() {
    if (!lib_) return;
    UnloadGame();
    retro_deinit_();
    SDL_UnloadObject(static_cast<SDL_SharedObject*>(lib_));
    lib_ = nullptr;
    options_.clear();
    categories_.clear();
}

std::vector<std::string> CoreHost::Extensions() const {
    std::vector<std::string> out;
    if (!sys_info_.valid_extensions) return out;
    std::string cur;
    for (const char* c = sys_info_.valid_extensions;; ++c) {
        if (*c == '|' || *c == 0) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
            if (*c == 0) break;
        } else {
            cur += static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
        }
    }
    return out;
}

bool CoreHost::LoadGame(const fs::path& rom, std::string* error) {
    if (!lib_) return false;
    UnloadGame();
    rom_data_.clear();
    if (!ReadFile(rom, rom_data_)) {
        if (error) *error = "No se pudo leer el archivo del juego.";
        return false;
    }
    const std::string path_utf8 = Paths::ToUtf8(rom);
    retro_game_info info{};
    info.path = path_utf8.c_str();
    if (!sys_info_.need_fullpath) {
        info.data = rom_data_.data();
        info.size = rom_data_.size();
    }
    rom_path_ = rom;
    frame_.clear();
    frame_w_ = frame_h_ = 0;
    if (!retro_load_game_(&info)) {
        if (error) *error = "El núcleo no pudo abrir este juego. Puede que el archivo esté dañado o que su chip no sea compatible.";
        rom_path_.clear();
        return false;
    }
    game_loaded_ = true;
    std::memset(&av_info_, 0, sizeof av_info_);
    retro_get_system_av_info_(&av_info_);
    geometry_changed_ = true;
    for (unsigned port = 0; port < 2; ++port) retro_set_controller_port_device_(port, RETRO_DEVICE_JOYPAD);
    LoadSram();
    Log::Info("Juego cargado: %s (%ux%u, %.4f fps, %.0f Hz)", path_utf8.c_str(), av_info_.geometry.base_width,
              av_info_.geometry.base_height, av_info_.timing.fps, av_info_.timing.sample_rate);
    return true;
}

void CoreHost::UnloadGame() {
    if (!game_loaded_) return;
    SaveSram();
    retro_unload_game_();
    game_loaded_ = false;
    rom_path_.clear();
    rom_data_.clear();
    sram_snapshot_.clear();
    for (auto& p : pads) p = {};
}

void CoreHost::RunFrame() {
    if (!game_loaded_) return;
    const Uint64 t0 = SDL_GetPerformanceCounter();
    retro_run_();
    const Uint64 t1 = SDL_GetPerformanceCounter();
    last_run_ms = static_cast<double>(t1 - t0) * 1000.0 / static_cast<double>(SDL_GetPerformanceFrequency());
}

void CoreHost::Reset() {
    if (game_loaded_) retro_reset_();
}

float CoreHost::AspectRatio() const {
    const auto& g = av_info_.geometry;
    if (g.aspect_ratio > 0.01f) return g.aspect_ratio;
    if (frame_h_) return static_cast<float>(frame_w_) / static_cast<float>(frame_h_);
    if (g.base_height) return static_cast<float>(g.base_width) / static_cast<float>(g.base_height);
    return 4.0f / 3.0f;
}

std::string CoreHost::GameKey() const { return Paths::SafeName(Paths::ToUtf8(rom_path_.stem())); }

// ---------------------------------------------------------------- estados y partidas

size_t CoreHost::StateSize() const { return game_loaded_ ? retro_serialize_size_() : 0; }

bool CoreHost::SaveState(std::vector<uint8_t>& out) const {
    const size_t size = StateSize();
    if (!size) return false;
    out.resize(size);
    return retro_serialize_(out.data(), size);
}

bool CoreHost::LoadState(const std::vector<uint8_t>& in) {
    if (!game_loaded_ || in.empty()) return false;
    return retro_unserialize_(in.data(), in.size());
}

bool CoreHost::SaveStateFile(const fs::path& file) const {
    std::vector<uint8_t> data;
    if (!SaveState(data)) return false;
    return WriteFileAtomic(file, data.data(), data.size());
}

bool CoreHost::LoadStateFile(const fs::path& file) {
    std::vector<uint8_t> data;
    if (!ReadFile(file, data)) return false;
    return LoadState(data);
}

fs::path CoreHost::SramPath() const { return Paths::Saves() / (GameKey() + ".srm"); }

void CoreHost::LoadSram() {
    const size_t size = retro_get_memory_size_(RETRO_MEMORY_SAVE_RAM);
    void* mem = retro_get_memory_data_(RETRO_MEMORY_SAVE_RAM);
    if (!size || !mem) return;
    std::vector<uint8_t> data;
    if (ReadFile(SramPath(), data)) {
        std::memcpy(mem, data.data(), std::min(size, data.size()));
        Log::Info("Partida cargada (%zu bytes)", data.size());
    }
    sram_snapshot_.assign(static_cast<uint8_t*>(mem), static_cast<uint8_t*>(mem) + size);
}

void CoreHost::SaveSram() {
    if (!game_loaded_) return;
    const size_t size = retro_get_memory_size_(RETRO_MEMORY_SAVE_RAM);
    const auto* mem = static_cast<const uint8_t*>(retro_get_memory_data_(RETRO_MEMORY_SAVE_RAM));
    if (!size || !mem) return;
    if (sram_snapshot_.size() == size && std::memcmp(sram_snapshot_.data(), mem, size) == 0) return;
    if (WriteFileAtomic(SramPath(), mem, size)) {
        sram_snapshot_.assign(mem, mem + size);
        Log::Info("Partida guardada (%zu bytes)", size);
    }
}

// ---------------------------------------------------------------- imagen

void CoreHost::Video(const void* data, unsigned w, unsigned h, size_t pitch) {
    if (!data || data == RETRO_HW_FRAME_BUFFER_VALID || !w || !h) return; // cuadro repetido
    if (!video_enabled) return;
    const size_t bpp = pixel_format_ == PixelFormat::XRGB8888 ? 4 : 2;
    const size_t row = static_cast<size_t>(w) * bpp;
    if (w != frame_w_ || h != frame_h_) geometry_changed_ = true;
    frame_.resize(row * h);
    const auto* src = static_cast<const uint8_t*>(data);
    for (unsigned y = 0; y < h; ++y) std::memcpy(frame_.data() + y * row, src + y * pitch, row);
    frame_w_ = w;
    frame_h_ = h;
    frame_pitch_ = row;
    ++frame_serial_;
}

std::vector<uint8_t> CoreHost::FrameRGBA(unsigned* w, unsigned* h) const {
    std::vector<uint8_t> out;
    *w = frame_w_;
    *h = frame_h_;
    if (frame_.empty()) return out;
    out.resize(static_cast<size_t>(frame_w_) * frame_h_ * 4);
    for (unsigned y = 0; y < frame_h_; ++y) {
        const uint8_t* row = frame_.data() + y * frame_pitch_;
        uint8_t* dst = out.data() + static_cast<size_t>(y) * frame_w_ * 4;
        for (unsigned x = 0; x < frame_w_; ++x, dst += 4) {
            uint8_t r, g, b;
            if (pixel_format_ == PixelFormat::XRGB8888) {
                uint32_t p;
                std::memcpy(&p, row + x * 4, 4);
                r = (p >> 16) & 0xFF; g = (p >> 8) & 0xFF; b = p & 0xFF;
            } else {
                uint16_t p;
                std::memcpy(&p, row + x * 2, 2);
                if (pixel_format_ == PixelFormat::RGB565) {
                    r = static_cast<uint8_t>(((p >> 11) & 0x1F) * 255 / 31);
                    g = static_cast<uint8_t>(((p >> 5) & 0x3F) * 255 / 63);
                    b = static_cast<uint8_t>((p & 0x1F) * 255 / 31);
                } else {
                    r = static_cast<uint8_t>(((p >> 10) & 0x1F) * 255 / 31);
                    g = static_cast<uint8_t>(((p >> 5) & 0x1F) * 255 / 31);
                    b = static_cast<uint8_t>((p & 0x1F) * 255 / 31);
                }
            }
            dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = 255;
        }
    }
    return out;
}

// ---------------------------------------------------------------- opciones

void CoreHost::SetOption(const std::string& key, const std::string& value) {
    for (auto& o : options_) {
        if (o.key == key && o.current != value) {
            o.current = value;
            options_updated_ = true;
        }
    }
}

void CoreHost::OverrideDefault(const std::string& key, const std::string& value) {
    for (auto& o : options_) {
        if (o.key != key) continue;
        for (const auto& v : o.values) {
            if (v.value == value) {
                o.def = value;
                o.current = value;
                options_updated_ = true;
            }
        }
    }
}

void CoreHost::ApplySavedOptions(const std::function<std::string(const std::string&)>& lookup) {
    for (auto& o : options_) {
        std::string v = lookup(o.key);
        if (v.empty()) continue;
        for (const auto& val : o.values) {
            if (val.value == v) {
                o.current = v;
                options_updated_ = true;
                break;
            }
        }
    }
}

void CoreHost::SetCoreOptionsV1(const retro_core_option_definition* defs) {
    options_.clear();
    for (; defs && defs->key; ++defs) {
        CoreOption o;
        o.key = defs->key;
        o.label = defs->desc ? defs->desc : defs->key;
        o.info = defs->info ? defs->info : "";
        for (const auto& v : defs->values) {
            if (!v.value) break;
            o.values.push_back({v.value, v.label ? v.label : v.value});
        }
        o.def = defs->default_value ? defs->default_value : (o.values.empty() ? "" : o.values[0].value);
        o.current = o.def;
        options_.push_back(std::move(o));
    }
}

void CoreHost::SetCoreOptionsV2(const retro_core_options_v2* us, const retro_core_options_v2* local) {
    options_.clear();
    categories_.clear();
    if (!us) return;
    auto find_local_def = [&](const char* key) -> const retro_core_option_v2_definition* {
        if (!local || !local->definitions) return nullptr;
        for (auto* d = local->definitions; d->key; ++d)
            if (std::strcmp(d->key, key) == 0) return d;
        return nullptr;
    };
    auto find_local_cat = [&](const char* key) -> const retro_core_option_v2_category* {
        if (!local || !local->categories) return nullptr;
        for (auto* c = local->categories; c->key; ++c)
            if (std::strcmp(c->key, key) == 0) return c;
        return nullptr;
    };
    if (us->categories) {
        for (auto* c = us->categories; c->key; ++c) {
            const auto* lc = find_local_cat(c->key);
            const char* desc = lc && lc->desc ? lc->desc : c->desc;
            categories_.push_back({c->key, desc ? desc : c->key});
        }
    }
    for (auto* d = us->definitions; d && d->key; ++d) {
        const auto* ld = find_local_def(d->key);
        CoreOption o;
        o.key = d->key;
        const char* desc = ld && ld->desc ? ld->desc : (d->desc_categorized ? d->desc_categorized : d->desc);
        if (ld && ld->desc_categorized) desc = ld->desc_categorized;
        o.label = desc ? desc : d->key;
        const char* info = ld && ld->info ? ld->info : d->info;
        o.info = info ? info : "";
        o.category = d->category_key ? d->category_key : "";
        for (int i = 0; i < RETRO_NUM_CORE_OPTION_VALUES_MAX && d->values[i].value; ++i) {
            const char* label = d->values[i].label;
            if (ld) {
                for (int j = 0; j < RETRO_NUM_CORE_OPTION_VALUES_MAX && ld->values[j].value; ++j)
                    if (std::strcmp(ld->values[j].value, d->values[i].value) == 0 && ld->values[j].label)
                        label = ld->values[j].label;
            }
            o.values.push_back({d->values[i].value, label ? label : d->values[i].value});
        }
        o.def = d->default_value ? d->default_value : (o.values.empty() ? "" : o.values[0].value);
        o.current = o.def;
        options_.push_back(std::move(o));
    }
}

// ---------------------------------------------------------------- entorno

bool CoreHost::Environment(unsigned cmd, void* data) {
    switch (Cmd(cmd)) {
    case Cmd(RETRO_ENVIRONMENT_GET_CAN_DUPE):
        *static_cast<bool*>(data) = true;
        return true;
    case Cmd(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT): {
        const auto fmt = *static_cast<const retro_pixel_format*>(data);
        if (fmt == RETRO_PIXEL_FORMAT_XRGB8888) pixel_format_ = PixelFormat::XRGB8888;
        else if (fmt == RETRO_PIXEL_FORMAT_RGB565) pixel_format_ = PixelFormat::RGB565;
        else if (fmt == RETRO_PIXEL_FORMAT_0RGB1555) pixel_format_ = PixelFormat::XRGB1555;
        else return false;
        return true;
    }
    case Cmd(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY):
        *static_cast<const char**>(data) = system_dir_.c_str();
        return true;
    case Cmd(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY):
        *static_cast<const char**>(data) = save_dir_.c_str();
        return true;
    case Cmd(RETRO_ENVIRONMENT_GET_LOG_INTERFACE):
        static_cast<retro_log_callback*>(data)->log = &CoreLog;
        return true;
    case Cmd(RETRO_ENVIRONMENT_GET_LANGUAGE):
        *static_cast<unsigned*>(data) = RETRO_LANGUAGE_SPANISH;
        return true;
    case Cmd(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS):
        return true;
    case Cmd(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION):
        *static_cast<unsigned*>(data) = 2;
        return true;
    case Cmd(RETRO_ENVIRONMENT_SET_VARIABLES): {
        options_.clear();
        for (auto* v = static_cast<const retro_variable*>(data); v && v->key; ++v) {
            CoreOption o;
            o.key = v->key;
            std::string spec = v->value ? v->value : "";
            const size_t semi = spec.find(';');
            o.label = semi == std::string::npos ? o.key : spec.substr(0, semi);
            std::string vals = semi == std::string::npos ? spec : spec.substr(semi + 1);
            size_t start = vals.find_first_not_of(' ');
            vals = start == std::string::npos ? "" : vals.substr(start);
            size_t pos = 0;
            while (pos <= vals.size()) {
                size_t bar = vals.find('|', pos);
                std::string item = vals.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
                if (!item.empty()) o.values.push_back({item, item});
                if (bar == std::string::npos) break;
                pos = bar + 1;
            }
            o.def = o.values.empty() ? "" : o.values[0].value;
            o.current = o.def;
            options_.push_back(std::move(o));
        }
        return true;
    }
    case Cmd(RETRO_ENVIRONMENT_SET_CORE_OPTIONS):
        SetCoreOptionsV1(static_cast<const retro_core_option_definition*>(data));
        return true;
    case Cmd(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL): {
        const auto* intl = static_cast<const retro_core_options_intl*>(data);
        SetCoreOptionsV1(intl->us);
        return true;
    }
    case Cmd(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2):
        SetCoreOptionsV2(static_cast<const retro_core_options_v2*>(data), nullptr);
        return true;
    case Cmd(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL): {
        const auto* intl = static_cast<const retro_core_options_v2_intl*>(data);
        SetCoreOptionsV2(intl->us, intl->local);
        return true;
    }
    case Cmd(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY): {
        const auto* d = static_cast<const retro_core_option_display*>(data);
        if (!d || !d->key) return true;
        for (auto& o : options_)
            if (o.key == d->key) o.visible = d->visible;
        return true;
    }
    case Cmd(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK):
        return true;
    case Cmd(RETRO_ENVIRONMENT_GET_VARIABLE): {
        auto* var = static_cast<retro_variable*>(data);
        if (!var || !var->key) return false;
        for (const auto& o : options_) {
            if (o.key == var->key) {
                var->value = o.current.c_str();
                return true;
            }
        }
        var->value = nullptr;
        return false;
    }
    case Cmd(RETRO_ENVIRONMENT_SET_VARIABLE): {
        const auto* var = static_cast<const retro_variable*>(data);
        if (!var) return true; // pregunta si se admite
        if (var->key && var->value) SetOption(var->key, var->value);
        return true;
    }
    case Cmd(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE):
        *static_cast<bool*>(data) = options_updated_;
        options_updated_ = false;
        return true;
    case Cmd(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME):
        support_no_game_ = *static_cast<const bool*>(data);
        return true;
    case Cmd(RETRO_ENVIRONMENT_SET_GEOMETRY): {
        const auto* g = static_cast<const retro_game_geometry*>(data);
        av_info_.geometry.base_width = g->base_width;
        av_info_.geometry.base_height = g->base_height;
        av_info_.geometry.aspect_ratio = g->aspect_ratio;
        geometry_changed_ = true;
        return true;
    }
    case Cmd(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO):
        av_info_ = *static_cast<const retro_system_av_info*>(data);
        geometry_changed_ = true;
        return true;
    case Cmd(RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION):
        *static_cast<unsigned*>(data) = 1;
        return true;
    case Cmd(RETRO_ENVIRONMENT_SET_MESSAGE): {
        const auto* m = static_cast<const retro_message*>(data);
        if (m && m->msg && on_message) on_message(m->msg, static_cast<int>(m->frames * 1000 / 60));
        return true;
    }
    case Cmd(RETRO_ENVIRONMENT_SET_MESSAGE_EXT): {
        const auto* m = static_cast<const retro_message_ext*>(data);
        if (m && m->msg && on_message && m->target != RETRO_MESSAGE_TARGET_LOG)
            on_message(m->msg, static_cast<int>(m->duration));
        if (m && m->msg) Log::Info("[núcleo] %s", m->msg);
        return true;
    }
    case Cmd(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE): {
        int flags = 0;
        if (video_enabled) flags |= 1;
        if (audio_enabled) flags |= 2;
        *static_cast<int*>(data) = flags;
        return true;
    }
    case Cmd(RETRO_ENVIRONMENT_GET_FASTFORWARDING):
        *static_cast<bool*>(data) = fast_forwarding;
        return true;
    case Cmd(RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE):
        *static_cast<float*>(data) = target_refresh;
        return true;
    case Cmd(RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS):
        *static_cast<unsigned*>(data) = 4;
        return true;
    case Cmd(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL):
    case Cmd(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS):
    case Cmd(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO):
    case Cmd(RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO):
    case Cmd(RETRO_ENVIRONMENT_SET_MEMORY_MAPS):
    case Cmd(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS):
    case Cmd(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS):
    case Cmd(RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE):
    case Cmd(RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY):
        return true;
    default:
        return false;
    }
}

} // namespace Satronho
