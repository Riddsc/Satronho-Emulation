// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace Satronho {

// Historial para rebobinar. En vez de guardar cada estado completo (varios MB en PS1, DS,
// Saturn y 64), guarda solo lo que cambió respecto al anterior, comprimido: entre dos cuadros
// casi toda la memoria de la consola sigue igual, así que cada paso ocupa poco.
//
// Cómo funciona: se guarda el estado más nuevo entero (latest_) y, por cada paso hacia atrás,
// un "parche" que convierte un estado en el anterior (XOR entre los dos, con los tramos de ceros
// comprimidos). Para retroceder se aplica el último parche y se descarta. Los parches más viejos
// se tiran cuando se pasa del tope de memoria.
class RewindBuffer {
public:
    void Clear();
    void SetBudget(size_t bytes) { budget_ = bytes; }
    void SetMaxSteps(size_t steps) { max_steps_ = steps; }

    void Push(const std::vector<uint8_t>& state);      // estado nuevo
    bool Pop(std::vector<uint8_t>& out);               // estado anterior (false si no hay más)
    bool Empty() const { return deltas_.empty(); }
    size_t Steps() const { return deltas_.size(); }
    size_t Bytes() const { return bytes_ + latest_.size(); }

private:
    struct Delta {
        std::vector<uint8_t> data; // parche comprimido, o el estado entero si cambió de tamaño
        bool full = false;
    };
    static void Encode(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, std::vector<uint8_t>& out);
    static void Decode(const std::vector<uint8_t>& patch, std::vector<uint8_t>& state);
    void Trim();

    std::vector<uint8_t> latest_;
    std::deque<Delta> deltas_;
    size_t bytes_ = 0;
    size_t budget_ = 256u * 1024 * 1024;
    size_t max_steps_ = 600;
};

} // namespace Satronho
