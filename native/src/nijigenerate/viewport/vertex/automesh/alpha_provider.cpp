#include "alpha_provider.hpp"
#include "nijigenerate/project.hpp"
#include "nijigenerate/widgets/button.hpp"
#include "nijigenerate/widgets/texture.hpp"
#include "nijigenerate/compat/numeric.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/compat/retained_string_slice.hpp"
#include "nijigenerate/compat/scope_exit.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <nijigenerate/compat/assert_error.hpp>
#include <nodes/part.hpp>
#include <nodes/composite.hpp>
#include <texture.hpp>
#include <compat/object_metadata.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nijigenerate::viewport::vertex::automesh {
using namespace nicxlive::core;
using namespace nicxlive::core::nodes;
using math::Vec2;
namespace {
template<class Type, class Allocate>
std::shared_ptr<Type> allocateAlphaProvider(Allocate allocate) {
    try {
        return std::shared_ptr<Type>(allocate());
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

void prepareAlphaProviderClassInfo() {
    static const bool registered = [] {
        const auto registerType = []<class T> {
            using namespace nicxlive::core::d_runtime;
            using Names = SourceObjectTypeNames<T>;
            const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
            ngRegisterObjectTypeNames<T>(name,
                compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
            ngRegisterObjectClassName(typeid(T), name);
        };
        registerType.template operator()<PartAlphaProvider>();
        registerType.template operator()<DeformableAlphaProvider>();
        registerType.template operator()<GenericProjectionAlphaProvider>();
        return true;
    }();
    (void)registered;
}

int add(int a, int b) { return std::bit_cast<std::int32_t>(std::uint32_t(a) + std::uint32_t(b)); }
int mul(int a, int b) { return std::bit_cast<std::int32_t>(std::uint32_t(a) * std::uint32_t(b)); }
compat::RetainedString copyEnforceMessage(std::string_view message) {
    try {
        return compat::RetainedString(message);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
}
struct ProjectionResult {
    compat::Slice<std::uint8_t> alpha;
    int w = 0, h = 0;
    RectF bounds;
};
Vec2 worldPoint(const math::Mat4& m, Vec2 v, Vec2 d) {
    const float values[] = {v.x + d.x, v.y + d.y, 0, 1};
    float result[2]{};
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 4; ++col)
            result[row] = math::detail::inmathAccumulate(result[row], math::detail::inmathProduct(m[row][col], values[col]));
    return {result[0], result[1]};
}
ProjectionResult projectAlphaExec(NodeSlice targets, std::shared_ptr<Puppet> puppet, ProjectionOptions opt) {
    if (targets.empty() || !puppet) return {{}, 0, 0, {0, 0, 0, 0}};
    const auto drawables = enumerateDrawablesForAutoMesh(targets);
    if (drawables.empty()) return {{}, 0, 0, {0, 0, 0, 0}};
    float minX = INFINITY, minY = INFINITY, maxX = -INFINITY, maxY = -INFINITY;
    for (auto d : drawables) {
        const auto m = d->getDynamicMatrix();
        auto verts = d->getVertices();
        if (verts.empty()) continue;
        if (d->deformation.size() != verts.size()) d->refreshDeform();
        verts.forEachValue([&](std::size_t i, Vec2 vertex) {
            const Vec2 offset = d->deformation[i].toVector();
            const auto p = worldPoint(m, vertex, offset);
            if (p.x < minX) minX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.x > maxX) maxX = p.x;
            if (p.y > maxY) maxY = p.y;
            return 0;
        });
    }
    if (!(minX < maxX && minY < maxY)) return {{}, 0, 0, {0, 0, 0, 0}};
    const int pad = opt.padding;
    int w = add(compat::ngFloatToInt32(std::ceil(maxX - minX)), mul(pad, 2));
    int h = add(compat::ngFloatToInt32(std::ceil(maxY - minY)), mul(pad, 2));
    if (opt.targetWidth > 0) w = opt.targetWidth;
    if (opt.targetHeight > 0) h = opt.targetHeight;
    if (w <= 0 || h <= 0) return {{}, 0, 0, {0, 0, 0, 0}};
    ProjectionResult result;
    result.alpha.resize(static_cast<std::size_t>(w) * h, 0);
    for (auto d : drawables) {
        const auto m = d->getDynamicMatrix();
        auto verts = d->getVertices();
        if (verts.empty()) continue;
        if (d->deformation.size() != verts.size()) d->refreshDeform();
        math::Vec2Array wpos(verts.size());
        verts.forEachValue([&](std::size_t i, Vec2 vertex) {
            const Vec2 offset = d->deformation[i].toVector();
            wpos.set(i, worldPoint(m, vertex, offset));
            return 0;
        });
        auto mesh = d->Drawable::getMesh();
        if (mesh.indices.empty() || mesh.vertices.empty()) continue;
        SourceTextureBytes texData;
        int tw = 0, th = 0;
        bool sampleTex = false;
        if (auto p = std::dynamic_pointer_cast<Part>(d)) {
            if (p->textures.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(
                0, 0, compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/vertex/automesh/alpha_provider.d"), 219);
            if (auto tex = p->textures[0]) {
                tw = tex->width(); th = tex->height();
                texData = tex->getSourceTextureData(); sampleTex = true;
            }
        }
        for (std::size_t tri = 0; tri < mesh.indices.size() / 3; ++tri) {
            const auto i0 = mesh.indices[tri * 3], i1 = mesh.indices[tri * 3 + 1], i2 = mesh.indices[tri * 3 + 2];
            const auto worldVertex = [&](std::size_t index) {
                return wpos[index].toVector();
            };
            const auto first = worldVertex(i0), second = worldVertex(i1), third = worldVertex(i2);
            auto toPix = [&](Vec2 p) { return Vec2{(p.x - minX) + pad, (p.y - minY) + pad}; };
            const auto a = toPix(first), b = toPix(second), c = toPix(third);
            int minPX = compat::ngFloatToInt32(std::floor(std::min(a.x, std::min(b.x, c.x))));
            int maxPX = compat::ngFloatToInt32(std::ceil(std::max(a.x, std::max(b.x, c.x))));
            int minPY = compat::ngFloatToInt32(std::floor(std::min(a.y, std::min(b.y, c.y))));
            int maxPY = compat::ngFloatToInt32(std::ceil(std::max(a.y, std::max(b.y, c.y))));
            if (maxPX < 0 || maxPY < 0 || minPX >= w || minPY >= h) continue;
            minPX = std::max(minPX, 0); minPY = std::max(minPY, 0);
            maxPX = std::min(maxPX, w - 1); maxPY = std::min(maxPY, h - 1);
            const float denom = (b.y-c.y)*(a.x-c.x) + (c.x-b.x)*(a.y-c.y);
            if (denom == 0) continue;
            Vec2 uv0{}, uv1{}, uv2{};
            const bool hasUV = mesh.uvs.size() == mesh.vertices.size() && sampleTex;
            if (hasUV) {
                uv0 = mesh.uvs[i0].toVector();
                uv1 = mesh.uvs[i1].toVector();
                uv2 = mesh.uvs[i2].toVector();
            }
            for (int py = minPY; py <= maxPY; ++py) for (int px = minPX; px <= maxPX; ++px) {
                const Vec2 p{px + 0.5f, py + 0.5f};
                const float w1 = ((b.y-c.y)*(p.x-c.x) + (c.x-b.x)*(p.y-c.y)) / denom;
                const float w2 = ((c.y-a.y)*(p.x-c.x) + (a.x-c.x)*(p.y-c.y)) / denom;
                const float w3 = 1 - w1 - w2;
                if (w1 < 0 || w2 < 0 || w3 < 0) continue;
                std::uint8_t alpha = 0;
                if (sampleTex && texData.size() >= static_cast<std::size_t>(tw)*th*4 && hasUV) {
                    float u = uv0.x*w1 + uv1.x*w2 + uv2.x*w3;
                    float v = uv0.y*w1 + uv1.y*w2 + uv2.y*w3;
                    if (std::isnan(u)) u = 0;
                    if (std::isnan(v)) v = 0;
                    u = u < 0 ? 0 : (u > 1 ? 1 : u);
                    v = v < 0 ? 0 : (v > 1 ? 1 : v);
                    const int tx = compat::ngFloatToInt32(std::floor(u*add(tw, -1)));
                    const int ty = compat::ngFloatToInt32(std::floor(v*add(th, -1)));
                    const auto idx = static_cast<std::size_t>(add(mul(add(mul(ty, tw), tx), 4), 3));
                    const auto textureLength = texData.size();
                    if (idx >= textureLength) throw compat::ngStaticError<compat::ArrayIndexError>(
                        idx, textureLength, compat::RetainedString::fromStaticLiteral(
                            "source/nijigenerate/viewport/vertex/automesh/alpha_provider.d"), 281);
                    alpha = texData[idx];
                }
                const auto outputIndex = static_cast<std::size_t>(add(mul(py, w), px));
                if (outputIndex >= result.alpha.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                    outputIndex, result.alpha.size(), compat::RetainedString::fromStaticLiteral(
                        "source/nijigenerate/viewport/vertex/automesh/alpha_provider.d"), 285);
                auto& dst = result.alpha[outputIndex];
                dst = static_cast<std::uint8_t>(alpha + (static_cast<unsigned>(dst)*(255-alpha))/255);
            }
        }
    }
    result.w = w; result.h = h; result.bounds = {minX, minY, maxX-minX, maxY-minY};
    return result;
}
thread_local std::shared_ptr<Texture> previewTexture;
thread_local int previewWidth = 0, previewHeight = 0;
thread_local compat::RetainedStringSlice previewSignature;
}

// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString PartAlphaProvider::toString() {
    prepareAlphaProviderClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t PartAlphaProvider::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool PartAlphaProvider::opEquals(std::shared_ptr<IAlphaProvider> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int PartAlphaProvider::opCmp(std::shared_ptr<IAlphaProvider>) {
    prepareAlphaProviderClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        std::string text = "need opCmp for class ";
        text += name.view();
        message = compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
compat::RetainedString DeformableAlphaProvider::toString() {
    prepareAlphaProviderClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DeformableAlphaProvider::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DeformableAlphaProvider::opEquals(std::shared_ptr<IAlphaProvider> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DeformableAlphaProvider::opCmp(std::shared_ptr<IAlphaProvider>) {
    prepareAlphaProviderClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        std::string text = "need opCmp for class ";
        text += name.view();
        message = compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
compat::RetainedString GenericProjectionAlphaProvider::toString() {
    prepareAlphaProviderClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t GenericProjectionAlphaProvider::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool GenericProjectionAlphaProvider::opEquals(std::shared_ptr<IAlphaProvider> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int GenericProjectionAlphaProvider::opCmp(std::shared_ptr<IAlphaProvider>) {
    prepareAlphaProviderClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        std::string text = "need opCmp for class ";
        text += name.view();
        message = compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

PartAlphaProvider::PartAlphaProvider(std::shared_ptr<Part> part) {
    initialize(std::move(part));
}
std::shared_ptr<PartAlphaProvider> PartAlphaProvider::createSource(std::shared_ptr<Part> part) {
    auto provider = allocateAlphaProvider<PartAlphaProvider>([] {
        return new PartAlphaProvider(DeferredInitialization{});
    });
    provider->initialize(std::move(part));
    return provider;
}
void PartAlphaProvider::initialize(std::shared_ptr<Part> part) {
    if (!part || part->textures.empty() || !part->textures[0]) {
        w_ = h_ = 0;
        bounds_ = {0, 0, 0, 0};
        return;
    }
    const auto texture = part->textures[0];
    w_ = texture->width(); h_ = texture->height();
    bounds_ = {0, 0, static_cast<float>(w_), static_cast<float>(h_)};
    auto data = texture->getSourceTextureData();
    if (data.size() >= static_cast<std::size_t>(w_)*h_*4) {
        alpha_.resize(static_cast<std::size_t>(w_)*h_);
        std::size_t position = 0;
        const int count = mul(w_, h_);
        for (int i = 0; i < count; ++i) {
            const auto index = position + 3;
            const auto length = data.size();
            if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(
                index, length, compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/vertex/automesh/alpha_provider.d"), 73);
            alpha_[static_cast<std::size_t>(i)] = data[index];
            position += 4;
        }
    }
}
DeformableAlphaProvider::DeformableAlphaProvider(std::shared_ptr<Node> meshGroup, ProjectionOptions opt) {
    initialize(std::move(meshGroup), opt);
}
std::shared_ptr<DeformableAlphaProvider> DeformableAlphaProvider::createSource(
        std::shared_ptr<Node> meshGroup, ProjectionOptions opt) {
    auto provider = allocateAlphaProvider<DeformableAlphaProvider>([] {
        return new DeformableAlphaProvider(DeferredInitialization{});
    });
    provider->initialize(std::move(meshGroup), opt);
    return provider;
}
void DeformableAlphaProvider::initialize(std::shared_ptr<Node> meshGroup, ProjectionOptions opt) {
    if (!meshGroup) {
        w_ = h_ = 0;
        bounds_ = {0, 0, 0, 0};
        return;
    }
    auto puppet = meshGroup->puppetRef() ? meshGroup->puppetRef() : project::incActivePuppet();
    if (!puppet) throw compat::ThrowableException(
        copyEnforceMessage("No active puppet for DeformableAlphaProvider"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/vertex/automesh/alpha_provider.d"), 101);
    auto r = projectAlphaExec({meshGroup}, puppet, opt);
    w_ = r.w; h_ = r.h; bounds_ = r.bounds; alpha_ = r.alpha;
}
GenericProjectionAlphaProvider::GenericProjectionAlphaProvider(NodeSlice targets, ProjectionOptions opt) {
    initialize(std::move(targets), opt);
}
std::shared_ptr<GenericProjectionAlphaProvider> GenericProjectionAlphaProvider::createSource(
        NodeSlice targets, ProjectionOptions opt) {
    auto provider = allocateAlphaProvider<GenericProjectionAlphaProvider>([] {
        return new GenericProjectionAlphaProvider(DeferredInitialization{});
    });
    provider->initialize(std::move(targets), opt);
    return provider;
}
void GenericProjectionAlphaProvider::initialize(NodeSlice targets, ProjectionOptions opt) {
    auto puppet = project::incActivePuppet();
    if (!puppet) throw compat::ThrowableException(
        copyEnforceMessage("No active puppet for GenericProjectionAlphaProvider"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/vertex/automesh/alpha_provider.d"), 126);
    auto r = projectAlphaExec(targets, puppet, opt);
    w_ = r.w; h_ = r.h; bounds_ = r.bounds; alpha_ = r.alpha;
}
compat::Slice<std::shared_ptr<Drawable>> enumerateDrawablesForAutoMesh(NodeSlice targets) {
    compat::Slice<std::shared_ptr<Drawable>> list;
    const auto visit = [&](auto&& recurse, std::shared_ptr<Node> node) -> void {
        if (!node) return;
        if (node->coverOthers()) {
            const auto children = node->children;
            for (auto child : children) recurse(recurse, child);
            return;
        }
        if (auto comp = std::dynamic_pointer_cast<Composite>(node)) {
            if (comp->propagateMeshGroup) {
                const auto children = node->children;
                for (auto child : children) recurse(recurse, child);
            }
            return;
        }
        if (auto drawable = std::dynamic_pointer_cast<Drawable>(node)) list.append(drawable);
    };
    for (auto target : targets) visit(visit, target);
    return list;
}
void alphaPreviewDisposeTexture() {
    if (previewTexture) {
        const auto texture = previewTexture;
        texture->dispose();
        previewTexture.reset();
    }
    previewWidth = 0; previewHeight = 0; previewSignature.reset();
}
void alphaPreviewWidget(AlphaPreviewState& state, ImVec2 size) {
    widgets::ngCheckbox("show_alpha_preview", &state.show);
    if (!state.show) return;
    const auto nodes = project::incSelectedNodes();
    compat::RetainedStringSlice signature;
    for (const auto node : nodes) {
        const auto id = compat::ngRequireObject(node)->getUUID();
        try {
            const auto suffix = std::to_string(id) + ":";
            auto next = std::string(signature.view()) + suffix;
            signature = compat::RetainedString(std::move(next));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    if (signature != previewSignature) {
        previewSignature = signature;
        std::shared_ptr<IAlphaProvider> provider;
        compat::ngRunWithScopeExit([&] {
            if (nodes.size() == 1) {
                if (auto part = std::dynamic_pointer_cast<Part>(nodes[0]))
                    provider = PartAlphaProvider::createSource(part);
                else if (std::dynamic_pointer_cast<Deformable>(nodes[0]) || std::dynamic_pointer_cast<Composite>(nodes[0]))
                    provider = DeformableAlphaProvider::createSource(nodes[0]);
            } else if (nodes.size() > 1) provider = GenericProjectionAlphaProvider::createSource(nodes);
            if (provider && provider->width() > 0 && provider->height() > 0) {
                const int w = provider->width(), h = provider->height();
                const auto ap = provider->alphaPtr();
                if (previewTexture && (previewWidth != w || previewHeight != h)) {
                    const auto texture = previewTexture;
                    texture->dispose();
                    previewTexture.reset();
                }
                if (!previewTexture) {
                    previewTexture = Texture::createSource(w, h); previewWidth = w; previewHeight = h;
                }
                const auto rgbaLength = static_cast<std::size_t>(mul(mul(w, h), 4));
                SourceTextureBytes rgba;
                try {
                    rgba = SourceTextureBytes(rgbaLength, 0);
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
                const auto count = mul(w, h);
                for (int i = 0; i < count; ++i) {
                    if (!ap) compat::ngFailNullAccess(false);
                    const auto alpha = ap[i];
                    for (int channel = 0; channel < 4; ++channel) {
                        const auto index = static_cast<std::size_t>(add(mul(i, 4), channel));
                        if (index >= rgba.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                            index, rgba.size(), compat::RetainedString::fromStaticLiteral(
                                "source/nijigenerate/viewport/vertex/automesh/alpha_provider.d"), 364 + channel);
                        rgba[index] = channel == 3 ? alpha : 0;
                    }
                }
                const auto texture = previewTexture;
                texture->setData(rgba);
            }
        }, [&] { if (provider) provider->dispose(); });
    }
    if (previewTexture) widgets::ngTextureSlot("Alpha", previewTexture, size);
}
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::automesh::ProjectionResult> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.automesh.alpha_provider.ProjectionResult)";
    inline static constexpr char unqualifiedArgument[] = "(ProjectionResult)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.automesh.alpha_provider.ProjectionResult";
    inline static constexpr char unqualifiedElement[] = "ProjectionResult";
};
}
