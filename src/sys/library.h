// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
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
    std::string legacy_key;         // el de antes (ruta completa), para no perder datos en modo portátil
    uint32_t hue = 0;               // color de la portada
    // Juegos dentro de un .zip o .7z: el archivo del juego adentro y, si trae varios, todos.
    std::string entry;
    std::vector<std::string> choices;
    bool Archived() const { return !entry.empty(); }
    bool favorite = false;
    bool hidden = false;            // oculto de la biblioteca (se puede volver a mostrar)
    long long playtime_s = 0;
    long long last_played = 0;      // segundos desde 1970
};

class Library {
public:
    ~Library();
    void Load();
    void Save();

    // Busca juegos en las carpetas en segundo plano. Si ya hay una búsqueda en curso, la nueva
    // queda en fila y se hace al terminar (así no se pierde una carpeta recién agregada).
    void Rescan(const std::vector<std::string>& folders);
    void CancelScan() { cancel_ = true; }
    bool Scanning() const { return scanning_; }
    bool TakeChanged();             // true una vez tras terminar un escaneo

    std::vector<Game> Snapshot() const;
    Game* Find(const std::string& key);
    static Game MakeGame(const fs::path& file);
    // La clave de un archivo (sin abrirlo): sirve para pasar los datos de un juego que se mudó.
    static std::string KeyFor(const fs::path& file);
    // Un juego cambió de carpeta: su tiempo, favorito y demás pasan de la clave vieja a la nueva.
    void Rekey(const std::vector<std::pair<std::string, std::string>>& old_new);
    // El mismo juego, pero con otro archivo de adentro del .zip/.7z (cuando trae varios).
    static Game MakeGameEntry(const fs::path& archive, const std::string& entry);

    void ToggleFavorite(const std::string& key);
    void SetHidden(const std::string& key, bool hidden);
    void AddPlaytime(const std::string& key, long long seconds);
    void MarkPlayed(const std::string& key);
    void ResetPlaytime(const std::string& key);
    size_t Count() const;

private:
    bool ApplyMeta(Game& g); // true si pasó datos de la clave vieja a la nueva
    void ScanLoop();

    mutable std::mutex mutex_;
    std::vector<Game> games_;
    Ini meta_;
    std::thread worker_;
    std::atomic<bool> scanning_{false};
    std::atomic<bool> changed_{false};
    std::atomic<bool> cancel_{false};
    std::vector<std::string> queued_folders_;
    bool queued_ = false; // protegido por mutex_
};

std::string FormatPlaytime(long long seconds);
std::string FormatLastPlayed(long long unix_time);

} // namespace Satronho
