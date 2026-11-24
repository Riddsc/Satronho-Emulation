// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/rewind.h"

#include <cstring>

namespace Satronho {

namespace {
inline uint64_t Load64(const uint8_t* p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return v;
}
inline void Put32(std::vector<uint8_t>& out, uint32_t v) {
    const uint8_t b[4] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24)};
    out.insert(out.end(), b, b + 4);
}
inline uint32_t Get32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
} // namespace

void RewindBuffer::Clear() {
    latest_.clear();
    latest_.shrink_to_fit();
    deltas_.clear();
    bytes_ = 0;
}

// Formato del parche: bloques [ceros: u32][largo: u32][bytes XOR]. «ceros» son bytes iguales
// en los dos estados (no se guardan); los otros se guardan como XOR, que sirve para ir y volver.
void RewindBuffer::Encode(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, std::vector<uint8_t>& out) {
    out.clear();
    const size_t n = a.size();
    const uint8_t* pa = a.data();
    const uint8_t* pb = b.data();
    size_t i = 0;
    while (i < n) {
        size_t j = i;
        while (j + 8 <= n && Load64(pa + j) == Load64(pb + j)) j += 8;
        while (j < n && pa[j] == pb[j]) ++j;
        size_t k = j;
        while (k < n) {
            if (k + 8 <= n) {
                if (Load64(pa + k) == Load64(pb + k)) break;
                k += 8;
            } else {
                if (pa[k] == pb[k]) break;
                ++k;
            }
        }
        Put32(out, static_cast<uint32_t>(j - i));
        Put32(out, static_cast<uint32_t>(k - j));
        for (size_t x = j; x < k; ++x) out.push_back(static_cast<uint8_t>(pa[x] ^ pb[x]));
        i = k;
    }
    out.shrink_to_fit();
}

void RewindBuffer::Decode(const std::vector<uint8_t>& patch, std::vector<uint8_t>& state) {
    size_t p = 0, pos = 0;
    while (p + 8 <= patch.size()) {
        const uint32_t zeros = Get32(&patch[p]);
        const uint32_t lit = Get32(&patch[p + 4]);
        p += 8;
        pos += zeros;
        if (pos + lit > state.size() || p + lit > patch.size()) return; // parche roto: se deja así
        for (uint32_t x = 0; x < lit; ++x) state[pos + x] ^= patch[p + x];
        p += lit;
        pos += lit;
    }
}

void RewindBuffer::Push(const std::vector<uint8_t>& state) {
    if (state.empty()) return;
    if (!latest_.empty()) {
        Delta d;
        if (latest_.size() == state.size()) {
            Encode(latest_, state, d.data);
        } else {
            d.full = true;
            d.data = latest_;
        }
        bytes_ += d.data.size();
        deltas_.push_back(std::move(d));
    }
    latest_ = state;
    Trim();
}

bool RewindBuffer::Pop(std::vector<uint8_t>& out) {
    if (deltas_.empty()) return false;
    Delta& d = deltas_.back();
    if (d.full) latest_ = std::move(d.data);
    else Decode(d.data, latest_);
    bytes_ -= d.full ? latest_.size() : d.data.size();
    deltas_.pop_back();
    out = latest_;
    return true;
}

void RewindBuffer::Trim() {
    while (!deltas_.empty() && (bytes_ + latest_.size() > budget_ || deltas_.size() > max_steps_)) {
        bytes_ -= deltas_.front().data.size();
        deltas_.pop_front();
    }
}

} // namespace Satronho
