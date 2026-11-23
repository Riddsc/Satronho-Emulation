// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "util/ini.h"
#include "util/paths.h"

namespace Satronho {

struct Game {
    fs::path path;
    std::string path_utf8;
    std::string title;              // nombre limpio, sin etiquetas
    std::vector<std::string> tags;  // (USA), (Rev 1)...
    std::string ext;
    std::string system;             // id de la consola (octa, bolsillo, bolsillo_plus)
    std::string key;                // identificador estable para tiempo de juego y favoritos
    uint32_t hue = 0;               // color de la portada
    bool favorite = false;
    long long playtime_s = 0;
    long long last_played = 0;      // segundos desde 1970
};

class Library {
public:
    ~Library();
    void Load();
    void Save();

    // Busca juegos en las carpetas en segundo plano.
    void Rescan(const std::vector<std::string>& folders);
    bool Scanning() const { return scanning_; }
    bool TakeChanged();             // true una vez tras terminar un escaneo

    std::vector<Game> Snapshot() const;
    Game* Find(const std::string& key);
    static Game MakeGame(const fs::path& file);

    void ToggleFavorite(const std::string& key);
    void AddPlaytime(const std::string& key, long long seconds);
    void MarkPlayed(const std::string& key);
    size_t Count() const;

private:
    void ApplyMeta(Game& g) const;

    mutable std::mutex mutex_;
    std::vector<Game> games_;
    Ini meta_;
    std::thread worker_;
    std::atomic<bool> scanning_{false};
    std::atomic<bool> changed_{false};
};

std::string FormatPlaytime(long long seconds);
std::string FormatLastPlayed(long long unix_time);

} // namespace Satronho
