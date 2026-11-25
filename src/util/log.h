// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Satronho::Log {

void Open();   // abre satronho.log (se reescribe en cada inicio)
void Close();
void Info(const char* fmt, ...);
void Warn(const char* fmt, ...);
void Error(const char* fmt, ...);
void Flush(); // escribe ya lo pendiente (antes de leer el archivo)

// Si la aplicación se cierra de golpe (un núcleo que falla, por ejemplo), deja anotado en el
// registro el código del error y en qué archivo pasó (satronho-emulation.exe o el .dll del núcleo).
// Solo Windows; en otros sistemas no hace nada.
void InstallCrashHandler();

} // namespace Satronho::Log
