// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gfx/gl.h"

#include <SDL3/SDL.h>

#include "util/log.h"

namespace Satronho::Gl {

#define SATRONHO_GL_DEF(ret, name, ...) name##Fn name = nullptr;
SATRONHO_GL_FUNCS(SATRONHO_GL_DEF)
#undef SATRONHO_GL_DEF

bool Load() {
    bool ok = true;
#define SATRONHO_GL_LOAD(ret, name, ...)                                                    \
    name = reinterpret_cast<name##Fn>(SDL_GL_GetProcAddress("gl" #name));                 \
    if (!name) {                                                                          \
        Log::Warn("OpenGL: falta gl%s", #name);                                            \
        ok = false;                                                                       \
    }
    SATRONHO_GL_FUNCS(SATRONHO_GL_LOAD)
#undef SATRONHO_GL_LOAD
    return ok;
}

namespace {
GLuint Compile(GLenum type, const char* src, const char* name) {
    const GLuint s = CreateShader(type);
    ShaderSource(s, 1, &src, nullptr);
    CompileShader(s);
    GLint ok = 0;
    GetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        GetShaderInfoLog(s, sizeof log, nullptr, log);
        Log::Error("OpenGL: no compiló «%s» (%s): %s", name, type == GL_VERTEX_SHADER ? "vértices" : "píxeles", log);
        DeleteShader(s);
        return 0;
    }
    return s;
}
} // namespace

GLuint Program(const char* vs, const char* fs, const char* name) {
    const GLuint v = Compile(GL_VERTEX_SHADER, vs, name);
    const GLuint f = Compile(GL_FRAGMENT_SHADER, fs, name);
    if (!v || !f) {
        if (v) DeleteShader(v);
        if (f) DeleteShader(f);
        return 0;
    }
    const GLuint p = CreateProgram();
    AttachShader(p, v);
    AttachShader(p, f);
    LinkProgram(p);
    DeleteShader(v);
    DeleteShader(f);
    GLint ok = 0;
    GetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        GetProgramInfoLog(p, sizeof log, nullptr, log);
        Log::Error("OpenGL: no enlazó «%s»: %s", name, log);
        DeleteProgram(p);
        return 0;
    }
    return p;
}

} // namespace Satronho::Gl
