// Copyright 2021, Inochi2D Project. BSD-2-Clause: native/port/licenses/psd-d.txt.
#include "nijigenerate/io/psd/parser.hpp"
#include "nijigenerate/io/psd/io.hpp"
#include "nijigenerate/io/psd/rle.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace nijigenerate::io::psd {
using namespace utils;
namespace {
template<class Message> void enforce(bool condition, Message&& message) {
    if (condition) return;
    if constexpr (std::is_array_v<std::remove_reference_t<Message>>)
        throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral(message));
    else
        throw compat::ThrowableException(compat::RetainedString(std::forward<Message>(message)()));
}
template<class T> std::span<const T> view(const compat::Slice<T>& values) {
    return {values.data(), values.size()};
}
template<class T> std::span<T> mutableView(compat::Slice<T>& values) {
    return {values.data(), values.size()};
}
template<class T> compat::Slice<T> allocate(std::size_t length) {
    if (length == 0) return {};
    return compat::Slice<T>(std::vector<T>(length));
}
std::uint8_t applyMask(std::uint8_t alpha, std::uint8_t mask) {
    return static_cast<std::uint8_t>((std::uint32_t{alpha} * mask + 127u) / 255u);
}
compat::Slice<std::uint8_t> channelData(Layer* layer, ChannelType type) {
    const auto index = findChannel(layer, type);
    return index == static_cast<std::uint32_t>(ChannelType::INVALID) ? compat::Slice<std::uint8_t>{}
        : layer->channels[index].data;
}
}

bool validPsdImageDimensions(std::int64_t width, std::int64_t height) {
    if (width < 0 || height < 0) return false;
    const auto w = static_cast<std::uint64_t>(width), h = static_cast<std::uint64_t>(height);
    return w == 0 || h == 0 || w <= MaxPsdDecodedPixels / h;
}
bool decodePsdLayerCount(std::int16_t encodedCount, std::uint32_t layerLength,
    std::uint32_t& layerCount, bool& hasTransparencyMask) {
    layerCount = 0;
    hasTransparencyMask = false;
    auto count = static_cast<std::int64_t>(encodedCount);
    hasTransparencyMask = count < 0;
    if (hasTransparencyMask) count = -count;
    if (layerLength < sizeof(std::int16_t) ||
        static_cast<std::uint64_t>(count) > (static_cast<std::uint64_t>(layerLength) - 2) / 34) return false;
    layerCount = static_cast<std::uint32_t>(count);
    return true;
}
bool decodedPsdLayerChannel(std::int16_t type) { return type >= -3 && type <= 2; }
bool reservePsdDecodedLayerChannel(std::uint64_t length, std::uint32_t& channels, std::uint64_t& bytes) {
    if (channels >= MaxPsdDecodedLayerChannels || bytes > MaxPsdDecodedLayerChannelBytes ||
        length > MaxPsdDecodedLayerChannelBytes - bytes) return false;
    ++channels;
    bytes += length;
    return true;
}
bool validPsdEncodedChannelLength(std::uint64_t encoded, std::uint64_t decoded, std::uint64_t rowTable) {
    if (encoded > MaxPsdEncodedLayerChannelBytes || decoded > MaxPsdEncodedLayerChannelBytes ||
        rowTable > MaxPsdEncodedLayerChannelBytes - decoded) return false;
    auto base = decoded + rowTable;
    const auto proportional = decoded / 16;
    if (proportional > MaxPsdEncodedLayerChannelBytes - base) return false;
    base += proportional;
    const auto supported = base > MaxPsdEncodedLayerChannelBytes - PsdEncodedLayerChannelFixedOverhead
        ? MaxPsdEncodedLayerChannelBytes : base + PsdEncodedLayerChannelFixedOverhead;
    return encoded <= supported;
}
bool validPsdChannelPayloadRange(std::uint64_t offset, std::uint64_t length, std::uint64_t fileLength) {
    return offset <= fileLength && length <= fileLength - offset;
}
bool validPsdUnicodeLayerNameLength(std::uint32_t blockLength, std::uint32_t count) {
    return blockLength >= 4 && count <= (blockLength - 4) / 2;
}
std::uint32_t findChannel(Layer* layer, ChannelType type) {
    for (std::uint32_t i = 0; i < layer->channels.size(); ++i) {
        const auto& channel = layer->channels[i];
        if (channel.dataLength > 0 && channel.type == static_cast<std::int32_t>(type)) return i;
    }
    return static_cast<std::uint32_t>(ChannelType::INVALID);
}
std::uint8_t applyMaskSettings(std::uint8_t mask, std::uint8_t density, bool disabled, bool invert) {
    if (disabled) return 255;
    if (invert) mask = static_cast<std::uint8_t>(255 - mask);
    return static_cast<std::uint8_t>(255u - density + (std::uint32_t{mask} * density + 127u) / 255u);
}
namespace {
compat::Slice<std::uint8_t> boxBlurMask(std::span<const std::uint8_t> data,
    std::int32_t width, std::int32_t height, std::int32_t radius, bool horizontal, std::uint8_t defaultColor) {
    auto result = allocate<std::uint8_t>(data.size());
    const auto lineLength = horizontal ? width : height;
    const auto lineCount = horizontal ? height : width;
    const auto windowSize = static_cast<std::uint64_t>(radius) * 2 + 1;
    std::vector<std::uint64_t> prefix(static_cast<std::size_t>(lineLength) + 1);
    for (std::int32_t line = 0; line < lineCount; ++line) {
        std::fill(prefix.begin(), prefix.end(), 0);
        for (std::int32_t position = 0; position < lineLength; ++position) {
            const auto index = horizontal ? static_cast<std::size_t>(line) * width + position
                : static_cast<std::size_t>(position) * width + line;
            prefix[static_cast<std::size_t>(position) + 1] = prefix[position] + data[index];
        }
        for (std::int32_t position = 0; position < lineLength; ++position) {
            const auto start = static_cast<std::int64_t>(position) - radius;
            const auto end = static_cast<std::int64_t>(position) + radius + 1;
            const auto insideStart = static_cast<std::int32_t>(start < 0 ? 0 : start > lineLength ? lineLength : start);
            const auto insideEnd = static_cast<std::int32_t>(end < 0 ? 0 : end > lineLength ? lineLength : end);
            const auto insideCount = static_cast<std::uint64_t>(insideEnd - insideStart);
            const auto sum = prefix[insideEnd] - prefix[insideStart] + (windowSize - insideCount) * defaultColor;
            const auto index = horizontal ? static_cast<std::size_t>(line) * width + position
                : static_cast<std::size_t>(position) * width + line;
            result[index] = static_cast<std::uint8_t>((sum + windowSize / 2) / windowSize);
        }
    }
    return result;
}
struct MaskGeometry {
    std::int32_t top{}, left{}, bottom{}, right{};
    std::uint8_t defaultColor{255};
    bool valid{};
};
template<class T> MaskGeometry maskGeometry(const T& mask) {
    return {mask.top, mask.left, mask.bottom, mask.right, mask.defaultColor, true};
}
MaskGeometry channelMaskGeometry(Layer& layer, std::int16_t type) {
    if (type == -3) return layer.layerMask.empty() ? MaskGeometry{} : maskGeometry(layer.layerMask[0]);
    if (type != -2) return {};
    if (!layer.vectorMask.empty()) return maskGeometry(layer.vectorMask[0]);
    if (!layer.layerMask.empty()) return maskGeometry(layer.layerMask[0]);
    return {};
}
double channelMaskFeather(Layer& layer, std::int16_t type) {
    if (type == -3 && !layer.layerMask.empty()) return layer.layerMask[0].feather;
    if (type == -2) {
        if (!layer.vectorMask.empty()) return layer.vectorMask[0].feather;
        if (!layer.layerMask.empty()) return layer.layerMask[0].feather;
    }
    return 0.0;
}
compat::Slice<std::uint8_t> featheredMaskData(Layer& layer, std::int16_t type, compat::Slice<std::uint8_t> data) {
    const auto mask = channelMaskGeometry(layer, type);
    if (!mask.valid || data.empty()) return data;
    const auto width = static_cast<std::int64_t>(mask.right) - mask.left;
    const auto height = static_cast<std::int64_t>(mask.bottom) - mask.top;
    enforce(validPsdImageDimensions(width, height), "Invalid or excessively large PSD mask bounds");
    return applyMaskFeather(view(data), static_cast<std::int32_t>(width), static_cast<std::int32_t>(height),
        channelMaskFeather(layer, type), mask.defaultColor);
}
void channelDimensions(Layer& layer, std::int16_t type, std::uint32_t& width, std::uint32_t& height) {
    width = 0; height = 0;
    width = layer.width(); height = layer.height();
    const auto mask = channelMaskGeometry(layer, type);
    if (mask.valid) {
        const auto w = static_cast<std::int64_t>(mask.right) - mask.left;
        const auto h = static_cast<std::int64_t>(mask.bottom) - mask.top;
        enforce(validPsdImageDimensions(w, h), "Invalid or excessively large PSD mask bounds");
        if (w > 0 && h > 0) { width = static_cast<std::uint32_t>(w); height = static_cast<std::uint32_t>(h); }
    }
}
}
compat::Slice<std::uint8_t> applyMaskFeather(std::span<const std::uint8_t> data,
    std::int32_t width, std::int32_t height, double feather, std::uint8_t defaultColor) {
    auto result = data.empty() ? compat::Slice<std::uint8_t>{} : compat::Slice<std::uint8_t>(data);
    if (width <= 0 || height <= 0 || data.size() != static_cast<std::size_t>(width) * height ||
        !std::isfinite(feather) || feather <= 0.0) return result;
    const auto radiusValue = std::round((std::sqrt(4.0 * feather * feather + 1.0) - 1.0) / 2.0);
    const auto radius = radiusValue < 1.0 ? 1 : radiusValue > 1000000.0 ? 1000000
        : static_cast<std::int32_t>(radiusValue);
    for (int pass = 0; pass < 3; ++pass) {
        result = boxBlurMask(view(result), width, height, radius, true, defaultColor);
        result = boxBlurMask(view(result), width, height, radius, false, defaultColor);
    }
    return result;
}
std::uint8_t sampleMaskAt(Layer& layer, std::int16_t type,
    std::span<const std::uint8_t> data, std::size_t pixelIndex) {
    if (data.empty()) return 255;
    const auto mask = channelMaskGeometry(layer, type);
    if (!mask.valid) return 255;
    bool disabled{}, invert{}, relative{};
    std::uint8_t density{255};
    const auto settings = [&](const auto& m) {
        disabled = m.disabled; invert = m.invert; relative = m.positionRelativeToLayer; density = m.density;
    };
    if (type == -3 && !layer.layerMask.empty()) settings(layer.layerMask[0]);
    else if (type == -2) {
        if (!layer.vectorMask.empty()) settings(layer.vectorMask[0]);
        else if (!layer.layerMask.empty()) settings(layer.layerMask[0]);
    }
    const auto width = static_cast<std::int64_t>(mask.right) - mask.left;
    const auto height = static_cast<std::int64_t>(mask.bottom) - mask.top;
    if (!validPsdImageDimensions(width, height)) return 255;
    auto value = mask.defaultColor;
    if (width <= 0 || height <= 0) return applyMaskSettings(value, density, disabled, invert);
    const auto x = static_cast<std::int64_t>(pixelIndex % layer.width()) + (relative ? 0 : layer.left) - mask.left;
    const auto y = static_cast<std::int64_t>(pixelIndex / layer.width()) + (relative ? 0 : layer.top) - mask.top;
    if (x >= 0 && y >= 0 && x < width && y < height) {
        const auto index = static_cast<std::size_t>(y) * width + x;
        if (index < data.size()) value = data[index];
    }
    return applyMaskSettings(value, density, disabled, invert);
}

void extractLayer(Layer& layer) {
    auto file = layer.filePtr;
    if (layer.width() == 0 && layer.height() == 0) return;
    std::uint32_t decodedChannels{};
    std::uint64_t decodedBytes{};
    for (auto& channel : layer.channels) {
        channel.data.clear();
        if (!decodedPsdLayerChannel(channel.type)) continue;
        file.seek(channel.fileOffset);
        std::uint32_t width{}, height{};
        channelDimensions(layer, channel.type, width, height);
        const auto compression = readValue<std::uint16_t>(file);
        enforce(channel.dataLength >= 2, "Invalid PSD channel length: missing compression header");
        const auto encodedLength = static_cast<std::size_t>(channel.dataLength) - 2;
        const auto decodedLength = static_cast<std::size_t>(width) * height;
        enforce(reservePsdDecodedLayerChannel(decodedLength, decodedChannels, decodedBytes),
            "PSD layer decoded channel data exceeds the supported memory budget");
        const auto rowTableBudget = compression == 1 ? static_cast<std::uint64_t>(height) * 2 : 0;
        enforce(validPsdEncodedChannelLength(encodedLength, decodedLength, rowTableBudget),
            "PSD layer encoded channel data exceeds the supported memory budget");
        enforce(validPsdChannelPayloadRange(file.tell(), encodedLength, file.size()), "Truncated PSD channel payload");
        switch (compression) {
        case 0:
            enforce(encodedLength >= decodedLength, "Truncated raw PSD channel data");
            channel.data = allocate<std::uint8_t>(decodedLength);
            file.rawRead(mutableView(channel.data));
            break;
        case 1: {
            std::size_t rleDataSize{};
            std::vector<std::uint16_t> rowSizes(height);
            for (auto& size : rowSizes) { size = readValue<std::uint16_t>(file); rleDataSize += size; }
            const auto tableLength = static_cast<std::size_t>(height) * 2;
            enforce(tableLength <= encodedLength && rleDataSize <= encodedLength - tableLength,
                "Truncated PSD RLE channel data");
            if (rleDataSize > 0) {
                auto compressed = allocate<std::uint8_t>(rleDataSize);
                file.rawRead(mutableView(compressed));
                channel.data = allocate<std::uint8_t>(decodedLength);
                std::size_t encodedOffset{};
                for (std::size_t row = 0; row < rowSizes.size(); ++row) {
                    decodeRLE(view(compressed).subspan(encodedOffset, rowSizes[row]),
                        mutableView(channel.data).subspan(row * width, width));
                    encodedOffset += rowSizes[row];
                }
            } else enforce(decodedLength == 0, "Truncated PSD RLE channel data");
            break;
        }
        case 2: case 3: {
            auto compressed = allocate<std::uint8_t>(encodedLength);
            file.rawRead(mutableView(compressed));
            channel.data = decodeZip(view(compressed), width, height, compression == 3);
            break;
        }
        default: enforce(false, [&] {
            return "Unsupported PSD channel compression type: " + std::to_string(compression);
        });
        }
    }
    auto rgba = allocate<std::uint8_t>(layer.width() * layer.height() * std::uint32_t{4});
    const auto r = channelData(&layer, ChannelType::R), g = channelData(&layer, ChannelType::G);
    const auto b = channelData(&layer, ChannelType::B), alpha = channelData(&layer, ChannelType::TRANSPARENCY_MASK);
    const auto vectorMask = featheredMaskData(layer, -2, channelData(&layer, ChannelType::LAYER_OR_VECTOR_MASK));
    const auto layerMask = featheredMaskData(layer, -3, channelData(&layer, ChannelType::LAYER_MASK));
    const auto count = static_cast<std::size_t>(layer.width()) * layer.height();
    for (std::size_t i = 0, j = 0; i < count; ++i, j += 4) {
        rgba[j] = r.size() > i ? r[i] : 0;
        rgba[j + 1] = g.size() > i ? g[i] : 0;
        rgba[j + 2] = b.size() > i ? b[i] : 0;
        std::uint8_t a = alpha.size() > i ? alpha[i] : 255;
        if (!vectorMask.empty()) a = applyMask(a, sampleMaskAt(layer, -2, view(vectorMask), i));
        if (!layerMask.empty()) a = applyMask(a, sampleMaskAt(layer, -3, view(layerMask), i));
        rgba[j + 3] = a;
    }
    layer.data = rgba;
}
} // namespace nijigenerate::io::psd
