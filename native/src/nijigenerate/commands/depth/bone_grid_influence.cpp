#include "detail/bone_grid_influence.hpp"
#include "detail/bone_grid_stopper.hpp"
#include "detail/bone_grid_index.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <common/nullable_string.hpp>
#include <param/binding_impl.hpp>
#include <algorithm>

namespace nijigenerate::commands::depth::bone::detail {
using nicxlive::core::math::Mat4;
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec2Array;
using nicxlive::core::nodes::Deformable;
using nicxlive::core::nodes::GridDeformer;
using nicxlive::core::nodes::Node;
using nicxlive::core::param::DeformationParameterBinding;
using nicxlive::core::param::Parameter;

namespace {
// Preserve the original column-wise float accumulation, including the w=0 products.
Vec2 gridTransformXY(const Mat4& matrix, float x, float y, float w) {
    const float input[4]{x, y, 0.0f, w};
    float output[4]{0, 0, 0, 0};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            output[row] = nicxlive::core::math::detail::inmathAccumulate(output[row],
                nicxlive::core::math::detail::inmathProduct(matrix[row][column], input[column]));
    return {output[0], output[1]};
}

void resetGridAccumulation(Vec2Array& accumulated, std::size_t count) {
    accumulated.setLength(count);
    for (std::size_t i = 0; i < count; ++i) accumulated.xAt(i) = 0.0f;
    for (std::size_t i = 0; i < count; ++i) accumulated.yAt(i) = 0.0f;
}

float gridSourceAxis(const compat::ConstSlice<float>& axis, std::size_t index, std::size_t line) {
    if (index >= axis.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, axis.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), line);
    return axis[index];
}

Vec2 gridSourceOffset(const Vec2Array& offsets, std::size_t index) {
#ifndef NDEBUG
    if (index >= offsets.size()) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("veca index out of range"),
        compat::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 139);
#endif
    if (index >= offsets.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, offsets.size(),
        compat::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 467);
    return {offsets.xAt(index), offsets.yAt(index)};
}

void addGridSourceOffset(Vec2Array& offsets, std::size_t index, Vec2 value) {
    const auto previous = gridSourceOffset(offsets, index);
    const Vec2 next{previous.x + value.x, previous.y + value.y};
    offsets.xAt(index) = next.x;
    offsets.yAt(index) = next.y;
}
} // namespace

bool sampleGridOffset(std::shared_ptr<GridDeformer> grid, Vec2Array offsets,
    compat::ConstSlice<float> axisX, compat::ConstSlice<float> axisY, Vec2 point, Vec2& sampled) {
    sampled = {0.0f, 0.0f};
    if (!grid || offsets.size() != grid->getVertices().size() || axisX.size() < 2 || axisY.size() < 2)
        return false;

    // The primary same-float min/max overloads preserve their first operand on ties and NaNs.
    const auto x = std::min(std::max(point.x, gridSourceAxis(axisX, 0, 2157)),
        gridSourceAxis(axisX, axisX.size() - 1, 2157));
    const auto y = std::min(std::max(point.y, gridSourceAxis(axisY, 0, 2158)),
        gridSourceAxis(axisY, axisY.size() - 1, 2158));
    const auto xi = gridAxisInterval(axisX, x);
    const auto yi = gridAxisInterval(axisY, y);
    const auto xSpan = gridSourceAxis(axisX, xi + 1, 2161) - gridSourceAxis(axisX, xi, 2161);
    const auto ySpan = gridSourceAxis(axisY, yi + 1, 2162) - gridSourceAxis(axisY, yi, 2162);
    const auto u = xSpan > 0.0f ? (x - gridSourceAxis(axisX, xi, 2163)) / xSpan : 0.0f;
    const auto v = ySpan > 0.0f ? (y - gridSourceAxis(axisY, yi, 2164)) / ySpan : 0.0f;

    const auto indexAt = [&](std::size_t xIndex, std::size_t yIndex, std::size_t line) {
        const auto vertices = grid->getVertices();
        const auto axisValueX = gridSourceAxis(axisX, xIndex, line);
        const auto axisValueY = gridSourceAxis(axisY, yIndex, line);
        return gridVertexIndex(vertices, axisValueX, axisValueY);
    };
    const auto i00 = indexAt(xi, yi, 2166);
    const auto i10 = indexAt(xi + 1, yi, 2167);
    const auto i01 = indexAt(xi, yi + 1, 2168);
    const auto i11 = indexAt(xi + 1, yi + 1, 2169);
    if (i00 < 0 || i10 < 0 || i01 < 0 || i11 < 0) return false;

    const auto p00 = gridSourceOffset(offsets, static_cast<std::size_t>(i00));
    const auto w00 = (1.0f - u) * (1.0f - v);
    Vec2 result{p00.x * w00, p00.y * w00};
    const auto p10 = gridSourceOffset(offsets, static_cast<std::size_t>(i10));
    const auto w10 = u * (1.0f - v);
    result.x += p10.x * w10;
    result.y += p10.y * w10;
    const auto p01 = gridSourceOffset(offsets, static_cast<std::size_t>(i01));
    const auto w01 = (1.0f - u) * v;
    result.x += p01.x * w01;
    result.y += p01.y * w01;
    const auto p11 = gridSourceOffset(offsets, static_cast<std::size_t>(i11));
    const auto w11 = u * v;
    result.x += p11.x * w11;
    result.y += p11.y * w11;
    sampled = result;
    return true;
}

bool addParentGridInfluence(std::shared_ptr<GridDeformer> parentGrid, std::shared_ptr<Deformable> target,
    Vec2Array parentOffsets, Vec2Array& accumulated) {
    if (!parentGrid || !target || parentOffsets.size() != parentGrid->getVertices().size()) return false;
    compat::Slice<float> axisX;
    compat::Slice<float> axisY;
    if (!prepareGridOffsetSampling(parentGrid, parentOffsets, axisX, axisY)) return false;

    if (accumulated.size() != target->getVertices().size())
        resetGridAccumulation(accumulated, target->getVertices().size());

    const auto parentInverse = core::math::source_matrix::inverse(parentGrid->transform().matrix());
    const auto targetMatrix = target->transform().matrix();
    const auto targetToParent = parentInverse * targetMatrix;
    const auto parentToTarget = core::math::source_matrix::inverse(targetToParent);
    const auto vertices = target->getVertices();
    const auto count = vertices.size();
    for (std::size_t i = 0; i < count; ++i) {
        const auto vertex = gridSourceOffset(vertices, i);
        auto parentPoint = gridTransformXY(targetToParent, vertex.x, vertex.y, 1.0f);
        if (parentGrid->dynamic && i < accumulated.size()) {
            const auto priorX = gridSourceOffset(accumulated, i).x;
            const auto priorY = gridSourceOffset(accumulated, i).y;
            const auto prior = gridTransformXY(targetToParent, priorX, priorY, 0.0f);
            parentPoint.x += prior.x;
            parentPoint.y += prior.y;
        }
        Vec2 parentOffset;
        if (!sampleGridOffset(parentGrid, parentOffsets, axisX, axisY, parentPoint, parentOffset)) continue;
        const auto targetOffset = gridTransformXY(parentToTarget, parentOffset.x, parentOffset.y, 0.0f);
        addGridSourceOffset(accumulated, i, targetOffset);
    }
    return true;
}

bool addParentGridNodeOriginInfluence(std::shared_ptr<GridDeformer> parentGrid,
    std::shared_ptr<Node> stopper, std::shared_ptr<Deformable> target,
    Vec2Array parentOffsets, Vec2Array& accumulated) {
    if (!parentGrid || !stopper || !target || std::dynamic_pointer_cast<Deformable>(stopper) ||
        !parentGrid->translateChildren) return false;

    compat::Slice<float> axisX;
    compat::Slice<float> axisY;
    if (!prepareGridOffsetSampling(parentGrid, parentOffsets, axisX, axisY)) return false;

    const auto stopperParentMatrix = !stopper->parentRef()
        ? Mat4::identity() : stopper->parentRef()->transform().matrix();
    const auto stopperToParentGrid = core::math::source_matrix::inverse(parentGrid->transform().matrix())
        * stopperParentMatrix;
    auto origin = gridTransformXY(stopperToParentGrid,
        stopper->localTransform.translation.x, stopper->localTransform.translation.y, 1.0f);
    if (parentGrid->dynamic) {
        const auto offsetX = stopper->getValueSource(compat::RetainedString::fromStaticLiteral("transform.t.x"));
        const auto offsetY = stopper->getValueSource(compat::RetainedString::fromStaticLiteral("transform.t.y"));
        const auto offset = gridTransformXY(stopperToParentGrid, offsetX, offsetY, 0.0f);
        origin.x += offset.x;
        origin.y += offset.y;
    }

    Vec2 parentOffset;
    if (!sampleGridOffset(parentGrid, parentOffsets, axisX, axisY, origin, parentOffset)) return false;

    const auto accumulatedLength = accumulated.size();
    const auto targetLength = target->getVertices().size();
    if (accumulatedLength != targetLength)
        resetGridAccumulation(accumulated, target->getVertices().size());
    const auto targetInverse = core::math::source_matrix::inverse(target->transform().matrix());
    const auto parentMatrix = parentGrid->transform().matrix();
    const auto parentToTarget = targetInverse * parentMatrix;
    const auto targetOffset = gridTransformXY(parentToTarget, parentOffset.x, parentOffset.y, 0.0f);
    for (std::size_t i = 0; i < accumulated.size(); ++i) accumulated.xAt(i) += targetOffset.x;
    for (std::size_t i = 0; i < accumulated.size(); ++i) accumulated.yAt(i) += targetOffset.y;
    return true;
}

Vec2Array depthBoneAncestorGridInfluence(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<Deformable> target, std::shared_ptr<Parameter> parameter,
    compat::Vec2u keypoint, Vec2Array targetOffsets, DepthBoneResolvedOffsets& resolvedOffsets) {
    const auto targetOffsetLength = targetOffsets.size();
    const auto initialTargetLength = compat::ngRequireObject(target)->getVertices().size();
    Vec2Array result = targetOffsetLength == initialTargetLength
        ? targetOffsets.dup() : Vec2Array{};
    const auto resultLength = result.size();
    const auto targetLength = target->getVertices().size();
    if (resultLength != targetLength)
        resetGridAccumulation(result, target->getVertices().size());
    const auto original = result.dup();

    compat::Slice<std::shared_ptr<Node>> ancestors;
    for (auto cursor = target->parentRef(); cursor; cursor = cursor->parentRef()) {
        if (std::dynamic_pointer_cast<GridDeformer>(cursor)) ancestors.append(cursor);
    }
    for (std::size_t i = ancestors.size(); i > 0; --i) {
        const auto parentGrid = std::dynamic_pointer_cast<GridDeformer>(ancestors[i - 1]);
        std::shared_ptr<Node> stopper;
        if (!parentGrid) continue;
        const auto parentUuid = parentGrid->getUUID();
        if (compat::ngRequireObject(root)->findBindingIndex(parentUuid) < 0 ||
            !parentGridInfluenceCanReachTarget(root, parentGrid, target) ||
            !gridPropagationStopper(parentGrid, target, stopper)) continue;

        Vec2Array parentOffsets;
        const auto resolved = resolvedOffsets.find(static_cast<std::uint32_t>(parentGrid->getUUID()));
        if (resolved != resolvedOffsets.end()) {
            parentOffsets = resolved->second.dup();
        } else if (parameter) {
            const auto binding = std::dynamic_pointer_cast<DeformationParameterBinding>(
                parameter->getBinding(parentGrid,
                    nicxlive::core::common::NullableString::fromStaticLiteral("deform")));
            if (binding) parentOffsets = binding->getValue({keypoint.x, keypoint.y}).vertexOffsets.dup();
        } else {
            parentOffsets = parentGrid->deformation.dup();
        }
        if (!stopper) addParentGridInfluence(parentGrid, target, parentOffsets, result);
        else addParentGridNodeOriginInfluence(parentGrid, stopper, target, parentOffsets, result);
    }
    result -= original;
    return result;
}
} // namespace nijigenerate::commands::depth::bone::detail
