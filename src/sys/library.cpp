// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/library.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>

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

Game Library::MakeGame(const fs::path& file) {
    Game g;
    g.path = file;
    g.path_utf8 = Paths::ToUtf8(file);
    SplitTitle(Paths::ToUtf8(file.stem()), g.title, g.tags);
    g.ext = Lower(Paths::ToUtf8(file.extension()));
    if (!g.ext.empty() && g.ext[0] == '.') g.ext.erase(0, 1);
    char hex[16];
    std::snprintf(hex, sizeof hex, "%08x", Fnv(Lower(g.path_utf8)));
    g.key = hex;
    g.hue = Fnv(g.title) % 360;
    return g;
}

void Library::ApplyMeta(Game& g) const {
    g.favorite = meta_.GetBool(g.key + ".favorito", false);
    g.playtime_s = meta_.GetI64(g.key + ".tiempo", 0);
    g.last_played = meta_.GetI64(g.key + ".ultima", 0);
}

void Library::Rescan(const std::vector<std::string>& folders, const std::vector<std::string>& extensions) {
    if (scanning_) return;
    if (worker_.joinable()) worker_.join();
    scanning_ = true;
    worker_ = std::thread([this, folders, extensions] {
        std::vector<Game> found;
        for (const auto& f : folders) {
            std::error_code ec;
            const fs::path root = Paths::FromUtf8(f);
            fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
            for (; !ec && it != end; it.increment(ec)) {
                if (it.depth() > 6) {
                    it.disable_recursion_pending();
                    continue;
                }
                if (!it->is_regular_file(ec)) continue;
                std::string ext = Lower(Paths::ToUtf8(it->path().extension()));
                if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
                if (std::find(extensions.begin(), extensions.end(), ext) == extensions.end()) continue;
                found.push_back(MakeGame(it->path()));
                if (found.size() > 20000) break;
            }
        }
        // Sin duplicados (la misma carpeta agregada dos veces).
        std::sort(found.begin(), found.end(), [](const Game& a, const Game& b) { return a.key < b.key; });
        found.erase(std::unique(found.begin(), found.end(), [](const Game& a, const Game& b) { return a.key == b.key; }), found.end());
        const size_t count = found.size();
        {
            std::lock_guard lock(mutex_);
            for (auto& g : found) ApplyMeta(g);
            games_ = std::move(found);
        }
        Log::Info("Biblioteca: %zu juegos", count);
        changed_ = true;
        scanning_ = false;
    });
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
