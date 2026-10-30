// Independent kra-d layer.d/package.d. BSD-2-Clause: kra.LICENSE.
#include "document.hpp"

namespace nijigenerate::io::kra {
const ColorMode ColorMode::RGBA{compat::RetainedString::fromStaticLiteral("RGBA")};
const ColorMode ColorMode::RGBA16{compat::RetainedString::fromStaticLiteral("RGBA16")};
#define KRA_BLEND(name, text) const BlendingMode BlendingMode::name{compat::RetainedString::fromStaticLiteral(text)};
KRA_BLEND(PassThrough, "pass through") KRA_BLEND(Normal, "normal") KRA_BLEND(Dissolve, "dissolve")
KRA_BLEND(Darken, "darken") KRA_BLEND(Multiply, "multiply") KRA_BLEND(ColorBurn, "burn")
KRA_BLEND(LinearBurn, "linear_burn") KRA_BLEND(DarkerColor, "darker color") KRA_BLEND(Lighten, "lighten")
KRA_BLEND(Screen, "screen") KRA_BLEND(ColorDodge, "dodge") KRA_BLEND(LinearDodge, "linear_dodge")
KRA_BLEND(LighterColor, "lighter color") KRA_BLEND(Overlay, "overlay") KRA_BLEND(SoftLight, "soft_light")
KRA_BLEND(HardLight, "hard_light") KRA_BLEND(VividLight, "vivid_light") KRA_BLEND(LinearLight, "linear light")
KRA_BLEND(PinLight, "pin_light") KRA_BLEND(HardMix, "hard mix") KRA_BLEND(Difference, "diff")
KRA_BLEND(Exclusion, "exclusion") KRA_BLEND(Subtract, "subtract") KRA_BLEND(Divide, "divide")
KRA_BLEND(Hue, "hue") KRA_BLEND(Saturation, "saturation") KRA_BLEND(Color, "color") KRA_BLEND(Luminosity, "luminize")
#undef KRA_BLEND
Tile::Tile(std::int32_t left, std::int32_t top, compat::Slice<std::uint8_t> bytes)
    : left_(left), top_(top), compressedData_(bytes) {}
std::int32_t Tile::left() const { return left_; }
std::int32_t Tile::top() const { return top_; }
ConstTileData Tile::expandedData() const { return ConstTileData(expandedData_); }
void Tile::expand(std::int32_t size) { expandedData_ = lzfDecompress(compressedData_, compressedData_.size(), size); }
Layer::Layer(const Layer& other) : psd::LayerBounds(other) { *this = other; }
Layer& Layer::operator=(const Layer& other) {
    if (this == &other) return *this;
    psd::LayerBounds::operator=(other);
    filePtr = other.filePtr; tiles = other.tiles;
    numberOfVersion = other.numberOfVersion; pixelSize = other.pixelSize;
    tileWidth = other.tileWidth; tileHeight = other.tileHeight; dataPtr = other.dataPtr;
    data = other.data; name = other.name; location = other.location; type = other.type;
    blendModeKey = other.blendModeKey; opacity = other.opacity; isVisible = other.isVisible;
    colorMode = other.colorMode; uuid = other.uuid;
    return *this;
}
std::array<std::uint32_t, 2> Layer::center() {
    return {static_cast<std::uint32_t>(left) + width() / 2, static_cast<std::uint32_t>(top) + height() / 2};
}
std::array<std::uint32_t, 2> Layer::size() { return {width(), height()}; }
std::uint32_t Layer::width() const { return static_cast<std::uint32_t>(right) - static_cast<std::uint32_t>(left); }
std::uint32_t Layer::height() const { return static_cast<std::uint32_t>(bottom) - static_cast<std::uint32_t>(top); }
bool Layer::isLayerGroup() { return type == LayerType::OpenFolder || type == LayerType::ClosedFolder; }
bool Layer::isLayerUseful() { return (!isLayerGroup() && width() != 0 && height() != 0) || isLayerGroup(); }
void Layer::extractLayerImage(bool crop) { extractLayer(*this, crop); }
}
