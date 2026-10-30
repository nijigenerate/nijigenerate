#pragma once

#include <cstdint>

namespace nijigenerate::io::psd { struct Layer; }

namespace nijigenerate::io::psdbudget {

inline constexpr std::uint64_t PsdMaskExtractionBytesPerPixel = 8;
bool ngReservePsdLayerExtraction(psd::Layer& layer, std::uint64_t layerBytesPerPixel,
    std::uint64_t maxBytes, std::uint64_t& retainedBytes);

// Internal helpers shared with the layer extraction entry point.
namespace detail {
bool reserveBytes(std::uint64_t requiredBytes, std::uint64_t maxBytes, std::uint64_t& reservedBytes);
bool reserveRectangle(std::int64_t width, std::int64_t height, std::uint64_t bytesPerPixel,
    std::uint64_t maxBytes, std::uint64_t& reservedBytes, bool reserveFeatherLine = false);
}

}
