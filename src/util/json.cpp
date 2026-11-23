// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/json.h"

#include <cstdlib>

namespace Satronho {
namespace {

struct Parser {
    const std::string& s;
    size_t i = 0;
    bool ok = true;

    void Ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    }

    static void PutUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    std::string Str() {
        std::string out;
        ++i; // "
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i >= s.size()) break;
            char e = s[i++];
            switch (e) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
                unsigned cp = std::strtoul(s.substr(i, 4).c_str(), nullptr, 16);
                i += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                    unsigned lo = std::strtoul(s.substr(i + 2, 4).c_str(), nullptr, 16);
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    i += 6;
                }
                PutUtf8(out, cp);
                break;
            }
            default: out += e;
            }
        }
        ++i; // "
        return out;
    }

    Json Value() {
        Ws();
        Json j;
        if (i >= s.size()) {
            ok = false;
            return j;
        }
        char c = s[i];
        if (c == '{') {
            j.type = Json::Type::Object;
            ++i;
            Ws();
            if (i < s.size() && s[i] == '}') { ++i; return j; }
            while (ok && i < s.size()) {
                Ws();
                if (s[i] != '"') { ok = false; break; }
                std::string k = Str();
                Ws();
                if (i >= s.size() || s[i] != ':') { ok = false; break; }
                ++i;
                j.obj[k] = Value();
                Ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == '}') { ++i; break; }
                ok = false;
            }
        } else if (c == '[') {
            j.type = Json::Type::Array;
            ++i;
            Ws();
            if (i < s.size() && s[i] == ']') { ++i; return j; }
            while (ok && i < s.size()) {
                j.arr.push_back(Value());
                Ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == ']') { ++i; break; }
                ok = false;
            }
        } else if (c == '"') {
            j.type = Json::Type::String;
            j.str = Str();
        } else if (s.compare(i, 4, "true") == 0) {
            j.type = Json::Type::Bool; j.b = true; i += 4;
        } else if (s.compare(i, 5, "false") == 0) {
            j.type = Json::Type::Bool; i += 5;
        } else if (s.compare(i, 4, "null") == 0) {
            i += 4;
        } else {
            char* end = nullptr;
            j.type = Json::Type::Number;
            j.num = std::strtod(s.c_str() + i, &end);
            if (end == s.c_str() + i) ok = false;
            i = static_cast<size_t>(end - s.c_str());
        }
        return j;
    }
};

const Json& Null() {
    static const Json n;
    return n;
}

} // namespace

Json Json::Parse(const std::string& text, bool* ok) {
    Parser p{text};
    Json j = p.Value();
    if (ok) *ok = p.ok;
    return j;
}

const Json& Json::operator[](const std::string& key) const {
    if (type != Type::Object) return Null();
    auto it = obj.find(key);
    return it == obj.end() ? Null() : it->second;
}

const Json& Json::operator[](size_t idx) const {
    if (type != Type::Array || idx >= arr.size()) return Null();
    return arr[idx];
}

} // namespace Satronho
