#include "psdbudget.hpp"
#include "psd/layer.hpp"

#include <limits>

namespace nijigenerate::io::psdbudget::detail {

bool reserveBytes(std::uint64_t requiredBytes, std::uint64_t maxBytes, std::uint64_t& reservedBytes) {
    if (reservedBytes > maxBytes || requiredBytes > maxBytes - reservedBytes) return false;
    reservedBytes += requiredBytes;
    return true;
}

} // namespace nijigenerate::io::psdbudget::detail

namespace nijigenerate::io::psdbudget {
bool ngReservePsdLayerExtraction(psd::Layer& layer, std::uint64_t layerBytesPerPixel,
    std::uint64_t maxBytes, std::uint64_t& retainedBytes) {
    auto proposedBytes = retainedBytes;
    if (!detail::reserveRectangle(layer.width(), layer.height(), layerBytesPerPixel, maxBytes, proposedBytes))
        return false;
    constexpr auto layerMaskKind = static_cast<std::int16_t>(psd::ChannelType::LAYER_MASK);
    constexpr auto vectorMaskKind = static_cast<std::int16_t>(psd::ChannelType::LAYER_OR_VECTOR_MASK);
    for (auto& channel : layer.channels) {
        if (channel.type != vectorMaskKind && channel.type != layerMaskKind) continue;
        std::int64_t maskWidth = layer.width(), maskHeight = layer.height();
        if (channel.type == layerMaskKind && layer.layerMask.size() > 0) {
            maskWidth = static_cast<std::int64_t>(layer.layerMask[0].right) - layer.layerMask[0].left;
            maskHeight = static_cast<std::int64_t>(layer.layerMask[0].bottom) - layer.layerMask[0].top;
        } else if (channel.type == vectorMaskKind) {
            if (layer.vectorMask.size() > 0) {
                maskWidth = static_cast<std::int64_t>(layer.vectorMask[0].right) - layer.vectorMask[0].left;
                maskHeight = static_cast<std::int64_t>(layer.vectorMask[0].bottom) - layer.vectorMask[0].top;
            } else if (layer.layerMask.size() > 0) {
                maskWidth = static_cast<std::int64_t>(layer.layerMask[0].right) - layer.layerMask[0].left;
                maskHeight = static_cast<std::int64_t>(layer.layerMask[0].bottom) - layer.layerMask[0].top;
            }
        }
        if (maskWidth < 0 || maskHeight < 0) return false;
        if (maskWidth == 0 || maskHeight == 0) {
            maskWidth = layer.width(); maskHeight = layer.height();
        }
        if (!detail::reserveRectangle(maskWidth, maskHeight, PsdMaskExtractionBytesPerPixel,
                maxBytes, proposedBytes, true)) return false;
    }
    retainedBytes = proposedBytes;
    return true;
}
}

namespace nijigenerate::io::psdbudget::detail {

bool reserveRectangle(std::int64_t width, std::int64_t height, std::uint64_t bytesPerPixel,
    std::uint64_t maxBytes, std::uint64_t& reservedBytes, bool reserveFeatherLine) {
    constexpr auto max = std::numeric_limits<std::uint64_t>::max();
    if (width < 0 || height < 0 || bytesPerPixel == 0) return false;
    if (width == 0 || height == 0) return true;
    const auto unsignedWidth = static_cast<std::uint64_t>(width);
    const auto unsignedHeight = static_cast<std::uint64_t>(height);
    if (unsignedWidth > max / unsignedHeight) return false;
    const auto pixelCount = unsignedWidth * unsignedHeight;
    if (pixelCount > max / bytesPerPixel) return false;
    if (!reserveBytes(pixelCount * bytesPerPixel, maxBytes, reservedBytes)) return false;
    if (!reserveFeatherLine) return true;

    // Preserve the D helper's partial reservation when feather allocation fails.
    const auto lineLength = unsignedWidth > unsignedHeight ? unsignedWidth : unsignedHeight;
    if (lineLength == max || lineLength + 1 > max / sizeof(std::uint64_t)) return false;
    const auto lineBytes = (lineLength + 1) * sizeof(std::uint64_t);
    if (lineBytes > max / 6) return false;
    return reserveBytes(lineBytes * 6, maxBytes, reservedBytes);
}

}
