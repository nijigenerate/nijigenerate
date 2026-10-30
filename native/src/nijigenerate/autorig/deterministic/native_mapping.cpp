#include "native_mapping.hpp"
#include "geometry.hpp"
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <puppet.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {

contracts::Point2 toRoot(std::shared_ptr<nicxlive::core::nodes::Node> node, double x, double y) {
    const auto rootMatrix = node->puppetRef()->root->transform().matrix();
    const auto inverse = core::math::source_matrix::inverse(rootMatrix);
    const auto nodeMatrix = node->transform().matrix();
    const auto matrix = inverse * nodeMatrix;
    // Keep the original unguarded double-to-float conversions, after the matrices.
    const auto point = core::math::ngTransformHomogeneousPoint(matrix,
        {static_cast<float>(x), static_cast<float>(y), 0});
    return contracts::Point2{point.x, point.y};
}

core::settings::SettingsJsonValue rootTextureMapping(std::shared_ptr<nicxlive::core::nodes::Part> part) {
    auto result = textureMapping(part);
    compat::Slice<contracts::Point2> vertices;
    const auto points = contracts::ngRigPoints(result.sourceIndexValue("vertices"));
    for (const auto point : points) vertices.append(toRoot(part, point[0], point[1]));
    result.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("vertices")) = contracts::ngRigPointsJson(vertices);
    return result;
}

bool sameTextureMapping(core::settings::SettingsJsonValue a, core::settings::SettingsJsonValue b) {
    return geometry::ngRigSameTextureMapping(a, b);
}

} // namespace nijigenerate::autorig::deterministic::native_detail
