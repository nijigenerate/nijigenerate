#pragma once
// Copyright 2021, Inochi2D Project. BSD-2-Clause: native/port/licenses/psd-d.txt.
#include "nijigenerate/compat/slice.hpp"
#include <cstdint>
#include <span>
namespace nijigenerate::io::psd {
void decodeRLE(std::span<const std::uint8_t> source, std::span<std::uint8_t> destination);
compat::Slice<std::uint8_t> decodeZip(std::span<const std::uint8_t> source,
    std::uint32_t width, std::uint32_t height, bool prediction);
}
