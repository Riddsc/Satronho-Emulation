// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/library.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <set>

#include "sys/systems.h"
#include "util/log.h"

namespace Satronho {
namespace {

uint32_t Fnv(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

std::string Lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// "Super Juego (USA) (Rev 1) [!]" -> título "Super Juego", etiquetas "USA", "Rev 1".
void SplitTitle(const std::string& name, std::string& title, std::vector<std::string>& tags) {
    title.clear();
    tags.clear();
    std::string cur;
    int depth = 0;
    char open = 0;
    for (char c : name) {
        if (depth == 0 && (c == '(' || c == '[')) {
            depth = 1;
            open = c;
            cur.clear();
        } else if (depth == 1 && ((open == '(' && c == ')') || (open == '[' && c == ']'))) {
            depth = 0;
            if (open == '(' && !cur.empty()) tags.push_back(cur);
        } else if (depth) {
            cur += c;
        } else {
            title += c;
        }
    }
    for (auto& c : title)
        if (c == '_') c = ' ';
    while (!title.empty() && title.back() == ' ') title.pop_back();
    while (!title.empty() && title.front() == ' ') title.erase(title.begin());
    // "Legend of Zelda, The" -> "The Legend of Zelda"
    for (const char* art : {", The", ", the", ", La", ", El", ", Los", ", Las"}) {
        const std::string a = art;
        if (title.size() > a.size() && title.compare(title.size() - a.size(), a.size(), a) == 0) {
            title = a.substr(2) + " " + title.substr(0, title.size() - a.size());
            break;
        }
    }
    if (title.empty()) title = name;
}

} // namespace

Library::~Library() {
    {
        std::lock_guard lock(mutex_);
        queued_ = false;
    }
    cancel_ = true; // al cerrar no se espera a que termine una búsqueda larga
    if (worker_.joinable()) worker_.join();
}

void Library::Load() {
    std::lock_guard lock(mutex_);
    meta_.Load(Paths::LibraryFile());
}

void Library::Save() {
    std::lock_guard lock(mutex_);
    meta_.Save(Paths::LibraryFile());
}

namespace {

// Busca un texto dentro de los primeros KB de un archivo.
bool FileHas(const fs::path& file, const std::string& needle, size_t limit = 256 * 1024) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::string buf(limit, '\0');
    in.read(buf.data(), static_cast<std::streamsize>(limit));
    buf.resize(static_cast<size_t>(in.gcount()));
    return buf.find(needle) != std::string::npos;
}

// Primer archivo de datos que nombra un .cue / .m3u / .ccd.
fs::path FirstTrack(const fs::path& file, const std::string& ext) {
    if (ext == "ccd") {
        fs::path img = file;
        img.replace_extension(".img");
        return img;
    }
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (ext == "m3u") {
            if (line.empty() || line[0] == '#') continue;
            const fs::path disc = file.parent_path() / Paths::FromUtf8(line);
            std::string e = Lower(Paths::ToUtf8(disc.extension()));
            if (!e.empty() && e[0] == '.') e.erase(0, 1);
            return e == "cue" || e == "ccd" ? FirstTrack(disc, e) : disc;
        }
        const size_t a = line.find('"'), b = line.rfind('"');
        if (Lower(line).find("file") != std::string::npos && a != std::string::npos && b > a)
            return file.parent_path() / Paths::FromUtf8(line.substr(a + 1, b - a - 1));
    }
    return {};
}

// Los juegos en disco de PlayStation y de Saturn usan las mismas extensiones: se mira el disco.
const SystemDef* DiscSystem(const fs::path& file, const std::string& ext) {
    const SystemDef* cd = SystemById("cd");
    const SystemDef* saturn = SystemById("anillo");
    if (!saturn) return cd;
    if (ext == "chd") {
        // El CHD va comprimido; se guía por la carpeta o el nombre («Saturn», «SS»).
        const std::string p = Lower(Paths::ToUtf8(file));
        return p.find("saturn") != std::string::npos || p.find("/ss/") != std::string::npos ||
                       p.find("\\ss\\") != std::string::npos
                   ? saturn
                   : cd;
    }
    const fs::path track = FirstTrack(file, ext);
    if (!track.empty() && FileHas(track, "SEGA SEGASATURN", 64 * 1024)) return saturn;
    return cd;
}

} // namespace

Game Library::MakeGame(const fs::path& file) {
    Game g;
    g.path = file;
    g.path_utf8 = Paths::ToUtf8(file);
    SplitTitle(Paths::ToUtf8(file.stem()), g.title, g.tags);
    g.ext = Lower(Paths::ToUtf8(file.extension()));
    if (!g.ext.empty() && g.ext[0] == '.') g.ext.erase(0, 1);
    const SystemDef* sys = SystemForExtension(g.ext);
    if (sys && sys->disc) sys = DiscSystem(file, g.ext);
    if (sys) g.system = sys->id;
    char hex[16];
    std::snprintf(hex, sizeof hex, "%08x", Fnv(Lower(Paths::StableId(file))));
    g.key = hex;
    std::snprintf(hex, sizeof hex, "%08x", Fnv(Lower(g.path_utf8)));
    g.legacy_key = hex;
    g.hue = Fnv(g.title) % 360;
    return g;
}

bool Library::ApplyMeta(Game& g) {
    bool migrated = false;
    // Modo portátil: los datos viejos se guardaron con la ruta completa; se pasan a la clave nueva.
    if (g.legacy_key != g.key) {
        for (const char* field : {".favorito", ".tiempo", ".ultima", ".oculto"}) {
            const std::string nk = g.key + field, ok = g.legacy_key + field;
            if (!meta_.Values().count(nk) && meta_.Values().count(ok)) {
                meta_.Set(nk, meta_.Get(ok));
                migrated = true;
            }
        }
    }
    g.hidden = meta_.GetBool(g.key + ".oculto", false);
    g.favorite = meta_.GetBool(g.key + ".favorito", false);
    g.playtime_s = meta_.GetI64(g.key + ".tiempo", 0);
    g.last_played = meta_.GetI64(g.key + ".ultima", 0);
    return migrated;
}

void Library::Rescan(const std::vector<std::string>& folders) {
    {
        std::lock_guard lock(mutex_);
        queued_folders_ = folders;
        queued_ = true;
        cancel_ = false;
    }
    if (scanning_) return; // la búsqueda en curso toma la fila al terminar
    if (worker_.joinable()) worker_.join();
    scanning_ = true;
    worker_ = std::thread([this] { ScanLoop(); });
}

void Library::ScanLoop() {
    const std::vector<std::string> extensions = AllExtensions();
    for (;;) {
        std::vector<std::string> folders;
        {
            std::lock_guard lock(mutex_);
            if (!queued_) {
                // Se apaga dentro del candado: un Rescan que llegue ahora ve scanning_ en false
                // y arranca otro hilo después de esperar a este.
                scanning_ = false;
                return;
            }
            folders = queued_folders_;
            queued_ = false;
        }
        cancel_ = false;
        std::vector<Game> found;
        for (const auto& f : folders) {
            std::error_code ec;
            const fs::path root = Paths::ResolveFolder(f);
            fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
            for (; !ec && it != end && !cancel_; it.increment(ec)) {
                if (it.depth() > 6) {
                    it.disable_recursion_pending();
                    continue;
                }
                if (!it->is_regular_file(ec)) continue;
                std::string ext = Lower(Paths::ToUtf8(it->path().extension()));
                if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
                if (std::find(extensions.begin(), extensions.end(), ext) == extensions.end()) continue;
                // Un .bin de más de 16 MB es una pista de disco, no un cartucho de Mega.
                if (ext == "bin" && it->file_size(ec) > 16u * 1024 * 1024) continue;
                found.push_back(MakeGame(it->path()));
                if (found.size() > 20000) break;
            }
        }
        if (cancel_) {
            // Cancelada: se queda la biblioteca anterior.
            Log::Info("Biblioteca: búsqueda cancelada");
            continue;
        }
        // Juegos en disco: los .bin que nombra un .cue y los discos que nombra un .m3u son partes
        // de un mismo juego, no juegos aparte (y un .bin de PlayStation no es de Mega).
        std::set<std::string> parts;
        for (const auto& g : found) {
            if (g.ext != "cue" && g.ext != "m3u") continue;
            std::ifstream in(g.path);
            std::string line;
            int lines = 0;
            while (std::getline(in, line) && ++lines < 400) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                std::string name;
                if (g.ext == "cue") {
                    const size_t a = line.find('"'), b = line.rfind('"');
                    if (Lower(line).find("file") == std::string::npos || a == std::string::npos || b <= a) continue;
                    name = line.substr(a + 1, b - a - 1);
                } else {
                    if (line.empty() || line[0] == '#') continue;
                    name = line;
                }
                std::error_code e2;
                const fs::path part = fs::weakly_canonical(g.path.parent_path() / Paths::FromUtf8(name), e2);
                if (!e2) parts.insert(Paths::ToUtf8(part));
            }
        }
        if (!parts.empty()) {
            found.erase(std::remove_if(found.begin(), found.end(),
                                       [&](const Game& g) {
                                           std::error_code e2;
                                           const fs::path p = fs::weakly_canonical(g.path, e2);
                                           return !e2 && parts.count(Paths::ToUtf8(p));
                                       }),
                        found.end());
        }
        // Sin duplicados (la misma carpeta agregada dos veces).
        std::sort(found.begin(), found.end(), [](const Game& a, const Game& b) { return a.key < b.key; });
        found.erase(std::unique(found.begin(), found.end(), [](const Game& a, const Game& b) { return a.key == b.key; }), found.end());
        const size_t count = found.size();
        {
            std::lock_guard lock(mutex_);
            bool migrated = false;
            for (auto& g : found) migrated |= ApplyMeta(g);
            games_ = std::move(found);
            if (migrated) meta_.Save(Paths::LibraryFile());
        }
        Log::Info("Biblioteca: %zu juegos", count);
        changed_ = true;
    }
}

bool Library::TakeChanged() { return changed_.exchange(false); }

std::vector<Game> Library::Snapshot() const {
    std::lock_guard lock(mutex_);
    return games_;
}

size_t Library::Count() const {
    std::lock_guard lock(mutex_);
    return games_.size();
}

Game* Library::Find(const std::string& key) {
    for (auto& g : games_)
        if (g.key == key) return &g;
    return nullptr;
}

void Library::ToggleFavorite(const std::string& key) {
    std::lock_guard lock(mutex_);
    const bool fav = !meta_.GetBool(key + ".favorito", false);
    meta_.SetBool(key + ".favorito", fav);
    if (Game* g = Find(key)) g->favorite = fav;
    meta_.Save(Paths::LibraryFile());
}

void Library::SetHidden(const std::string& key, bool hidden) {
    std::lock_guard lock(mutex_);
    if (hidden) meta_.SetBool(key + ".oculto", true);
    else meta_.Erase(key + ".oculto");
    if (Game* g = Find(key)) g->hidden = hidden;
    meta_.Save(Paths::LibraryFile());
}

void Library::AddPlaytime(const std::string& key, long long seconds) {
    if (seconds <= 0) return;
    std::lock_guard lock(mutex_);
    const long long t = meta_.GetI64(key + ".tiempo", 0) + seconds;
    meta_.SetI64(key + ".tiempo", t);
    if (Game* g = Find(key)) g->playtime_s = t;
    meta_.Save(Paths::LibraryFile());
}

void Library::MarkPlayed(const std::string& key) {
    std::lock_guard lock(mutex_);
    const long long now = static_cast<long long>(std::time(nullptr));
    meta_.SetI64(key + ".ultima", now);
    if (Game* g = Find(key)) g->last_played = now;
    meta_.Save(Paths::LibraryFile());
}

std::string FormatPlaytime(long long s) {
    if (s < 60) return s <= 0 ? "Sin jugar" : "Menos de 1 min";
    const long long h = s / 3600, m = (s % 3600) / 60;
    if (h == 0) return std::to_string(m) + " min";
    return std::to_string(h) + " h " + std::to_string(m) + " min";
}

std::string FormatLastPlayed(long long t) {
    if (t <= 0) return "Nunca";
    const long long diff = static_cast<long long>(std::time(nullptr)) - t;
    if (diff < 120) return "Hace un momento";
    if (diff < 3600) return "Hace " + std::to_string(diff / 60) + " min";
    if (diff < 86400) return "Hace " + std::to_string(diff / 3600) + " h";
    const long long d = diff / 86400;
    if (d == 1) return "Ayer";
    if (d < 30) return "Hace " + std::to_string(d) + " días";
    if (d < 365) return "Hace " + std::to_string(d / 30) + (d / 30 == 1 ? " mes" : " meses");
    return "Hace más de un año";
}

} // namespace Satronho
