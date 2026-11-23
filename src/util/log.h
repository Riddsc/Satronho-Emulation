// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Satronho::Log {

void Open();   // abre satronho.log (se reescribe en cada inicio)
void Close();
void Info(const char* fmt, ...);
void Warn(const char* fmt, ...);
void Error(const char* fmt, ...);

} // namespace Satronho::Log
