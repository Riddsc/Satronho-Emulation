// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "sys/library.h"
#include "ui/kit.h"

namespace Satronho {

// Carátulas reales de los juegos. Salen de libretro-thumbnails (las mismas que usa RetroArch),
// se guardan en «caratulas/<consola>/» y se cargan en segundo plano para no trabar la interfaz.
// La persona también puede poner la suya («caratulas/propias/»), que manda sobre la descargada.
class Covers {
public:
    ~Covers();
    void Init();
    void Shutdown();

    // Imagen lista para dibujar, o nullptr mientras se carga (o si no hay).
    const Ui::Image* Get(const Game& g);
    // La misma carátula muy reducida (se ve borrosa al estirarla): para el fondo de la biblioteca.
    const Ui::Image* GetBlur(const Game& g);
    // En el hilo principal, cada cuadro: sube a la tarjeta gráfica lo que ya se decodificó.
    void Pump();
    // Busca en internet las carátulas que faltan (en segundo plano, una por una).
    void QueueDownloads(const std::vector<Game>& games);
    void CancelDownloads();
    bool Downloading() const { return downloading_; }
    int Remaining() const { return remaining_; }
    int Found() const { return found_; }
    // true una vez cuando llegó alguna carátula nueva (para volver a dibujar).
    bool TakeChanged() { return changed_.exchange(false); }

    void SetCustom(const Game& g, const fs::path& image);
    void RemoveCustom(const Game& g);
    bool HasCustom(const Game& g) const;
    bool HasCover(const Game& g) const;
    // Archivo de la carátula (propia o descargada); puede no existir todavía. Para la interfaz nativa.
    fs::path PathFor(const Game& g) const { return FileFor(g); }
    // Libera las texturas (al cambiar de motor gráfico).
    void DropTextures();
    size_t Count() const;

private:
    struct Decoded {
        std::string key;
        std::vector<unsigned char> rgba, small;
        int w = 0, h = 0, sw = 0, sh = 0;
        bool ok = false;
    };
    struct Entry {
        Ui::Image img, blur;
        bool loading = false;
        bool missing = false;
        double used = 0;
    };
    struct Job {
        std::string key, system, title, stem, ext;
        std::vector<std::string> tags;
    };

    fs::path FileFor(const Game& g) const;   // la que se dibuja (propia o descargada)
    fs::path DownloadPath(const std::string& system, const std::string& key) const;
    fs::path CustomPath(const std::string& key) const;
    void DecodeLoop();
    void DownloadLoop();
    bool FetchOne(const Job& j);
    const std::vector<std::string>& Index(const std::string& repo);

    fs::path dir_;
    std::map<std::string, Entry> cache_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::pair<std::string, fs::path>> decode_queue_;
    std::vector<Decoded> decoded_;
    std::vector<std::string> pending_reload_; // carátulas recién descargadas
    std::deque<Job> jobs_;
    std::set<std::string> queued_;
    std::map<std::string, std::vector<std::string>> index_;
    std::thread decoder_, downloader_;
    std::condition_variable dl_cv_;
    std::atomic<bool> quit_{false}, downloading_{false}, cancel_{false}, changed_{false};
    std::atomic<int> remaining_{0}, found_{0};
};

} // namespace Satronho
