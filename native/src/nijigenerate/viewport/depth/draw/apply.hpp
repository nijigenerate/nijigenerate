#pragma once
#include "composer.hpp"
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <memory>

namespace nijigenerate::commands { class Context; }

namespace nijigenerate::viewport::depth::draw {
struct DepthDrawApplySummary {
    bool succeeded = false;
    std::size_t changedTargets = 0;
    std::size_t changedVertices = 0;
    std::size_t layersApplied = 0;
    std::size_t layersSkipped = 0;
    std::size_t missingSamples = 0;
};
DepthDrawApplySummary ngApplyDepthDrawTargetResultWithSummary(
    std::shared_ptr<commands::Context> context,
    std::shared_ptr<common::targetview::DepthTargetView> target,
    DepthDrawComposeResult& result);
bool ngApplyDepthDrawTargetResult(std::shared_ptr<commands::Context> context,
    std::shared_ptr<common::targetview::DepthTargetView> target,
    DepthDrawComposeResult& result);
}

namespace nijigenerate::viewport::depth::draw::apply {
using ::nijigenerate::viewport::depth::draw::DepthDrawApplySummary;
using ::nijigenerate::viewport::depth::draw::ngApplyDepthDrawTargetResultWithSummary;
using ::nijigenerate::viewport::depth::draw::ngApplyDepthDrawTargetResult;
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::DepthDrawApplySummary> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.draw.apply.DepthDrawApplySummary)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDrawApplySummary)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.draw.apply.DepthDrawApplySummary";
    inline static constexpr char unqualifiedElement[] = "DepthDrawApplySummary";
};
}
