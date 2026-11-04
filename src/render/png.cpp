#include "render/png.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace osu {

namespace {

uint32_t crc_table[256];
bool crc_ready = false;

void init_crc() {
    if (crc_ready) return;
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[n] = c;
    }
    crc_ready = true;
}

uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    init_crc();
    for (size_t i = 0; i < len; ++i) crc = crc_table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc;
}

uint32_t adler32(const uint8_t* data, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void put_u32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

void put_chunk(std::vector<uint8_t>& out, const char type[4], const std::vector<uint8_t>& data) {
    put_u32(out, static_cast<uint32_t>(data.size()));
    std::vector<uint8_t> body;
    body.reserve(4 + data.size());
    body.insert(body.end(), type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    out.insert(out.end(), body.begin(), body.end());
    put_u32(out, crc32(body.data(), body.size()) ^ 0xFFFFFFFFu);
}

std::vector<uint8_t> deflate_stored(const uint8_t* data, size_t len) {
    std::vector<uint8_t> out;
    size_t offset = 0;
    while (offset < len || len == 0) {
        const size_t chunk = std::min<size_t>(65535, len - offset);
        const bool last = offset + chunk >= len;
        out.push_back(last ? 0x01 : 0x00);
        const uint16_t n = static_cast<uint16_t>(chunk);
        out.push_back(static_cast<uint8_t>(n & 0xFF));
        out.push_back(static_cast<uint8_t>(n >> 8));
        out.push_back(static_cast<uint8_t>(~n & 0xFF));
        out.push_back(static_cast<uint8_t>((~n >> 8) & 0xFF));
        out.insert(out.end(), data + offset, data + offset + chunk);
        offset += chunk;
        if (len == 0) break;
    }
    return out;
}

}  // namespace

bool write_png_rgba(const char* path, const uint8_t* rgba, int width, int height) {
    if (!rgba || width <= 0 || height <= 0) return false;

    // Raw scanlines with filter byte 0.
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(height) * (static_cast<size_t>(width) * 4u + 1u));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const uint8_t* row = rgba + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u;
        raw.insert(raw.end(), row, row + static_cast<size_t>(width) * 4u);
    }

    std::vector<uint8_t> zlib;
    zlib.push_back(0x78);  // CMF: deflate, 32K window
    zlib.push_back(0x01);  // FLG: no dictionary, fastest
    const std::vector<uint8_t> deflated = deflate_stored(raw.data(), raw.size());
    zlib.insert(zlib.end(), deflated.begin(), deflated.end());
    put_u32(zlib, adler32(raw.data(), raw.size()));

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

    std::vector<uint8_t> ihdr;
    put_u32(ihdr, static_cast<uint32_t>(width));
    put_u32(ihdr, static_cast<uint32_t>(height));
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(6);  // colour type: RGBA
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filter
    ihdr.push_back(0);  // interlace
    put_chunk(png, "IHDR", ihdr);
    put_chunk(png, "IDAT", zlib);
    put_chunk(png, "IEND", {});

    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const size_t written = std::fwrite(png.data(), 1, png.size(), f);
    std::fclose(f);
    return written == png.size();
}

}  // namespace osu
