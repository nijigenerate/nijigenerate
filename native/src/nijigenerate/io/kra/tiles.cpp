// Independent kra-d parser.d tile reader/compositor/crop bodies. BSD-2-Clause.
#include "parser_detail.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <cstring>
#include <limits>

namespace nijigenerate::io::kra::detail {
namespace {
template<class Array> decltype(auto) arrayAt(Array& value, std::size_t index, std::size_t line) {
    if (index >= value.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, value.size(),
        compat::RetainedString::fromStaticLiteral("kra/parser.d"), line);
    return value.data()[index];
}
compat::Slice<std::uint8_t> byteSlice(compat::Slice<std::uint8_t> value,
        std::size_t first, std::size_t last, std::size_t line) {
    if (first > last || last > value.size())
        throw compat::ngStaticError<compat::ArraySliceError>(first, last, value.size(),
            compat::RetainedString::fromStaticLiteral("kra/parser.d"), line);
    return value.slice(first, last);
}
const std::string& headerValue(const std::unordered_map<std::string, std::string>& headers,
        const char* key, std::size_t line) {
    const auto found = headers.find(key);
    if (found == headers.end()) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("kra/parser.d"), line);
    return found->second;
}
std::vector<std::string> split(std::string_view text, char delimiter) {
    std::vector<std::string> result;
    for (std::size_t start = 0;;) {
        const auto end = text.find(delimiter, start);
        result.emplace_back(text.substr(start, end == std::string_view::npos ? end : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return result;
}
std::string readLayerLine(compat::Slice<std::uint8_t> data, std::size_t& offset) {
    std::string line;
    while (data[offset] != 10) line.push_back(static_cast<char>(data[offset++]));
    ++offset; return line;
}
std::unordered_map<std::string, std::string> readLayerInfo(compat::Slice<std::uint8_t> data, std::size_t& offset) {
    std::unordered_map<std::string, std::string> headers;
    for (unsigned i = 0; i < 5; ++i) {
        const auto parts = split(readLayerLine(data, offset), ' ');
        headers[arrayAt(parts, 0, 225)] = arrayAt(parts, 1, 225);
    }
    return headers;
}
}
bool parseLayerData(compat::Slice<std::uint8_t> bytes, Layer& layer) {
    std::size_t offset = 0;
    const auto info = readLayerInfo(bytes, offset);
    layer.numberOfVersion = integer(headerValue(info, "VERSION", 162));
    layer.tileWidth = integer(headerValue(info, "TILEWIDTH", 163));
    layer.tileHeight = integer(headerValue(info, "TILEHEIGHT", 164));
    layer.pixelSize = integer(headerValue(info, "PIXELSIZE", 165));
    layer.top = layer.left = std::numeric_limits<std::int32_t>::max(); layer.bottom = layer.right = 0;
    const auto count = static_cast<std::uint32_t>(integer(headerValue(info, "DATA", 171)));
    if (!count) return false;
    layer.tiles = {}; layer.tiles.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto parts = split(readLayerLine(bytes, offset), ',');
        const auto left = integer(arrayAt(parts, 0, 184));
        const auto top = integer(arrayAt(parts, 1, 185));
        const auto length = integer(arrayAt(parts, 3, 186));
        if (!length) continue;
        if (length < 0 || static_cast<std::size_t>(length) >= bytes.size() - offset)
            throw std::out_of_range("KRA compressed tile bounds");
        // Preserve the original one-byte skip and length-only cursor advance.
        const auto compressed = bytes.slice(offset + 1, offset + static_cast<std::size_t>(length) + 1);
        offset += static_cast<std::size_t>(length);
        layer.tiles[i] = std::make_shared<Tile>(left, top, compressed);
        if (left < layer.left) layer.left = left;
        if (add(left, layer.tileWidth) > layer.right) layer.right = add(left, layer.tileWidth);
        if (top < layer.top) layer.top = top;
        if (add(top, layer.tileHeight) > layer.bottom) layer.bottom = add(top, layer.tileHeight);
    }
    return true;
}
void cropLayer(compat::Slice<std::uint8_t> bytes, Layer& layer) {
    std::int32_t xmin = std::numeric_limits<std::int32_t>::max(), ymin = xmin, xmax = 0, ymax = 0;
    for (std::uint32_t i = 0; i < layer.height(); ++i) {
        const std::size_t row = (i * layer.width()) * 4u;
        for (std::uint32_t x = 0; x < layer.width(); ++x) {
            const auto index = row + x * 4u;
            if (arrayAt(bytes, index + 3, 247) > 0) {
                if (static_cast<std::uint32_t>(xmin) > x) xmin = wrap(x);
                if (static_cast<std::uint32_t>(ymin) > i) ymin = wrap(i);
                if (static_cast<std::uint32_t>(xmax) < x) xmax = wrap(x);
                if (static_cast<std::uint32_t>(ymax) < i) ymax = wrap(i);
            }
        }
    }
    xmax = add(xmax, 1); ymax = add(ymax, 1);
    const auto width = sub(xmax, xmin), height = sub(ymax, ymin);
    auto output = allocate(mul(mul(width, height), layer.pixelSize));
    for (std::int32_t y = 0; y < height; ++y) {
        const std::size_t line = (static_cast<std::uint32_t>(add(ymin, y)) * layer.width() +
            static_cast<std::uint32_t>(xmin)) * static_cast<std::uint32_t>(layer.pixelSize);
        const auto start = static_cast<std::size_t>(mul(mul(y, width), layer.pixelSize));
        const auto length = static_cast<std::size_t>(mul(width, layer.pixelSize));
        auto dst = byteSlice(output, start, start + length, 285);
        const auto src = byteSlice(bytes, line, line + length, 285);
        for (std::size_t i = 0; i < length; ++i) dst[i] = src[i];
    }
    layer.left = add(layer.left, xmin); layer.top = add(layer.top, ymin);
    layer.right = add(layer.left, width); layer.bottom = add(layer.top, height); layer.data = output;
}
}
namespace nijigenerate::io::kra {
void extractLayer(Layer& layer, bool crop) {
    using namespace detail;
    if (!layer.isLayerUseful())
        throw compat::ThrowableException{
            compat::RetainedString::fromStaticLiteral("cannot extract a layer that is not of type 'any'"),
            compat::RetainedString::fromStaticLiteral("kra/parser.d"), 299};
    const std::uint32_t expanded = static_cast<std::uint32_t>(mul(mul(layer.pixelSize, layer.tileWidth), layer.tileHeight));
    if (!layer.tileWidth || !layer.tileHeight) throw std::domain_error("KRA tile division by zero");
    const auto columns = wrap(layer.width() / static_cast<std::uint32_t>(layer.tileWidth));
    const auto rows = wrap(layer.height() / static_cast<std::uint32_t>(layer.tileHeight));
    const auto rowLength = mul(mul(columns, layer.pixelSize), layer.tileWidth);
    const auto composed = wrap(static_cast<std::uint32_t>(mul(columns, rows)) * expanded);
    auto output = allocate(composed);
    for (const auto tile : layer.tiles) {
        compat::ngRequireObject(tile)->expand(wrap(expanded));
        const auto unsorted = tile->expandedData();
        compat::Slice<std::uint8_t> sorted; sorted.resize(expanded);
        const std::uint32_t bytesPerChannel = layer.colorMode.value.view() == "RGBA16" ? 2u : 1u;
        std::vector<std::int32_t> channels;
        for (std::int32_t i = 0; i < layer.pixelSize; ++i) channels.push_back(i);
        // std.algorithm.swapRanges stops when either range is exhausted.
        if (channels.size() < bytesPerChannel)
            throw compat::ngStaticError<compat::ArraySliceError>(0, bytesPerChannel, channels.size(),
                compat::RetainedString::fromStaticLiteral("kra/parser.d"), 337);
        if (channels.size() < bytesPerChannel * 2)
            throw compat::ngStaticError<compat::ArraySliceError>(bytesPerChannel * 2, channels.size(), channels.size(),
                compat::RetainedString::fromStaticLiteral("kra/parser.d"), 337);
        for (std::size_t i = 0; i < bytesPerChannel && bytesPerChannel * 2 + i < channels.size(); ++i)
            std::swap(channels[i], channels[bytesPerChannel * 2 + i]);
        const auto area = mul(layer.tileHeight, layer.tileWidth);
        for (std::int32_t pixel = 0; pixel < area; ++pixel) {
            std::uint32_t index = 0;
            for (const auto channel : channels) {
                auto& destination = arrayAt(sorted,
                    static_cast<std::uint32_t>(mul(pixel, layer.pixelSize)) + index, 347);
                const auto value = arrayAt(unsorted, static_cast<std::size_t>(add(mul(channel, area), pixel)), 348);
                destination = value;
                ++index;
            }
        }
        const auto left = sub(tile->left(), layer.left), top = sub(tile->top(), layer.top);
        const auto size = mul(layer.pixelSize, layer.tileWidth);
        for (std::int32_t row = 0; row < layer.tileHeight; ++row) {
            const std::int64_t destination = static_cast<std::int64_t>(mul(row, rowLength)) +
                mul(left, layer.pixelSize) + mul(top, rowLength);
            const auto source = mul(size, row);
            if (destination < 0 || source < 0 || size < 0) throw std::out_of_range("KRA tile copy bounds");
            auto dst = output.slice(static_cast<std::size_t>(destination), static_cast<std::size_t>(destination) + size);
            const auto src = sorted.slice(static_cast<std::size_t>(source), static_cast<std::size_t>(source) + size);
            if (size) std::memcpy(dst.data(), src.data(), static_cast<std::size_t>(size));
        }
    }
    if (crop) cropLayer(output, layer); else layer.data = output;
    layer.left = add(layer.left, layer.x); layer.top = add(layer.top, layer.y);
    layer.right = add(layer.right, layer.x); layer.bottom = add(layer.bottom, layer.y);
}
}
