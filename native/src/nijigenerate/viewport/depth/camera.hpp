#pragma once
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>

#include <cstdint>
#include <cmath>
#include <memory>
#include <math/types.hpp>
#include <compat/object_metadata.hpp>

struct ImGuiIO;
namespace nicxlive::core::nodes { class GridDeformer; }

namespace nijigenerate::viewport::depth {

enum class DepthToolMode : std::int32_t {
    DirectDepth,
    Ring,
    AttachedPoint,
    Plane,
};

struct DepthBrushSettings {
    bool snapToGrid{true};
    float amount{0.18f};
    float radiusX{58.0f};
    float radiusY{42.0f};
    float angle{0.0f};
    float hardness{1.85f};
    float flattenStrength{0.75f};
};

struct DepthCamera3D {
    float yaw{0.0f};
    float pitch{0.0f};
    float zoom{1.0f};
    nicxlive::core::math::Vec2 pan{0.0f, 0.0f};
};

bool ngUpdateDepthCamera3D(DepthCamera3D& camera, ImGuiIO* io,
    bool rotateRequested, bool panRequested, bool zoomRequested);
inline bool updateDepthCamera3D(DepthCamera3D& camera, ImGuiIO* io,
    bool rotateRequested, bool panRequested, bool zoomRequested) {
    return ngUpdateDepthCamera3D(camera, io, rotateRequested, panRequested, zoomRequested);
}
void focusGridOrigin(std::shared_ptr<nicxlive::core::nodes::GridDeformer> grid);

inline nicxlive::core::math::Vec2 projectDepthPoint(nicxlive::core::math::Vec2 point, float depth,
    DepthCamera3D& camera) {
    const float cy = std::cos(camera.yaw);
    const float sy = std::sin(camera.yaw);
    const float cp = std::cos(camera.pitch);
    const float sp = std::sin(camera.pitch);
    const float x = point.x;
    const float y = point.y;
    const float z = depth;
    const float rx = x * cy + z * sy;
    const float rz = -x * sy + z * cy;
    const float ry = y * cp - rz * sp;
    return {rx * camera.zoom + camera.pan.x, ry * camera.zoom + camera.pan.y};
}

inline nicxlive::core::math::Vec2 unprojectDepthPoint(nicxlive::core::math::Vec2 point, float depth,
    DepthCamera3D& camera) {
    float cy = std::cos(camera.yaw);
    const float sy = std::sin(camera.yaw);
    float cp = std::cos(camera.pitch);
    const float sp = std::sin(camera.pitch);
    if (std::abs(cy) < 0.0001f) cy = cy < 0 ? -0.0001f : 0.0001f;
    if (std::abs(cp) < 0.0001f) cp = cp < 0 ? -0.0001f : 0.0001f;
    const nicxlive::core::math::Vec2 normalized{
        (point.x - camera.pan.x) / camera.zoom, (point.y - camera.pan.y) / camera.zoom};
    const float x = (normalized.x - depth * sy) / cy;
    const float rz = -x * sy + depth * cy;
    const float y = (normalized.y + rz * sp) / cp;
    return {x, y};
}

} // namespace nijigenerate::viewport::depth

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::viewport::depth::DepthToolMode> {
    using Enum = ::nijigenerate::viewport::depth::DepthToolMode;
    inline static constexpr std::string_view name = "DepthToolMode";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::DirectDepth, "DirectDepth"},
        FormatEnumMember<Enum>{Enum::Ring, "Ring"},
        FormatEnumMember<Enum>{Enum::AttachedPoint, "AttachedPoint"},
        FormatEnumMember<Enum>{Enum::Plane, "Plane"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {

template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::DepthToolMode> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.camera.DepthToolMode)";
    inline static constexpr char unqualifiedArgument[] = "(DepthToolMode)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.camera.DepthToolMode";
    inline static constexpr char unqualifiedElement[] = "DepthToolMode";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::DepthBrushSettings> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.camera.DepthBrushSettings)";
    inline static constexpr char unqualifiedArgument[] = "(DepthBrushSettings)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.camera.DepthBrushSettings";
    inline static constexpr char unqualifiedElement[] = "DepthBrushSettings";
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::DepthCamera3D> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.camera.DepthCamera3D)";
    inline static constexpr char unqualifiedArgument[] = "(DepthCamera3D)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.camera.DepthCamera3D";
    inline static constexpr char unqualifiedElement[] = "DepthCamera3D";
};
}
