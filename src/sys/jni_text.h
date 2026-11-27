// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Textos entre Java y C++ sin dañar los emojis (build 28). Las funciones «UTF» de Java usan una
// variante de UTF-8 que parte cada emoji en dos pedazos: una ruta como «Pokémon 🔥.zip» llegaba a C++
// con otros bytes y el archivo «no existía». Aquí se pasa siempre por UTF-16, que es lo que usa Java.
#ifdef __ANDROID__
#include <jni.h>

#include <cstdint>
#include <string>

namespace Satronho::Jni {

inline std::string ToUtf8(JNIEnv* env, jstring s) {
    if (!env || !s) return {};
    const jsize n = env->GetStringLength(s);
    const jchar* u = env->GetStringChars(s, nullptr);
    if (!u) return {};
    std::string out;
    out.reserve(static_cast<size_t>(n) + 8);
    for (jsize i = 0; i < n; ++i) {
        uint32_t cp = u[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < n && u[i + 1] >= 0xDC00 && u[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (u[i + 1] - 0xDC00);
            ++i;
        }
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
    env->ReleaseStringChars(s, u);
    return out;
}

inline jstring FromUtf8(JNIEnv* env, const std::string& s) {
    std::u16string u;
    u.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        uint32_t cp;
        int n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c >> 5) == 0x6) { cp = c & 0x1F; n = 2; }
        else if ((c >> 4) == 0xE) { cp = c & 0x0F; n = 3; }
        else if ((c >> 3) == 0x1E) { cp = c & 0x07; n = 4; }
        else { ++i; continue; }
        if (i + n > s.size()) break;
        for (int k = 1; k < n; ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        i += n;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            u += static_cast<char16_t>(0xD800 + (cp >> 10));
            u += static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
        } else {
            u += static_cast<char16_t>(cp);
        }
    }
    return env->NewString(reinterpret_cast<const jchar*>(u.data()), static_cast<jsize>(u.size()));
}

} // namespace Satronho::Jni
#endif
