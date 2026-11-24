// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gfx/video_gl.h"

#include <algorithm>
#include <cmath>

#include "gfx/hw_render.h"
#include "util/log.h"

namespace Satronho {

namespace {
const char* kVs = R"(#version 330 core
uniform vec4 u_rect;
uniform vec4 u_uv;
out vec2 v_uv;
out vec2 v_pos;
void main() {
    vec2 c = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));
    v_pos = c;
    v_uv = mix(u_uv.xy, u_uv.zw, c);
    gl_Position = vec4(mix(u_rect.xy, u_rect.zw, c), 0.0, 1.0);
}
)";

// Imagen del juego: filtro (píxel / nítido suave / suave) y efectos de pantalla.
const char* kFs = R"(#version 330 core
in vec2 v_uv;
in vec2 v_pos;
out vec4 o_color;
uniform sampler2D u_tex;
uniform vec2 u_tex_size;   // tamaño de la textura completa (px)
uniform vec2 u_src_size;   // tamaño de la imagen del juego (px)
uniform vec2 u_out_size;   // tamaño en pantalla (px)
uniform int u_mode;        // 0 píxel, 1 nítido suave, 2 suave
uniform vec4 u_fx;         // x: líneas (oscuridad), y: máscara (k), z: viñeta, w: rejilla LCD
uniform float u_lines;     // líneas del televisor que tiene la imagen
void main() {
    vec2 uv = v_uv;
    if (u_mode == 1) {
        // Nítido suave: agranda sin suavizar al múltiplo entero y suaviza solo el borde de cada píxel.
        vec2 prescale = max(floor(u_out_size / u_src_size), vec2(1.0));
        vec2 texel = uv * u_tex_size;
        vec2 base = floor(texel);
        vec2 center = fract(texel) - 0.5;
        vec2 range = 0.5 - 0.5 / prescale;
        vec2 f = (center - clamp(center, -range, range)) * prescale + 0.5;
        uv = (base + f) / u_tex_size;
    }
    vec3 c = texture(u_tex, uv).rgb;
    if (u_fx.x > 0.0) {
        float l = fract(v_pos.y * u_lines);
        c *= 1.0 - u_fx.x * (0.5 + 0.5 * cos(6.2831853 * l));
    }
    if (u_fx.y < 1.0) {
        int m = int(mod(gl_FragCoord.x, 3.0));
        vec3 mask = vec3(u_fx.y);
        mask[m] = 1.0;
        c *= mask;
    }
    if (u_fx.w > 0.0) {
        vec2 cell = fract(v_pos * u_src_size);
        if (cell.x > 0.75 || cell.y > 0.75) c *= 1.0 - u_fx.w;
    }
    if (u_fx.z > 0.0) {
        vec2 d = v_pos * 2.0 - 1.0;
        float dist = sqrt(dot(d, d) * 0.7);
        float a = clamp((dist - 0.55) / 0.6, 0.0, 1.0);
        c *= 1.0 - a * a * 0.78 * u_fx.z;
    }
    o_color = vec4(c, 1.0);
}
)";

// Fondo ambiente: los colores del juego muy borrosos, oscurecidos y con viñeta.
const char* kAmbFs = R"(#version 330 core
in vec2 v_uv;
in vec2 v_pos;
out vec4 o_color;
uniform sampler2D u_tex;
uniform vec4 u_tint;
uniform float u_vignette;
void main() {
    vec3 c = vec3(0.0);
    float wsum = 0.0;
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x) {
            float w = 1.0 / (1.0 + float(x * x + y * y));
            c += texture(u_tex, v_uv + vec2(float(x), float(y)) / vec2(32.0, 30.0)).rgb * w;
            wsum += w;
        }
    c = c / wsum * u_tint.rgb;
    c = mix(c, vec3(10.0, 8.0, 18.0) / 255.0, 150.0 / 255.0);
    vec2 d = v_pos * 2.0 - 1.0;
    float dist = sqrt(dot(d, d) * 0.7);
    float a = clamp((dist - 0.55) / 0.6, 0.0, 1.0);
    c *= 1.0 - a * a * (200.0 / 255.0) * u_vignette;
    o_color = vec4(c, 1.0);
}
)";

void SetRect(GLint loc, float x0, float y0, float x1, float y1, float W, float H) {
    // Píxeles de la ventana (arriba-izquierda) a coordenadas de OpenGL.
    Gl::Uniform4f(loc, x0 / W * 2 - 1, 1 - y0 / H * 2, x1 / W * 2 - 1, 1 - y1 / H * 2);
}
} // namespace

bool VideoOutGl::Init() {
    prog_ = Gl::Program(kVs, kFs, "juego");
    amb_prog_ = Gl::Program(kVs, kAmbFs, "fondo ambiente");
    if (!prog_ || !amb_prog_) return false;
    Gl::GenVertexArrays(1, &vao_);
    u_.rect = Gl::GetUniformLocation(prog_, "u_rect");
    u_.uv = Gl::GetUniformLocation(prog_, "u_uv");
    u_.tex = Gl::GetUniformLocation(prog_, "u_tex");
    u_.tex_size = Gl::GetUniformLocation(prog_, "u_tex_size");
    u_.src_size = Gl::GetUniformLocation(prog_, "u_src_size");
    u_.out_size = Gl::GetUniformLocation(prog_, "u_out_size");
    u_.mode = Gl::GetUniformLocation(prog_, "u_mode");
    u_.effect = Gl::GetUniformLocation(prog_, "u_fx");
    u_.lines = Gl::GetUniformLocation(prog_, "u_lines");
    a_.rect = Gl::GetUniformLocation(amb_prog_, "u_rect");
    a_.uv = Gl::GetUniformLocation(amb_prog_, "u_uv");
    a_.tex = Gl::GetUniformLocation(amb_prog_, "u_tex");
    a_.tint = Gl::GetUniformLocation(amb_prog_, "u_tint");
    a_.vignette = Gl::GetUniformLocation(amb_prog_, "u_vignette");
    return true;
}

void VideoOutGl::Shutdown() {
    if (tex_) Gl::DeleteTextures(1, &tex_);
    if (small_tex_) Gl::DeleteTextures(1, &small_tex_);
    if (small_fbo_) Gl::DeleteFramebuffers(1, &small_fbo_);
    if (vao_) Gl::DeleteVertexArrays(1, &vao_);
    if (prog_) Gl::DeleteProgram(prog_);
    if (amb_prog_) Gl::DeleteProgram(amb_prog_);
    tex_ = small_tex_ = small_fbo_ = vao_ = prog_ = amb_prog_ = 0;
}

void VideoOutGl::Upload(const CoreHost& core) {
    if (core.Frame().empty() || core.FrameSerial() == uploaded_serial_) return;
    const int w = static_cast<int>(core.FrameWidth()), h = static_cast<int>(core.FrameHeight());
    GLenum internal = GL_RGBA8, format = GL_BGRA, type = GL_UNSIGNED_INT_8_8_8_8_REV;
    if (core.Format() == PixelFormat::RGB565) {
        internal = GL_RGB;
        format = GL_RGB;
        type = GL_UNSIGNED_SHORT_5_6_5;
    } else if (core.Format() == PixelFormat::XRGB1555) {
        internal = GL_RGB5_A1;
        format = GL_BGRA;
        type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
    }
    if (!tex_) Gl::GenTextures(1, &tex_);
    Gl::BindTexture(GL_TEXTURE_2D, tex_);
    Gl::PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    Gl::PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    if (w != tex_w_ || h != tex_h_ || core.Format() != tex_fmt_) {
        Gl::TexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, type, core.Frame().data());
        Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        tex_w_ = w;
        tex_h_ = h;
        tex_fmt_ = core.Format();
    } else {
        Gl::TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, format, type, core.Frame().data());
    }
    Gl::BindTexture(GL_TEXTURE_2D, 0);
    uploaded_serial_ = core.FrameSerial();
}

void VideoOutGl::EnsureSmall() {
    if (small_fbo_) return;
    Gl::GenTextures(1, &small_tex_);
    Gl::BindTexture(GL_TEXTURE_2D, small_tex_);
    Gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 30, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    Gl::GenFramebuffers(1, &small_fbo_);
    Gl::BindFramebuffer(GL_FRAMEBUFFER, small_fbo_);
    Gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, small_tex_, 0);
    Gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
    Gl::BindTexture(GL_TEXTURE_2D, 0);
}

void VideoOutGl::Draw(const CoreHost& core, const HwRender* hw, const Settings& s, int win_w, int win_h, bool handheld) {
    // Qué textura se muestra: la del núcleo (tarjeta gráfica) o la subida por software.
    GLuint tex = tex_;
    float tw = static_cast<float>(tex_w_), th = static_cast<float>(tex_h_);
    float iw = tw, ih = th;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    if (hw && hw->Active() && core.Frame().empty()) {
        if (!hw->Texture() || !hw->FrameWidth()) return;
        tex = hw->Texture();
        tw = static_cast<float>(hw->TexWidth());
        th = static_cast<float>(hw->TexHeight());
        iw = static_cast<float>(hw->FrameWidth());
        ih = static_cast<float>(hw->FrameHeight());
        u1 = iw / tw;
        if (hw->BottomLeft()) {
            v0 = ih / th; // arriba de la imagen = fila más alta de la textura
            v1 = 0;
        } else {
            v1 = ih / th;
        }
    }
    if (!tex || iw < 1 || ih < 1) return;

    const float W = static_cast<float>(win_w), H = static_cast<float>(win_h);
    const float aspect = core.AspectRatio();
    float dw, dh;
    switch (static_cast<ScaleMode>(s.scale_mode)) {
    case ScaleMode::Stretch:
        dw = W;
        dh = H;
        break;
    case ScaleMode::Integer: {
        // Con resolución interna alta (PS1 al 4×) el múltiplo se cuenta sobre la imagen original.
        const float base_h = ih > 300 ? ih / std::max(1.0f, std::round(ih / 240.0f)) : ih;
        int k = static_cast<int>(std::floor(H / base_h));
        while (k > 1 && base_h * k * aspect > W) --k;
        k = std::max(1, k);
        dh = base_h * k;
        dw = std::round(dh * aspect);
        break;
    }
    default:
        if (W / H > aspect) {
            dh = H;
            dw = std::round(H * aspect);
        } else {
            dw = W;
            dh = std::round(W / aspect);
        }
    }
    dest_ = {std::floor((W - dw) * 0.5f), std::floor((H - dh) * 0.5f), dw, dh};

    Gl::Disable(GL_BLEND);
    Gl::Disable(GL_SCISSOR_TEST);
    Gl::Disable(GL_DEPTH_TEST);
    Gl::Disable(GL_CULL_FACE);
    Gl::BindVertexArray(vao_);
    Gl::ActiveTexture(GL_TEXTURE0);

    // Fondo ambiente.
    if (s.ambient && s.scale_mode != static_cast<int>(ScaleMode::Stretch)) {
        EnsureSmall();
        if (core.FrameSerial() - ambient_serial_ >= 4 || !ambient_serial_) {
            ambient_serial_ = core.FrameSerial();
            Gl::BindFramebuffer(GL_FRAMEBUFFER, small_fbo_);
            Gl::Viewport(0, 0, 32, 30);
            Gl::UseProgram(prog_);
            Gl::BindTexture(GL_TEXTURE_2D, tex);
            Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            Gl::Uniform1i(u_.tex, 0);
            Gl::Uniform4f(u_.rect, -1, 1, 1, -1);
            Gl::Uniform4f(u_.uv, u0, v0, u1, v1);
            Gl::Uniform2f(u_.tex_size, tw, th);
            Gl::Uniform2f(u_.src_size, iw, ih);
            Gl::Uniform2f(u_.out_size, 32, 30);
            Gl::Uniform1i(u_.mode, 2);
            Gl::Uniform4f(u_.effect, 0, 1, 0, 0);
            Gl::Uniform1f(u_.lines, 1);
            Gl::DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            Gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        Gl::Viewport(0, 0, win_w, win_h);
        const float cover = std::max(W / dw, H / dh) * 1.15f;
        Gl::UseProgram(amb_prog_);
        Gl::BindTexture(GL_TEXTURE_2D, small_tex_);
        Gl::Uniform1i(a_.tex, 0);
        SetRect(a_.rect, (W - dw * cover) * 0.5f, (H - dh * cover) * 0.5f, (W + dw * cover) * 0.5f, (H + dh * cover) * 0.5f, W, H);
        Gl::Uniform4f(a_.uv, 0, 0, 1, 1);
        Gl::Uniform4f(a_.tint, 120 / 255.0f, 120 / 255.0f, 135 / 255.0f, 1);
        Gl::Uniform1f(a_.vignette, 1.0f);
        Gl::DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    Gl::Viewport(0, 0, win_w, win_h);

    // Imagen del juego.
    const auto filter = static_cast<Filter>(s.filter);
    Gl::UseProgram(prog_);
    Gl::BindTexture(GL_TEXTURE_2D, tex);
    const GLint f = filter == Filter::Pixel ? GL_NEAREST : GL_LINEAR;
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    Gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
    Gl::Uniform1i(u_.tex, 0);
    SetRect(u_.rect, dest_.x, dest_.y, dest_.x + dw, dest_.y + dh, W, H);
    Gl::Uniform4f(u_.uv, u0, v0, u1, v1);
    Gl::Uniform2f(u_.tex_size, tw, th);
    Gl::Uniform2f(u_.src_size, iw, ih);
    Gl::Uniform2f(u_.out_size, dw, dh);
    Gl::Uniform1i(u_.mode, filter == Filter::SharpSmooth ? 1 : filter == Filter::Smooth ? 2 : 0);

    // Efectos de pantalla (los mismos límites que con el dibujador de SDL).
    float scan = 0, mask = 1, vig = 0, lcd = 0, lines = 1;
    int effect = s.effect;
    if (effect == static_cast<int>(Effect::Auto)) effect = static_cast<int>(handheld ? Effect::Lcd : Effect::Crt);
    if (effect == static_cast<int>(Effect::Lcd) && s.effect_strength > 0.01f) {
        if (dh / ih >= 3.0f) lcd = std::clamp(s.effect_strength, 0.0f, 1.0f) * 150.0f / 255.0f;
    } else if (effect != static_cast<int>(Effect::None) && s.effect_strength > 0.01f) {
        const int div = ih > 300 ? std::max(1, static_cast<int>(std::lround(ih / 240.0))) : 1;
        lines = std::max(1.0f, ih / div);
        if (dh / lines >= 2.0f) scan = (effect == static_cast<int>(Effect::Crt) ? 0.55f : 0.65f) * s.effect_strength * 0.5f;
        if (effect == static_cast<int>(Effect::Crt)) {
            if (dw >= (iw / div) * 3) mask = 1.0f - 0.32f * s.effect_strength;
            vig = 0.55f * s.effect_strength + 0.2f;
        }
    }
    Gl::Uniform4f(u_.effect, scan, mask, vig, lcd);
    Gl::Uniform1f(u_.lines, lines);
    Gl::DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    Gl::BindTexture(GL_TEXTURE_2D, 0);
    Gl::UseProgram(0);
    Gl::BindVertexArray(0);
}

} // namespace Satronho
