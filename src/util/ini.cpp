// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/ini.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace Satronho {

static std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

static std::string Escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

static std::string Unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            out += s[i] == 'n' ? '\n' : s[i];
        } else {
            out += s[i];
        }
    }
    return out;
}

bool Ini::Load(const fs::path& file) {
    values_.clear();
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        values_[Trim(line.substr(0, eq))] = Unescape(Trim(line.substr(eq + 1)));
    }
    return true;
}

bool Ini::Save(const fs::path& file) const {
    std::ostringstream ss;
    ss << "# Satronho Emulation\n";
    for (const auto& [k, v] : values_) ss << k << " = " << Escape(v) << "\n";
    const std::string data = ss.str();

    fs::path tmp = file;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        out.flush();
        if (!out) return false;
    }
    std::error_code ec;
    fs::rename(tmp, file, ec);
    if (ec) {
        fs::remove(file, ec);
        fs::rename(tmp, file, ec);
    }
    return !ec;
}

std::string Ini::Get(const std::string& key, const std::string& def) const {
    auto it = values_.find(key);
    return it == values_.end() ? def : it->second;
}
int Ini::GetInt(const std::string& key, int def) const {
    auto it = values_.find(key);
    return it == values_.end() || it->second.empty() ? def : std::atoi(it->second.c_str());
}
float Ini::GetFloat(const std::string& key, float def) const {
    auto it = values_.find(key);
    return it == values_.end() || it->second.empty() ? def : static_cast<float>(std::atof(it->second.c_str()));
}
bool Ini::GetBool(const std::string& key, bool def) const {
    auto it = values_.find(key);
    if (it == values_.end()) return def;
    return it->second == "1" || it->second == "true" || it->second == "si";
}
long long Ini::GetI64(const std::string& key, long long def) const {
    auto it = values_.find(key);
    return it == values_.end() || it->second.empty() ? def : std::atoll(it->second.c_str());
}
void Ini::Set(const std::string& key, const std::string& value) { values_[key] = value; }
void Ini::SetInt(const std::string& key, int value) { values_[key] = std::to_string(value); }
void Ini::SetFloat(const std::string& key, float value) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.3f", value);
    values_[key] = buf;
}
void Ini::SetBool(const std::string& key, bool value) { values_[key] = value ? "1" : "0"; }
void Ini::SetI64(const std::string& key, long long value) { values_[key] = std::to_string(value); }
void Ini::Erase(const std::string& key) { values_.erase(key); }

} // namespace Satronho
