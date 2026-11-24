// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/hash.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace Satronho::Hash {
namespace {

inline uint32_t Rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
inline uint32_t Ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

std::string Hex(const uint8_t* d, size_t n) {
    static const char* k = "0123456789abcdef";
    std::string s(n * 2, '0');
    for (size_t i = 0; i < n; ++i) {
        s[i * 2] = k[d[i] >> 4];
        s[i * 2 + 1] = k[d[i] & 15];
    }
    return s;
}

// Las dos huellas procesan bloques de 64 bytes con el largo al final en big endian.
template <typename Block>
void Feed(std::vector<uint8_t>& buf, uint64_t& total, const uint8_t* data, size_t size, Block&& block) {
    total += size;
    while (size) {
        const size_t take = std::min<size_t>(64 - buf.size(), size);
        buf.insert(buf.end(), data, data + take);
        data += take;
        size -= take;
        if (buf.size() == 64) {
            block(buf.data());
            buf.clear();
        }
    }
}

template <typename Block>
void Finish(std::vector<uint8_t>& buf, uint64_t total, Block&& block) {
    const uint64_t bits = total * 8;
    buf.push_back(0x80);
    if (buf.size() > 56) {
        buf.resize(64, 0);
        block(buf.data());
        buf.clear();
    }
    buf.resize(56, 0);
    for (int i = 7; i >= 0; --i) buf.push_back(static_cast<uint8_t>(bits >> (i * 8)));
    block(buf.data());
    buf.clear();
}

uint32_t Be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }

struct Sha1 {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    std::vector<uint8_t> buf;
    uint64_t total = 0;
    void Block(const uint8_t* p) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) w[i] = Be32(p + i * 4);
        for (int i = 16; i < 80; ++i) w[i] = Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            const uint32_t t = Rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = Rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    void Update(const uint8_t* d, size_t n) { Feed(buf, total, d, n, [this](const uint8_t* p) { Block(p); }); }
    std::string Done() {
        Finish(buf, total, [this](const uint8_t* p) { Block(p); });
        uint8_t out[20];
        for (int i = 0; i < 5; ++i)
            for (int j = 0; j < 4; ++j) out[i * 4 + j] = static_cast<uint8_t>(h[i] >> (24 - j * 8));
        return Hex(out, 20);
    }
};

struct Sha256State {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::vector<uint8_t> buf;
    uint64_t total = 0;
    void Block(const uint8_t* p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
            0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
            0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
            0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) w[i] = Be32(p + i * 4);
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = Ror(w[i - 15], 7) ^ Ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = Ror(w[i - 2], 17) ^ Ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t S1 = Ror(e, 6) ^ Ror(e, 11) ^ Ror(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            const uint32_t S0 = Ror(a, 2) ^ Ror(a, 13) ^ Ror(a, 22);
            const uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    void Update(const uint8_t* d, size_t n) { Feed(buf, total, d, n, [this](const uint8_t* p) { Block(p); }); }
    std::string Done() {
        Finish(buf, total, [this](const uint8_t* p) { Block(p); });
        uint8_t out[32];
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 4; ++j) out[i * 4 + j] = static_cast<uint8_t>(h[i] >> (24 - j * 8));
        return Hex(out, 32);
    }
};

template <typename H>
std::string FileHash(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return {};
    H h;
    std::vector<char> chunk(1 << 16);
    while (in) {
        in.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        const auto n = in.gcount();
        if (n > 0) h.Update(reinterpret_cast<const uint8_t*>(chunk.data()), static_cast<size_t>(n));
    }
    return h.Done();
}

} // namespace

std::string Sha1File(const fs::path& file) { return FileHash<Sha1>(file); }
std::string Sha256File(const fs::path& file) { return FileHash<Sha256State>(file); }
std::string Sha256(const void* data, size_t size) {
    Sha256State h;
    h.Update(static_cast<const uint8_t*>(data), size);
    return h.Done();
}

} // namespace Satronho::Hash
