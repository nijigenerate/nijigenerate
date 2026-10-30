#include "depthmap_psd_loading.hpp"
#include "depthmap_psd_names.hpp"
#include "psdlayers.hpp"
#include "psdbudget.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <algorithm>
#include <array>
#include <bit>
namespace nijigenerate::io::depthmap_psd {
namespace detail {
namespace {
bool pathSeparator(char c) {
#if defined(_WIN32)
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}
bool pathNameSeparator(char c) {
#if defined(_WIN32)
    return pathSeparator(c) || c == ':';
#else
    return pathSeparator(c);
#endif
}
}
void enforce(bool condition, std::string_view message, std::size_t sourceLine) {
    if (!condition) {
        auto retainedMessage=message.empty()
            ? compat::RetainedString::fromStaticLiteral("Enforcement failed")
            : allocateImportBytes([&] { return compat::RetainedString(message); });
        if (sourceLine) throw compat::ThrowableException(std::move(retainedMessage),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), sourceLine);
        throw compat::ThrowableException(std::move(retainedMessage));
    }
}
compat::RetainedString sourceBaseName(compat::RetainedString path) {
    const auto text = path.view();
    std::size_t begin = 0;
#if defined(_WIN32)
    const bool unc = text.size() >= 3 && pathSeparator(text[0]) && pathSeparator(text[1]) && !pathSeparator(text[2]);
    if (text.size() >= 2 && text[1] == ':') begin = 2;
    else if (unc) {
        begin = 3;
        while (begin < text.size() && !pathSeparator(text[begin])) ++begin;
        if (begin < text.size()) {
            auto share = begin;
            do { ++share; } while (share < text.size() && pathSeparator(text[share]));
            if (share < text.size()) {
                do { ++share; } while (share < text.size() && !pathSeparator(text[share]));
                begin = share;
            }
        }
    }
    if (begin == text.size()) return unc ? path.subslice(0, 1) : compat::RetainedString{};
#else
    if (text.empty()) return {};
#endif
    auto end = text.size();
    while (end > begin && pathSeparator(text[end - 1])) --end;
    if (end == begin) return path.subslice(begin, begin + 1);
    auto start = end;
    while (start > begin && !pathNameSeparator(text[start - 1])) --start;
    return path.subslice(start, end);
}
compat::RetainedString sourceStem(compat::RetainedString path) {
    auto base = sourceBaseName(path);
    const auto view = base.view();
    for (auto end = view.size(); end;) {
        const auto i = --end;
        if (pathNameSeparator(view[i])) break;
        if (view[i] == '.' && i && !pathNameSeparator(view[i - 1])) return base.subslice(0, i);
    }
    return base;
}
compat::RetainedString sourceExtensionLower(compat::RetainedString path) {
    const auto view = path.view();
    for (auto end = view.size(); end;) {
        const auto i = --end;
        if (pathNameSeparator(view[i])) break;
        if (view[i] == '.' && i && !pathNameSeparator(view[i - 1])) {
            return ngPsdDepthLowerString(path.subslice(i, path.size()));
        }
    }
    return {};
}
void setCompositionMode(PsdDepthImportResult& result, PsdDepthCompositionMode mode) {
    result.compositionMode = mode;
    result.compositionModeName = ngPsdDepthCompositionModeName(mode);
}
void addCompositionDiagnostic(PsdDepthImportResult& result, compat::RetainedString type,
    compat::RetainedString message, compat::RetainedString path, compat::RetainedString name, std::size_t count) {
    PsdDepthCompositionDiagnostic diagnostic;
    diagnostic.type = type;
    diagnostic.message = message;
    diagnostic.layerPath = path;
    diagnostic.layerName = name;
    diagnostic.count = count;
    // Source array extension publishes its descriptor only after it succeeds.
    auto diagnostics = result.compositionDiagnostics;
    diagnostics.append(diagnostic);
    result.compositionDiagnostics = diagnostics;
}
void validateCompositionDimensions(PsdDepthImportResult& result, compat::RetainedString context) {
    // D enforce evaluates its message lazily, after the corresponding predicate fails.
    if (!(result.compositionWidth > 0 && result.compositionHeight > 0)) {
        const auto message = allocateImportBytes([&] {
            return "Composition dimensions must be positive for " + std::string(context.view());
        });
        enforce(false, message, 636);
    }
    if (!(result.colorSource.width == result.compositionWidth &&
        result.colorSource.height == result.compositionHeight &&
        result.depthSource.width == result.compositionWidth && result.depthSource.height == result.compositionHeight)) {
        const auto message = allocateImportBytes([&] {
            return "Color and depth source dimensions must match for " + std::string(context.view());
        });
        enforce(false, message, 638);
    }
    ngPsdDepthRefreshDerivedState(result);
}
bool psdLayerVisible(psd::Layer& layer) { return (layer.flags & psd::LayerFlags::Visible) == 0; }
PsdDepthCompositeSourceLayer makeCompositeSourceLayer(compat::RetainedString id, compat::RetainedString name,
    int left, int top, int width, int height, bool visible, bool hasPixels, std::size_t maskedPixels, DeformableRef target) {
    PsdDepthCompositeSourceLayer layer;
    layer.id = id; layer.name = name; layer.left = left; layer.top = top; layer.width = width; layer.height = height;
    layer.visible = visible; layer.hasPixels = hasPixels; layer.maskedPixels = maskedPixels;
    if (target) {
        layer.targetGridUuid = compat::ngRequireObject(target)->getUUID();
        layer.targetGridName = compat::RetainedString(compat::ngRequireObject(target)->getName());
    }
    return layer;
}
void setCompositeSourceLayerPixels(PsdDepthCompositeSourceLayer& layer, compat::Slice<std::uint8_t> rgba,
    compat::Slice<std::uint8_t> mask) {
    layer.rgba = rgba;
    layer.maskRgba = mask.empty() ? rgba : mask;
    layer.hasPixels = !layer.rgba.empty();
}
std::size_t countAcceptedAlphaPixels(std::span<const std::uint8_t> rgba, float opacity) {
    std::size_t count = 0;
    if (rgba.size() < 4) return 0;
    for (std::size_t i = 0; i < rgba.size() / 4; ++i) {
        const auto index = i * 4;
        if (effectiveAlpha(rgba, index, opacity) > 0.0f) ++count;
    }
    return count;
}
void loadPsdCompositeSourceLayers(compat::RetainedString path, PsdDepthCompositeSource& source,
    std::size_t& count, std::uint64_t& retainedBytes) {
    psd::RetainedFile file(path);
    compat::ngRunWithScopeExit([&] {
    auto document = psd::parseDocument(file);
    source.kind = PsdDepthCompositeSourceKind::PsdLayers;
    source.kindName = ngPsdDepthCompositeSourceKindName(source.kind);
    source.sourcePath = path; source.width = document.width; source.height = document.height;
    const auto groups = psdlayers::ngPsdLayerGroupStates({document.layers.data(), document.layers.size()});
    PsdLayerPathOccurrences occurrences;
    nicxlive::core::common::StringKeyMap<PsdClippingBaseState> clipping;
    for (auto i = document.layers.size(); i-- > 0;) {
        auto layer = document.layers[i];
        if (!ngPsdDepthLayerHasPixelData(layer)) continue;
        const auto group = groups[i];
        const std::array pathArguments{
            core::translation::ngFormatArgument(group.path),
            core::translation::ngFormatArgument(layer.name)};
        auto pathBytes = core::translation::ngFormatTyped("%s/%s", pathArguments);
        auto retainedPath = allocateImportBytes([&] { return compat::RetainedString(std::move(pathBytes)); });
        const auto layerPath = uniquePsdLayerPath(std::move(retainedPath), occurrences);
        enforce(psdbudget::ngReservePsdLayerExtraction(layer, PsdDepthRetainedColorLayerBytesPerPixel,
            PsdDepthMaxRetainedBytes, retainedBytes), "PSD Depth Map color layers exceed the retained image memory budget", 948);
        layer.extractLayerImage();
        if (layer.data.empty()) continue;
        bool visible = group.visible && psdLayerVisible(layer);
        float opacity = group.opacity * layerOpacity01(layer.opacity);
        DepthLayerImage image;
        image.left = layer.left; image.top = layer.top;
        image.width = std::bit_cast<std::int32_t>(layer.width());
        image.height = std::bit_cast<std::int32_t>(layer.height());
        image.data = layer.data; layer.data.clear();
        if (layer.clipping) {
            clipping.setRetained(group.path, psdClippingBaseState(image, visible, opacity));
        } else if (const auto found = clipping.findRetained(group.path.view())) {
            const auto& base = *found;
            visible = visible && base.visible;
            const float localOpacity = group.opacity > 0.0f ? base.opacity / group.opacity : 0.0f;
            opacity *= std::max(0.0f, std::min(1.0f, localOpacity));
            applyPsdClippingBaseAlpha(image, base);
        }
        const auto pixels = countAcceptedAlphaPixels({image.data.data(), image.data.size()}, opacity);
        auto value = makeCompositeSourceLayer(layerPath, layer.name, layer.left, layer.top,
            std::bit_cast<std::int32_t>(layer.width()), std::bit_cast<std::int32_t>(layer.height()), visible, true, pixels);
        value.opacity = opacity;
        setCompositeSourceLayerPixels(value, image.data);
        source.layers.append(value); ++count;
    }
    }, [&] { file.close(); });
}
void addComposedLayer(PsdDepthImportResult& result, compat::RetainedString sourcePath,
    DepthLayerImage& image, PsdDepthImportSettings& settings, bool visible, bool enabled, DeformableRef target) {
    PsdDepthComposedLayer layer;
    layer.id = image.layerPath; layer.layerPath = image.layerPath; layer.layerName = image.layerName;
    layer.colorLayerPath = image.layerPath; layer.colorLayerName = image.layerName;
    layer.sourcePath = sourcePath; layer.depthLayerName = image.layerName;
    layer.left = image.left; layer.top = image.top; layer.width = image.width; layer.height = image.height;
    layer.visible = visible; layer.enabled = enabled; layer.invert = false; layer.depthOffset = 0; layer.depthScale = 1;
    layer.backDepth = settings.backDepth; layer.frontDepth = settings.frontDepth; layer.sourceDepthScale = settings.depthScale;
    layer.alphaThreshold = settings.alphaThreshold; layer.customRadius = settings.customRadius;
    layer.convolution = settings.convolution; layer.depthStats = computeLayerDepthStats(image, settings);
    if (target) {
        layer.targetGridUuid = compat::ngRequireObject(target)->getUUID();
        layer.targetGridName = compat::RetainedString(compat::ngRequireObject(target)->getName());
        layer.puppetBindingOverride = retainImportUnsignedDecimal(compat::ngRequireObject(target)->getUUID());
    }
    auto preview = buildColorLayerPreviewRgba(image);
    layer.colorRgba = preview.rgba;
    layer.depthRgba = buildDepthMaskPreviewFromSurface(image, settings, preview.rgba);
    layer.maskRgba.resize(preview.rgba.size());
    const auto pixelCount = preview.rgba.size() / 4;
    for (std::size_t i = 0; i < pixelCount; ++i) {
        const auto index = i * 4;
        const auto alpha = compat::ngArrayAt(preview.rgba, index + 3);
        if (!alpha) continue;
        compat::ngArrayAt(layer.maskRgba, index) = 255;
        compat::ngArrayAt(layer.maskRgba, index + 1) = 255;
        compat::ngArrayAt(layer.maskRgba, index + 2) = 255;
        compat::ngArrayAt(layer.maskRgba, index + 3) = alpha;
    }
    layer.coverageMaskRgba = layer.maskRgba.duplicate();
    result.composedLayers.append(layer); result.composedLayerCount = result.composedLayers.size();
}
void addMissingComposedLayer(PsdDepthImportResult& result, compat::RetainedString path, compat::RetainedString name,
    int left, int top, int width, int height, DeformableRef target) {
    if (!target) return;
    PsdDepthComposedLayer layer;
    layer.id = path; layer.layerPath = path; layer.layerName = name; layer.colorLayerPath = path; layer.colorLayerName = name;
    layer.left = left; layer.top = top; layer.width = width; layer.height = height;
    layer.targetGridUuid = compat::ngRequireObject(target)->getUUID();
    layer.targetGridName = compat::RetainedString(compat::ngRequireObject(target)->getName());
    layer.puppetBindingOverride = retainImportUnsignedDecimal(compat::ngRequireObject(target)->getUUID());
    layer.puppetFitEnabled = true; layer.enabled = false;
    result.composedLayers.append(layer); result.composedLayerCount = result.composedLayers.size();
}
void addMissingComposedLayer(PsdDepthImportResult& result, DeformableRef target) {
    if (!target) return;
    const auto pathName = compat::ngRequireObject(target)->getName();
    auto path = allocateImportBytes([&] { return compat::RetainedString("/" + pathName); });
    auto name = compat::RetainedString(compat::ngRequireObject(target)->getName());
    addMissingComposedLayer(result, path, name, 0, 0, 0, 0, target);
}
}
bool ngPsdDepthLayerHasPixelData(psd::Layer& layer) {
    return layer.type == psd::LayerType::Any && (layer.flags & psd::LayerFlags::PixelIrrel) == 0 &&
        layer.width() > 0 && layer.height() > 0;
}
}
