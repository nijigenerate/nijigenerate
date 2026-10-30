// Copyright 2021, Inochi2D Project. BSD-2-Clause: native/port/licenses/psd-d.txt.
#include "nijigenerate/io/psd/parser.hpp"
#include "nijigenerate/io/psd/io.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include <algorithm>
#include <bit>
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
template<class T> compat::Slice<T> allocate(std::size_t size) {
    if (size == 0) return {};
    return compat::Slice<T>(std::vector<T>(size));
}
template<Integral T> T readPaddedValue(RetainedFile& file, T multipleOf = 2, T addTo = 0) {
    const T value = readValue<T>(file);
    using U = std::conditional_t<(sizeof(T) < 4), std::uint32_t, std::make_unsigned_t<T>>;
    return static_cast<T>(roundUpToMultiple(static_cast<U>(value) + static_cast<U>(addTo), static_cast<U>(multipleOf)));
}
void parseHeader(RetainedFile& file, PSD& document) {
    enforce(readStr(file, 4).view() == "8BPS", "Invalid file, must be a Photoshop PSD file. PSB's are not supported!");
    enforce(readValue<std::uint16_t>(file) == 1, "Version does not match 1.");
    const auto reserved = read(file, 6);
    enforce(reserved.size() == 6 && std::all_of(reserved.begin(), reserved.end(), [](auto v) { return v == 0; }),
        "Unexpected reserve bytes, file may be corrupted.");
    document.channels = std::bit_cast<std::int16_t>(readValue<std::uint16_t>(file));
    const auto height = readValue<std::uint32_t>(file), width = readValue<std::uint32_t>(file);
    enforce(height <= INT32_MAX && width <= INT32_MAX && height > 0 && width > 0 && validPsdImageDimensions(width, height),
        "PSD document dimensions exceed the supported decoded image size");
    document.height = static_cast<std::int32_t>(height); document.width = static_cast<std::int32_t>(width);
    document.bitsPerChannel = readValue<std::uint16_t>(file);
    enforce(document.bitsPerChannel == 8, [&] {
        return "Only 8-bit PSD channels are supported (found " +
            std::to_string(document.bitsPerChannel) + "-bit)";
    });
    document.colorMode = static_cast<ColorMode>(readValue<std::uint16_t>(file));
    const char* colorNames[] = {"Bitmap", "Grayscale", "Indexed", "RGB", "CMYK", "Multichannel", "Duotone", "Lab"};
    const auto color = static_cast<std::uint16_t>(document.colorMode);
    enforce(document.colorMode == ColorMode::RGB, [&] {
        return "Only RGB PSD documents are supported (found color mode " +
            (color < 8 ? std::string(colorNames[color]) : std::to_string(color)) + ")";
    });
}
void parseColorModeSection(RetainedFile& file, PSD& document) {
    document.colorModeDataSectionLength = readValue<std::uint32_t>(file);
    document.colorModeDataSectionOffset = file.tell();
    skip(file, document.colorModeDataSectionLength);
}

// Present in the original module even though parseDocument intentionally skips this parser.
[[maybe_unused]] void parseImageResourceSection(RetainedFile& file, PSD& document) {
    document.imageResourceSectionLength = readValue<std::uint32_t>(file);
    document.imageResourceSectionOffset = file.tell();
    std::uint64_t leftToRead = document.imageResourceSectionLength;
    auto& resources = document.imageResourcesData;
    const auto allocateChannels = [&] {
        if (resources.alphaChannels.empty()) {
            const auto count = static_cast<std::uint32_t>(static_cast<std::int32_t>(document.channels) - 3);
            resources.alphaChannelCount = count;
            resources.alphaChannels.resize(count);
        }
    };
    while (leftToRead > 0) {
        const auto signature = readStr(file, 4);
        enforce(signature.view() == "8BIM" || signature.view() == "psdM",
            "Image resources section seems to be corrupt, signature does not match \"8BIM\" nor \"psdM\".");
        const auto id = static_cast<ImageResourceType>(readValue<std::uint16_t>(file));
        const auto nameLength = readPaddedValue<std::uint8_t>(file, 2, 1);
        [[maybe_unused]] const auto name = readStr(file, static_cast<std::uint32_t>(nameLength - 1));
        const auto size = readPaddedValue<std::uint32_t>(file);
        switch (id) {
        case ImageResourceType::DISPLAY_INFO:
            allocateChannels();
            (void)readValue<std::uint32_t>(file);
            for (std::uint32_t i = 0; i < resources.alphaChannelCount; ++i) {
                auto& channel = resources.alphaChannels[i];
                channel.colorSpace = readValue<std::uint16_t>(file);
                for (auto& component : channel.color) component = readValue<std::uint16_t>(file);
                channel.opacity = readValue<std::uint16_t>(file);
                channel.mode = static_cast<AlphaChannel::Mode>(readValue<std::uint8_t>(file));
            }
            break;
        case ImageResourceType::VERSION_INFO:
            (void)readValue<std::uint32_t>(file);
            resources.containsRealMergedData = readValue<std::uint8_t>(file) != 0;
            skip(file, size - 5u);
            break;
        case ImageResourceType::THUMBNAIL_RESOURCE: {
            (void)readValue<std::uint32_t>(file);
            const auto width = readValue<std::uint32_t>(file), height = readValue<std::uint32_t>(file);
            (void)readValue<std::uint32_t>(file); (void)readValue<std::uint32_t>(file);
            const auto jpegSize = readValue<std::uint32_t>(file);
            (void)readValue<std::uint16_t>(file); (void)readValue<std::uint16_t>(file);
            resources.thumbnail.width = width; resources.thumbnail.height = height;
            resources.thumbnail.binaryJpegSize = jpegSize;
            resources.thumbnail.binaryJpeg = read(file, jpegSize);
            skip(file, size - 28u - jpegSize);
            break;
        }
        case ImageResourceType::XMP_METADATA:
            enforce(!resources.xmpMetadata.empty(), "File contains more than one XMP metadata resource.");
            resources.xmpMetadata = read(file, size);
            break;
        case ImageResourceType::ICC_PROFILE:
            enforce(!resources.iccProfile.empty(), "File contains more than one ICC profile.");
            resources.iccProfile = read(file, size); resources.sizeOfICCProfile = size;
            break;
        case ImageResourceType::EXIF_DATA:
            enforce(!resources.exifData.empty(), "File contains more than one EXIF data block.");
            resources.exifData = read(file, size); resources.sizeOfExifData = size;
            break;
        case ImageResourceType::ALPHA_CHANNEL_ASCII_NAMES: {
            allocateChannels();
            std::uint32_t channel{};
            std::int64_t remaining = size;
            while (remaining > 0) {
                compat::RetainedString name;
                const auto length = readValue<std::uint8_t>(file);
                if (length > 0) name = readStr(file, length);
                remaining -= 1 + length;
                if (channel < resources.alphaChannelCount) resources.alphaChannels[channel++].asciiName = name;
            }
            break;
        }
        default: skip(file, size); break;
        }
        leftToRead -= std::uint32_t{10} + nameLength + size;
    }
}
std::int64_t ReadMaskRectangle(RetainedFile& file, MaskData& mask) {
    mask.top = readValue<std::int32_t>(file); mask.left = readValue<std::int32_t>(file);
    mask.bottom = readValue<std::int32_t>(file); mask.right = readValue<std::int32_t>(file);
    return 16;
}
std::int64_t ReadMaskDensity(RetainedFile& file, std::uint8_t& density) {
    density = readValue<std::uint8_t>(file); return 1;
}
std::int64_t ReadMaskFeather(RetainedFile& file, double& feather) {
    feather = readValue<double>(file); return 8;
}
std::int64_t ReadMaskParameters(RetainedFile& file, std::uint8_t& layerDensity, double& layerFeather,
    std::uint8_t& vectorDensity, double& vectorFeather) {
    const auto flags = readValue<std::uint8_t>(file);
    std::int64_t count = 1;
    if (flags & 1) count += ReadMaskDensity(file, layerDensity);
    if (flags & 2) count += ReadMaskFeather(file, layerFeather);
    if (flags & 4) count += ReadMaskDensity(file, vectorDensity);
    if (flags & 8) count += ReadMaskFeather(file, vectorFeather);
    return count;
}
template<class T> void ApplyMaskData(const MaskData& data, double feather, std::uint8_t density, T* mask) {
    mask->top = data.top; mask->left = data.left; mask->bottom = data.bottom; mask->right = data.right;
    mask->feather = feather; mask->density = density; mask->defaultColor = data.defaultColor;
    mask->disabled = data.disabled; mask->invert = data.invert; mask->positionRelativeToLayer = data.positionRelativeToLayer;
}
std::string utf16ToUtf8(const std::vector<std::uint16_t>& text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        std::uint32_t value = text[i];
        if (value >= 0xd800 && value <= 0xdbff) {
            if (i + 1 == text.size()) value = 0xfffd;
            else {
                // byUTF consumes the following unit even when it is not a low surrogate.
                const auto low = text[++i];
                value = low < 0xdc00 || low > 0xdfff ? 0xfffd :
                    0x10000 + ((value - 0xd800) << 10) + low - 0xdc00;
            }
        } else if (value >= 0xdc00 && value <= 0xdfff) value = 0xfffd;
        if (value < 0x80) result.push_back(static_cast<char>(value));
        else if (value < 0x800) {
            result.push_back(static_cast<char>(0xc0 | (value >> 6)));
            result.push_back(static_cast<char>(0x80 | (value & 63)));
        } else if (value < 0x10000) {
            result.push_back(static_cast<char>(0xe0 | (value >> 12)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 63)));
            result.push_back(static_cast<char>(0x80 | (value & 63)));
        } else {
            result.push_back(static_cast<char>(0xf0 | (value >> 18)));
            result.push_back(static_cast<char>(0x80 | ((value >> 12) & 63)));
            result.push_back(static_cast<char>(0x80 | ((value >> 6) & 63)));
            result.push_back(static_cast<char>(0x80 | (value & 63)));
        }
    }
    return result;
}

LayerMaskSection parseLayer(RetainedFile& file, PSD& document,
    std::uint64_t sectionOffset, std::uint32_t sectionLength, std::uint32_t layerLength) {
    LayerMaskSection section;
    if (layerLength != 0) {
        const auto encodedCount = readValue<std::int16_t>(file);
        std::uint32_t count{};
        enforce(decodePsdLayerCount(encodedCount, layerLength, count, section.hasTransparencyMask),
            "PSD layer count exceeds the available layer info section");
        section.layerCount = count;
        section.layers = allocate<Layer>(count);
        for (std::size_t i = 0; i < section.layers.size(); ++i) {
            Layer layer;
            layer.filePtr = file;
            layer.type = LayerType::Any;
            layer.top = readValue<std::int32_t>(file); layer.left = readValue<std::int32_t>(file);
            layer.bottom = readValue<std::int32_t>(file); layer.right = readValue<std::int32_t>(file);
            enforce(validPsdImageDimensions(static_cast<std::int64_t>(layer.right) - layer.left,
                static_cast<std::int64_t>(layer.bottom) - layer.top),
                [&] {
                    return "Invalid or excessively large PSD layer bounds: (" +
                        std::to_string(layer.left) + ", " + std::to_string(layer.top) + ")-(" +
                        std::to_string(layer.right) + ", " + std::to_string(layer.bottom) + ")";
                });
            layer.channels = allocate<ChannelInfo>(readValue<std::uint16_t>(file));
            for (auto& channel : layer.channels) {
                channel.type = readValue<std::int16_t>(file); channel.dataLength = readValue<std::uint32_t>(file);
            }
            const auto signature = readStr(file, 4);
            enforce(signature.view() == "8BIM", [&] {
                return "Layer mask info section seems to be corrupt, "
                    "signature does not match \"8BIM\". (was \"" +
                    std::string(signature.view()) + "\")";
            });
            layer.blendModeKey = BlendingMode(readStr(file, 4));
            layer.opacity = readValue<std::uint8_t>(file);
            layer.clipping = !readValue<bool>(file);
            layer.flags = static_cast<LayerFlags>(readValue<std::uint8_t>(file));
            skip(file, 1);
            const auto extraLength = readValue<std::uint32_t>(file);
            const auto maskLength = readValue<std::uint32_t>(file);
            if (maskLength != 0) {
                std::array<MaskData, 2> masks{};
                std::uint32_t maskCount = 1;
                double layerFeather = 0.0, vectorFeather = 0.0;
                std::uint8_t layerDensity = 255, vectorDensity = 255;
                std::int64_t remaining = maskLength;
                remaining -= ReadMaskRectangle(file, masks[0]);
                masks[0].defaultColor = readValue<std::uint8_t>(file); --remaining;
                const auto flags = readValue<std::uint8_t>(file); --remaining;
                const auto setFlags = [](MaskData& mask, std::uint8_t value) {
                    mask.isVectorMask = (value & 8) != 0; mask.disabled = (value & 2) != 0;
                    mask.invert = (value & 4) != 0; mask.positionRelativeToLayer = (value & 1) != 0;
                };
                setFlags(masks[0], flags);
                bool hasParameters = (flags & 16) != 0;
                if (hasParameters && maskLength <= 28)
                    remaining -= ReadMaskParameters(file, layerDensity, layerFeather, vectorDensity, vectorFeather);
                if (remaining >= 18) {
                    maskCount = 2;
                    const auto realFlags = readValue<std::uint8_t>(file); --remaining;
                    masks[1].defaultColor = readValue<std::uint8_t>(file); --remaining;
                    remaining -= ReadMaskRectangle(file, masks[1]);
                    setFlags(masks[1], realFlags);
                    hasParameters |= (realFlags & 16) != 0;
                    if (hasParameters)
                        remaining -= ReadMaskParameters(file, layerDensity, layerFeather, vectorDensity, vectorFeather);
                }
                enforce(remaining >= 0, "Parsing failed, #d bytes left");
                skip(file, remaining);
                for (std::uint32_t m = 0; m < maskCount; ++m) {
                    if (masks[m].isVectorMask) {
                        enforce(layer.vectorMask.isNull(), "A vector mask already exists.");
                        layer.vectorMask = allocate<VectorMask>(1);
                        ApplyMaskData(masks[m], vectorFeather, vectorDensity, &layer.vectorMask[0]);
                    } else {
                        enforce(layer.layerMask.isNull(), "A layer mask already exists.");
                        layer.layerMask = allocate<LayerMask>(1);
                        ApplyMaskData(masks[m], layerFeather, layerDensity, &layer.layerMask[0]);
                    }
                }
            }
            const auto blendingLength = readValue<std::uint32_t>(file);
            skip(file, blendingLength);
            const auto nameLength = peekValue<std::uint8_t>(file);
            const auto paddedNameLength = roundUpToMultiple(nameLength + 1u, 4u);
            layer.name = readPascalStr(file, paddedNameLength - 1u);
            const std::uint32_t additionalSize = extraLength - maskLength - blendingLength - paddedNameLength - 8u;
            std::int64_t remaining = additionalSize;
            while (remaining > 0) {
                enforce(remaining >= 12, "Additional Layer Information block header is truncated");
                const auto signature = readStr(file, 4);
                enforce(signature.view() == "8BIM", [&] {
                    return "Additional Layer Information section seems to be corrupt, "
                        "signature does not match \"8BIM\". (was \"" +
                        std::string(signature.view()) + "\")";
                });
                const auto key = readStr(file, 4);
                const auto dataLength = readValue<std::uint32_t>(file);
                enforce(dataLength < UINT32_MAX, "Additional Layer Information block length is too large");
                const auto length = roundUpToMultiple(dataLength, 2u);
                enforce(length <= remaining - 12, "Additional Layer Information block exceeds its containing section");
                if (key.view() == "lsct") {
                    enforce(dataLength >= 4, "PSD section divider block is truncated");
                    layer.type = static_cast<LayerType>(readValue<std::uint32_t>(file));
                    if (dataLength >= 12) {
                        enforce(readStr(file, 4).view() == "8BIM", "Invalid PSD section divider blend signature");
                        layer.blendModeKey = BlendingMode(readStr(file, 4));
                        skip(file, length - 12u);
                    } else skip(file, length - 4u);
                } else if (key.view() == "luni") {
                    enforce(dataLength >= 4, "PSD Unicode layer name block is truncated");
                    const auto characters = readValue<std::uint32_t>(file);
                    enforce(validPsdUnicodeLayerNameLength(dataLength, characters),
                        "PSD Unicode layer name exceeds its additional-info block");
                    std::vector<std::uint16_t> name;
                    for (std::uint32_t c = 0; c < characters; ++c) name.push_back(readValue<std::uint16_t>(file));
                    auto unicodeName = utf16ToUtf8(name);
                    if (!unicodeName.empty() && unicodeName.back() == '\0') unicodeName.pop_back();
                    if (!unicodeName.empty()) layer.name = compat::RetainedString(std::move(unicodeName));
                    skip(file, static_cast<std::size_t>(length) - 4u - static_cast<std::size_t>(characters) * 2);
                } else skip(file, length);
                remaining -= static_cast<std::size_t>(12) + length;
            }
            section.layers[i] = layer;
        }
        for (auto& layer : section.layers) {
            for (auto& channel : layer.channels) {
                channel.fileOffset = static_cast<std::uint32_t>(file.tell());
                skip(file, channel.dataLength);
            }
        }
    }
    if (sectionLength > 0) {
        const auto globalOffset = sectionOffset + layerLength + 4u;
        file.seek(static_cast<std::int64_t>(globalOffset));
        if (sectionOffset + sectionLength > globalOffset) {
            auto remaining = static_cast<std::int64_t>(sectionOffset + sectionLength - globalOffset);
            const auto maskLength = readValue<std::uint32_t>(file);
            remaining -= 4;
            if (maskLength != 0) {
                section.overlayColorSpace = readValue<std::uint16_t>(file);
                skip(file, 8);
                section.opacity = readValue<std::uint16_t>(file);
                section.kind = readValue<std::uint8_t>(file);
                remaining -= 13;
                const std::uint32_t padding = maskLength - 13u;
                skip(file, padding); remaining -= padding;
            }
            while (remaining > 0) {
                enforce(readStr(file, 4).view() == "8BIM",
                    "Additional Layer Information section seems to be corrupt, signature does not match \"8BIM\".");
                const auto key = readStr(file, 4);
                auto length = readValue<std::uint32_t>(file);
                length = roundUpToMultiple(length, 4u);
                if (key.view() == "Lr16" || key.view() == "Lr32") {
                    const auto offset = file.tell();
                    section = parseLayer(file, document, 0, 0, length);
                    file.seek(static_cast<std::int64_t>(offset + length));
                } else skip(file, length);
                remaining -= static_cast<std::size_t>(12) + length;
            }
        }
    }
    return section;
}
void parseLayerMaskInfoSection(RetainedFile& file, PSD& document) {
    document.layerMaskInfoSectionLength = readValue<std::uint32_t>(file);
    document.layerMaskInfoSectionOffset = file.tell();
    if (document.layerMaskInfoSectionLength == 0) { document.layers.clear(); return; }
    const auto layerLength = readValue<std::uint32_t>(file);
    auto section = parseLayer(file, document, document.layerMaskInfoSectionOffset,
        static_cast<std::uint32_t>(document.layerMaskInfoSectionLength), layerLength);
    document.layers = section.layers;
}
void parseImageDataSectionOffset(RetainedFile& file, PSD& document) {
    document.imageDataSectionOffset = file.tell();
    document.imageDataSectionLength = file.size() - document.imageDataSectionOffset;
}
}
PSD parseDocument(RetainedFile& file) {
    PSD document;
    file.seek(0);
    parseHeader(file, document);
    parseColorModeSection(file, document);
    document.imageResourceSectionOffset = file.tell();
    document.imageResourceSectionLength = readValue<std::uint32_t>(file);
    skip(file, document.imageResourceSectionLength);
    parseLayerMaskInfoSection(file, document);
    parseImageDataSectionOffset(file, document);
    return document;
}
PSD parseDocument(compat::RetainedString fileName) {
    RetainedFile file(std::move(fileName));
    return parseDocument(file);
}
} // namespace nijigenerate::io::psd
