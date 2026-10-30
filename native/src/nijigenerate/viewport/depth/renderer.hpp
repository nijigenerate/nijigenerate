#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <functional>
#include <compat/object_metadata.hpp>
#include <common/resource_key_map.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

#include <math/types.hpp>
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/const_slice.hpp"
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/viewport/depth/camera.hpp"
#include "nijigenerate/viewport/depth/detail/float_sort.hpp"
#include <math/camera.hpp>

namespace nicxlive::core { class Texture; class Shader; }
namespace nicxlive::core::nodes { class Node; class Deformable; }

namespace nijigenerate::viewport::depth {

struct DepthCamera3D;
namespace common::targetview { class DepthTargetView; }

inline constexpr float DepthRenderPi = 3.14159265358979323846f;
inline constexpr std::uint64_t DepthTargetOffscreenMaxPixels = std::uint64_t{4096} * 4096;

inline bool ngDepthTargetOffscreenTextureSize(nicxlive::core::math::Vec2 minPoint,
    nicxlive::core::math::Vec2 maxPoint, std::int32_t maxTextureSize,
    std::int32_t& width, std::int32_t& height, nicxlive::core::math::Vec2& pixelScale) {
    width = 0;
    height = 0;
    pixelScale = {1.0f, 1.0f};
    if (maxTextureSize <= 0 || !std::isfinite(minPoint.x) || !std::isfinite(minPoint.y)
        || !std::isfinite(maxPoint.x) || !std::isfinite(maxPoint.y)) return false;
    const double sourceWidth = std::max(1.0, static_cast<double>(maxPoint.x) - static_cast<double>(minPoint.x));
    const double sourceHeight = std::max(1.0, static_cast<double>(maxPoint.y) - static_cast<double>(minPoint.y));
    double reduction = 1.0;
    reduction = std::min(reduction, static_cast<double>(maxTextureSize) / sourceWidth);
    reduction = std::min(reduction, static_cast<double>(maxTextureSize) / sourceHeight);
    const double sourcePixels = sourceWidth * sourceHeight;
    if (sourcePixels > static_cast<double>(DepthTargetOffscreenMaxPixels)) {
        reduction = std::min(reduction, std::sqrt(static_cast<double>(DepthTargetOffscreenMaxPixels) / sourcePixels));
    }
    if (!std::isfinite(reduction) || reduction <= 0.0) return false;
    const double roundedWidth = reduction < 1.0 ? std::floor(sourceWidth * reduction) : std::ceil(sourceWidth);
    const double roundedHeight = reduction < 1.0 ? std::floor(sourceHeight * reduction) : std::ceil(sourceHeight);
    width = static_cast<std::int32_t>(std::min(static_cast<double>(maxTextureSize), std::max(1.0, roundedWidth)));
    height = static_cast<std::int32_t>(std::min(static_cast<double>(maxTextureSize), std::max(1.0, roundedHeight)));
    pixelScale = {static_cast<float>(static_cast<double>(width) / sourceWidth),
        static_cast<float>(static_cast<double>(height) / sourceHeight)};
    return static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) <= DepthTargetOffscreenMaxPixels;
}

struct DepthTargetRenderMesh {
    compat::Slice<nicxlive::core::math::Vec2> positions;
    compat::Slice<nicxlive::core::math::Vec2> uvs;
    compat::Slice<std::uint16_t> indices;
};

struct DepthTargetRenderLine {
    nicxlive::core::math::Vec2 p0{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    nicxlive::core::math::Vec2 p1{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
};

struct DepthTargetRenderPoint {
    nicxlive::core::math::Vec2 point{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    float size{std::numeric_limits<float>::quiet_NaN()};
};

namespace detail {
inline compat::Slice<DepthTargetRenderLine> buildRenderGridLines(
    compat::Slice<nicxlive::core::math::Vec2> vertices, compat::Slice<nicxlive::core::math::Vec2> projected) {
    compat::Slice<DepthTargetRenderLine> lines;
    if (vertices.empty() || projected.size() != vertices.size()) return lines;
    compat::Slice<float> xs, ys;
    for (auto vertex : vertices) { xs.append(vertex.x); ys.append(vertex.y); }
    xs = sortedUniqueFloats(xs);
    ys = sortedUniqueFloats(ys);
    if (xs.size() < 2 || ys.size() < 2 || xs.size() * ys.size() != vertices.size()) return lines;
    struct UlongHash {
        std::size_t operator()(std::uint64_t value) const noexcept {
            if constexpr (sizeof(std::size_t) == 4)
                return static_cast<std::size_t>(value ^ (value >> 32));
            else return static_cast<std::size_t>(value);
        }
    };
    nicxlive::core::common::ResourceKeyMap<std::uint64_t, std::uint16_t,
        UlongHash, std::equal_to<std::uint64_t>> lookup;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        std::size_t xi = 0, yi = 0;
        for (std::size_t j = 0; j < xs.size(); ++j) if (xs[j] == vertices[i].x) { xi = j; break; }
        for (std::size_t j = 0; j < ys.size(); ++j) if (ys[j] == vertices[i].y) { yi = j; break; }
        try {
            lookup[yi * xs.size() + xi] = static_cast<std::uint16_t>(i);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    const auto appendLine = [&](std::uint64_t key0, std::uint64_t key1) {
        const auto p0 = lookup.find(key0), p1 = lookup.find(key1);
        if (p0 == lookup.end() || p1 == lookup.end()) return;
        if (p0->second >= projected.size() || p1->second >= projected.size()) return;
        lines.append(DepthTargetRenderLine{projected[p0->second], projected[p1->second]});
    };
    for (std::size_t y = 0; y < ys.size(); ++y)
        for (std::size_t x = 0; x < xs.size() - 1; ++x)
            appendLine(y * xs.size() + x, y * xs.size() + x + 1);
    for (std::size_t x = 0; x < xs.size(); ++x)
        for (std::size_t y = 0; y < ys.size() - 1; ++y)
            appendLine(y * xs.size() + x, (y + 1) * xs.size() + x);
    return lines;
}
inline DepthTargetRenderMesh buildRenderMesh(compat::Slice<nicxlive::core::math::Vec2> vertices,
    compat::ConstSlice<float> depths, compat::Slice<std::uint16_t> indices,
    nicxlive::core::math::Vec2 minPoint, nicxlive::core::math::Vec2 maxPoint,
    float depthDisplayScale, DepthCamera3D& depthCamera) {
    DepthTargetRenderMesh mesh;
    if (vertices.empty() || vertices.size() != depths.size()) return mesh;
    mesh.positions.resize(vertices.size());
    mesh.uvs.resize(vertices.size());
    mesh.indices = indices.duplicate();
    nicxlive::core::math::Vec2 size{maxPoint.x - minPoint.x, maxPoint.y - minPoint.y};
    if (size.x == 0) size.x = 1;
    if (size.y == 0) size.y = 1;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const auto vertex = vertices[i];
        mesh.positions[i] = projectDepthPoint(vertex, -depths[i] * depthDisplayScale, depthCamera);
        mesh.uvs[i] = {(vertex.x - minPoint.x) / size.x, 1.0f - ((vertex.y - minPoint.y) / size.y)};
    }
    return mesh;
}
inline DepthTargetRenderLine buildRenderLine(nicxlive::core::math::Vec2 p0, nicxlive::core::math::Vec2 p1,
    float depth, float depthDisplayScale, DepthCamera3D& depthCamera) {
    const auto first = projectDepthPoint(p0, -depth * depthDisplayScale, depthCamera);
    const auto second = projectDepthPoint(p1, -depth * depthDisplayScale, depthCamera);
    return {first, second};
}
inline DepthTargetRenderLine buildRenderDepthLine(nicxlive::core::math::Vec2 p0, float depth0,
    nicxlive::core::math::Vec2 p1, float depth1, float depthDisplayScale, DepthCamera3D& depthCamera) {
    const auto first = projectDepthPoint(p0, -depth0 * depthDisplayScale, depthCamera);
    const auto second = projectDepthPoint(p1, -depth1 * depthDisplayScale, depthCamera);
    return {first, second};
}
inline DepthTargetRenderPoint buildRenderPoint(nicxlive::core::math::Vec2 point, float depth,
    float depthDisplayScale, DepthCamera3D& depthCamera, float size = 1.0f) {
    return {projectDepthPoint(point, -depth * depthDisplayScale, depthCamera), size};
}

template<class BuildLine>
compat::Slice<DepthTargetRenderLine> buildRenderEllipseLines(nicxlive::core::math::Vec2 center,
    float radiusX, float radiusY, float angleDeg, float depth, float depthDisplayScale,
    DepthCamera3D& depthCamera, std::int32_t segments, BuildLine&& buildLine) {
    compat::Slice<DepthTargetRenderLine> lines;
    if (segments <= 0) return lines;
    const float angle = angleDeg * DepthRenderPi / 180.0f;
    const nicxlive::core::math::Vec2 ux{std::cos(angle), std::sin(angle)};
    const nicxlive::core::math::Vec2 uy{-std::sin(angle), std::cos(angle)};
    for (std::int32_t i = 0; i < segments; ++i) {
        const float a0 = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * DepthRenderPi;
        const float a1 = static_cast<float>(i + 1) / static_cast<float>(segments) * 2.0f * DepthRenderPi;
        const float x0 = std::cos(a0) * radiusX, y0 = std::sin(a0) * radiusY;
        const float x1 = std::cos(a1) * radiusX, y1 = std::sin(a1) * radiusY;
        const nicxlive::core::math::Vec2 l0{center.x + ux.x * x0 + uy.x * y0, center.y + ux.y * x0 + uy.y * y0};
        const nicxlive::core::math::Vec2 l1{center.x + ux.x * x1 + uy.x * y1, center.y + ux.y * x1 + uy.y * y1};
        lines.append(buildLine(l0, l1, depth, depthDisplayScale, depthCamera));
    }
    return lines;
}

template<class BuildLine>
compat::Slice<DepthTargetRenderLine> buildRenderPlaneLines(nicxlive::core::math::Vec2 minPoint,
    nicxlive::core::math::Vec2 maxPoint, float depth, float depthDisplayScale,
    DepthCamera3D& depthCamera, BuildLine&& buildLine) {
    compat::Slice<DepthTargetRenderLine> lines{
        DepthTargetRenderLine{}, DepthTargetRenderLine{}, DepthTargetRenderLine{}, DepthTargetRenderLine{}};
    lines[0] = buildLine({minPoint.x, minPoint.y}, {maxPoint.x, minPoint.y}, depth, depthDisplayScale, depthCamera);
    lines[1] = buildLine({maxPoint.x, minPoint.y}, {maxPoint.x, maxPoint.y}, depth, depthDisplayScale, depthCamera);
    lines[2] = buildLine({maxPoint.x, maxPoint.y}, {minPoint.x, maxPoint.y}, depth, depthDisplayScale, depthCamera);
    lines[3] = buildLine({minPoint.x, maxPoint.y}, {minPoint.x, minPoint.y}, depth, depthDisplayScale, depthCamera);
    return lines;
}

template<class BuildLine>
compat::Slice<DepthTargetRenderLine> buildRenderRangeLines(nicxlive::core::math::Vec2 minPoint,
    nicxlive::core::math::Vec2 maxPoint, float minDepth, float maxDepth, float depthDisplayScale,
    DepthCamera3D& depthCamera, BuildLine&& buildLine) {
    const float centerX = (minPoint.x + maxPoint.x) * 0.5f;
    compat::Slice<DepthTargetRenderLine> lines{
        DepthTargetRenderLine{}, DepthTargetRenderLine{}, DepthTargetRenderLine{}, DepthTargetRenderLine{}};
    lines[0] = buildLine({minPoint.x, minPoint.y}, {maxPoint.x, minPoint.y}, minDepth, depthDisplayScale, depthCamera);
    lines[1] = buildLine({minPoint.x, maxPoint.y}, {maxPoint.x, maxPoint.y}, maxDepth, depthDisplayScale, depthCamera);
    lines[2] = buildLine({centerX, minPoint.y}, {centerX, maxPoint.y}, minDepth, depthDisplayScale, depthCamera);
    lines[3] = buildLine({centerX, minPoint.y}, {centerX, maxPoint.y}, maxDepth, depthDisplayScale, depthCamera);
    return lines;
}
} // namespace detail

// Declaration scope only; child implementation leaves supply the method bodies.
class DepthTargetRenderer : public std::enable_shared_from_this<DepthTargetRenderer> {
    using Vec2 = nicxlive::core::math::Vec2;
    using Target = std::shared_ptr<common::targetview::DepthTargetView>;
public:
    DepthTargetRenderer() = default;
    virtual ~DepthTargetRenderer();
    DepthTargetRenderer(const DepthTargetRenderer&) = delete;
    DepthTargetRenderer& operator=(const DepthTargetRenderer&) = delete;

    virtual DepthTargetRenderMesh buildMesh(compat::Slice<Vec2> vertices, compat::ConstSlice<float> depths,
        compat::Slice<std::uint16_t> indices, Vec2 minPoint, Vec2 maxPoint,
        float depthDisplayScale, DepthCamera3D& depthCamera) {
        return detail::buildRenderMesh(vertices, depths, indices, minPoint, maxPoint, depthDisplayScale, depthCamera);
    }
    virtual DepthTargetRenderMesh buildMesh(Target target, DepthCamera3D& depthCamera);
    virtual compat::Slice<DepthTargetRenderLine> buildGridLines(compat::Slice<Vec2> vertices,
        compat::Slice<Vec2> projected) {
        return detail::buildRenderGridLines(vertices, projected);
    }
    virtual compat::Slice<DepthTargetRenderLine> buildGridLines(Target target, DepthCamera3D& depthCamera);
    virtual DepthTargetRenderLine buildLine(Vec2 p0, Vec2 p1, float depth, float depthDisplayScale,
        DepthCamera3D& depthCamera) {
        return detail::buildRenderLine(p0, p1, depth, depthDisplayScale, depthCamera);
    }
    virtual DepthTargetRenderLine buildDepthLine(Vec2 p0, float depth0, Vec2 p1, float depth1,
        float depthDisplayScale, DepthCamera3D& depthCamera) {
        return detail::buildRenderDepthLine(p0, depth0, p1, depth1, depthDisplayScale, depthCamera);
    }
    virtual DepthTargetRenderPoint buildPoint(Vec2 point, float depth, float depthDisplayScale,
        DepthCamera3D& depthCamera, float size = 1.0f) {
        return detail::buildRenderPoint(point, depth, depthDisplayScale, depthCamera, size);
    }
    virtual compat::Slice<DepthTargetRenderLine> buildEllipseLines(Vec2 center, float radiusX,
        float radiusY, float angleDeg, float depth, float depthDisplayScale,
        DepthCamera3D& depthCamera, std::int32_t segments = 48) {
        const auto receiver = weak_from_this().lock();
        return detail::buildRenderEllipseLines(center, radiusX, radiusY, angleDeg, depth, depthDisplayScale,
            depthCamera, segments, [this](Vec2 p0, Vec2 p1, float d, float s, DepthCamera3D& c) {
                return this->buildLine(p0, p1, d, s, c);
            });
    }
    virtual compat::Slice<DepthTargetRenderLine> buildPlaneLines(Vec2 minPoint, Vec2 maxPoint,
        float depth, float depthDisplayScale, DepthCamera3D& depthCamera) {
        const auto receiver = weak_from_this().lock();
        return detail::buildRenderPlaneLines(minPoint, maxPoint, depth, depthDisplayScale, depthCamera,
            [this](Vec2 p0, Vec2 p1, float d, float s, DepthCamera3D& c) {
                return this->buildLine(p0, p1, d, s, c);
            });
    }
    virtual compat::Slice<DepthTargetRenderLine> buildRangeLines(Vec2 minPoint, Vec2 maxPoint,
        float minDepth, float maxDepth, float depthDisplayScale, DepthCamera3D& depthCamera) {
        const auto receiver = weak_from_this().lock();
        return detail::buildRenderRangeLines(minPoint, maxPoint, minDepth, maxDepth, depthDisplayScale, depthCamera,
            [this](Vec2 p0, Vec2 p1, float d, float s, DepthCamera3D& c) {
                return this->buildLine(p0, p1, d, s, c);
            });
    }
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DepthTargetRenderer> other);
    virtual int opCmp(std::shared_ptr<DepthTargetRenderer> other);
protected:
    virtual void prepareSourceClassInfo();
private:
    compat::Slice<float> sortedUnique(compat::Slice<float> values) {
        return detail::sortedUniqueFloats(values);
    }
};

class DepthTextureMeshRenderer : public std::enable_shared_from_this<DepthTextureMeshRenderer> {
    std::uint32_t vao = 0, vbo = 0, ibo = 0;
    std::shared_ptr<nicxlive::core::Shader> shader, solidShader;
    int mvpUniform = -1, textureUniform = -1, solidMvpUniform = -1, solidColorUniform = -1;
    bool loggedEnsure = false, loggedDrawSuccess = false, loggedDrawSkip = false, loggedGlError = false;
    void ensureBuffers();
    void ensure();
    void ensureSolid();
public:
    DepthTextureMeshRenderer() = default;
    DepthTextureMeshRenderer(const DepthTextureMeshRenderer&) = delete;
    DepthTextureMeshRenderer& operator=(const DepthTextureMeshRenderer&) = delete;
    virtual ~DepthTextureMeshRenderer() = default;
    virtual void drawSolid(compat::Slice<nicxlive::core::math::Vec2> positions, compat::Slice<std::uint16_t> indices,
        std::shared_ptr<nicxlive::core::math::Camera> camera, nicxlive::core::math::Vec4 color);
    virtual void draw(std::shared_ptr<nicxlive::core::Texture> texture, compat::Slice<nicxlive::core::math::Vec2> positions,
        compat::Slice<nicxlive::core::math::Vec2> uvs, compat::Slice<std::uint16_t> indices,
        std::shared_ptr<nicxlive::core::math::Camera> camera);
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DepthTextureMeshRenderer> other);
    virtual int opCmp(std::shared_ptr<DepthTextureMeshRenderer> other);
protected:
    virtual void prepareSourceClassInfo();
};
class DepthTargetOffscreenTextureRenderer : public std::enable_shared_from_this<DepthTargetOffscreenTextureRenderer> {
    using Vec2 = nicxlive::core::math::Vec2;
    using Target = std::shared_ptr<nicxlive::core::nodes::Deformable>;
    using Node = std::shared_ptr<nicxlive::core::nodes::Node>;
    std::shared_ptr<nicxlive::core::Texture> texture;
    std::uint32_t textureFbo = 0;
    int textureWidth = 0, textureHeight = 0;
    Vec2 sourceSize{1, 1}, texturePixelScale{1, 1}, minPoint{0, 0}, maxPoint{1, 1};
    bool rebuildTexture(Target target);
public:
    DepthTargetOffscreenTextureRenderer() = default;
    DepthTargetOffscreenTextureRenderer(const DepthTargetOffscreenTextureRenderer&) = delete;
    DepthTargetOffscreenTextureRenderer& operator=(const DepthTargetOffscreenTextureRenderer&) = delete;
    virtual ~DepthTargetOffscreenTextureRenderer() noexcept(false);
    static compat::Slice<Node> drawableChildren(Target target);
    virtual void dispose();
    virtual std::shared_ptr<nicxlive::core::Texture> render(Target target);
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DepthTargetOffscreenTextureRenderer> other);
    virtual int opCmp(std::shared_ptr<DepthTargetOffscreenTextureRenderer> other);
protected:
    virtual void prepareSourceClassInfo();
};
} // namespace nijigenerate::viewport::depth
namespace nijigenerate::viewport::depth::renderer {
using ::nijigenerate::viewport::depth::DepthRenderPi;
using ::nijigenerate::viewport::depth::DepthTargetOffscreenMaxPixels;
using ::nijigenerate::viewport::depth::ngDepthTargetOffscreenTextureSize;
using ::nijigenerate::viewport::depth::DepthTargetRenderMesh;
using ::nijigenerate::viewport::depth::DepthTargetRenderLine;
using ::nijigenerate::viewport::depth::DepthTargetRenderPoint;
using ::nijigenerate::viewport::depth::DepthTargetRenderer;
using ::nijigenerate::viewport::depth::DepthTextureMeshRenderer;
using ::nijigenerate::viewport::depth::DepthTargetOffscreenTextureRenderer;
}

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_RENDER_VALUE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.renderer." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.renderer." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_DEPTH_RENDER_VALUE_ORIGIN(DepthTargetRenderMesh);
NG_DEPTH_RENDER_VALUE_ORIGIN(DepthTargetRenderLine);
NG_DEPTH_RENDER_VALUE_ORIGIN(DepthTargetRenderPoint);
#undef NG_DEPTH_RENDER_VALUE_ORIGIN

template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::DepthTargetRenderer> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.renderer.DepthTargetRenderer)";
    inline static constexpr char unqualifiedArgument[] = "(DepthTargetRenderer)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.renderer.DepthTargetRenderer";
    inline static constexpr char unqualifiedElement[] = "DepthTargetRenderer";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::DepthTargetRenderer> {
    inline static constexpr char className[] = "nijigenerate.viewport.depth.renderer.DepthTargetRenderer";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.depth.renderer.DepthTargetRenderer)";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::DepthTextureMeshRenderer> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.renderer.DepthTextureMeshRenderer)";
    inline static constexpr char unqualifiedArgument[] = "(DepthTextureMeshRenderer)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.renderer.DepthTextureMeshRenderer";
    inline static constexpr char unqualifiedElement[] = "DepthTextureMeshRenderer";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::DepthTextureMeshRenderer> {
    inline static constexpr char className[] = "nijigenerate.viewport.depth.renderer.DepthTextureMeshRenderer";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.depth.renderer.DepthTextureMeshRenderer)";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::DepthTargetOffscreenTextureRenderer> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.renderer.DepthTargetOffscreenTextureRenderer)";
    inline static constexpr char unqualifiedArgument[] = "(DepthTargetOffscreenTextureRenderer)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.renderer.DepthTargetOffscreenTextureRenderer";
    inline static constexpr char unqualifiedElement[] = "DepthTargetOffscreenTextureRenderer";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::DepthTargetOffscreenTextureRenderer> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.renderer.DepthTargetOffscreenTextureRenderer";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.renderer.DepthTargetOffscreenTextureRenderer)";
};
} // namespace nicxlive::core::d_runtime
