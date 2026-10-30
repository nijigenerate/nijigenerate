#pragma once
// Copyright 2021, Inochi2D Project. BSD-2-Clause: native/port/licenses/psd-d.txt.
#include "nijigenerate/io/psd/retained_file.hpp"
#include "nijigenerate/compat/slice.hpp"
#include <array>
#include <cstdint>
#include <limits>

namespace nijigenerate::io::psd {
struct BlendingMode {
    compat::RetainedString value = compat::RetainedString::fromStaticLiteral("pass");
    BlendingMode() = default;
    explicit BlendingMode(compat::RetainedString text) : value(std::move(text)) {}
    std::string_view view() const { return value.view(); }
    friend bool operator==(const BlendingMode&, const BlendingMode&) = default;
    static const BlendingMode PassThrough, Normal, Dissolve, Darken, Multiply, ColorBurn, LinearBurn,
        DarkerColor, Lighten, Screen, ColorDodge, LinearDodge, LighterColor, Overlay, SoftLight,
        HardLight, VividLight, LinearLight, PinLight, HardMix, Difference, Exclusion, Subtract,
        Divide, Hue, Saturation, Color, Luminosity;
};

struct LayerMask {
    std::int32_t top{}, left{}, bottom{}, right{};
    std::uint64_t fileOffset{};
    compat::Slice<std::uint8_t> data;
    double feather = std::numeric_limits<double>::quiet_NaN();
    std::uint8_t density{};
    bool disabled{}, invert{}, positionRelativeToLayer{};
    std::uint8_t defaultColor{};
};
struct VectorMask {
    std::int32_t top{}, left{}, bottom{}, right{};
    std::uint64_t fileOffset{};
    compat::Slice<std::uint8_t> data;
    double feather = std::numeric_limits<double>::quiet_NaN();
    std::uint8_t density{};
    bool disabled{}, invert{}, positionRelativeToLayer{};
    std::uint8_t defaultColor{};
};
struct MaskData {
    std::int32_t top{}, left{}, bottom{}, right{};
    std::uint8_t defaultColor{};
    bool isVectorMask{}, disabled{}, invert{}, positionRelativeToLayer{};
};
enum class ChannelType : std::int32_t {
    INVALID = 32767, R = 0, G = 1, B = 2, TRANSPARENCY_MASK = -1, LAYER_OR_VECTOR_MASK = -2, LAYER_MASK = -3,
};
struct ChannelInfo {
    std::int16_t type{};
    std::uint32_t fileOffset{}, dataLength{};
    compat::Slice<std::uint8_t> data;
    bool isMask() const { return type < 0; }
};
enum class LayerType : std::int32_t { Any = 0, OpenFolder = 1, ClosedFolder = 2, SectionDivider = 3 };
enum class LayerFlags : std::uint8_t {
    TransProtect = 1, Visible = 2, Obsolete = 4, ModernDoc = 8, PixelIrrel = 16, GroupMask = 24,
};
inline std::uint8_t operator&(LayerFlags a, LayerFlags b) {
    return static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b);
}

// Preserve both field and indexed access to the same four values without union type punning.
struct LayerBounds {
    std::array<std::int32_t, 4> bounds{};
    std::int32_t& top = bounds[0];
    std::int32_t& left = bounds[1];
    std::int32_t& bottom = bounds[2];
    std::int32_t& right = bounds[3];
    LayerBounds() = default;
    LayerBounds(const LayerBounds& other) : bounds(other.bounds) {}
    LayerBounds& operator=(const LayerBounds& other) { bounds = other.bounds; return *this; }
};
struct Layer : LayerBounds {
    RetainedFile filePtr;
    compat::RetainedString name;
    BlendingMode blendModeKey;
    compat::Slice<ChannelInfo> channels;
    compat::Slice<LayerMask> layerMask;
    compat::Slice<VectorMask> vectorMask;
    std::uint8_t opacity{};
    bool clipping{};
    LayerFlags flags{LayerFlags::TransProtect};
    compat::Slice<std::uint8_t> data;
    LayerType type{LayerType::Any};
    bool isVisible{};
    std::array<std::uint32_t, 2> center() const {
        return {static_cast<std::uint32_t>(left) + width() / 2, static_cast<std::uint32_t>(top) + height() / 2};
    }
    std::array<std::uint32_t, 2> size() const { return {width(), height()}; }
    std::uint32_t width() const { return static_cast<std::uint32_t>(right) - static_cast<std::uint32_t>(left); }
    std::uint32_t height() const { return static_cast<std::uint32_t>(bottom) - static_cast<std::uint32_t>(top); }
    bool isLayerGroup() const { return type == LayerType::OpenFolder || type == LayerType::ClosedFolder; }
    bool isLayerUseful() const { return !isLayerGroup() && width() != 0 && height() != 0; }
    std::size_t dataLengthUncompressed() const { return area() * channels.size(); }
    std::size_t totalDataCount() const {
        std::uint32_t length{};
        for (const auto& channel : channels) length += channel.dataLength;
        return length;
    }
    std::size_t area() const { return width() * height(); }
    void extractLayerImage();
};
struct LayerMaskSection {
    compat::Slice<Layer> layers;
    std::uint32_t layerCount{};
    std::uint16_t overlayColorSpace{}, opacity{};
    std::uint8_t kind{128};
    bool hasTransparencyMask{};
};
} // namespace nijigenerate::io::psd
