// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Escritor de CHD v5 para discos de CD (lo mismo que hace «chdman createcd» con el códec cdzl).
// Formato: cabecera de 124 bytes, bloques («hunks») de 8 cuadros de 2448 bytes (2352 del sector +
// 96 del subcódigo) comprimidos con deflate, el mapa de bloques comprimido y los metadatos de las
// pistas (CHT2). Todo en big endian.
#include "util/chd.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#include "miniz.h"
#include "util/log.h"

namespace Satronho::Chd {

namespace {
constexpr uint32_t kFrame = 2448, kSector = 2352, kSub = 96, kFramesPerHunk = 8;
constexpr uint32_t kHunkBytes = kFrame * kFramesPerHunk;

// ---------------------------------------------------------------- SHA-1 y CRC-16
struct Sha1 {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t buf[64];
    size_t used = 0;
    uint64_t total = 0;
    static uint32_t Rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
    void Block(const uint8_t* p) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) w[i] = (p[i * 4] << 24) | (p[i * 4 + 1] << 16) | (p[i * 4 + 2] << 8) | p[i * 4 + 3];
        for (int i = 16; i < 80; ++i) w[i] = Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) f = (b & c) | (~b & d), k = 0x5A827999;
            else if (i < 40) f = b ^ c ^ d, k = 0x6ED9EBA1;
            else if (i < 60) f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
            else f = b ^ c ^ d, k = 0xCA62C1D6;
            const uint32_t t = Rol(a, 5) + f + e + k + w[i];
            e = d, d = c, c = Rol(b, 30), b = a, a = t;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
    }
    void Add(const void* data, size_t n) {
        const auto* p = static_cast<const uint8_t*>(data);
        total += n;
        while (n) {
            const size_t take = std::min(n, 64 - used);
            std::memcpy(buf + used, p, take);
            used += take, p += take, n -= take;
            if (used == 64) Block(buf), used = 0;
        }
    }
    void Finish(uint8_t out[20]) {
        const uint64_t bits = total * 8;
        const uint8_t one = 0x80, zero = 0;
        Add(&one, 1);
        while (used != 56) Add(&zero, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = static_cast<uint8_t>(bits >> (56 - i * 8));
        Add(len, 8);
        for (int i = 0; i < 5; ++i)
            for (int j = 0; j < 4; ++j) out[i * 4 + j] = static_cast<uint8_t>(h[i] >> (24 - j * 8));
    }
};

uint16_t Crc16(const uint8_t* p, size_t n) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; ++i) {
        crc ^= static_cast<uint16_t>(p[i]) << 8;
        for (int b = 0; b < 8; ++b) crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

void Put(std::vector<uint8_t>& v, uint64_t value, int bytes) {
    for (int i = bytes - 1; i >= 0; --i) v.push_back(static_cast<uint8_t>(value >> (i * 8)));
}

// Bits de mayor a menor, como los lee libchdr.
struct BitWriter {
    std::vector<uint8_t> out;
    uint32_t acc = 0;
    int bits = 0;
    void Write(uint32_t value, int n) {
        for (int i = n - 1; i >= 0; --i) {
            acc = (acc << 1) | ((value >> i) & 1);
            if (++bits == 8) out.push_back(static_cast<uint8_t>(acc)), acc = 0, bits = 0;
        }
    }
    void Flush() {
        if (bits) out.push_back(static_cast<uint8_t>(acc << (8 - bits))), acc = 0, bits = 0;
    }
};

// Deflate puro (sin cabecera de zlib), como el códec «zlib» de CHD.
bool Deflate(const uint8_t* src, size_t n, std::vector<uint8_t>& out) {
    size_t len = 0;
    const int flags = static_cast<int>(tdefl_create_comp_flags_from_zip_params(9, -15, MZ_DEFAULT_STRATEGY));
    void* p = tdefl_compress_mem_to_heap(src, n, &len, flags);
    if (!p) return false;
    out.assign(static_cast<uint8_t*>(p), static_cast<uint8_t*>(p) + len);
    mz_free(p);
    return true;
}

// ---------------------------------------------------------------- hoja .cue
struct Track {
    fs::path file;
    std::string type;    // MODE1_RAW, MODE2_RAW, AUDIO
    int index0 = -1, index1 = -1; // en cuadros desde el comienzo del archivo
    int pregap_cmd = 0, postgap = 0;
    uint64_t start = 0;  // cuadro donde empieza (en su archivo)
    uint32_t frames = 0; // cuadros en el archivo (incluye la pausa previa si está en el archivo)
};

int Msf(const std::string& s) {
    int m = 0, sec = 0, f = 0;
    if (std::sscanf(s.c_str(), "%d:%d:%d", &m, &sec, &f) != 3) return -1;
    return (m * 60 + sec) * 75 + f;
}

std::string Upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

bool ParseCue(const fs::path& cue, std::vector<Track>& tracks, std::string* error) {
    std::ifstream in(cue);
    if (!in) {
        if (error) *error = "No se pudo leer el .cue.";
        return false;
    }
    fs::path file;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;
        cmd = Upper(cmd);
        if (cmd == "FILE") {
            const size_t a = line.find('"'), b = line.rfind('"');
            std::string name;
            if (a != std::string::npos && b > a) name = line.substr(a + 1, b - a - 1);
            else ss >> name;
            file = cue.parent_path() / Paths::FromUtf8(name);
        } else if (cmd == "TRACK") {
            int num = 0;
            std::string mode;
            ss >> num >> mode;
            mode = Upper(mode);
            Track t;
            t.file = file;
            if (mode == "MODE1/2352") t.type = "MODE1_RAW";
            else if (mode == "MODE2/2352") t.type = "MODE2_RAW";
            else if (mode == "AUDIO") t.type = "AUDIO";
            else {
                if (error) *error = "Este disco usa pistas «" + mode + "», que todavía no sé convertir.";
                return false;
            }
            tracks.push_back(t);
        } else if (cmd == "INDEX" && !tracks.empty()) {
            int idx = 0;
            std::string at;
            ss >> idx >> at;
            if (idx == 0) tracks.back().index0 = Msf(at);
            if (idx == 1) tracks.back().index1 = Msf(at);
        } else if (cmd == "PREGAP" && !tracks.empty()) {
            std::string at;
            ss >> at;
            tracks.back().pregap_cmd = std::max(0, Msf(at));
        } else if (cmd == "POSTGAP" && !tracks.empty()) {
            std::string at;
            ss >> at;
            tracks.back().postgap = std::max(0, Msf(at));
        }
    }
    if (tracks.empty()) {
        if (error) *error = "El .cue no tiene pistas.";
        return false;
    }
    // Dónde empieza y cuántos cuadros tiene cada pista dentro de su archivo.
    for (size_t i = 0; i < tracks.size(); ++i) {
        Track& t = tracks[i];
        if (t.index1 < 0) {
            if (error) *error = "Una pista del .cue no tiene INDEX 01.";
            return false;
        }
        t.start = static_cast<uint64_t>(t.index0 >= 0 ? t.index0 : t.index1);
        uint64_t end;
        if (i + 1 < tracks.size() && tracks[i + 1].file == t.file) {
            const Track& n = tracks[i + 1];
            end = static_cast<uint64_t>(n.index0 >= 0 ? n.index0 : n.index1);
        } else {
            std::error_code ec;
            const uint64_t size = fs::file_size(t.file, ec);
            if (ec) {
                if (error) *error = "Falta el archivo «" + Paths::ToUtf8(t.file.filename()) + "» del disco.";
                return false;
            }
            end = size / kSector;
        }
        if (end <= t.start) {
            if (error) *error = "Las pistas del .cue no cuadran con los archivos.";
            return false;
        }
        t.frames = static_cast<uint32_t>(end - t.start);
    }
    return true;
}
} // namespace

uint64_t DiscBytes(const fs::path& cue) {
    std::vector<Track> tracks;
    uint64_t total = 0;
    std::error_code ec;
    total += fs::file_size(cue, ec);
    if (!ParseCue(cue, tracks, nullptr)) return total;
    fs::path last;
    for (const auto& t : tracks) {
        if (t.file == last) continue;
        last = t.file;
        total += fs::file_size(t.file, ec);
    }
    return total;
}

bool FromCue(const fs::path& cue, const fs::path& out, std::string* error, const std::function<bool(float)>& progress) {
    std::vector<Track> tracks;
    if (!ParseCue(cue, tracks, error)) return false;
    uint64_t total_frames = 0;
    for (const auto& t : tracks) total_frames += (t.frames + 3) & ~3u; // cada pista se rellena a 4 cuadros
    const uint64_t logical = total_frames * kFrame;
    const uint32_t hunks = static_cast<uint32_t>((logical + kHunkBytes - 1) / kHunkBytes);

    fs::path part = out;
    part += ".part";
    std::ofstream f(part, std::ios::binary | std::ios::trunc);
    if (!f) {
        if (error) *error = "No se pudo crear el .chd (¿la carpeta es de solo lectura?).";
        return false;
    }
    std::vector<uint8_t> header(124, 0);
    f.write(reinterpret_cast<const char*>(header.data()), header.size());

    // Cuadros en orden: cada pista desde su archivo, rellenada con ceros a múltiplo de 4.
    Sha1 raw;
    std::vector<uint8_t> hunk(kHunkBytes);
    uint32_t in_hunk = 0;
    struct MapEntry {
        uint8_t type;
        uint32_t length;
        uint64_t offset;
        uint16_t crc;
    };
    std::vector<MapEntry> map;
    map.reserve(hunks);
    uint64_t offset = 124;
    uint64_t frames_done = 0;
    std::vector<uint8_t> base, sub, comp_base, comp_sub;

    auto flush_hunk = [&]() -> bool {
        // Cuadros que faltan en el último bloque: ceros (no cuentan para el SHA-1).
        std::fill(hunk.begin() + in_hunk * kFrame, hunk.end(), 0);
        const uint16_t crc = Crc16(hunk.data(), kHunkBytes);
        base.resize(kFramesPerHunk * kSector);
        sub.resize(kFramesPerHunk * kSub);
        for (uint32_t i = 0; i < kFramesPerHunk; ++i) {
            std::memcpy(&base[i * kSector], &hunk[i * kFrame], kSector);
            std::memcpy(&sub[i * kSub], &hunk[i * kFrame + kSector], kSub);
        }
        std::vector<uint8_t> block;
        bool packed = Deflate(base.data(), base.size(), comp_base) && Deflate(sub.data(), sub.size(), comp_sub);
        if (packed) {
            // Cabecera del códec de CD: mapa de ECC (ninguno), largo de la parte de datos (2 bytes).
            block.assign((kFramesPerHunk + 7) / 8, 0);
            Put(block, comp_base.size(), 2);
            block.insert(block.end(), comp_base.begin(), comp_base.end());
            block.insert(block.end(), comp_sub.begin(), comp_sub.end());
            packed = block.size() < kHunkBytes && comp_base.size() < 65536;
        }
        if (packed) {
            map.push_back({0, static_cast<uint32_t>(block.size()), offset, crc});
            f.write(reinterpret_cast<const char*>(block.data()), block.size());
            offset += block.size();
        } else {
            map.push_back({4, kHunkBytes, offset, crc}); // sin comprimir
            f.write(reinterpret_cast<const char*>(hunk.data()), kHunkBytes);
            offset += kHunkBytes;
        }
        in_hunk = 0;
        return static_cast<bool>(f);
    };

    std::vector<uint8_t> sector(kSector);
    for (const auto& t : tracks) {
        std::ifstream src(t.file, std::ios::binary);
        if (!src) {
            if (error) *error = "No se pudo leer «" + Paths::ToUtf8(t.file.filename()) + "».";
            return false;
        }
        src.seekg(static_cast<std::streamoff>(t.start * kSector));
        const uint32_t padded = (t.frames + 3) & ~3u;
        for (uint32_t i = 0; i < padded; ++i) {
            uint8_t* fr = &hunk[in_hunk * kFrame];
            std::memset(fr, 0, kFrame);
            if (i < t.frames) {
                src.read(reinterpret_cast<char*>(fr), kSector);
                if (src.gcount() != static_cast<std::streamsize>(kSector)) {
                    if (error) *error = "Un archivo del disco está incompleto.";
                    return false;
                }
                // CHD guarda el audio en big endian.
                if (t.type == "AUDIO")
                    for (uint32_t b = 0; b < kSector; b += 2) std::swap(fr[b], fr[b + 1]);
            }
            raw.Add(fr, kFrame);
            ++frames_done;
            if (++in_hunk == kFramesPerHunk && !flush_hunk()) {
                if (error) *error = "No se pudo escribir el .chd (¿espacio lleno?).";
                return false;
            }
            if ((frames_done & 1023) == 0 && progress && !progress(static_cast<float>(frames_done) / total_frames)) {
                f.close();
                std::error_code ec;
                fs::remove(part, ec);
                if (error) *error = "Conversión cancelada.";
                return false;
            }
        }
    }
    if (in_hunk && !flush_hunk()) {
        if (error) *error = "No se pudo escribir el .chd.";
        return false;
    }

    // Mapa: tipos de bloque con un código de Huffman de 4 bits para los 16 símbolos, y después el
    // largo y el CRC de cada bloque.
    uint32_t max_len = 1;
    for (const auto& m : map) max_len = std::max(max_len, m.length);
    int lengthbits = 0;
    while ((1ull << lengthbits) <= max_len) ++lengthbits;
    BitWriter bw;
    for (int i = 0; i < 16; ++i) bw.Write(4, 4); // árbol: todos los códigos de 4 bits
    for (const auto& m : map) bw.Write(m.type, 4);
    for (const auto& m : map) {
        if (m.type == 0) bw.Write(m.length, lengthbits);
        bw.Write(m.crc, 16);
    }
    bw.Flush();
    std::vector<uint8_t> rawmap;
    for (const auto& m : map) {
        rawmap.push_back(m.type);
        Put(rawmap, m.length, 3);
        Put(rawmap, m.offset, 6);
        Put(rawmap, m.crc, 2);
    }
    const uint64_t mapoffset = offset;
    std::vector<uint8_t> maphdr;
    Put(maphdr, bw.out.size(), 4);
    Put(maphdr, 124, 6);
    Put(maphdr, Crc16(rawmap.data(), rawmap.size()), 2);
    maphdr.push_back(static_cast<uint8_t>(lengthbits));
    maphdr.push_back(0); // selfbits
    maphdr.push_back(0); // parentbits
    maphdr.push_back(0);
    f.write(reinterpret_cast<const char*>(maphdr.data()), maphdr.size());
    f.write(reinterpret_cast<const char*>(bw.out.data()), bw.out.size());
    offset += maphdr.size() + bw.out.size();

    // Metadatos de cada pista (CHT2), encadenados.
    const uint64_t metaoffset = offset;
    struct MetaHash {
        uint8_t data[24];
    };
    std::vector<MetaHash> metahashes;
    for (size_t i = 0; i < tracks.size(); ++i) {
        const Track& t = tracks[i];
        const int pregap_dv = t.index0 >= 0 ? t.index1 - t.index0 : 0;
        char text[256];
        if (pregap_dv > 0)
            std::snprintf(text, sizeof text, "TRACK:%d TYPE:%s SUBTYPE:NONE FRAMES:%u PREGAP:%d PGTYPE:V%s PGSUB:NONE POSTGAP:%d",
                          static_cast<int>(i + 1), t.type.c_str(), t.frames, pregap_dv, t.type.c_str(), t.postgap);
        else
            std::snprintf(text, sizeof text, "TRACK:%d TYPE:%s SUBTYPE:NONE FRAMES:%u PREGAP:%d PGTYPE:%s PGSUB:NONE POSTGAP:%d",
                          static_cast<int>(i + 1), t.type.c_str(), t.frames, t.pregap_cmd, t.pregap_cmd ? t.type.c_str() : "MODE1", t.postgap);
        const size_t len = std::strlen(text) + 1;
        std::vector<uint8_t> e;
        Put(e, 0x43485432, 4); // «CHT2»
        Put(e, (0x01u << 24) | static_cast<uint32_t>(len), 4); // con suma de verificación
        Put(e, i + 1 < tracks.size() ? offset + 16 + len : 0, 8);
        f.write(reinterpret_cast<const char*>(e.data()), e.size());
        f.write(text, static_cast<std::streamsize>(len));
        offset += 16 + len;
        MetaHash mh{};
        mh.data[0] = 'C', mh.data[1] = 'H', mh.data[2] = 'T', mh.data[3] = '2';
        Sha1 s;
        s.Add(text, len);
        s.Finish(mh.data + 4);
        metahashes.push_back(mh);
    }

    // Cabecera con las huellas.
    uint8_t rawsha[20], overall[20];
    raw.Finish(rawsha);
    std::sort(metahashes.begin(), metahashes.end(), [](const MetaHash& a, const MetaHash& b) { return std::memcmp(a.data, b.data, 24) < 0; });
    Sha1 all;
    all.Add(rawsha, 20);
    for (const auto& m : metahashes) all.Add(m.data, 24);
    all.Finish(overall);
    header.clear();
    const char tag[] = "MComprHD";
    header.insert(header.end(), tag, tag + 8);
    Put(header, 124, 4);
    Put(header, 5, 4);
    Put(header, 0x63647A6C, 4); // «cdzl»
    Put(header, 0, 4);
    Put(header, 0, 4);
    Put(header, 0, 4);
    Put(header, logical, 8);
    Put(header, mapoffset, 8);
    Put(header, metaoffset, 8);
    Put(header, kHunkBytes, 4);
    Put(header, kFrame, 4);
    header.insert(header.end(), rawsha, rawsha + 20);
    header.insert(header.end(), overall, overall + 20);
    header.resize(124, 0); // sin disco padre
    f.seekp(0);
    f.write(reinterpret_cast<const char*>(header.data()), header.size());
    f.close();
    if (!f) {
        if (error) *error = "No se pudo terminar el .chd.";
        return false;
    }
    std::error_code ec;
    fs::remove(out, ec);
    fs::rename(part, out, ec);
    if (ec) {
        if (error) *error = "No se pudo renombrar el .chd.";
        return false;
    }
    if (progress) progress(1.0f);
    Log::Info("CHD creado: %s (%u bloques)", Paths::ToUtf8(out).c_str(), hunks);
    return true;
}

} // namespace Satronho::Chd
