#include "source.hpp"
#include "nijigenerate/io/psd/package.hpp"
#include "nijigenerate/io/psdbudget.hpp"
#include "nijigenerate/io/psdlayers.hpp"
#include "nijigenerate/io/pngheader.hpp"
#include "nijigenerate/compat/druntime_utf8.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include "nijigenerate/compat/scope_exit.hpp"
#include <texture.hpp>
#include <common/resource_key_map.hpp>
#include <compat/phobos_string_hash.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <functional>
#include <exception>
#include <utility>
#include <new>
#include <stdexcept>
#include <string_view>
#include <source_location>

namespace nijigenerate::viewport::depth::draw {
namespace {
using Bytes = compat::Slice<std::uint8_t>;
template<class Operation> decltype(auto) allocateSourceMemory(Operation&& operation) {
    try { return std::forward<Operation>(operation)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
struct LayerGroupPathHash {
    std::size_t operator()(const DepthDrawString& path) const noexcept {
        return nicxlive::core::d_runtime::ngPhobosStringHash(path.view());
    }
};
using ClippingBases = nicxlive::core::common::ResourceKeyMap<DepthDrawString, DepthDrawLayer,
    LayerGroupPathHash, std::equal_to<DepthDrawString>>;
std::string_view view(const DepthDrawString& s) { return s.view(); }
DepthDrawString text(std::string s) {
    return allocateSourceMemory([&] { return DepthDrawString(std::move(s)); });
}
int add(int a, int b) { return std::bit_cast<std::int32_t>(std::uint32_t(a)+std::uint32_t(b)); }
int sub(int a, int b) { return std::bit_cast<std::int32_t>(std::uint32_t(a)-std::uint32_t(b)); }
int mul(int a, int b) { return std::bit_cast<std::int32_t>(std::uint32_t(a)*std::uint32_t(b)); }
compat::RetainedString copyEnforceMessage(std::string_view message) {
    if (message.empty()) return compat::RetainedString::fromStaticLiteral("Enforcement failed");
    try {
        return compat::RetainedString(message);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<std::size_t N>
void enforce(bool ok, const char (&message)[N], std::size_t sourceLine) {
    if (!ok) throw compat::ThrowableException{copyEnforceMessage(std::string_view(message, N - 1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/source.d"), sourceLine};
}
bool reserve(std::int64_t w, std::int64_t h, std::uint64_t bpp, std::uint64_t maximum, std::uint64_t& retained) {
    if (w < 0 || h < 0) return false;
    const auto uw = static_cast<std::uint64_t>(w), uh = static_cast<std::uint64_t>(h);
    if (uh && uw > UINT64_MAX / uh) return false;
    const auto count = uw * uh;
    if (count > UINT64_MAX / bpp) return false;
    const auto required = count * bpp;
    if (retained > maximum || required > maximum-retained) return false;
    retained += required; return true;
}
// Phobos path semantics, matching the current production PSD-name implementation.
// D Language Foundation, Boost Software License 1.0: native/port/licenses/phobos-locale-case.txt.
bool separator(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}
bool nameSeparator(char c) {
#ifdef _WIN32
    return separator(c) || c == ':';
#else
    return separator(c);
#endif
}
std::string_view pngBaseName(std::string_view path) {
    auto rest = path;
#ifdef _WIN32
    const bool unc = path.size() >= 3 && separator(path[0]) && separator(path[1]) && !separator(path[2]);
    if (path.size() >= 2 && path[1] == ':') rest.remove_prefix(2);
    else if (unc) {
        std::size_t end = 3;
        while (end < path.size() && !separator(path[end])) ++end;
        if (end < path.size()) {
            auto share = end;
            do { ++share; } while (share < path.size() && separator(path[share]));
            if (share < path.size()) {
                do { ++share; } while (share < path.size() && !separator(path[share]));
                end = share;
            }
        }
        rest.remove_prefix(end);
    }
    if (rest.empty()) return unc ? path.substr(0, 1) : std::string_view{};
#else
    if (rest.empty()) return {};
#endif
    auto end = rest.size();
    while (end && separator(rest[end-1])) --end;
    if (!end) return rest.substr(0, 1);
    auto start = end;
    while (start && !nameSeparator(rest[start-1])) --start;
    rest = rest.substr(start, end-start);
    for (auto i = rest.size(); i;) {
        --i;
        if (nameSeparator(rest[i])) break;
        if (rest[i] == '.' && i && !nameSeparator(rest[i-1])) return rest.substr(0, i);
    }
    return rest;
}
std::string normalizeLayerName(DepthDrawString name) {
    std::string result;
    bool lastWasSpace = true;
    const auto input = view(name);
    compat::DruntimeUtf8Cursor cursor(input);
    while (!cursor.empty()) {
        const auto start = cursor.offset();
        auto ch = static_cast<std::uint32_t>(cursor.take());
        const bool space = ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
        if (space) {
            if (!lastWasSpace && !result.empty()) allocateSourceMemory([&] { result += ' '; });
            lastWasSpace = true; continue;
        }
        if (ch >= 'A' && ch <= 'Z') ch += 'a'-'A';
        if (ch <= 0x7f) allocateSourceMemory([&] { result += static_cast<char>(ch); });
        // Strict decoding guarantees the unchanged scalar's canonical UTF-8.
        else allocateSourceMemory([&] { result.append(input.substr(start, cursor.offset() - start)); });
        lastWasSpace = false;
    }
    if (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}
bool usable(const DepthDrawLayer& layer) { return layer.enabled && layer.visible; }
bool emptyFallback(const DepthDrawLayer& layer) {
    return normalizeLayerName(layer.displayName).empty() && layer.bounds.left == 0 && layer.bounds.top == 0 &&
        layer.width == 0 && layer.height == 0;
}
std::string layerMatchKey(const DepthDrawLayer& l) {
    auto result = normalizeLayerName(l.displayName);
    return allocateSourceMemory([&] {
        for (const auto value : {l.bounds.left, l.bounds.top, l.width, l.height}) {
            result += '\0'; result += std::to_string(value);
        }
        return std::move(result);
    });
}
int overlap(const DepthDrawLayer& a, const DepthDrawLayer& b) {
    const int left = std::max(a.bounds.left,b.bounds.left), top = std::max(a.bounds.top,b.bounds.top);
    const int right = std::min(add(a.bounds.left,a.width),add(b.bounds.left,b.width));
    const int bottom = std::min(add(a.bounds.top,a.height),add(b.bounds.top,b.height));
    return mul(std::max(0,sub(right,left)), std::max(0,sub(bottom,top)));
}
std::int64_t score(const DepthDrawLayer& normal, const DepthDrawLayer& depth, std::size_t ni, std::size_t di) {
    const auto nn = normalizeLayerName(normal.displayName), dn = normalizeLayerName(depth.displayName);
    std::int64_t result = overlap(normal,depth);
    if (!nn.empty() && nn == dn) result += 100000000;
    if (normal.bounds.left == depth.bounds.left && normal.bounds.top == depth.bounds.top) result += 1000000;
    if (normal.width == depth.width && normal.height == depth.height) result += 100000;
    if (ni == di) result += 1000;
    return result;
}
std::ptrdiff_t match(const DepthDrawLayer& depth, std::size_t di,
    const compat::ConstSlice<DepthDrawLayer> normals, const compat::ConstSlice<bool> used) {
    for (std::size_t i = 0; i < normals.size(); ++i) {
        if (used[i] || !usable(normals[i])) continue;
        const auto normalKey = layerMatchKey(normals[i]);
        const auto depthKey = layerMatchKey(depth);
        if (normalKey == depthKey) return static_cast<std::ptrdiff_t>(i);
    }
    const auto dn = normalizeLayerName(depth.displayName);
    std::ptrdiff_t best = -1; std::int64_t bestScore = -1;
    if (!dn.empty()) for (std::size_t i = 0; i < normals.size(); ++i) {
        if (used[i] || !usable(normals[i]) || normalizeLayerName(normals[i].displayName) != dn) continue;
        const auto value = score(normals[i],depth,i,di);
        if (value > bestScore) { bestScore = value; best = static_cast<std::ptrdiff_t>(i); }
    }
    if (best >= 0) return best;
    bestScore = -1; int bestOverlap = 0;
    for (std::size_t i = 0; i < normals.size(); ++i) {
        if (used[i] || !usable(normals[i])) continue;
        const auto area = overlap(normals[i],depth);
        if (area <= 0) continue;
        const auto value = score(normals[i],depth,i,di);
        if (value > bestScore) { bestScore = value; bestOverlap = area; best = static_cast<std::ptrdiff_t>(i); }
    }
    if (best >= 0 && bestOverlap > 0) return best;
    for (std::size_t i = 0; i < normals.size(); ++i)
        if (!used[i] && usable(normals[i]) && emptyFallback(normals[i])) return static_cast<std::ptrdiff_t>(i);
    return -1;
}
Bytes normalCoverage(const DepthDrawLayer& depth, const DepthDrawLayer& normal) {
    Bytes result; result.resize(static_cast<std::size_t>(std::max(0,mul(depth.width,depth.height)))*4);
    if (normal.rgba.size() < static_cast<std::size_t>(std::max(0,mul(normal.width,normal.height)))*4) return result;
    for (int y = 0; y < depth.height; ++y) {
        const int ny = sub(add(depth.bounds.top,y),normal.bounds.top);
        if (ny < 0 || ny >= normal.height) continue;
        for (int x = 0; x < depth.width; ++x) {
            const int nx = sub(add(depth.bounds.left,x),normal.bounds.left);
            if (nx < 0 || nx >= normal.width) continue;
            const auto oi = (static_cast<std::size_t>(y)*static_cast<std::size_t>(depth.width)+x)*4;
            const auto ni = (static_cast<std::size_t>(ny)*static_cast<std::size_t>(normal.width)+nx)*4;
            const float value = static_cast<float>(normal.rgba[ni+3])*normal.opacity;
            const auto rounded = std::llround(static_cast<long double>(value));
            const int narrowed = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(rounded));
            result[oi] = 255;
            result[oi+1] = 255;
            result[oi+2] = 255;
            result[oi+3] = static_cast<std::uint8_t>(std::min(255,std::max(0,narrowed)));
        }
    }
    return result;
}
void releasePsdLayerFiles(io::psd::PSD& document) {
    // Clear real File members before the vector's noexcept destruction.
    // Retain cleanup failures while still detaching every remaining member.
    std::exception_ptr pending;
    for (auto& layer : document.layers) {
        try {
            compat::ngRunWithScopeExit([&] {
                if (pending) std::rethrow_exception(pending);
            }, [&] { layer.filePtr.detach(); });
        } catch (...) { pending = std::current_exception(); }
    }
    if (pending) std::rethrow_exception(pending);
}
}
bool ngReserveDepthDrawPsdRetainedLayer(std::int64_t w, std::int64_t h, std::uint64_t& retained) {
    return reserve(w,h,DepthDrawPsdRetainedBytesPerPixel,DepthDrawMaxPsdRetainedBytes,retained);
}
bool ngReserveDepthDrawPngRetainedLayer(std::int64_t w, std::int64_t h, std::uint64_t& retained) {
    return reserve(w,h,DepthDrawPngRetainedBytesPerPixel,DepthDrawMaxPngRetainedBytes,retained);
}
DepthDrawLayer ngLoadDepthDrawPngLayer(DepthDrawString path, DepthDrawString id) {
    std::uint64_t retained = 0; return ngLoadDepthDrawPngLayer(path,id,retained);
}
DepthDrawLayer ngLoadDepthDrawPngLayer(DepthDrawString path, DepthDrawString id, std::uint64_t& retained) {
    int w = 0,h = 0;
    io::pngheader::ngInspectPngDimensions(compat::RetainedStringSlice(path),w,h);
    enforce(ngReserveDepthDrawPngRetainedLayer(w,h,retained),
        "PNG layers exceed the DepthDraw retained memory budget", 69);
    nicxlive::core::ShallowTexture texture(path,4);
    enforce(texture.width == w && texture.height == h,
        "Decoded PNG dimensions do not match the IHDR header", 72);
    DepthDrawLayer layer;
    const auto base = pngBaseName(path.view());
    layer.id = id.empty() ? (base.data() ? path.subslice(
        static_cast<std::size_t>(base.data() - path.data()),
        static_cast<std::size_t>(base.data() - path.data()) + base.size()) : DepthDrawString{}) : id;
    layer.sourcePath = path;
    layer.layerPath = allocateSourceMemory([&] { return text("/"+std::string(view(layer.id))); });
    layer.displayName = layer.id;
    layer.width = texture.width; layer.height = texture.height;
    layer.bounds = {0,0,texture.width,texture.height};
    layer.rgba = Bytes::retainedView(texture.data.retentionOwner(), texture.data.data(), texture.data.size());
    layer.depthPixels = layer.rgba.duplicate();
    return layer;
}
void ngDepthDrawApplyMaskToLayerAlpha(DepthDrawLayer& layer, const compat::ConstSlice<std::uint8_t> mask,
    int mw, int mh, int ml, int mt, bool relative, std::uint8_t defaultMask) {
    if (layer.width <= 0 || layer.height <= 0 || mw <= 0 || mh <= 0) return;
    const auto count = static_cast<std::size_t>(layer.width)*static_cast<std::size_t>(layer.height);
    if (layer.rgba.size() < count*4 || mask.size() < static_cast<std::size_t>(mw)*static_cast<std::size_t>(mh)) return;
    Bytes alpha;
    alpha.resize(count);
    for (std::size_t i = 0; i < count; ++i) alpha[i] = layer.rgba[i*4+3];
    const auto masked = io::depthimage::ngDepthDrawApplyPsdMaskToAlphaSource(alpha,layer.width,layer.height,
        layer.bounds.left,layer.bounds.top,mask,mw,mh,ml,mt,relative,defaultMask);
    for (std::size_t i = 0; i < masked.size(); ++i) {
        layer.rgba[i*4+3] = masked[i];
        if (layer.depthPixels.size() >= count*4) layer.depthPixels[i*4+3] = masked[i];
    }
    layer.alphaMask = io::depthimage::ngDepthDrawAlphaMaskFromRgbaSource(layer.rgba);
}
void ngDepthDrawApplyClippingBaseCoverage(DepthDrawLayer& clipped, const DepthDrawLayer& base, float shared) {
    clipped.visible = clipped.visible && base.visible; clipped.enabled = clipped.enabled && base.enabled;
    const auto opacity = shared > 0 ? base.opacity/shared : 0.0f;
    clipped.opacity *= std::max(0.0f,std::min(1.0f,opacity));
    const auto count = static_cast<std::size_t>(base.width)*static_cast<std::size_t>(base.height);
    Bytes alpha;
    alpha.resize(count);
    if (base.rgba.size() >= count*4) for (std::size_t i = 0; i < count; ++i) alpha[i] = base.rgba[i*4+3];
    else for (std::size_t i = 0; i < count; ++i) alpha[i] = i < base.alphaMask.size() && base.alphaMask[i] ? 255 : 0;
    ngDepthDrawApplyMaskToLayerAlpha(clipped,alpha,base.width,base.height,base.bounds.left,base.bounds.top,false,0);
}
bool ngDepthDrawPsdLayerHasPixelData(io::psd::Layer& layer) {
    return layer.type == io::psd::LayerType::Any && (layer.flags & io::psd::LayerFlags::PixelIrrel) == 0 &&
        layer.width() > 0 && layer.height() > 0;
}
DepthDrawPsdLoadResult ngLoadDepthDrawPsd(DepthDrawString path) {
    const auto retainedPath = path;
    // Retain the ordinary stream owner while a partial parser result unwinds.
    io::psd::RetainedFile file(retainedPath);
    io::psd::PSD document;
    DepthDrawPsdLoadResult result;
    compat::ngRunWithScopeExit([&] {
        document = io::psd::parseDocument(file);
        result.documentWidth = document.width; result.documentHeight = document.height;
        result.session = std::allocate_shared<DepthDrawSession>(
            nicxlive::core::d_runtime::SourceAllocator<DepthDrawSession>{});
        result.session->documentWidth = result.documentWidth; result.session->documentHeight = result.documentHeight;
        const auto groups = io::psdlayers::ngPsdLayerGroupStates({document.layers.data(),document.layers.size()});
        std::size_t index = 0; std::uint64_t retained = 0;
        ClippingBases bases;
        const auto sourceLayers = document.layers;
        for (std::size_t i = sourceLayers.size(); i;) {
            auto layer = sourceLayers[--i];
            if (!ngDepthDrawPsdLayerHasPixelData(layer)) continue;
            const auto group = groups[i];
            if (!io::psdbudget::ngReservePsdLayerExtraction(layer,DepthDrawPsdRetainedBytesPerPixel,
                    DepthDrawMaxPsdRetainedBytes,retained))
                throw compat::ThrowableException{copyEnforceMessage(
                    "PSD layers exceed the DepthDraw retained memory budget"),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/source.d"), 158};
            layer.extractLayerImage();
            auto channels = layer.channels;
            for (auto& channel : channels) channel.data = {};
            DepthDrawLayer draw;
            draw.id = allocateSourceMemory([&] { return text("psd:"+std::to_string(index)); });
            draw.sourcePath = path;
            draw.layerPath = allocateSourceMemory([&] {
                return text(std::string(group.path.view())+"/"+std::string(layer.name.view()));
            });
            draw.displayName = layer.name;
            draw.width = std::bit_cast<std::int32_t>(layer.width());
            draw.height = std::bit_cast<std::int32_t>(layer.height());
            draw.bounds = {layer.left,layer.top,draw.width,draw.height};
            draw.opacity = group.opacity*static_cast<float>(layer.opacity)/255.0f;
            draw.visible = group.visible && (layer.flags & io::psd::LayerFlags::Visible) == 0;
            draw.enabled = draw.visible;
            draw.rgba = layer.data; layer.data = {}; draw.depthPixels = draw.rgba.duplicate();
            draw.alphaMask = io::depthimage::ngDepthDrawAlphaMaskFromRgbaSource(draw.rgba);
            if (layer.clipping) bases[group.path] = draw;
            else if (auto base = bases.find(group.path); base != bases.end())
                ngDepthDrawApplyClippingBaseCoverage(draw,base->second,group.opacity);
            result.layers.append(draw); result.session->layers.append(draw); ++index;
        }
    }, [&] {
        compat::ngRunWithScopeExit([&] { releasePsdLayerFiles(document); }, [&] { file.detach(); });
    });
    return result;
}
DepthDrawLayerPairingResult ngDepthDrawAttachNormalCoverage(std::shared_ptr<DepthDrawSession> session,
    const compat::ConstSlice<DepthDrawLayer> normals) {
    DepthDrawLayerPairingResult result;
    if (!session) return result;
    compat::Slice<bool> used;
    used.resize(normals.size());
    const auto layerCount = session->layers.size();
    for (std::size_t i = 0; i < layerCount; ++i) {
        if (!usable(session->layers[i])) continue;
        std::ptrdiff_t found;
        {
            auto matchLayers = session->layers;
            found = match(matchLayers[i],i,normals,used);
        }
        if (found < 0) {
            ++result.missingNormalCoverage;
            result.missingNormalLayerIds.append(session->layers[i].id);
            continue;
        }
        const auto index = static_cast<std::size_t>(found);
        {
            auto outputLayers = session->layers;
            auto& output = outputLayers[i].normalCoverage;
            auto inputLayers = session->layers;
            auto& depthLayer = inputLayers[i];
            const auto& normalLayer = normals[index];
            output = normalCoverage(depthLayer,normalLayer);
        }
        used[index] = true;
        ++result.matchedDepthLayers;
        result.matchedDepthLayerIds.append(session->layers[i].id);
    }
    return result;
}
}
