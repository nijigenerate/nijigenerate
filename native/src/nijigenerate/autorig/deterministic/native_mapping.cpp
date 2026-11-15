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

namespace {
nicxlive::core::math::Mat4 authoredRootMatrix(std::shared_ptr<nicxlive::core::nodes::Node> node) {
    // UV registration belongs to the authored rest mesh. Runtime parameter and
    // physics offsets must not turn a valid source mapping into a frame mismatch.
    const auto authoredTransform = [](auto&& self, std::shared_ptr<nicxlive::core::nodes::Node> node)
        -> nicxlive::core::math::Transform {
        auto local = node->localTransform;
        local.update();
        if (node->lockToRoot) {
            auto root = node->puppetRef()->root->localTransform;
            root.update();
            return local * root;
        }
        if (auto parent = node->parentPtr()) return local * self(self, parent);
        return local;
    };
    const auto root = authoredTransform(authoredTransform, node->puppetRef()->root).matrix();
    return core::math::source_matrix::inverse(root) * authoredTransform(authoredTransform, node).matrix();
}
}

contracts::Point2 toAuthoredRoot(std::shared_ptr<nicxlive::core::nodes::Node> node, double x, double y) {
    const auto point = core::math::ngTransformHomogeneousPoint(authoredRootMatrix(node),
        {static_cast<float>(x), static_cast<float>(y), 0});
    return contracts::Point2{point.x,point.y};
}

core::settings::SettingsJsonValue authoredRootTextureMapping(std::shared_ptr<nicxlive::core::nodes::Part> part) {
    const auto matrix = authoredRootMatrix(part);
    auto result = textureMapping(part);
    compat::Slice<contracts::Point2> vertices;
    for (const auto point : contracts::ngRigPoints(result.sourceIndexValue("vertices"))) {
        const auto transformed = core::math::ngTransformHomogeneousPoint(matrix,
            {static_cast<float>(point[0]), static_cast<float>(point[1]), 0});
        vertices.append(contracts::Point2{transformed.x, transformed.y});
    }
    result.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("vertices")) = contracts::ngRigPointsJson(vertices);
    return result;
}

} // namespace nijigenerate::autorig::deterministic::native_detail
