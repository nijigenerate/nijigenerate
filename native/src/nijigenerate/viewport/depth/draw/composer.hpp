#pragma once
#include "string.hpp"
#include <compat/object_metadata.hpp>

#include <nijigenerate/compat/slice.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

namespace nijigenerate::viewport::depth::common::targetview {
class DepthTargetView;
}

namespace nijigenerate::viewport::depth::draw {

class DepthDrawSession;
struct DepthDrawLayerComposeStats {
    DepthDrawString layerId;
    std::size_t sampledVertices = 0;
    std::size_t missingVertices = 0;
    std::size_t winningVertices = 0;
    std::size_t contributedVertices = 0;
    bool additiveMerge = false;
    bool hasDepthRange = false;
    float minDepth = std::numeric_limits<float>::quiet_NaN();
    float maxDepth = std::numeric_limits<float>::quiet_NaN();
};

struct DepthDrawComposeResult {
    std::uint64_t targetGridUuid = 0;
    compat::Slice<float> depths;
    compat::Slice<DepthDrawString> winningLayerIds;
    compat::Slice<DepthDrawLayerComposeStats> layerStats;
    std::size_t sampledVertices = 0;
    std::size_t missingVertices = 0;
    bool hasDepthRange = false;
    float minDepth = std::numeric_limits<float>::quiet_NaN();
    float maxDepth = std::numeric_limits<float>::quiet_NaN();
};

DepthDrawComposeResult ngComposeDepthDrawTarget(
    std::shared_ptr<DepthDrawSession> session,
    std::shared_ptr<common::targetview::DepthTargetView> target,
    std::int32_t documentWidth,
    std::int32_t documentHeight);

DepthDrawComposeResult ngPreviewDepthDrawTarget(
    std::shared_ptr<DepthDrawSession> session,
    std::shared_ptr<common::targetview::DepthTargetView> target,
    std::int32_t documentWidth,
    std::int32_t documentHeight);

} // namespace nijigenerate::viewport::depth::draw

namespace nijigenerate::viewport::depth::draw::composer {
using ::nijigenerate::viewport::depth::draw::DepthDrawLayerComposeStats;
using ::nijigenerate::viewport::depth::draw::DepthDrawComposeResult;
using ::nijigenerate::viewport::depth::draw::ngComposeDepthDrawTarget;
using ::nijigenerate::viewport::depth::draw::ngPreviewDepthDrawTarget;
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::DepthDrawLayerComposeStats> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.draw.composer.DepthDrawLayerComposeStats)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDrawLayerComposeStats)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.draw.composer.DepthDrawLayerComposeStats";
    inline static constexpr char unqualifiedElement[] = "DepthDrawLayerComposeStats";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::DepthDrawComposeResult> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.draw.composer.DepthDrawComposeResult)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDrawComposeResult)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.draw.composer.DepthDrawComposeResult";
    inline static constexpr char unqualifiedElement[] = "DepthDrawComposeResult";
};
}
