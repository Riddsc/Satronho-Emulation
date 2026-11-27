// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/android.h"

#include <jni.h>

#include <fstream>
#include <sstream>

#include <SDL3/SDL.h>

#include "sys/jni_text.h"

namespace Satronho::Android {

namespace {
// Llama a un método estático de SatronhoActivity (sirve desde cualquier hilo: SDL lo engancha a Java).
struct Call {
    JNIEnv* env = nullptr;
    jclass cls = nullptr;
    Call() {
        env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        jobject act = static_cast<jobject>(SDL_GetAndroidActivity());
        if (!env || !act) return;
        cls = env->GetObjectClass(act);
        env->DeleteLocalRef(act);
    }
    ~Call() {
        if (env && cls) env->DeleteLocalRef(cls);
        if (env && env->ExceptionCheck()) env->ExceptionClear();
    }
    jmethodID M(const char* name, const char* sig) { return cls ? env->GetStaticMethodID(cls, name, sig) : nullptr; }
    jstring S(const std::string& s) { return Jni::FromUtf8(env, s); }
};
} // namespace

bool HasFileAccess() {
    Call c;
    jmethodID m = c.M("tieneAccesoArchivos", "()Z");
    return m && c.env->CallStaticBooleanMethod(c.cls, m);
}

void RequestFileAccess() {
    Call c;
    if (jmethodID m = c.M("pedirAccesoArchivos", "()V")) c.env->CallStaticVoidMethod(c.cls, m);
}

std::vector<std::string> Roots() {
    std::vector<std::string> out;
    Call c;
    jmethodID m = c.M("raices", "()Ljava/lang/String;");
    if (!m) return out;
    auto js = static_cast<jstring>(c.env->CallStaticObjectMethod(c.cls, m));
    if (!js) return out;
    std::stringstream ss(Jni::ToUtf8(c.env, js));
    std::string line;
    while (std::getline(ss, line))
        if (!line.empty()) out.push_back(line);
    c.env->DeleteLocalRef(js);
    return out;
}

bool Download(const std::string& url, const std::string& path) {
    Call c;
    jmethodID m = c.M("descargar", "(Ljava/lang/String;Ljava/lang/String;)Z");
    if (!m) return false;
    jstring a = c.S(url), b = c.S(path);
    const bool ok = c.env->CallStaticBooleanMethod(c.cls, m, a, b);
    c.env->DeleteLocalRef(a);
    c.env->DeleteLocalRef(b);
    return ok;
}

void Vibrate(int ms, int strength) {
    Call c;
    if (jmethodID m = c.M("vibrar", "(II)V")) c.env->CallStaticVoidMethod(c.cls, m, static_cast<jint>(ms), static_cast<jint>(strength));
}

bool InstallApk(const std::string& path) {
    Call c;
    jmethodID m = c.M("instalarApk", "(Ljava/lang/String;)Z");
    if (!m) return false;
    jstring a = c.S(path);
    const bool ok = c.env->CallStaticBooleanMethod(c.cls, m, a);
    c.env->DeleteLocalRef(a);
    return ok;
}

void ShareText(const std::string& subject, const std::string& text) {
    Call c;
    jmethodID m = c.M("compartirTexto", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!m) return;
    jstring a = c.S(subject), b = c.S(text);
    c.env->CallStaticVoidMethod(c.cls, m, a, b);
    c.env->DeleteLocalRef(a);
    c.env->DeleteLocalRef(b);
}

std::string CoresDir() {
    const char* base = SDL_GetAndroidInternalStoragePath();
    if (!base) return {};
    std::ifstream in(std::string(base) + "/nucleos.txt");
    std::string line;
    std::getline(in, line);
    return line;
}

namespace {
std::string CallString(const char* name) {
    Call c;
    jmethodID m = c.M(name, "()Ljava/lang/String;");
    if (!m) return {};
    auto js = static_cast<jstring>(c.env->CallStaticObjectMethod(c.cls, m));
    if (!js) return {};
    std::string out = Jni::ToUtf8(c.env, js);
    c.env->DeleteLocalRef(js);
    return out;
}
} // namespace

std::string Received() { return CallString("recibido"); }
std::string ReceiveStatus() { return CallString("estadoRecepcion"); }

void SetOrientation(int mode) {
    Call c;
    if (jmethodID m = c.M("orientacion", "(I)V")) c.env->CallStaticVoidMethod(c.cls, m, static_cast<jint>(mode));
}

int KeyboardHeight() {
    Call c;
    jmethodID m = c.M("tecladoAlto", "()I");
    return m ? c.env->CallStaticIntMethod(c.cls, m) : 0;
}

} // namespace Satronho::Android
