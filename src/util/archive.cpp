// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/archive.h"

#include <algorithm>

#include <cstring>
#include <fstream>

#include "miniz.h"
#include "util/log.h"

extern "C" {
#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"
#include "7zFile.h"
}

namespace Satronho::Archive {

namespace {
std::string Ext(const fs::path& p) {
    std::string e = Paths::ToUtf8(p.extension());
    for (auto& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (!e.empty() && e[0] == '.') e.erase(0, 1);
    return e;
}

// ---------------------------------------------------------------- .zip (miniz)
// Se abre con un FILE* propio para que las rutas con tildes funcionen también en Windows.
struct Zip {
    mz_zip_archive z{};
    FILE* f = nullptr;
    bool ok = false;
    explicit Zip(const fs::path& p) {
#ifdef _WIN32
        f = _wfopen(p.c_str(), L"rb");
#else
        f = std::fopen(p.c_str(), "rb");
#endif
        if (!f) return;
        std::fseek(f, 0, SEEK_END);
        const long long size = _ftelli64_compat(f);
        std::fseek(f, 0, SEEK_SET);
        ok = mz_zip_reader_init_cfile(&z, f, static_cast<mz_uint64>(size), 0);
    }
    static long long _ftelli64_compat(FILE* file) {
#ifdef _WIN32
        return _ftelli64(file);
#else
        return ftello(file);
#endif
    }
    ~Zip() {
        if (ok) mz_zip_reader_end(&z);
        if (f) std::fclose(f);
    }
};

// ---------------------------------------------------------------- .7z (LZMA SDK)
struct SevenZip {
    CFileInStream stream{};
    CLookToRead2 look{};
    CSzArEx db{};
    ISzAlloc alloc{SzAlloc, SzFree};
    ISzAlloc temp{SzAllocTemp, SzFreeTemp};
    bool opened = false, ok = false;
    explicit SevenZip(const fs::path& p) {
        static bool crc = (CrcGenerateTable(), true);
        (void)crc;
#ifdef _WIN32
        if (InFile_OpenW(&stream.file, p.c_str()) != 0) return;
#else
        if (InFile_Open(&stream.file, p.c_str()) != 0) return;
#endif
        opened = true;
        FileInStream_CreateVTable(&stream);
        LookToRead2_CreateVTable(&look, False);
        look.buf = static_cast<Byte*>(ISzAlloc_Alloc(&alloc, 1 << 18));
        look.bufSize = 1 << 18;
        look.realStream = &stream.vt;
        LookToRead2_INIT(&look);
        SzArEx_Init(&db);
        ok = SzArEx_Open(&db, &look.vt, &alloc, &temp) == SZ_OK;
    }
    ~SevenZip() {
        if (opened) {
            SzArEx_Free(&db, &alloc);
            ISzAlloc_Free(&alloc, look.buf);
            File_Close(&stream.file);
        }
    }
    std::string Name(size_t i) const {
        const size_t len = SzArEx_GetFileNameUtf16(&db, i, nullptr);
        std::vector<UInt16> w(len);
        SzArEx_GetFileNameUtf16(&db, i, w.data());
        std::string out;
        for (size_t k = 0; k + 1 < len; ++k) {
            uint32_t c = w[k];
            if (c >= 0xD800 && c <= 0xDBFF && k + 2 < len) {
                c = 0x10000 + ((c - 0xD800) << 10) + (w[k + 1] - 0xDC00);
                ++k;
            }
            if (c < 0x80) out += static_cast<char>(c);
            else if (c < 0x800) out += static_cast<char>(0xC0 | (c >> 6)), out += static_cast<char>(0x80 | (c & 0x3F));
            else if (c < 0x10000)
                out += static_cast<char>(0xE0 | (c >> 12)), out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)), out += static_cast<char>(0x80 | (c & 0x3F));
            else
                out += static_cast<char>(0xF0 | (c >> 18)), out += static_cast<char>(0x80 | ((c >> 12) & 0x3F)),
                    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)), out += static_cast<char>(0x80 | (c & 0x3F));
        }
        return out;
    }
};

bool WriteFile(const fs::path& file, const void* data, size_t size) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary);
    if (!out) return false;
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
}

// Que una ruta dentro del archivo no se salga de la carpeta («../../algo»).
bool SafeName(const std::string& n) {
    if (n.empty() || n[0] == '/' || n[0] == '\\' || n.find(':') != std::string::npos) return false;
    size_t pos = 0;
    while ((pos = n.find("..", pos)) != std::string::npos) {
        const bool start = pos == 0 || n[pos - 1] == '/' || n[pos - 1] == '\\';
        const bool end = pos + 2 >= n.size() || n[pos + 2] == '/' || n[pos + 2] == '\\';
        if (start && end) return false;
        pos += 2;
    }
    return true;
}
} // namespace

bool IsArchive(const std::string& ext) { return ext == "zip" || ext == "7z"; }

bool List(const fs::path& archive, std::vector<Entry>& out) {
    out.clear();
    const std::string ext = Ext(archive);
    if (ext == "zip") {
        Zip z(archive);
        if (!z.ok) return false;
        const mz_uint n = mz_zip_reader_get_num_files(&z.z);
        for (mz_uint i = 0; i < n; ++i) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&z.z, i, &st) || st.m_is_directory) continue;
            out.push_back({st.m_filename, st.m_uncomp_size, i});
        }
        return true;
    }
    if (ext == "7z") {
        SevenZip s(archive);
        if (!s.ok) return false;
        for (UInt32 i = 0; i < s.db.NumFiles; ++i) {
            if (SzArEx_IsDir(&s.db, i)) continue;
            out.push_back({s.Name(i), SzArEx_GetFileSize(&s.db, i), i});
        }
        return true;
    }
    return false;
}

bool ExtractToMemory(const fs::path& archive, const std::string& name, std::vector<uint8_t>& out) {
    const std::string ext = Ext(archive);
    if (ext == "zip") {
        Zip z(archive);
        if (!z.ok) return false;
        const int idx = mz_zip_reader_locate_file(&z.z, name.c_str(), nullptr, 0);
        if (idx < 0) return false;
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z.z, static_cast<mz_uint>(idx), &st)) return false;
        out.resize(static_cast<size_t>(st.m_uncomp_size));
        return mz_zip_reader_extract_to_mem(&z.z, static_cast<mz_uint>(idx), out.data(), out.size(), 0);
    }
    if (ext == "7z") {
        SevenZip s(archive);
        if (!s.ok) return false;
        for (UInt32 i = 0; i < s.db.NumFiles; ++i) {
            if (SzArEx_IsDir(&s.db, i) || s.Name(i) != name) continue;
            UInt32 block = 0xFFFFFFFF;
            Byte* buf = nullptr;
            size_t buf_size = 0, offset = 0, processed = 0;
            const SRes r = SzArEx_Extract(&s.db, &s.look.vt, i, &block, &buf, &buf_size, &offset, &processed, &s.alloc, &s.temp);
            if (r == SZ_OK) out.assign(buf + offset, buf + offset + processed);
            ISzAlloc_Free(&s.alloc, buf);
            return r == SZ_OK;
        }
    }
    return false;
}

bool ExtractAll(const fs::path& archive, const fs::path& dir, const std::function<bool(float)>& progress) {
    const std::string ext = Ext(archive);
    if (ext == "zip") {
        Zip z(archive);
        if (!z.ok) return false;
        const mz_uint n = mz_zip_reader_get_num_files(&z.z);
        // El avance se mide en bytes (un disco suele ser un .cue chiquito y un .bin enorme).
        uint64_t total = 0;
        for (mz_uint i = 0; i < n; ++i) {
            mz_zip_archive_file_stat st;
            if (mz_zip_reader_file_stat(&z.z, i, &st) && !st.m_is_directory) total += st.m_uncomp_size;
        }
        struct Sink {
            FILE* f;
            uint64_t done, total;
            const std::function<bool(float)>* progress;
            bool cancelled;
        };
        Sink sink{nullptr, 0, std::max<uint64_t>(1, total), &progress, false};
        for (mz_uint i = 0; i < n; ++i) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&z.z, i, &st) || st.m_is_directory || !SafeName(st.m_filename)) continue;
            const fs::path target = dir / Paths::FromUtf8(st.m_filename);
            std::error_code ec;
            fs::create_directories(target.parent_path(), ec);
#ifdef _WIN32
            sink.f = _wfopen(target.c_str(), L"wb");
#else
            sink.f = std::fopen(target.c_str(), "wb");
#endif
            if (!sink.f) return false;
            const bool ok = mz_zip_reader_extract_to_callback(
                &z.z, i,
                [](void* opaque, mz_uint64, const void* buf, size_t len) -> size_t {
                    auto* k = static_cast<Sink*>(opaque);
                    if (std::fwrite(buf, 1, len, k->f) != len) return 0;
                    const uint64_t before = k->done;
                    k->done += len;
                    // Se avisa más o menos cada 4 MB para no gastar tiempo en eso.
                    if (*k->progress && (before >> 22) != (k->done >> 22) && !(*k->progress)(static_cast<float>(k->done) / k->total)) {
                        k->cancelled = true;
                        return 0;
                    }
                    return len;
                },
                &sink, 0);
            std::fclose(sink.f);
            if (!ok || sink.cancelled) return false;
        }
        if (progress) progress(1.0f);
        return true;
    }
    if (ext == "7z") {
        SevenZip s(archive);
        if (!s.ok) return false;
        UInt32 block = 0xFFFFFFFF;
        Byte* buf = nullptr;
        size_t buf_size = 0;
        bool ok = true;
        for (UInt32 i = 0; i < s.db.NumFiles && ok; ++i) {
            if (SzArEx_IsDir(&s.db, i)) continue;
            const std::string name = s.Name(i);
            if (!SafeName(name)) continue;
            size_t offset = 0, processed = 0;
            // El mismo bloque sirve para varios archivos seguidos: no se descomprime dos veces.
            if (SzArEx_Extract(&s.db, &s.look.vt, i, &block, &buf, &buf_size, &offset, &processed, &s.alloc, &s.temp) != SZ_OK) {
                ok = false;
                break;
            }
            ok = WriteFile(dir / Paths::FromUtf8(name), buf + offset, processed);
            if (progress && !progress(static_cast<float>(i + 1) / s.db.NumFiles)) ok = false;
        }
        ISzAlloc_Free(&s.alloc, buf);
        return ok;
    }
    return false;
}

} // namespace Satronho::Archive
