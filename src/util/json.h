// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Satronho {

// Lector JSON mínimo (solo lectura) para novedades.json y consejos.json.
struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Json> arr;
    std::map<std::string, Json> obj;

    static Json Parse(const std::string& text, bool* ok = nullptr);

    const Json& operator[](const std::string& key) const;
    const Json& operator[](size_t i) const;
    size_t size() const { return type == Type::Array ? arr.size() : obj.size(); }
    std::string String(const std::string& def = {}) const { return type == Type::String ? str : def; }
    bool IsArray() const { return type == Type::Array; }
};

} // namespace Satronho
