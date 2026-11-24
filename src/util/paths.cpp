// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/paths.h"

#include <SDL3/SDL.h>

#include "sys/android.h"

namespace Satronho::Paths {
namespace {
fs::path g_base, g_assets, g_cores, g_user, g_public;
bool g_portable = false;

fs::path Sub(const char* name) {
    fs::path p = g_user / name;
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

fs::path PublicSub(const char* name) {
    fs::path p = g_public / name;
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

namespace {
// Relativa a la carpeta del programa si están en la misma unidad; si no, vacía.
fs::path RelativeToBase(const fs::path& p) {
    if (!p.is_absolute() || p.root_name() != g_base.root_name()) return {};
    const fs::path rel = p.lexically_normal().lexically_relative(g_base.lexically_normal());
    return rel.empty() ? fs::path{} : rel;
}

std::string Slashes(std::string s) {
    for (auto& c : s)
        if (c == '\\') c = '/';
    return s;
}
} // namespace

std::string StoreFolder(const fs::path& folder) {
    if (g_portable) {
        const fs::path rel = RelativeToBase(folder);
        if (!rel.empty()) return Slashes(ToUtf8(rel));
    }
    return ToUtf8(folder);
}

fs::path ResolveFolder(const std::string& stored) {
    const fs::path p = FromUtf8(stored);
    if (p.is_relative()) return (g_base / p).lexically_normal();
    return p;
}

std::string StableId(const fs::path& file) {
    if (g_portable) {
        const fs::path rel = RelativeToBase(file);
        if (!rel.empty()) return Slashes(ToUtf8(rel));
    }
    return ToUtf8(file);
}

namespace {
// Lo que trae Satronho para los núcleos (base de datos de Nestopia, letras de la PSP…) se copia a
// la carpeta del sistema. Lo que ya está no se toca (la persona puede haber puesto lo suyo).
void CopySystemAssets(const fs::path& assets_system, const fs::path& system) {
    std::error_code ec;
    if (!fs::exists(assets_system, ec)) return;
    for (auto it = fs::recursive_directory_iterator(assets_system, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const fs::path dst = system / fs::relative(it->path(), assets_system, ec);
        if (fs::exists(dst, ec)) continue;
        fs::create_directories(dst.parent_path(), ec);
        fs::copy_file(it->path(), dst, ec);
    }
}
} // namespace

void Init() {
#ifdef __ANDROID__
    // Android: los assets se copian a files/assets (SatronhoActivity), los núcleos están donde
    // Android instala las librerías de la app y los datos en la carpeta de la app en la memoria.
    {
        const char* internal = SDL_GetAndroidInternalStoragePath();
        const char* external = SDL_GetAndroidExternalStoragePath();
        g_base = FromUtf8(internal ? internal : ".");
        g_assets = g_base / "assets";
        g_cores = FromUtf8(Android::CoresDir());
        g_user = FromUtf8(external ? external : (internal ? internal : "."));
        std::error_code ec;
        fs::create_directories(g_user, ec);
        CopySystemAssets(g_assets / "system", System());
        return;
    }
#endif
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

    // La base de datos de Nestopia (cartuchos raros) y las letras de la PSP (PPSSPP).
    CopySystemAssets(g_assets / "system", System());
}

const fs::path& Base() { return g_base; }
const fs::path& Assets() { return g_assets; }
const fs::path& Cores() { return g_cores; }
const fs::path& User() { return g_user; }
fs::path Saves() { return g_public.empty() ? Sub("partidas") : PublicSub("Partidas"); }
fs::path States() { return g_public.empty() ? Sub("estados") : PublicSub("Estados"); }
fs::path PrivateSaves() { return g_user / "partidas"; }
fs::path PrivateStates() { return g_user / "estados"; }
void SetPublicRoot(const fs::path& root) { g_public = root; }
const fs::path& PublicRoot() { return g_public; }
fs::path System() { return Sub("sistema"); }
fs::path Screenshots() { return Sub("capturas"); }
fs::path ConfigFile() { return g_user / "satronho.ini"; }
fs::path LibraryFile() { return g_user / "biblioteca.ini"; }
fs::path LogFile() { return g_user / "satronho.log"; }
bool Portable() { return g_portable; }

} // namespace Satronho::Paths
