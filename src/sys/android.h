// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

// Lo que Android hace aparte (SatronhoActivity.java). En PC estas funciones no hacen nada.
namespace Satronho::Android {

#ifdef __ANDROID__
bool HasFileAccess();                 // permiso de «acceso a todos los archivos»
void RequestFileAccess();             // abre la pantalla de Android para darlo
std::vector<std::string> Roots();     // memoria interna y tarjetas SD
bool Download(const std::string& url, const std::string& path);
void Vibrate(int ms, int strength);   // 1..255
bool InstallApk(const std::string& path);
std::string CoresDir();               // donde Android instaló los núcleos (nativeLibraryDir)
// Juegos que llegan con «Abrir con» o «Compartir» desde otra app (WhatsApp, Archivos…).
std::string Received();               // «ok|ruta», «error|nombre|motivo», «sinespacio|nombre|total|libre» o ""
std::string ReceiveStatus();          // «nombre|copiados|total|pendientes» mientras copia, o ""
void SetOrientation(int mode);        // 0 horizontal, 1 girar sola, 2 vertical
int KeyboardHeight();                 // alto del teclado en pantalla (píxeles), 0 si no está
void ShareText(const std::string& subject, const std::string& text); // menú «Compartir» de Android
#else
inline bool HasFileAccess() { return true; }
inline void RequestFileAccess() {}
inline std::vector<std::string> Roots() { return {}; }
inline bool Download(const std::string&, const std::string&) { return false; }
inline void Vibrate(int, int) {}
inline bool InstallApk(const std::string&) { return false; }
inline std::string CoresDir() { return {}; }
inline std::string Received() { return {}; }
inline std::string ReceiveStatus() { return {}; }
inline void SetOrientation(int) {}
inline int KeyboardHeight() { return 0; }
inline void ShareText(const std::string&, const std::string&) {}
#endif

} // namespace Satronho::Android
