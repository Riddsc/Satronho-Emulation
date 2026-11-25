// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/native_ui.h"

#include <atomic>
#include <cstdio>
#include <mutex>

#include "util/i18n.h"

#ifdef __ANDROID__
#include <jni.h>
#endif

namespace Satronho::NativeUi {

namespace {
std::atomic<bool> g_active{false};
std::mutex g_mutex;
std::vector<std::string> g_commands;
std::vector<std::string> g_events;
std::string g_state, g_library;
int g_state_version = 0, g_library_version = 0;

std::mutex g_touch_mutex;
Touch g_touch;
} // namespace

bool Active() { return g_active.load(); }

std::vector<std::string> TakeCommands() {
    std::lock_guard lock(g_mutex);
    std::vector<std::string> out;
    out.swap(g_commands);
    return out;
}

void PublishState(const std::string& json) {
    std::lock_guard lock(g_mutex);
    if (json == g_state) return;
    g_state = json;
    ++g_state_version;
}

void PublishLibrary(const std::string& json) {
    std::lock_guard lock(g_mutex);
    if (json == g_library) return;
    g_library = json;
    ++g_library_version;
}

void Emit(const std::string& json) {
    std::lock_guard lock(g_mutex);
    g_events.push_back(json);
    if (g_events.size() > 64) g_events.erase(g_events.begin());
}

Touch TouchState() {
    std::lock_guard lock(g_touch_mutex);
    return g_touch;
}

void ResetTouch() {
    std::lock_guard lock(g_touch_mutex);
    g_touch = {};
}

std::string Quote(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out += ch;
            }
        }
    }
    out += '"';
    return out;
}

} // namespace Satronho::NativeUi

#ifdef __ANDROID__
// ---------------------------------------------------------------- lado de Kotlin (object Puente)

using namespace Satronho::NativeUi;

namespace {
std::string ToStd(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* chars = env->GetStringUTFChars(s, nullptr);
    std::string out = chars ? chars : "";
    if (chars) env->ReleaseStringUTFChars(s, chars);
    return out;
}

// NewStringUTF de Java no acepta UTF-8 de 4 bytes (emojis): se arma desde UTF-16.
jstring ToJava(JNIEnv* env, const std::string& s) {
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
} // namespace

extern "C" {

JNIEXPORT void JNICALL Java_com_ridds_satronho_emulation_Puente_activar(JNIEnv*, jclass, jboolean on) {
    Satronho::NativeUi::g_active = on == JNI_TRUE;
}

JNIEXPORT void JNICALL Java_com_ridds_satronho_emulation_Puente_orden(JNIEnv* env, jclass, jstring text) {
    std::string s = ToStd(env, text);
    std::lock_guard lock(Satronho::NativeUi::g_mutex);
    Satronho::NativeUi::g_commands.push_back(std::move(s));
}

JNIEXPORT jint JNICALL Java_com_ridds_satronho_emulation_Puente_versionEstado(JNIEnv*, jclass) {
    std::lock_guard lock(Satronho::NativeUi::g_mutex);
    return Satronho::NativeUi::g_state_version;
}

JNIEXPORT jstring JNICALL Java_com_ridds_satronho_emulation_Puente_estado(JNIEnv* env, jclass) {
    std::string copy;
    {
        std::lock_guard lock(Satronho::NativeUi::g_mutex);
        copy = Satronho::NativeUi::g_state;
    }
    return ToJava(env, copy);
}

JNIEXPORT jint JNICALL Java_com_ridds_satronho_emulation_Puente_versionBiblioteca(JNIEnv*, jclass) {
    std::lock_guard lock(Satronho::NativeUi::g_mutex);
    return Satronho::NativeUi::g_library_version;
}

JNIEXPORT jstring JNICALL Java_com_ridds_satronho_emulation_Puente_biblioteca(JNIEnv* env, jclass) {
    std::string copy;
    {
        std::lock_guard lock(Satronho::NativeUi::g_mutex);
        copy = Satronho::NativeUi::g_library;
    }
    return ToJava(env, copy);
}

// Los avisos sueltos, uno por línea (cada uno es un JSON sin saltos de línea).
JNIEXPORT jstring JNICALL Java_com_ridds_satronho_emulation_Puente_avisos(JNIEnv* env, jclass) {
    std::string joined;
    {
        std::lock_guard lock(Satronho::NativeUi::g_mutex);
        for (const auto& e : Satronho::NativeUi::g_events) {
            joined += e;
            joined += '\n';
        }
        Satronho::NativeUi::g_events.clear();
    }
    return ToJava(env, joined);
}

JNIEXPORT void JNICALL Java_com_ridds_satronho_emulation_Puente_tactil(JNIEnv*, jclass, jint buttons, jfloat lx, jfloat ly, jfloat rx, jfloat ry) {
    std::lock_guard lock(Satronho::NativeUi::g_touch_mutex);
    Satronho::NativeUi::g_touch.buttons = static_cast<uint32_t>(buttons);
    Satronho::NativeUi::g_touch.lx = lx;
    Satronho::NativeUi::g_touch.ly = ly;
    Satronho::NativeUi::g_touch.rx = rx;
    Satronho::NativeUi::g_touch.ry = ry;
}

JNIEXPORT jstring JNICALL Java_com_ridds_satronho_emulation_Puente_tr(JNIEnv* env, jclass, jstring es) {
    return ToJava(env, Satronho::Tr(ToStd(env, es)));
}

} // extern "C"
#endif
