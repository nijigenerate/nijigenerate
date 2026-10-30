// Existing regression.d15204/15576-15601; include once outside all namespaces.
#include "root_existing_depthdraw_gpu.hpp"

namespace nijigenerate_tests::regression::detail {
extern thread_local std::uint32_t fakeDepthDrawGpuNextJobId;
extern thread_local std::uint32_t fakeDepthDrawGpuSubmitCount;
extern thread_local std::uint32_t fakeDepthDrawGpuPollCount;
extern thread_local std::uint32_t fakeDepthDrawGpuNotReadyPolls;
thread_local UIntAssociativeArray<
    nijigenerate::viewport::depth::draw::DepthDrawGpuComposeReadback> fakeDepthDrawGpuReadbacks;

bool fakeDepthDrawGpuSubmit(nijigenerate::viewport::depth::draw::DepthDrawGpuComposePacket& packet,
    std::uint32_t& jobId, nijigenerate::viewport::depth::draw::GpuError& error) {
    jobId = 0;
    error = {};
    jobId = fakeDepthDrawGpuNextJobId++;
    error = {};
    ++fakeDepthDrawGpuSubmitCount;
    return true;
}

bool fakeDepthDrawGpuPoll(std::uint32_t jobId,
    nijigenerate::viewport::depth::draw::DepthDrawGpuDispatchPollResult& result,
    nijigenerate::viewport::depth::draw::GpuError& error) {
    // D out initialization precedes the original explicit assignments.
    result = {};
    error = {};
    error = {};
    result = {};
    if (fakeDepthDrawGpuNotReadyPolls > 0) {
        --fakeDepthDrawGpuNotReadyPolls;
        ++fakeDepthDrawGpuPollCount;
        return true;
    }
    auto readback = fakeDepthDrawGpuReadbacks.find(jobId);
    if (!readback) {
        error = nijigenerate::compat::RetainedString::fromStaticLiteral("missing fake DepthDraw GPU job");
        return false;
    }
    result.ready = true;
    result.readback = *readback;
    ++fakeDepthDrawGpuPollCount;
    fakeDepthDrawGpuReadbacks.remove(jobId);
    return true;
}
}
