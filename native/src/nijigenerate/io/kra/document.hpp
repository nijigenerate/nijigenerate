#pragma once
// Copyright 2021, Inochi2D Project. BSD-2-Clause: kra.LICENSE.
// Declaration surface from the independent kra-d package/layer originals.
// Parsing, ZIP storage and tile expansion belong to their vendor implementations.
#include <nijigenerate/io/psd/retained_file.hpp>
#include <nijigenerate/io/psd/layer.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <array>
#include <memory>

namespace nijigenerate::io::kra {
class ZipArchive;
// Const retained D slice view; its definition accompanies the tile implementation.
class ConstTileData {
    compat::Slice<std::uint8_t> owner_;
public:
    ConstTileData() = default;
    explicit ConstTileData(compat::Slice<std::uint8_t> owner) : owner_(std::move(owner)) {}
    std::size_t size() const { return owner_.size(); }
    const std::uint8_t* data() const { return owner_.data(); }
    const std::uint8_t& operator[](std::size_t index) const { return owner_[index]; }
};
struct ColorMode {
    compat::RetainedString value = compat::RetainedString::fromStaticLiteral("RGBA");
    static const ColorMode RGBA, RGBA16;
};
struct BlendingMode {
    compat::RetainedString value = compat::RetainedString::fromStaticLiteral("pass through");
    static const BlendingMode PassThrough, Normal, Dissolve, Darken, Multiply, ColorBurn,
        LinearBurn, DarkerColor, Lighten, Screen, ColorDodge, LinearDodge, LighterColor,
        Overlay, SoftLight, HardLight, VividLight, LinearLight, PinLight, HardMix,
        Difference, Exclusion, Subtract, Divide, Hue, Saturation, Color, Luminosity;
};
enum class LayerType : std::int32_t { Any, OpenFolder, ClosedFolder, SectionDivider };
class Tile final {
public:
    Tile(std::int32_t left, std::int32_t top, compat::Slice<std::uint8_t> compressedData);
    std::int32_t left() const;
    std::int32_t top() const;
    ConstTileData expandedData() const;
    void expand(std::int32_t expandedSize);
private:
    std::int32_t left_{}, top_{};
    compat::Slice<std::uint8_t> compressedData_, expandedData_;
};
struct Layer : psd::LayerBounds {
    psd::RetainedFile filePtr;
    compat::Slice<std::shared_ptr<Tile>> tiles;
    std::int32_t numberOfVersion{}, pixelSize{}, tileWidth{}, tileHeight{};
    std::uint8_t* dataPtr{};
    compat::Slice<std::uint8_t> data;
    compat::RetainedString name;
    std::array<std::int32_t, 2> location{};
    std::int32_t& x = location[0];
    std::int32_t& y = location[1];
    LayerType type{LayerType::Any};
    BlendingMode blendModeKey;
    std::int32_t opacity{};
    bool isVisible{};
    ColorMode colorMode;
    compat::RetainedString uuid;
    Layer() = default;
    Layer(const Layer&);
    Layer& operator=(const Layer&);
    std::array<std::uint32_t, 2> center();
    std::array<std::uint32_t, 2> size();
    std::uint32_t width() const;
    std::uint32_t height() const;
    bool isLayerGroup();
    bool isLayerUseful();
    void extractLayerImage(bool crop = true);
};
struct KRA {
    std::shared_ptr<ZipArchive> fileRef;
    compat::RetainedString name;
    compat::Slice<Layer> layers;
    ColorMode colorMode;
    std::int32_t width{}, height{};
};
KRA parseDocument(compat::RetainedString fileName);
void extractLayer(Layer& layer, bool crop);
compat::Slice<std::uint8_t> lzfDecompress(compat::Slice<std::uint8_t> input,
    std::uint64_t length, std::int32_t maxout);
} // namespace nijigenerate::io::kra
