// Existing regression.d15304-15345/15603-15656; include once outside all namespaces.
#include "root_existing_depth_gpu.hpp"
#include "uint_associative_array.hpp"
#include <nijigenerate/commands/depth/bone_constants.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/ext/nodes/exgriddeformer_node.hpp>
#include <common/shared_array.hpp>
#include <cstddef>
#include <limits>
#include <memory>

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
