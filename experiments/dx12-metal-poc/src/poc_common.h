// Shared helpers for the offscreen PoC samples (extracted per code-review #1).
// CPU-side only (no Metal impl macros here — those live in each sample's .cpp).
#pragma once

#include <Metal/Metal.hpp>

#include <cstdint>
#include <cstdio>
#include <vector>

namespace poc {

// Read an RGBA8 texture back into a CPU buffer (w*h*4 bytes).
inline std::vector<uint8_t> read_rgba8(MTL::Texture* tex, uint32_t w, uint32_t h) {
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4);
    tex->getBytes(px.data(), w * 4, MTL::Region::Make2D(0, 0, w, h), 0);
    return px;
}

// Write RGBA8 pixels as a binary PPM (P6, RGB — drops alpha). Returns false on open failure.
inline bool write_ppm(const char* path, const std::vector<uint8_t>& px, uint32_t w, uint32_t h) {
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) return false;
    std::fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (uint32_t i = 0; i < w * h; ++i) std::fwrite(&px[i * 4], 1, 3, f);
    std::fclose(f);
    return true;
}

} // namespace poc
