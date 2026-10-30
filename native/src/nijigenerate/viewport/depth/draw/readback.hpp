#pragma once
#include "nijigenerate/viewport/depth/draw/composer.hpp"
#include "nijigenerate/compat/retained_string.hpp"
#include <nijigenerate/compat/const_slice.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <memory>
#include <string>

namespace nijigenerate::viewport::depth::draw {
struct DepthDrawReadbackComparison {
    bool ok = true;
    compat::RetainedString error;
    std::size_t depthMismatches{};
    std::size_t winnerMismatches{};
    std::size_t missingFlagMismatches{};
    std::size_t targetMismatches{};
    std::size_t layerStatMismatches{};
    std::size_t aggregateStatMismatches{};
};
DepthDrawReadbackComparison ngCompareDepthDrawReadback(
    const compat::ConstSlice<float> cpuDepths,
    const compat::ConstSlice<DepthDrawString> cpuWinners,
    const compat::ConstSlice<float> readbackDepths,
    const compat::ConstSlice<DepthDrawString> readbackWinners,
    float tolerance = 0.00001f);
DepthDrawReadbackComparison ngCompareDepthDrawComposeReadback(
    const DepthDrawComposeResult cpu, const DepthDrawComposeResult readback, float tolerance = 0.00001f);
} // namespace nijigenerate::viewport::depth::draw

namespace nijigenerate::viewport::depth::draw::readback {
using ::nijigenerate::viewport::depth::draw::DepthDrawReadbackComparison;
using ::nijigenerate::viewport::depth::draw::ngCompareDepthDrawReadback;
using ::nijigenerate::viewport::depth::draw::ngCompareDepthDrawComposeReadback;
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::DepthDrawReadbackComparison> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.draw.readback.DepthDrawReadbackComparison)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDrawReadbackComparison)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.draw.readback.DepthDrawReadbackComparison";
    inline static constexpr char unqualifiedElement[] = "DepthDrawReadbackComparison";
};
} // namespace nicxlive::core::d_runtime
