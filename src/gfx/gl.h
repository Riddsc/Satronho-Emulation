// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Funciones de OpenGL que usa Satronho (se cargan al crear el contexto con SDL_GL_GetProcAddress;
// no hace falta enlazar con opengl32 ni con libGL).
#include <cstddef>
#include <cstdint>

#include <SDL3/SDL_opengl.h>

#ifndef APIENTRY
#define APIENTRY
#endif

namespace Satronho::Gl {

using Sync = struct __GLsync*;

#define SATRONHO_GL_FUNCS(X)                                                                                        \
    X(const GLubyte*, GetString, GLenum)                                                                            \
    X(void, GetIntegerv, GLenum, GLint*)                                                                            \
    X(GLenum, GetError, void)                                                                                       \
    X(void, Viewport, GLint, GLint, GLsizei, GLsizei)                                                               \
    X(void, ClearColor, GLfloat, GLfloat, GLfloat, GLfloat)                                                         \
    X(void, Clear, GLbitfield)                                                                                      \
    X(void, Enable, GLenum)                                                                                         \
    X(void, Disable, GLenum)                                                                                        \
    X(void, BlendFunc, GLenum, GLenum)                                                                              \
    X(void, BlendFuncSeparate, GLenum, GLenum, GLenum, GLenum)                                                      \
    X(void, BlendEquation, GLenum)                                                                                  \
    X(void, Scissor, GLint, GLint, GLsizei, GLsizei)                                                                \
    X(void, ColorMask, GLboolean, GLboolean, GLboolean, GLboolean)                                                  \
    X(void, GenTextures, GLsizei, GLuint*)                                                                          \
    X(void, DeleteTextures, GLsizei, const GLuint*)                                                                 \
    X(void, BindTexture, GLenum, GLuint)                                                                            \
    X(void, TexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)                 \
    X(void, TexSubImage2D, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)              \
    X(void, TexParameteri, GLenum, GLenum, GLint)                                                                   \
    X(void, PixelStorei, GLenum, GLint)                                                                             \
    X(void, ActiveTexture, GLenum)                                                                                  \
    X(void, GenFramebuffers, GLsizei, GLuint*)                                                                      \
    X(void, DeleteFramebuffers, GLsizei, const GLuint*)                                                             \
    X(void, BindFramebuffer, GLenum, GLuint)                                                                        \
    X(void, FramebufferTexture2D, GLenum, GLenum, GLenum, GLuint, GLint)                                            \
    X(void, GenRenderbuffers, GLsizei, GLuint*)                                                                     \
    X(void, DeleteRenderbuffers, GLsizei, const GLuint*)                                                            \
    X(void, BindRenderbuffer, GLenum, GLuint)                                                                       \
    X(void, RenderbufferStorage, GLenum, GLenum, GLsizei, GLsizei)                                                  \
    X(void, FramebufferRenderbuffer, GLenum, GLenum, GLenum, GLuint)                                                \
    X(GLenum, CheckFramebufferStatus, GLenum)                                                                       \
    X(GLuint, CreateShader, GLenum)                                                                                 \
    X(void, ShaderSource, GLuint, GLsizei, const GLchar* const*, const GLint*)                                      \
    X(void, CompileShader, GLuint)                                                                                  \
    X(void, GetShaderiv, GLuint, GLenum, GLint*)                                                                    \
    X(void, GetShaderInfoLog, GLuint, GLsizei, GLsizei*, GLchar*)                                                   \
    X(GLuint, CreateProgram, void)                                                                                  \
    X(void, AttachShader, GLuint, GLuint)                                                                           \
    X(void, LinkProgram, GLuint)                                                                                    \
    X(void, GetProgramiv, GLuint, GLenum, GLint*)                                                                   \
    X(void, GetProgramInfoLog, GLuint, GLsizei, GLsizei*, GLchar*)                                                  \
    X(void, DeleteShader, GLuint)                                                                                   \
    X(void, DeleteProgram, GLuint)                                                                                  \
    X(void, UseProgram, GLuint)                                                                                     \
    X(GLint, GetUniformLocation, GLuint, const GLchar*)                                                             \
    X(void, Uniform1i, GLint, GLint)                                                                                \
    X(void, Uniform1f, GLint, GLfloat)                                                                              \
    X(void, Uniform2f, GLint, GLfloat, GLfloat)                                                                     \
    X(void, Uniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat)                                                   \
    X(void, GenVertexArrays, GLsizei, GLuint*)                                                                      \
    X(void, DeleteVertexArrays, GLsizei, const GLuint*)                                                             \
    X(void, BindVertexArray, GLuint)                                                                                \
    X(void, DrawArrays, GLenum, GLint, GLsizei)                                                                     \
    X(void, ReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)                                      \
    X(void, Flush, void)                                                                                            \
    X(void, Finish, void)                                                                                           \
    X(Sync, FenceSync, GLenum, GLbitfield)                                                                          \
    X(void, WaitSync, Sync, GLbitfield, GLuint64)                                                                   \
    X(void, DeleteSync, Sync)

#define SATRONHO_GL_DECL(ret, name, ...) using name##Fn = ret(APIENTRY*)(__VA_ARGS__); extern name##Fn name;
SATRONHO_GL_FUNCS(SATRONHO_GL_DECL)
#undef SATRONHO_GL_DECL

// Carga las funciones del contexto actual. false si falta alguna.
bool Load();
// Compila un programa de sombreado (vértices + píxeles). 0 si falla (se anota en el registro).
GLuint Program(const char* vs, const char* fs, const char* name);

} // namespace Satronho::Gl
