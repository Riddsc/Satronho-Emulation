// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/paths.h"

#include <SDL3/SDL.h>

namespace Satronho::Paths {
namespace {
fs::path g_base, g_assets, g_cores, g_user;
bool g_portable = false;

fs::path Sub(const char* name) {
    fs::path p = g_user / name;
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}
} // namespace

fs::path FromUtf8(const std::string& s) {
#if defined(__cpp_char8_t)
    return fs::path(std::u8string(s.begin(), s.end()));
#else
    return fs::u8path(s);
#endif
}

std::string ToUtf8(const fs::path& p) {
#if defined(__cpp_char8_t)
    auto u = p.u8string();
    return std::string(u.begin(), u.end());
#else
    return p.u8string();
#endif
}

std::string SafeName(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (c < 32 || c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' ||
            c == '|' || c == '?' || c == '*')
            out += '_';
        else
            out += static_cast<char>(c);
    }
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
    return out.empty() ? "juego" : out;
}

void Init() {
    const char* base = SDL_GetBasePath();
    g_base = base ? FromUtf8(base) : fs::current_path();
    g_assets = g_base / "assets";
    g_cores = g_base / "cores";

    std::error_code ec;
    g_portable = fs::exists(g_base / "portable.txt", ec);
    if (g_portable) {
        g_user = g_base / "user";
    } else {
        char* pref = SDL_GetPrefPath("Satronho", "Satronho Emulation");
        g_user = pref ? FromUtf8(pref) : (g_base / "user");
        SDL_free(pref);
    }
    fs::create_directories(g_user, ec);

    // La base de datos de Nestopia mejora la detección de cartuchos raros.
    const fs::path db_src = g_assets / "system" / "NstDatabase.xml";
    const fs::path db_dst = System() / "NstDatabase.xml";
    if (fs::exists(db_src, ec) && !fs::exists(db_dst, ec))
        fs::copy_file(db_src, db_dst, ec);
}

const fs::path& Base() { return g_base; }
const fs::path& Assets() { return g_assets; }
const fs::path& Cores() { return g_cores; }
const fs::path& User() { return g_user; }
fs::path Saves() { return Sub("partidas"); }
fs::path States() { return Sub("estados"); }
fs::path System() { return Sub("sistema"); }
fs::path Screenshots() { return Sub("capturas"); }
fs::path ConfigFile() { return g_user / "satronho.ini"; }
fs::path LibraryFile() { return g_user / "biblioteca.ini"; }
fs::path LogFile() { return g_user / "satronho.log"; }
bool Portable() { return g_portable; }

} // namespace Satronho::Paths
