#pragma once

#include "exdepthbone.hpp"
#include <nodes/node.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <algorithm>
#include <cstdint>

namespace nijigenerate::ext::nodes::detail {

inline std::shared_ptr<nicxlive::core::nodes::Node> depthRigChildAt(
        nicxlive::core::nodes::Node::ChildArray children, std::size_t index, std::size_t sourceLine) {
    if (index >= children.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
        index, children.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), sourceLine);
    return children[index];
}

inline void collectDepthRigUuidMap(std::shared_ptr<nicxlive::core::nodes::Node> sourceNode,
        std::shared_ptr<nicxlive::core::nodes::Node> copiedNode, DepthRigUuidMap& uuidMap) {
    if (!sourceNode || !copiedNode) return;
    const std::uint64_t sourceUuid = sourceNode->getUUID();
    const std::uint64_t copiedUuid = copiedNode->getUUID();
    uuidMap[sourceUuid] = copiedUuid;
    const auto sourceLength = sourceNode->children.size();
    const auto copiedLength = copiedNode->children.size();
    const auto childCount = sourceLength < copiedLength ? sourceNode->children.size() : copiedNode->children.size();
    for (std::size_t i = 0; i < childCount; ++i) {
        const auto sourceChild = depthRigChildAt(sourceNode->children, i, 485);
        const auto copiedChild = depthRigChildAt(copiedNode->children, i, 485);
        collectDepthRigUuidMap(sourceChild, copiedChild, uuidMap);
    }
}

inline nicxlive::core::math::Vec3 depthSkeletonPoint(const nicxlive::core::math::Mat4& rootToLocal,
        float width, float height, float cx, float top, float bottom, float scale,
        float xRatio, float yRatio, float z = 0) {
    const float world[4]{cx + xRatio * width, top + yRatio * (bottom - top) * scale, z * height, 1};
    float result[4]{0, 0, 0, 0};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            result[row] = nicxlive::core::math::detail::inmathAccumulate(result[row],
                nicxlive::core::math::detail::inmathProduct(rootToLocal[row][column], world[column]));
    return {result[0], result[1], result[2]};
}

} // namespace nijigenerate::ext::nodes::detail
