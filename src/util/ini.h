// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <map>
#include <string>

#include "util/paths.h"

namespace Satronho {

// Archivo clave=valor sencillo. Se guarda de forma atómica (archivo temporal + renombrar),
// así un cierre forzado nunca deja la configuración vacía o a medias.
class Ini {
public:
    bool Load(const fs::path& file);
    bool Save(const fs::path& file) const;

    std::string Get(const std::string& key, const std::string& def = {}) const;
    int GetInt(const std::string& key, int def) const;
    float GetFloat(const std::string& key, float def) const;
    bool GetBool(const std::string& key, bool def) const;
    long long GetI64(const std::string& key, long long def) const;

    void Set(const std::string& key, const std::string& value);
    void SetInt(const std::string& key, int value);
    void SetFloat(const std::string& key, float value);
    void SetBool(const std::string& key, bool value);
    void SetI64(const std::string& key, long long value);
    void Erase(const std::string& key);

    const std::map<std::string, std::string>& Values() const { return values_; }

private:
    std::map<std::string, std::string> values_;
};

} // namespace Satronho
