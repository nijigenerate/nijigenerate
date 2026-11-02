// Existing regression.d15304-15345/15603-15656; include once outside all namespaces.
#include "root_existing_depth_gpu.hpp"
#include "uint_associative_array.hpp"
#include <nijigenerate/commands/depth/bone_constants.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/ext/nodes/exgriddeformer_node.hpp>
#include <nijigenerate/platform/detail/path_allocation.hpp>
#include <math/mat4.hpp>
#include <common/shared_array.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>

namespace nijigenerate_tests::regression::detail {
extern thread_local std::uint32_t fakeDepthBoneGpuNextJobId;
extern thread_local UIntAssociativeArray<std::size_t> fakeDepthBoneGpuJobVertexCounts;
extern thread_local std::uint32_t fakeDepthBoneGpuSubmitCount;
extern thread_local std::uint32_t fakeDepthBoneGpuPollCount;
extern thread_local std::uint32_t fakeDepthBoneGpuSubmitFailAfter;
extern thread_local std::uint32_t fakeDepthBoneGpuNotReadyPolls;
extern thread_local float fakeDepthBoneGpuOutputX;
extern thread_local float fakeDepthBoneGpuOutputY;
extern thread_local float fakeDepthBoneGpuLastSourceDepthOffset;
extern thread_local float fakeDepthBoneGpuLastSourceSinRotation;
extern thread_local float fakeDepthBoneGpuLastSourceCosRotation;
extern thread_local bool fakeDepthBoneGpuVaryOutputByVertex;
extern thread_local bool fakeDepthBoneGpuMutateDepthOnReady;
extern thread_local nicxlive::core::common::SharedArray<float> fakeDepthBoneGpuLastBones;
extern thread_local std::shared_ptr<nijigenerate::ext::nodes::ExGridDeformer> fakeDepthBoneGpuMutatingTarget;
extern thread_local std::uint32_t fakeDepthBoneTransformNextJobId;
extern thread_local UIntAssociativeArray<
    nijigenerate::commands::depth::bone_gpu_async::NgDepthBoneGpuAsyncResult> fakeDepthBoneTransformResults;
extern thread_local std::uint32_t fakeDepthBoneTransformSubmitCount;
extern thread_local std::uint32_t fakeDepthBoneTransformNonIdentitySourceCount;
extern thread_local nicxlive::core::common::SharedArray<float> fakeDepthBoneTransformLastZOffsets;
extern thread_local nicxlive::core::common::SharedArray<nicxlive::core::math::Vec3>
    fakeDepthBoneTransformLastDeformedPoints;
nicxlive::core::math::Mat4 emulateDepthBoneShaderMat4FromRowMajorPacket(
    std::span<const float> values);

bool fakeDepthBoneTransformSubmit(
    nijigenerate::commands::depth::bone_gpu_async::DepthBoneGpuDispatchPacket& packet,
    std::uint32_t& jobId, nijigenerate::compat::RetainedStringSlice& error) {
    using namespace nijigenerate::commands::depth::bone;
    using nijigenerate::commands::depth::bone_gpu_async::DepthBoneGpuDispatchPacket;
    using nijigenerate::commands::depth::bone_gpu_async::NgDepthBoneGpuAsyncResult;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::core::translation::TypedFormatArgument;
    using nijigenerate::core::translation::ngFormatArgument;
    using nijigenerate::core::translation::ngFormatTyped;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::Vec4;
    jobId = 0;
    error = {};
    if (packet.sourceCount != 1 || packet.boneCount == 0 ||
        packet.sources.size() < DepthBoneGpuSourceStride || packet.depths.size() != packet.vertices.size()) {
        error = RetainedString::fromStaticLiteral(
            "depth-driven transform fixture requires one valid source and one depth per vertex");
        return false;
    }

    const auto boneIndex = static_cast<std::uint32_t>(packet.sources[0]);
    const auto fullMatrixOffset = static_cast<std::size_t>(boneIndex) * DepthBoneGpuBoneStride + 8;
    const auto noYawMatrixOffset = static_cast<std::size_t>(DepthBoneGpuSourceNoYawSkinMatrixIndex);
    if (fullMatrixOffset + 16 > packet.bones.size() ||
        noYawMatrixOffset + 16 > packet.sources.size()) {
        error = RetainedString::fromStaticLiteral("depth-driven transform fixture matrix is out of bounds");
        return false;
    }
    const auto fullSkinMatrix = emulateDepthBoneShaderMat4FromRowMajorPacket(
        std::span<const float>(packet.bones.data() + fullMatrixOffset, 16));
    const auto noYawSkinMatrix = emulateDepthBoneShaderMat4FromRowMajorPacket(
        std::span<const float>(packet.sources.data() + noYawMatrixOffset, 16));
    const auto depthScale = packet.sources[5];
    const auto depthOffset = packet.sources[6];
    const auto sinRotation = packet.sources[DepthBoneGpuSourceSinRotationIndex];
    const auto cosRotation = packet.sources[DepthBoneGpuSourceCosRotationIndex];
    const auto rotationPivotXShift = packet.sources[DepthBoneGpuSourceRotationPivotXShiftIndex];
    const auto poseYaw = packet.sources[DepthBoneGpuSourcePoseYawIndex];
    for (std::size_t i = fullMatrixOffset; i < fullMatrixOffset + 16; ++i) {
        if (!std::isfinite(packet.bones[i])) {
            error = RetainedString::fromStaticLiteral(
                "depth-driven transform fixture full skin matrix contains a non-finite value");
            return false;
        }
    }
    for (std::size_t i = noYawMatrixOffset; i < noYawMatrixOffset + 16; ++i) {
        if (!std::isfinite(packet.sources[i])) {
            error = RetainedString::fromStaticLiteral(
                "depth-driven transform fixture no-yaw skin matrix contains a non-finite value");
            return false;
        }
    }
    if (depthScale != 1.0f || depthOffset != 0.0f) ++fakeDepthBoneTransformNonIdentitySourceCount;

    NgDepthBoneGpuAsyncResult result;
    result.ready = true;
    result.xs.resize(packet.vertices.size(), std::numeric_limits<float>::quiet_NaN());
    result.ys.resize(packet.vertices.size(), std::numeric_limits<float>::quiet_NaN());
    fakeDepthBoneTransformLastZOffsets.resize(packet.vertices.size());
    fakeDepthBoneTransformLastDeformedPoints.resize(packet.vertices.size());
    for (std::size_t i = 0; i < packet.vertices.size(); ++i) {
        const auto vertex = packet.vertices[i].toVector();
        const auto sourceDepth = packet.depths[i] * depthScale + depthOffset;
        const Vec4 sourceLocal{vertex.x, vertex.y, sourceDepth, 1.0f};
        const auto sourceInRoot = packet.targetToRoot * sourceLocal;
        Vec4 deformedInRoot;
        if (poseYaw == 0.0f || rotationPivotXShift == 0.0f ||
            (sinRotation == 0.0f && cosRotation == 1.0f)) {
            deformedInRoot = fullSkinMatrix * sourceInRoot;
        } else {
            const auto rotationInRoot = sourceInRoot + Vec4{rotationPivotXShift, 0.0f, 0.0f, 0.0f};
            const auto baseDeformed = noYawSkinMatrix * sourceInRoot;
            const auto rotationWithYaw = fullSkinMatrix * rotationInRoot;
            const auto rotationWithoutYaw = noYawSkinMatrix * rotationInRoot;
            deformedInRoot = baseDeformed + rotationWithYaw - rotationWithoutYaw;
        }
        if (!std::isfinite(deformedInRoot.x) || !std::isfinite(deformedInRoot.y) ||
            !std::isfinite(deformedInRoot.z) || !std::isfinite(deformedInRoot.w)) {
            const std::array<TypedFormatArgument, 5> arguments{
                ngFormatArgument(i), ngFormatArgument(sourceDepth), ngFormatArgument(poseYaw),
                ngFormatArgument(sinRotation), ngFormatArgument(cosRotation)};
            auto rendered = ngFormatTyped(
                "depth-driven transform fixture produced a non-finite deformed point at vertex %s: depth=%s yaw=%s sinR=%s cosR=%s",
                arguments);
            error = nijigenerate::platform::detail::ngPathAllocation([&] {
                return RetainedString(std::move(rendered));
            });
            return false;
        }
        const auto restLocal = packet.rootToTarget * sourceInRoot;
        const auto deformedLocal = packet.rootToTarget * deformedInRoot;
        result.xs[i] = deformedLocal.x - restLocal.x;
        result.ys[i] = deformedLocal.y - restLocal.y;
        fakeDepthBoneTransformLastZOffsets[i] = deformedLocal.z - restLocal.z;
        fakeDepthBoneTransformLastDeformedPoints[i] = Vec3{deformedLocal.x, deformedLocal.y, deformedLocal.z};
        if (!std::isfinite(result.xs[i]) || !std::isfinite(result.ys[i])) {
            const std::array<TypedFormatArgument, 5> arguments{
                ngFormatArgument(i), ngFormatArgument(static_cast<float>(restLocal.x)),
                ngFormatArgument(static_cast<float>(restLocal.y)),
                ngFormatArgument(static_cast<float>(deformedLocal.x)),
                ngFormatArgument(static_cast<float>(deformedLocal.y))};
            auto rendered = ngFormatTyped(
                "depth-driven transform fixture produced a non-finite output offset at vertex %s: rest=(%s,%s) deformed=(%s,%s)",
                arguments);
            error = nijigenerate::platform::detail::ngPathAllocation([&] {
                return RetainedString(std::move(rendered));
            });
            return false;
        }
    }

    jobId = fakeDepthBoneTransformNextJobId++;
    fakeDepthBoneTransformResults.insertOrAssign(jobId, result);
    ++fakeDepthBoneTransformSubmitCount;
    return true;
}

bool fakeDepthBoneGpuSubmit(
    nijigenerate::commands::depth::bone_gpu_async::DepthBoneGpuDispatchPacket& packet,
    std::uint32_t& jobId, nijigenerate::compat::RetainedStringSlice& error) {
    using nijigenerate::compat::RetainedString;
    using namespace nijigenerate::commands::depth::bone;
    jobId = 0;
    error = {};
    if (fakeDepthBoneGpuSubmitFailAfter > 0 && fakeDepthBoneGpuSubmitCount >= fakeDepthBoneGpuSubmitFailAfter) {
        jobId = 0;
        error = RetainedString::fromStaticLiteral("forced fake GPU submit failure");
        ++fakeDepthBoneGpuSubmitCount;
        return false;
    }
    jobId = fakeDepthBoneGpuNextJobId++;
    error = {};
    fakeDepthBoneGpuJobVertexCounts.insertOrAssign(jobId, packet.vertices.size());
    if (packet.sourceCount > 0 && packet.sources.size() >= DepthBoneGpuSourceStride) {
        fakeDepthBoneGpuLastSourceDepthOffset = packet.sources[6];
        fakeDepthBoneGpuLastSourceSinRotation = packet.sources[DepthBoneGpuSourceSinRotationIndex];
        fakeDepthBoneGpuLastSourceCosRotation = packet.sources[DepthBoneGpuSourceCosRotationIndex];
    }
    auto bones = packet.bones.duplicate();
    fakeDepthBoneGpuLastBones = nicxlive::core::common::SharedArray<float>::retainedView(
        bones.retentionOwner(), bones.data(), bones.size());
    ++fakeDepthBoneGpuSubmitCount;
    return true;
}

bool fakeDepthBoneGpuPoll(std::uint32_t jobId,
    nijigenerate::commands::depth::bone_gpu_async::NgDepthBoneGpuAsyncResult& result,
    nijigenerate::compat::RetainedStringSlice& error) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    // D initializes out parameters before the original body's explicit resets.
    result = {};
    error = {};
    error = {};
    result = {};
    auto count = fakeDepthBoneGpuJobVertexCounts.find(jobId);
    if (!count) {
        error = RetainedString::fromStaticLiteral("missing fake GPU job");
        return false;
    }
    if (fakeDepthBoneGpuNotReadyPolls > 0) {
        --fakeDepthBoneGpuNotReadyPolls;
        ++fakeDepthBoneGpuPollCount;
        result.ready = false;
        return true;
    }
    result.ready = true;
    ++fakeDepthBoneGpuPollCount;
    if (fakeDepthBoneGpuMutateDepthOnReady && fakeDepthBoneGpuMutatingTarget) {
        auto depths = ngRequireObject(fakeDepthBoneGpuMutatingTarget)->copyDepths();
        if (depths.size() > 0) {
            depths[0] += 1.0f;
            ngRequireObject(fakeDepthBoneGpuMutatingTarget)->replaceDepths(depths);
        }
    }
    auto vertexCount = *count;
    result.xs.resize(vertexCount, std::numeric_limits<float>::quiet_NaN());
    result.ys.resize(vertexCount, std::numeric_limits<float>::quiet_NaN());
    for (std::size_t i = 0; i < vertexCount; ++i) {
        result.xs[i] = fakeDepthBoneGpuOutputX + (fakeDepthBoneGpuVaryOutputByVertex ? static_cast<float>(i) : 0.0f);
        result.ys[i] = fakeDepthBoneGpuOutputY;
    }
    fakeDepthBoneGpuJobVertexCounts.remove(jobId);
    return true;
}

// Existing regression.d15304-15332.
bool fakeDepthBoneSourceDepthSubmit(
    nijigenerate::commands::depth::bone_gpu_async::DepthBoneGpuDispatchPacket& packet,
    std::uint32_t& jobId, nijigenerate::compat::RetainedStringSlice& error) {
    using nijigenerate::commands::depth::bone::DepthBoneGpuSourceStride;
    using nijigenerate::commands::depth::bone_gpu_async::NgDepthBoneGpuAsyncResult;
    using nijigenerate::compat::RetainedString;
    // D out initialization followed by the authored explicit assignments.
    jobId = 0;
    error = {};
    jobId = 0;
    error = {};
    if (packet.sourceCount != 1 || packet.sources.size() < DepthBoneGpuSourceStride ||
        packet.depths.size() != packet.vertices.size()) {
        error = RetainedString::fromStaticLiteral("source-depth fixture requires one source and one depth per vertex");
        return false;
    }
    NgDepthBoneGpuAsyncResult result;
    result.ready = true;
    result.xs.resize(packet.vertices.size(), std::numeric_limits<float>::quiet_NaN());
    result.ys.resize(packet.vertices.size(), std::numeric_limits<float>::quiet_NaN());
    auto depthScale = packet.sources[5];
    auto depthOffset = packet.sources[6];
    const auto vertexCount = packet.vertices.size();
    for (std::size_t i = 0; i < vertexCount; ++i) {
        result.xs[i] = packet.depths[i] * depthScale + depthOffset;
        result.ys[i] = 0.0f;
    }
    jobId = fakeDepthBoneTransformNextJobId++;
    fakeDepthBoneTransformResults.insertOrAssign(jobId, result);
    return true;
}

// Existing regression.d15334-15345.
bool fakeDepthBoneTransformPoll(std::uint32_t jobId,
    nijigenerate::commands::depth::bone_gpu_async::NgDepthBoneGpuAsyncResult& result,
    nijigenerate::compat::RetainedStringSlice& error) {
    using nijigenerate::compat::RetainedString;
    result = {};
    error = {};
    error = {};
    result = {};
    auto stored = fakeDepthBoneTransformResults.find(jobId);
    if (!stored) {
        error = RetainedString::fromStaticLiteral("missing depth-driven transform fixture job");
        return false;
    }
    result = *stored;
    fakeDepthBoneTransformResults.remove(jobId);
    return true;
}
}
