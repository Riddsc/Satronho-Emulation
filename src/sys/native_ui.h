// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Puente con la interfaz nativa de Android (build 25, en Kotlin, como Satronho NX).
//
// En el celular las pantallas (biblioteca, carga, control en pantalla, menú del juego, tarjetas de
// los guías) las dibuja Android. El código en C++ sigue siendo el «cerebro»: juegos, núcleos,
// ajustes y partidas. Los dos hablan por aquí, sin llamarse directo entre hilos:
//   - Kotlin → C++: órdenes de texto («jugar|clave») que la app atiende en su propio hilo.
//   - C++ → Kotlin: el estado (JSON que Kotlin lee cuando cambia la versión) y avisos sueltos.
//   - El control en pantalla escribe los botones apretados directo (sin esperar a nadie).
// En PC todo esto existe pero queda apagado (Active() es false).
namespace Satronho::NativeUi {

bool Active();                        // la interfaz nativa está en pantalla (solo Android)
std::vector<std::string> TakeCommands();

// Estado completo para Kotlin. Solo se publica si cambió (se compara el texto).
void PublishState(const std::string& json);
void PublishLibrary(const std::string& json);
void Emit(const std::string& json);   // aviso suelto: tarjeta de un guía, aviso corto…

// Control en pantalla nativo.
struct Touch {
    uint32_t buttons = 0;             // bits del RetroPad
    float lx = 0, ly = 0;             // palanca izquierda, -1..1
    float rx = 0, ry = 0;             // palanca derecha
};
Touch TouchState();
void ResetTouch();

// Texto JSON con comillas y escapes.
std::string Quote(const std::string& s);

} // namespace Satronho::NativeUi
