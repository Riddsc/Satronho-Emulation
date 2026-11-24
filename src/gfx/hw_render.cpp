// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gfx/hw_render.h"

#include <algorithm>
#include <cstring>

#include "gfx/vulkan_bridge.h"
#include "util/log.h"

namespace Satronho {

HwRender::HwRender() : vk_(new VulkanBridge(this)) {}

HwRender::~HwRender() {
    Shutdown();
    delete vk_;
}

void HwRender::Init(SDL_Window* window, SDL_GLContext main_ctx) {
    window_ = window;
    main_ = main_ctx;
}

void HwRender::Shutdown() {
    if (probe_.joinable()) probe_.join();
    if (active_) Stop();
    if (main_ && read_fbo_) {
        Gl::DeleteFramebuffers(1, &read_fbo_);
        read_fbo_ = 0;
    }
}

void HwRender::ProbeVulkan() {
    if (probe_.joinable()) return;
    probe_ = std::thread([this] {
        std::string name;
        const bool ok = VulkanBridge::Probe(&name);
        {
            std::lock_guard lock(probe_mutex_);
            vk_gpu_ = name;
        }
        vk_ok_ = ok;
        vk_probed_ = true;
        Log::Info("Vulkan: %s%s", ok ? "disponible" : "no disponible", ok ? (" (" + name + ")").c_str() : "");
    });
}

std::string HwRender::VulkanGpu() const {
    std::lock_guard lock(probe_mutex_);
    return vk_gpu_;
}

std::string HwRender::ApiName() const {
    if (api_ == RETRO_HW_CONTEXT_VULKAN) return "Vulkan";
    if (api_ == RETRO_HW_CONTEXT_OPENGLES2 || api_ == RETRO_HW_CONTEXT_OPENGLES3 || api_ == RETRO_HW_CONTEXT_OPENGLES_VERSION) return "OpenGL ES";
    if (api_ == RETRO_HW_CONTEXT_OPENGL || api_ == RETRO_HW_CONTEXT_OPENGL_CORE) {
        if (api_ == RETRO_HW_CONTEXT_OPENGL_CORE && cb_.version_major) return "OpenGL " + std::to_string(cb_.version_major) + "." + std::to_string(cb_.version_minor);
        return "OpenGL";
    }
    return "Procesador";
}

// ---------------------------------------------------------------- qué se le ofrece al núcleo

bool HwRender::Accept(retro_hw_render_callback* cb) {
    if (!main_ || wanted_ == RETRO_HW_CONTEXT_NONE) return false;
    switch (cb->context_type) {
#ifdef __ANDROID__
    case RETRO_HW_CONTEXT_OPENGLES2:
    case RETRO_HW_CONTEXT_OPENGLES3:
    case RETRO_HW_CONTEXT_OPENGLES_VERSION:
        break;
#else
    case RETRO_HW_CONTEXT_OPENGL:
    case RETRO_HW_CONTEXT_OPENGL_CORE:
        break;
#endif
    case RETRO_HW_CONTEXT_VULKAN:
        if (probe_.joinable() && !vk_probed_) probe_.join();
        if (!vk_ok_) return false;
        break;
    default:
        return false;
    }
    api_ = cb->context_type;
    cb_ = *cb;
    bottom_left_ = api_ != RETRO_HW_CONTEXT_VULKAN ? cb->bottom_left_origin : false;
    Log::Info("El núcleo dibuja con la tarjeta gráfica: %s%s%s", ApiName().c_str(), cb->depth ? " · profundidad" : "", cb->stencil ? " · stencil" : "");
    return true;
}

unsigned HwRender::Preferred() const {
    if (wanted_ == RETRO_HW_CONTEXT_VULKAN && vk_ok_) return RETRO_HW_CONTEXT_VULKAN;
    if (wanted_ == RETRO_HW_CONTEXT_NONE) return RETRO_HW_CONTEXT_NONE;
    return RETRO_HW_CONTEXT_OPENGL_CORE;
}

bool HwRender::Interface(const retro_hw_render_interface** out) {
    if (api_ != RETRO_HW_CONTEXT_VULKAN || !active_) return false;
    *out = vk_->Interface();
    return *out != nullptr;
}

bool HwRender::Negotiation(const retro_hw_render_context_negotiation_interface* iface) {
    negotiation_ = iface;
    return true;
}

// ---------------------------------------------------------------- inicio y cierre

bool HwRender::Start(const retro_system_av_info& av) {
    if (active_) Stop();
    bool ok = false;
    if (api_ == RETRO_HW_CONTEXT_VULKAN) {
        ok = vk_->Start(negotiation_);
        if (ok) EnsureTexture(std::max(1u, av.geometry.max_width), std::max(1u, av.geometry.max_height), false);
    } else {
        ok = StartGl(av);
    }
    active_ = ok;
    frame_w_ = frame_h_ = 0;
    return ok;
}

void HwRender::Stop() {
    if (!active_) return;
    if (api_ == RETRO_HW_CONTEXT_VULKAN) {
        vk_->Stop(negotiation_);
        SDL_GL_MakeCurrent(window_, main_);
        if (tex_) Gl::DeleteTextures(1, &tex_);
        tex_ = 0;
    } else {
        StopGl();
    }
    if (read_fbo_) {
        Gl::DeleteFramebuffers(1, &read_fbo_);
        read_fbo_ = 0;
    }
    active_ = false;
    api_ = RETRO_HW_CONTEXT_NONE;
    negotiation_ = nullptr;
    tex_w_ = tex_h_ = 0;
}

bool HwRender::StartGl(const retro_system_av_info& av) {
    SDL_GL_MakeCurrent(window_, main_);
    // Contexto propio del núcleo, que comparte texturas con el de la ventana.
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    if (cb_.context_type == RETRO_HW_CONTEXT_OPENGLES2 || cb_.context_type == RETRO_HW_CONTEXT_OPENGLES3 ||
        cb_.context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION) {
        int major = 3, minor = 0;
        if (cb_.context_type == RETRO_HW_CONTEXT_OPENGLES2) major = 2;
        if (cb_.context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION) major = static_cast<int>(cb_.version_major), minor = static_cast<int>(cb_.version_minor);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
    } else if (cb_.context_type == RETRO_HW_CONTEXT_OPENGL_CORE) {
        unsigned major = cb_.version_major, minor = cb_.version_minor;
        if (major < 3 || (major == 3 && minor < 2)) major = 3, minor = 2;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, static_cast<int>(major));
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, static_cast<int>(minor));
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    } else {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    }
    if (cb_.debug_context) SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    core_ctx_ = SDL_GL_CreateContext(window_);
    SDL_GL_ResetAttributes();
    if (!core_ctx_) {
        Log::Error("OpenGL: no se pudo crear el contexto del núcleo (%u.%u): %s", cb_.version_major, cb_.version_minor, SDL_GetError());
        SDL_GL_MakeCurrent(window_, main_);
        return false;
    }
    // Ya está activo el contexto del núcleo: ahí se crean el framebuffer y la profundidad.
    in_core_ = true;
    EnsureTexture(std::max(1u, av.geometry.max_width), std::max(1u, av.geometry.max_height), true);
    in_core_ = false;
    SDL_GL_MakeCurrent(window_, main_);
    if (!fbo_) {
        StopGl();
        return false;
    }
    Log::Info("OpenGL del núcleo listo: %dx%d%s", tex_w_, tex_h_, bottom_left_ ? " (origen abajo)" : "");
    return true;
}

void HwRender::StopGl() {
    if (core_ctx_) {
        SDL_GL_MakeCurrent(window_, core_ctx_);
        if (fbo_) Gl::DeleteFramebuffers(1, &fbo_);
        if (depth_) Gl::DeleteRenderbuffers(1, &depth_);
        Gl::Finish();
        SDL_GL_MakeCurrent(window_, main_);
        SDL_GL_DestroyContext(core_ctx_);
        core_ctx_ = nullptr;
    }
    SDL_GL_MakeCurrent(window_, main_);
    if (tex_) Gl::DeleteTextures(1, &tex_);
    tex_ = fbo_ = depth_ = 0;
    in_core_ = false;
}

void HwRender::EnsureTexture(unsigned w, unsigned h, bool core_ctx_current) {
    if (tex_ && static_cast<int>(w) <= tex_w_ && static_cast<int>(h) <= tex_h_) return;
    const int nw = std::max<int>(w, tex_w_), nh = std::max<int>(h, tex_h_);
    // La textura es compartida: se puede crear en cualquiera de los dos contextos.
    if (tex_) Gl::DeleteTextures(1, &tex_);
    Gl::GenTextures(1, &tex_);
    Gl::BindTexture(GL_TEXTURE_2D, tex_);
    Gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, nw, nh, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    Gl::BindTexture(GL_TEXTURE_2D, 0);
    tex_w_ = nw;
    tex_h_ = nh;
    if (read_fbo_) {
        // El framebuffer de lectura es del contexto principal: se rehace la próxima vez.
        if (!core_ctx_current) Gl::DeleteFramebuffers(1, &read_fbo_);
        read_fbo_ = 0;
    }
    if (api_ == RETRO_HW_CONTEXT_VULKAN || !core_ctx_current) return;
    // Framebuffer del núcleo (vive en su contexto).
    if (!fbo_) Gl::GenFramebuffers(1, &fbo_);
    Gl::BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    Gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);
    if (cb_.depth) {
        if (!depth_) Gl::GenRenderbuffers(1, &depth_);
        Gl::BindRenderbuffer(GL_RENDERBUFFER, depth_);
        Gl::RenderbufferStorage(GL_RENDERBUFFER, cb_.stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24, nw, nh);
        Gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, cb_.stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
        Gl::BindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    const GLenum st = Gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        Log::Error("OpenGL: el framebuffer del núcleo no quedó completo (0x%x)", st);
        Gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
        Gl::DeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
        return;
    }
    Gl::Viewport(0, 0, nw, nh);
    Gl::ClearColor(0, 0, 0, 1);
    Gl::Clear(GL_COLOR_BUFFER_BIT | (cb_.depth ? GL_DEPTH_BUFFER_BIT : 0) | (cb_.stencil ? GL_STENCIL_BUFFER_BIT : 0));
    Gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ---------------------------------------------------------------- cuadro a cuadro

void HwRender::Enter() {
    if (!active_ || api_ == RETRO_HW_CONTEXT_VULKAN || !core_ctx_ || in_core_) return;
    SDL_GL_MakeCurrent(window_, core_ctx_);
    in_core_ = true;
}

void HwRender::Leave() {
    if (!in_core_) return;
    // Que lo que dibujó el núcleo quede listo antes de usar la textura en el otro contexto.
    Gl::Sync fence = Gl::FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    Gl::Flush();
    SDL_GL_MakeCurrent(window_, main_);
    in_core_ = false;
    if (fence) {
        Gl::WaitSync(fence, 0, GL_TIMEOUT_IGNORED);
        Gl::DeleteSync(fence);
    }
}

void HwRender::Frame(unsigned w, unsigned h) {
    frame_w_ = std::min<unsigned>(w, tex_w_);
    frame_h_ = std::min<unsigned>(h, tex_h_);
    if (api_ == RETRO_HW_CONTEXT_VULKAN) {
        if (w > static_cast<unsigned>(tex_w_) || h > static_cast<unsigned>(tex_h_)) EnsureTexture(w, h, false);
        frame_w_ = w;
        frame_h_ = h;
        vk_->CopyFrame(w, h);
    }
}

void HwRender::Geometry(const retro_system_av_info& av) {
    if (!active_) return;
    const unsigned w = std::max(1u, av.geometry.max_width), h = std::max(1u, av.geometry.max_height);
    if (static_cast<int>(w) <= tex_w_ && static_cast<int>(h) <= tex_h_) return;
    if (api_ == RETRO_HW_CONTEXT_VULKAN) {
        EnsureTexture(w, h, false);
        return;
    }
    // Más grande que antes (por ejemplo, el juego subió la resolución interna).
    const bool was_in = in_core_;
    if (!was_in) SDL_GL_MakeCurrent(window_, core_ctx_);
    in_core_ = true;
    EnsureTexture(w, h, true);
    in_core_ = was_in;
    if (!was_in) SDL_GL_MakeCurrent(window_, main_);
    Log::Info("OpenGL del núcleo: nuevo tamaño %dx%d", tex_w_, tex_h_);
}

retro_proc_address_t HwRender::ProcAddress(const char* sym) {
    auto p = reinterpret_cast<retro_proc_address_t>(SDL_GL_GetProcAddress(sym));
#ifdef __ANDROID__
    // En Android las funciones básicas de GLES no siempre salen por eglGetProcAddress.
    if (!p) {
        static void* gles = SDL_LoadObject("libGLESv3.so");
        if (gles) p = reinterpret_cast<retro_proc_address_t>(SDL_LoadFunction(static_cast<SDL_SharedObject*>(gles), sym));
    }
#endif
    return p;
}

bool HwRender::ReadRGBA(std::vector<uint8_t>& out, unsigned* w, unsigned* h) {
    *w = frame_w_;
    *h = frame_h_;
    if (!active_ || !tex_ || !frame_w_ || !frame_h_) return false;
    if (api_ == RETRO_HW_CONTEXT_VULKAN) return vk_->LastFrame(out);
    if (!read_fbo_) {
        Gl::GenFramebuffers(1, &read_fbo_);
        Gl::BindFramebuffer(GL_FRAMEBUFFER, read_fbo_);
        Gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);
    } else {
        Gl::BindFramebuffer(GL_FRAMEBUFFER, read_fbo_);
    }
    out.resize(static_cast<size_t>(frame_w_) * frame_h_ * 4);
    Gl::PixelStorei(GL_PACK_ALIGNMENT, 1);
    Gl::ReadPixels(0, 0, frame_w_, frame_h_, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    Gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
    if (bottom_left_) {
        const size_t row = static_cast<size_t>(frame_w_) * 4;
        std::vector<uint8_t> tmp(row);
        for (unsigned y = 0; y < frame_h_ / 2; ++y) {
            uint8_t* a = out.data() + y * row;
            uint8_t* b = out.data() + (frame_h_ - 1 - y) * row;
            std::memcpy(tmp.data(), a, row);
            std::memcpy(a, b, row);
            std::memcpy(b, tmp.data(), row);
        }
    }
    for (size_t i = 3; i < out.size(); i += 4) out[i] = 255;
    return true;
}

} // namespace Satronho
