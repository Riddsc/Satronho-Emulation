// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "sys/library.h"
#include "util/paths.h"

namespace Satronho {

// Busca juegos en todo el teléfono (memoria interna y tarjetas SD), en segundo plano. Se salta lo
// del sistema y de otras apps (fotos, música, Android/data) para ir rápido, pero sí mira lo que
// llega por WhatsApp y Telegram (Android/media). También encuentra las claves (.keys) y el firmware
// de la Switch que la persona ya tenga en el teléfono, para pasárselos a Satronho NX.
class PhoneScan {
public:
    struct Folder {
        fs::path path;
        std::vector<Game> games;     // juegos nuevos (no están en la biblioteca)
        int known = 0;               // juegos que ya estaban
        bool chosen = true;          // la persona lo deja marcado para agregar u ordenar
    };
    struct Result {
        std::vector<Folder> folders;
        std::vector<fs::path> keys;      // prod.keys, title.keys
        std::vector<fs::path> firmware;  // .zip de firmware o carpeta con sus .nca
        int files = 0;
        int NewGames() const {
            int n = 0;
            for (const auto& f : folders) n += static_cast<int>(f.games.size());
            return n;
        }
    };

    ~PhoneScan();
    // known: claves de los juegos que ya están en la biblioteca.
    void Start(const std::vector<fs::path>& roots, std::set<std::string> known, const fs::path& skip_root);
    void Cancel() { cancel_ = true; }
    bool Running() const { return running_; }
    int Files() const { return files_; }
    int Found() const { return found_; }
    std::string Current() const;
    // true una vez al terminar (cancelada o no).
    bool TakeDone(Result& out);

private:
    void Run(std::vector<fs::path> roots, std::set<std::string> known, fs::path skip_root);

    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> done_{false};
    std::atomic<bool> cancel_{false};
    std::atomic<int> files_{0};
    std::atomic<int> found_{0};
    mutable std::mutex mutex_;
    std::string current_;
    Result result_;
};

} // namespace Satronho
