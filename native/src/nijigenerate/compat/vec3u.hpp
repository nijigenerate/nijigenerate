#pragma once

#include <cstdint>

namespace nijigenerate::compat {

// Value lanes of independent inmath Vector!(uint, 3).
struct Vec3u {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t z{};
    bool operator==(const Vec3u&) const = default;
};

}
