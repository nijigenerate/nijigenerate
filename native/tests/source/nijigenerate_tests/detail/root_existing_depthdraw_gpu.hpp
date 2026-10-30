#pragma once
// Declarations of existing regression.d15204/15576/15583 only.
#include "uint_associative_array.hpp"
#include <nijigenerate/viewport/depth/draw/gpu.hpp>
namespace nijigenerate_tests::regression::detail {
extern thread_local UIntAssociativeArray<
    nijigenerate::viewport::depth::draw::DepthDrawGpuComposeReadback> fakeDepthDrawGpuReadbacks;
bool fakeDepthDrawGpuSubmit(nijigenerate::viewport::depth::draw::DepthDrawGpuComposePacket& packet,
    std::uint32_t& jobId, nijigenerate::viewport::depth::draw::GpuError& error);
bool fakeDepthDrawGpuPoll(std::uint32_t jobId,
    nijigenerate::viewport::depth::draw::DepthDrawGpuDispatchPollResult& result,
    nijigenerate::viewport::depth::draw::GpuError& error);
}
