// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/library.h"
#include "util/i18n.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <set>

#include "sys/systems.h"
#include "util/archive.h"
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

namespace {
std::string ExtOf(const std::string& name) {
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos || name.find_first_of("/\\", dot) != std::string::npos) return "";
    return Lower(name.substr(dot + 1));
}

// Juegos de adentro de un .zip / .7z. Si hay un .cue (o .m3u), los .bin son sus pistas.
std::vector<std::string> ArchiveCandidates(const std::vector<Archive::Entry>& entries) {
    std::vector<std::string> discs, carts;
    bool has_sheet = false;
    for (const auto& e : entries) {
        const std::string x = ExtOf(e.name);
        if (x == "cue" || x == "m3u" || x == "ccd") has_sheet = true;
    }
    for (const auto& e : entries) {
        const std::string x = ExtOf(e.name);
        const SystemDef* sys = SystemForExtension(x);
        if (!sys) continue;
        if (x == "bin" && (has_sheet || e.size > 16u * 1024 * 1024)) continue;
        if (has_sheet && (x == "cue" || x == "ccd")) {
            // Con .m3u, los .cue son los discos que nombra.
            bool m3u = false;
            for (const auto& o : entries) m3u |= ExtOf(o.name) == "m3u";
            if (m3u) continue;
        }
        (sys->disc ? discs : carts).push_back(e.name);
    }
    std::vector<std::string> out = discs;
    out.insert(out.end(), carts.begin(), carts.end());
    std::sort(out.begin(), out.end());
    return out;
}
} // namespace

Game Library::MakeGameEntry(const fs::path& archive, const std::string& entry) {
    Game g = MakeGame(archive);
    if (!g.Archived() || g.entry == entry) return g;
    g.entry = entry;
    g.ext = ExtOf(entry);
    if (const SystemDef* sys = SystemForExtension(g.ext)) {
        if (sys->disc) {
            const std::string p = Lower(Paths::ToUtf8(archive) + "/" + entry);
            sys = p.find("saturn") != std::string::npos ? SystemById("anillo") : SystemById("cd");
        }
        if (sys) g.system = sys->id;
    }
    return g;
}

Game Library::MakeGame(const fs::path& file) {
    Game g;
    g.path = file;
    g.path_utf8 = Paths::ToUtf8(file);
    SplitTitle(Paths::ToUtf8(file.stem()), g.title, g.tags);
    g.ext = Lower(Paths::ToUtf8(file.extension()));
    if (!g.ext.empty() && g.ext[0] == '.') g.ext.erase(0, 1);
    const SystemDef* sys = nullptr;
    if (Archive::IsArchive(g.ext)) {
        // .zip / .7z: se mira adentro sin descomprimir. El nombre del juego es el del archivo.
        std::vector<Archive::Entry> entries;
        if (Archive::List(file, entries)) g.choices = ArchiveCandidates(entries);
        if (!g.choices.empty()) {
            // Si el nombre de adentro coincide con el del archivo, ese va primero.
            const std::string stem = Lower(Paths::ToUtf8(file.stem()));
            g.entry = g.choices.front();
            for (const auto& c : g.choices) {
                std::string base = Lower(c.substr(c.find_last_of("/\\") == std::string::npos ? 0 : c.find_last_of("/\\") + 1));
                if (base.rfind(stem, 0) == 0) {
                    g.entry = c;
                    break;
                }
            }
            g.ext = ExtOf(g.entry);
            sys = SystemForExtension(g.ext);
            if (sys && sys->disc) {
                const std::string p = Lower(Paths::ToUtf8(file) + "/" + g.entry);
                sys = p.find("saturn") != std::string::npos || p.find("/ss/") != std::string::npos ? SystemById("anillo") : SystemById("cd");
            }
        }
        if (g.choices.size() <= 1) g.choices.clear();
    } else {
        sys = SystemForExtension(g.ext);
        if (sys && sys->disc) sys = DiscSystem(file, g.ext);
    }
    if (sys) g.system = sys->id;
    char hex[16];
    std::snprintf(hex, sizeof hex, "%08x", Fnv(Lower(Paths::StableId(file))));
    g.key = hex;
    std::snprintf(hex, sizeof hex, "%08x", Fnv(Lower(g.path_utf8)));
    g.legacy_key = hex;
    g.hue = Fnv(g.title) % 360;
    return g;
}

std::string Library::KeyFor(const fs::path& file) {
    char hex[16];
    std::snprintf(hex, sizeof hex, "%08x", Fnv(Lower(Paths::StableId(file))));
    return hex;
}

void Library::Rekey(const std::vector<std::pair<std::string, std::string>>& old_new) {
    {
        std::lock_guard lock(mutex_);
        for (const auto& [ok, nk] : old_new) {
            if (ok == nk) continue;
            for (const char* field : {".favorito", ".tiempo", ".ultima", ".oculto"}) {
                if (!meta_.Values().count(ok + field)) continue;
                meta_.Set(nk + field, meta_.Get(ok + field));
                meta_.Erase(ok + field);
            }
        }
    }
    Save();
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
    std::vector<std::string> extensions = AllExtensions();
    extensions.push_back("zip");
    extensions.push_back("7z");
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
                Game g = MakeGame(it->path());
                if (g.system.empty()) continue; // un .zip sin juegos adentro
                found.push_back(std::move(g));
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
        // Un disco que ya se convirtió a .chd (mismo nombre, misma carpeta) no se muestra dos veces.
        found.erase(std::remove_if(found.begin(), found.end(),
                                   [](const Game& g) {
                                       if (g.ext != "cue" || g.Archived()) return false;
                                       fs::path chd = g.path;
                                       chd.replace_extension(".chd");
                                       std::error_code e2;
                                       return fs::exists(chd, e2);
                                   }),
                    found.end());
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

void Library::ResetPlaytime(const std::string& key) {
    std::lock_guard lock(mutex_);
    meta_.SetI64(key + ".tiempo", 0);
    if (Game* g = Find(key)) g->playtime_s = 0;
    meta_.Save(Paths::LibraryFile());
}

void Library::MarkPlayed(const std::string& key) {
    std::lock_guard lock(mutex_);
    const long long now = static_cast<long long>(std::time(nullptr));
    meta_.SetI64(key + ".ultima", now);
    if (Game* g = Find(key)) g->last_played = now;
    meta_.Save(Paths::LibraryFile());
}

// Textos de tiempo traducidos (los formatos son las claves de los idiomas).
namespace {
std::string TrFmt(const char* fmt, long long n) {
    char buf[128];
    std::snprintf(buf, sizeof buf, Tr(fmt), n);
    return buf;
}
} // namespace

std::string FormatPlaytime(long long s) {
    if (s < 60) return Tr(s <= 0 ? "Sin jugar" : "Menos de 1 min");
    const long long h = s / 3600, m = (s % 3600) / 60;
    if (h == 0) return TrFmt("%lld min", m);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%lld h %lld min", h, m);
    return buf;
}

std::string FormatLastPlayed(long long t) {
    if (t <= 0) return Tr("Nunca");
    const long long diff = static_cast<long long>(std::time(nullptr)) - t;
    if (diff < 120) return Tr("Hace un momento");
    if (diff < 3600) return TrFmt("Hace %lld min", diff / 60);
    if (diff < 86400) return TrFmt("Hace %lld h", diff / 3600);
    const long long d = diff / 86400;
    if (d == 1) return Tr("Ayer");
    if (d < 30) return TrFmt("Hace %lld días", d);
    if (d < 365) return d / 30 == 1 ? Tr(std::string("Hace 1 mes")) : TrFmt("Hace %lld meses", d / 30);
    return Tr("Hace más de un año");
}

} // namespace Satronho
