#pragma once

#include <cmath>
#include <limits>

#include <math/veca.hpp>
#include "nijigenerate/ext/nodes/exdepthops.hpp"
#include "nijigenerate/ext/nodes/exgriddeformer_node.hpp"

namespace nijigenerate::ext::nodes {

inline compat::Slice<ExDepthOp> ngRemapGridIndexBoundDepthOperations(
    nicxlive::core::math::Vec2Array oldVertices,
    nicxlive::core::math::Vec2Array newVertices,
    compat::Slice<ExDepthOp> operations, bool preserveSameCount = true) {
    constexpr float matchingVertexDistanceSquared = 1.0e-8f;
    const bool preserveAttachedIndices = preserveSameCount && oldVertices.size() == newVertices.size();
    compat::Slice<ExDepthOp> result;
    for (auto operation : operations) {
        if (operation.type != ExDepthOpType::AttachedPoint) {
            result.append(operation);
            continue;
        }
        if (operation.index >= oldVertices.size() || newVertices.empty()) continue;
        if (preserveAttachedIndices) {
            result.append(operation);
            continue;
        }
        const auto source = oldVertices.get(operation.index);
        float bestDistance = std::numeric_limits<float>::infinity();
        std::size_t bestIndex = 0;
        bool found = false;
        for (std::size_t i = 0; i < newVertices.size(); ++i) {
            const auto candidate = newVertices.get(i);
            const float dx = candidate.x - source.x;
            const float dy = candidate.y - source.y;
            const float distance = dx * dx + dy * dy;
            if (!std::isfinite(distance) || (found && distance >= bestDistance)) continue;
            bestDistance = distance;
            bestIndex = i;
            found = true;
        }
        if (!found || bestDistance > matchingVertexDistanceSquared) continue;
        operation.index = bestIndex;
        result.append(operation);
    }
    return result;
}

} // namespace nijigenerate::ext::nodes

