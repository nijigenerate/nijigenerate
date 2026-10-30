#pragma once

#include <cstdint>

namespace nijigenerate::compat {
struct Vec2u {
    std::uint32_t x{};
    std::uint32_t y{};
    bool operator==(const Vec2u&) const = default;
};
}
