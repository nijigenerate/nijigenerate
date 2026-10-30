#include "pngexport.hpp"
#include "manifest.hpp"
#include "detail/phobos_simple_lowercase_data.hpp"
#include <nijigenerate/io/imagefmt/writer.hpp>
#include <common/unicode_lowercase.hpp>
#include <nijigenerate/platform/autosave_files.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <array>
#include <vector>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <compat/source_allocator.hpp>

namespace nijigenerate::viewport::depth::draw::pngexport {
namespace {
template<class Operation> decltype(auto) allocatePngMemory(Operation&& operation) {
    try { return std::forward<Operation>(operation)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
std::string_view text(const DepthDrawString& value) { return value.view(); }
DepthDrawString retain(std::string value) {
    return allocatePngMemory([&] { return DepthDrawString(std::move(value)); });
}
template<std::size_t N> DepthDrawString retain(const char (&value)[N]) {
    return DepthDrawString::fromStaticLiteral(value);
}
DepthDrawString copyEnforceMessage(std::string_view message) {
    if (message.empty()) return retain("Enforcement failed");
    return allocatePngMemory([&] { return DepthDrawString(message); });
}
compat::RetainedString pngExportSourceFile() {
    return compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/pngexport.d");
}
template<std::size_t N> void enforce(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) throw compat::ThrowableException(
        copyEnforceMessage(std::string_view(message, N - 1)), pngExportSourceFile(), line);
}
bool separator(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}
constexpr char separatorChar =
#ifdef _WIN32
    '\\';
#else
    '/';
#endif
bool unc(std::string_view path) {
    return path.size() >= 3 && separator(path[0]) && separator(path[1]) && !separator(path[2]);
}
std::size_t uncRoot(std::string_view path) {
    std::size_t i = 3;
    while (i < path.size() && !separator(path[i])) ++i;
    if (i < path.size()) {
        auto j = i;
        do { ++j; } while (j < path.size() && separator(path[j]));
        if (j < path.size()) {
            do { ++j; } while (j < path.size() && !separator(path[j]));
            i = j;
        }
    }
    return i;
}
bool absolute(std::string_view path) {
#ifdef _WIN32
    return (path.size() >= 3 && path[1] == ':' && separator(path[2])) || unc(path);
#else
    return !path.empty() && path[0] == '/';
#endif
}
std::string_view trimSeparators(std::string_view path) {
    while (!path.empty() && separator(path.back())) path.remove_suffix(1);
    return path;
}
// Required current Phobos path.dirName/pathSplitter/relativePath branches.
// BSL-1.0 and original authors: native/port/licenses/phobos-depth-manifest.txt.
std::string_view directoryName(std::string_view path) {
    if (path.empty()) return ".";
    auto p = trimSeparators(path);
    if (p.empty()) return path.substr(0, 1);
#ifdef _WIN32
    if (unc(p) && uncRoot(p) == p.size()) return p;
    if (p.size() == 2 && p[1] == ':' && path.size() > 2) return path.substr(0, 3);
    auto i = p.find_last_of("/\\:");
#else
    auto i = p.find_last_of('/');
#endif
    if (i == std::string_view::npos) return ".";
    if (i == 0) return p.substr(0, 1);
#ifdef _WIN32
    if (p[i] == ':' || p[i - 1] == ':') return p.substr(0, i + 1);
#endif
    return trimSeparators(p.substr(0, i));
}
std::vector<std::string_view> splitPath(std::string_view path) {
    std::vector<std::string_view> result;
    std::size_t pos = 0, root = 0;
#ifdef _WIN32
    if (unc(path)) root = uncRoot(path);
    else if (path.size() >= 3 && path[1] == ':' && separator(path[2])) root = 3;
    else if (!path.empty() && separator(path[0])) root = 1;
#else
    if (!path.empty() && separator(path[0])) root = 1;
#endif
    if (root) { result.push_back(path.substr(0, root)); pos = root; }
    while (pos < path.size()) {
        while (pos < path.size() && separator(path[pos])) ++pos;
        auto first = pos;
        while (pos < path.size() && !separator(path[pos])) ++pos;
        if (pos > first) result.push_back(path.substr(first, pos - first));
    }
    return result;
}
std::uint32_t simpleLower(std::uint32_t scalar) {
    namespace data = io::depthmap_psd::detail::phobos_simple_lowercase_data;
    if (scalar < 0xaa) return scalar >= 'A' && scalar <= 'Z' ? scalar + 32 : scalar;
    const auto packed = [](std::uint64_t offset, std::size_t index, unsigned bits) {
        const auto word = data::trieWords[offset + index / (64u / bits)];
        return static_cast<std::uint32_t>((word >> (bits * (index % (64u / bits)))) & ((1ULL << bits) - 1));
    };
    auto index = packed(data::trieOffsets[0], scalar >> 13, 8);
    index = packed(data::trieOffsets[1], (index << 7) + ((scalar >> 6) & 0x7f), 16);
    index = packed(data::trieOffsets[2], (index << 6) + (scalar & 0x3f), 16);
    return index == 0xffff ? scalar : data::lowerValues[index];
}
bool filenameEqual(std::string_view a, std::string_view b) {
#ifdef _WIN32
    nicxlive::core::common::unicode::ReplacementUtf8Cursor left{a}, right{b};
    while (!left.empty() && !right.empty()) {
        auto x = left.take(), y = right.take();
        if ((x == '/' || x == '\\') && (y == '/' || y == '\\')) continue;
        if (simpleLower(x) != simpleLower(y)) return false;
    }
    return left.empty() && right.empty();
#else
    return a == b;
#endif
}
DepthDrawString relativePath(DepthDrawString path, std::string_view base) {
    const auto pathBytes=text(path);
    if (!absolute(pathBytes)) return path;
    if (!absolute(base)) throw compat::ThrowableException(
        compat::RetainedString::fromStaticLiteral("Base directory must be absolute"),
        compat::RetainedString::fromStaticLiteral("std/path.d"), 2916);
    auto pathParts = allocatePngMemory([&] { return splitPath(pathBytes); });
    auto baseParts = allocatePngMemory([&] { return splitPath(base); });
    const bool sameRoot = filenameEqual(baseParts.at(0), pathParts.at(0));
    std::size_t common = 1;
    while (common < pathParts.size() && common < baseParts.size() &&
        filenameEqual(baseParts[common], pathParts[common])) ++common;
    // Source asRelativePath still forms its mismatch ranges before choosing
    // the original-root branch; to!string materializes that byte range.
    if (!sameRoot) return allocatePngMemory([&] { return retain(std::string(pathBytes)); });
    if (common == pathParts.size() && common == baseParts.size())
        return allocatePngMemory([&] { return retain(std::string(".")); });
    std::string result;
    const auto add = [&](std::string_view component) {
        if (!result.empty()) allocatePngMemory([&] { result += separatorChar; });
        // joiner fronts decode the actual narrow-string component before
        // byChar reencodes it. Validate with that real strict cursor; a valid
        // shortest UTF-8 scalar has the same original bytes on reencoding.
        compat::phobos::detail::UnicodeCursor<std::string_view, char> cursor{component};
        std::size_t offset = 0;
        while (!cursor.empty()) {
            const auto scalar = cursor.front();
            const std::size_t width = scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
            allocatePngMemory([&] { result.append(component.substr(offset, width)); });
            cursor.popFront();
            offset += width;
        }
    };
    for (auto i = common; i < baseParts.size(); ++i) add("..");
    for (auto i = common; i < pathParts.size(); ++i) add(pathParts[i]);
    return retain(std::move(result));
}
std::string safeLayerFileName(DepthDrawLayer layer, std::size_t index) {
    const auto fallback = [&] { return "layer-" + std::to_string(index); };
    std::string generated;
    if (text(layer.id).empty() && text(layer.displayName).empty()) generated = fallback();
    auto source = !text(layer.id).empty() ? text(layer.id) :
        !text(layer.displayName).empty() ? text(layer.displayName) : std::string_view(generated);
    std::string result;
    for (unsigned char c : source) {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_';
        result += safe ? static_cast<char>(c) : '_';
    }
    if (result.empty()) return fallback();
    return result;
}
}
compat::Slice<std::uint8_t> ngDepthDrawLayerToExportRgba(DepthDrawLayer layer) {
    enforce(layer.width > 0 && layer.height > 0, "DepthDraw layer dimensions must be positive", 36);
    const auto pixelCount = static_cast<std::size_t>(layer.width) * static_cast<std::size_t>(layer.height);
    enforce(layer.depthPixels.size() >= pixelCount * 4, "DepthDraw layer must have RGBA depth pixels", 37);
    auto depthPixels = io::depthimage::ngDepthDrawDecodeDepthPixelsFromRgbaSource(
        compat::ConstSlice<std::uint8_t>(layer.depthPixels), layer.channel);
    compat::Slice<std::uint8_t> result;
    result.resize(pixelCount * 4);
    const auto output = [&](std::size_t index, std::size_t sourceLine) -> std::uint8_t& {
        if (index >= result.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, result.size(), pngExportSourceFile(), sourceLine);
        return result[index];
    };
    for (std::size_t i = 0; i < pixelCount; ++i) {
        if (i >= depthPixels.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, depthPixels.size(),
            pngExportSourceFile(), 44);
        auto depth = depthPixels[i];
        auto offset = i * 4;
        output(offset, 46) = depth;
        output(offset + 1, 47) = depth;
        output(offset + 2, 48) = depth;
        auto& alpha = output(offset + 3, 49);
        if (offset + 3 >= layer.depthPixels.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            offset + 3, layer.depthPixels.size(), pngExportSourceFile(), 49);
        alpha = layer.depthPixels[offset + 3];
    }
    return result;
}
void ngSaveDepthDrawLayerPng(DepthDrawLayer layer, DepthDrawString path) {
    auto rgba = ngDepthDrawLayerToExportRgba(layer);
    auto encoded = io::imagefmt::writeImageMemory(io::imagefmt::png, layer.width, layer.height,
        {rgba.data(), rgba.size()}, 4);
    if (encoded.error != io::imagefmt::Error::none) {
        const auto index = static_cast<std::size_t>(encoded.error);
        constexpr auto count = sizeof(io::imagefmt::errorText) / sizeof(io::imagefmt::errorText[0]);
        if (index >= count) throw compat::ngStaticError<compat::ArrayIndexError>(index, count,
            pngExportSourceFile(), 58);
        const auto message = allocatePngMemory([&] {
            return retain(std::string("DepthDraw PNG export failed: ") + io::imagefmt::errorText[index]);
        });
        const auto copiedMessage = copyEnforceMessage(message.view());
        throw compat::ThrowableException(copiedMessage, pngExportSourceFile(), 58);
    }
    compat::ngRunWithScopeExit([&] {
        ngDepthDrawWriteFileBytes(path, {encoded.buffer.data(), encoded.length});
    }, [&] {
        encoded.buffer.sourceFree();
    });
}
DepthDrawPngExportResult ngExportDepthDrawPngSession(std::shared_ptr<DepthDrawSession> session,
    DepthDrawString outputDir, DepthDrawString manifestPath) {
    enforce(session != nullptr, "DepthDraw session is required", 68);
    enforce(!text(outputDir).empty(), "DepthDraw PNG export directory is required", 69);
    enforce(!text(manifestPath).empty(), "DepthDraw PNG export manifest path is required", 70);
    platform::ngAutosaveCreateDirectories(text(outputDir));
    auto exported = std::allocate_shared<DepthDrawSession>(
        nicxlive::core::d_runtime::SourceAllocator<DepthDrawSession>{});
    exported->selectedLayerId = session->selectedLayerId;
    exported->selectedGridUuid = session->selectedGridUuid;
    exported->display = session->display;
    exported->bindings = session->bindings.duplicate();
    exported->documentWidth = session->documentWidth;
    exported->documentHeight = session->documentHeight;
    DepthDrawPngExportResult result;
    result.manifestPath = manifestPath;
    const auto layers = session->layers;
    for (std::size_t i = 0; i < layers.size(); ++i) {
        auto layer = layers[i];
        auto exportedLayer = layer;
        if (!layer.hasDepthPixels()) {
            ++result.skippedLayers;
            exported->layers.append(exportedLayer);
            continue;
        }
        const auto safeName = allocatePngMemory([&] { return safeLayerFileName(layer, i); });
        auto fileName = allocatePngMemory([&] {
            auto index = std::to_string(i);
            if (index.size() < 3) index.insert(0, 3 - index.size(), '0');
            return index + "-" + safeName + ".png";
        });
        const std::array pathSegments{outputDir, retain(std::move(fileName))};
        auto path = compat::phobos::ngBuildPath(pathSegments);
        ngSaveDepthDrawLayerPng(layer, path);
        exportedLayer.sourcePath = relativePath(path, directoryName(text(manifestPath)));
        exportedLayer.rgba.clear(); exportedLayer.depthPixels.clear(); exportedLayer.alphaMask.clear();
        exportedLayer.normalCoverage.clear(); exportedLayer.cleanupOperations.clear();
        exported->layers.append(exportedLayer);
        result.layerPaths.append(path);
        ++result.exportedLayers;
    }
    ngSaveDepthDrawManifest(exported, manifestPath);
    result.succeeded = true;
    return result;
}
} // namespace nijigenerate::viewport::depth::draw::pngexport
