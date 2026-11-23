// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace Satronho {

// Busca builds nuevas en los Releases de GitHub y las instala (solo Windows).
class Updater {
public:
    enum class State { Idle, Checking, UpToDate, Available, Downloading, Ready, Failed };

    ~Updater();
    void Check(int current_build);   // en segundo plano
    void Install();                  // descarga e instala; al terminar queda en Ready
    bool Supported() const;

    State GetState() const { return state_; }
    int LatestBuild() const { return latest_; }
    std::string ReleaseUrl() const;
    std::string Error() const;
    float Progress() const { return progress_; }

    // Lanza el instalador y la aplicación debe cerrarse enseguida.
    bool LaunchInstaller();

private:
    void Join();

    std::atomic<State> state_{State::Idle};
    std::atomic<int> latest_{0};
    std::atomic<float> progress_{0};
    mutable std::mutex mutex_;
    std::string zip_url_, page_url_, error_, script_;
    std::thread worker_;
};

} // namespace Satronho
