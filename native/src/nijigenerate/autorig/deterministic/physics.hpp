#pragma once

#include "contracts.hpp"
#include "nijigenerate/compat/slice.hpp"
#include <common/resource_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_map>

namespace nijigenerate::autorig::deterministic::physics {

using contracts::Point2;
using PhysicsAffineTransform = compat::FixedArray<double, 6>;
using Json = core::settings::SettingsJsonValue;
struct RigPhysicsAsset {
    compat::Slice<Point2> points;
    compat::Slice<bool> mask;
    std::size_t width = 0, height = 0;
    PhysicsAffineTransform pixelToModel;
    bool affineMapping = false;
};
struct RigPhysicsMesh {
    compat::Slice<Point2> vertices;
    compat::Slice<Point2> uv;
    compat::Slice<std::uint32_t> indices;
    // Row-major neutral affine transform: xx, xy, tx, yx, yy, ty.
    PhysicsAffineTransform toModel;
};

struct RigPhysicsFrame {
    Point2 origin;
    Point2 tangent;
    Point2 normal;
    double length = std::numeric_limits<double>::quiet_NaN();
};
RigPhysicsFrame ngRigPhysicsFrame(Point2 fixed, Point2 free);
bool ngRigPhysicsNoInversion(compat::Slice<Point2> vertices, compat::Slice<std::uint32_t> indices,
                             compat::Slice<Point2> values, double scale = 1.0);

Point2 ngRigPhysicsTransform(PhysicsAffineTransform matrix, Point2 point, bool direction = false);
PhysicsAffineTransform ngRigPhysicsInverse(PhysicsAffineTransform matrix);
namespace detail {
struct PhysicsUuidHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        if constexpr(sizeof(std::size_t)==4) return static_cast<std::size_t>(value^(value>>32));
        return static_cast<std::size_t>(value);
    }
};
}
template<class Value> using PhysicsUuidMap = nicxlive::core::common::ResourceKeyMap<std::uint64_t,Value,
    detail::PhysicsUuidHash,std::equal_to<std::uint64_t>>;
using PhysicsAssets = PhysicsUuidMap<RigPhysicsAsset>;
using PhysicsMeshes = PhysicsUuidMap<RigPhysicsMesh>;
Json ngRigPhysicsPolicy();
compat::Slice<double> ngRigPhysicsWeights(Json, compat::Slice<Point2>, Json);
compat::Slice<Point2> ngRigPhysicsDisplacement(Json, compat::Slice<Point2>, Point2, Json);
compat::Slice<Point2> ngRigPhysicsProtectTriangles(compat::Slice<Point2>, compat::Slice<Point2>,
    compat::Slice<std::uint32_t>, Json, Json);
RigPhysicsAsset ngRigPhysicsAsset(Json, std::shared_ptr<framework::AutoRigTaskContext> = {});
Json ngRigPhysicsStructure(Json, Json, PhysicsAssets&, std::shared_ptr<framework::AutoRigTaskContext> = {});
Json ngRigCompilePhysicsOperations(Json&, PhysicsAssets, PhysicsMeshes,
    std::shared_ptr<framework::AutoRigTaskContext> = {});

} // namespace nijigenerate::autorig::deterministic::physics

namespace nicxlive::core::d_runtime {
#define NG_PHYSICS_VALUE_ORIGINS(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::autorig::deterministic::physics::Type> { \
    inline static constexpr bool isNativeObject = false; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.deterministic.physics." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.deterministic.physics." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
};
NG_PHYSICS_VALUE_ORIGINS(RigPhysicsAsset)
NG_PHYSICS_VALUE_ORIGINS(RigPhysicsMesh)
NG_PHYSICS_VALUE_ORIGINS(RigPhysicsFrame)
#undef NG_PHYSICS_VALUE_ORIGINS
}
