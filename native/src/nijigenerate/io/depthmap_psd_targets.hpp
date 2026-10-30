#pragma once
#include "depthmap_psd_public_targets.hpp"
#include "depthmap_psd_internal_values.hpp"
#include <common/string_key_map.hpp>
#include <puppet.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <nodes/part.hpp>
#include <functional>
namespace nijigenerate::io::depthmap_psd {
namespace detail {
using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
using PartRef = std::shared_ptr<nicxlive::core::nodes::Part>;
using DeformableRef = std::shared_ptr<nicxlive::core::nodes::Deformable>;
using PuppetRef = std::shared_ptr<nicxlive::core::Puppet>;
struct CoverageUuidHash {
    std::size_t operator()(std::uint64_t value) const noexcept { return static_cast<std::size_t>(value); }
};
using CoverageSeen = nicxlive::core::common::ResourceKeyMap<std::uint64_t, bool,
    CoverageUuidHash, std::equal_to<std::uint64_t>>;
using UsedArtLayerPaths = nicxlive::core::common::StringKeyMap<bool>;
template<class T> compat::Slice<std::shared_ptr<T>> findNodesType(NodeRef node) {
    compat::Slice<std::shared_ptr<T>> result;
    if (auto value = std::dynamic_pointer_cast<T>(node)) result.append(value);
    for (const auto& child : node->children) result.append(findNodesType<T>(child));
    return result;
}
DeformableRef containingDepthTarget(NodeRef node);
bool isActiveArtPart(PartRef part);
compat::RetainedString activeArtLayerName(PartRef part);
compat::RetainedString activeArtLayerPath(PartRef part);
compat::RetainedString uniqueActiveArtLayerPath(PartRef part, UsedArtLayerPaths& used);
bool nodeTreeContains(NodeRef root, NodeRef target);
bool nodeCapturedByGrid(DeformableRef grid, NodeRef target);
DeformableRef activeArtDepthTarget(PuppetRef puppet, PartRef part);
bool targetHasArtCoverage(PuppetRef puppet, DeformableRef target);
CandidateTargetResolution resolveCandidateTarget(PuppetRef puppet, compat::Slice<Candidate> candidates);
compat::Slice<Candidate> matchCandidates(PuppetRef puppet, compat::RetainedString layerPath,
    compat::RetainedString layerName, PsdDepthImportSettings& settings,
    int left = 0, int top = 0, int width = 0, int height = 0,
    int documentWidth = 0, int documentHeight = 0, bool useBoundsFallback = false);
std::size_t findAccum(compat::Slice<GridAccum>& accums, DeformableRef grid);
std::size_t findLayerMask(GridAccum& accum, compat::RetainedString path, compat::RetainedString name);
void attachPartCoverage(DepthLayerImage& image, PartRef part);
void attachPartCoverage(DepthLayerImage& image, PartRef part, CoverageSeen& seen);
void attachNodeCoverage(DepthLayerImage& image, NodeRef node, CoverageSeen& seen);
void attachGridCoverage(DepthLayerImage& image, PuppetRef puppet, DeformableRef grid,
    CoverageSeen& seen);
void attachMatchedCoverage(DepthLayerImage& image, PuppetRef puppet, NodeRef node, DeformableRef grid);
DeformableRef findGridByUuid(PuppetRef puppet, compat::RetainedString uuid);
nicxlive::core::math::Vec2 gridVertexDocumentPosition(DeformableRef grid,
    nicxlive::core::math::Vec2 vertex, int documentWidth, int documentHeight);
}
}
