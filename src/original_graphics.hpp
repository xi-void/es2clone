#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace siege::original {
using Bytes = std::vector<uint8_t>;
using Palette = std::array<std::array<uint8_t, 3>, 256>;
struct Bitmap {
    uint16_t width=0, height=0;
    Bytes indices;
    Bytes rgba(const Palette& palette, bool transparentZero=false) const;
};

// Verified ES2 8-bit DBM and DBA (including HB0/HBA), not Windows BMP.
// Throws on unsupported variants, inconsistent lengths or excessive allocations.
Bytes readFile(const std::filesystem::path& path);
Palette decodePalette(const Bytes& bytes);
std::vector<Bitmap> decodeBitmaps(const Bytes& bytes);
}
