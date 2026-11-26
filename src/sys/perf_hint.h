// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

// Modo juego de Android (build 27): le dice al sistema cuánto debe durar cada cuadro y cuánto duró de
// verdad (APerformanceHint, Android 13+), para que suba la velocidad del procesador a tiempo y no la
// baje de golpe a mitad del juego. En PC y en Android viejo no hace nada.
namespace Satronho::PerfHint {

void Begin(int64_t target_ns);   // al empezar un juego (target: 1/60 s, por ejemplo)
void Report(int64_t actual_ns);  // cada cuadro: lo que tardó el trabajo (sin la espera de la pantalla)
void End();                      // al salir del juego

} // namespace Satronho::PerfHint
