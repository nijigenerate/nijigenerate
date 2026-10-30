#include "depthmap_psd_targets.hpp"
#include "depthmap_psd_helpers.hpp"
#include "depthmap_psd_math.hpp"
#include "detail/psd_depth_allocation.hpp"
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/ext/nodes/excamera_math.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nodes/dynamic_composite.hpp>
#include <cmath>
#include <span>
#include <string>
namespace nijigenerate::io::depthmap_psd {
namespace detail {
using namespace nicxlive::core::nodes;
using namespace nicxlive::core::math;
DeformableRef containingDepthTarget(NodeRef node) {
    for (auto cursor = node; cursor; cursor = cursor->parentPtr()) {
        if (auto grid = std::dynamic_pointer_cast<GridDeformer>(cursor)) return grid;
        if (auto path = std::dynamic_pointer_cast<PathDeformer>(cursor)) return path;
    }
    return {};
}
bool isActiveArtPart(PartRef part) {
    return part && part->renderEnabled() && !std::dynamic_pointer_cast<DynamicComposite>(part);
}
compat::RetainedString activeArtLayerName(PartRef part) {
    if (!isActiveArtPart(part)) return {};
    if (!part->getName().empty()) return ext::nodes::ngRetainNodeName(part->getName()).retainedString();
    return retainImportUnsignedDecimal(part->getUUID());
}
compat::RetainedString activeArtLayerPath(PartRef part) {
    if (!part) return {};
    if (auto extended = std::dynamic_pointer_cast<ext::nodes::ExPart>(part))
        if (!extended->layerPath.empty()) return extended->layerPath;
    const auto name = activeArtLayerName(part);
    if (name.empty()) return {};
    return allocateImportBytes([&] { return compat::RetainedString("/" + std::string(name.view())); });
}
compat::RetainedString uniqueActiveArtLayerPath(PartRef part, UsedArtLayerPaths& used) {
    const auto path = activeArtLayerPath(part);
    if (path.empty()) return {};
    if (!used.contains(path.view())) { used.setRetained(path, true); return path; }
    const auto uuid = part->getUUID();
    const auto decimal = retainImportUnsignedDecimal(uuid);
    const auto unique = allocateImportBytes([&] {
        return compat::RetainedString(std::string(path.view()) + "#" + std::string(decimal.view()));
    });
    used.setRetained(unique, true);
    return unique;
}
bool nodeTreeContains(NodeRef root, NodeRef target) {
    if (!root || !target) return false;
    if (root == target) return true;
    for (auto children = root->children; auto child : children)
        if (nodeTreeContains(child, target)) return true;
    return false;
}
bool nodeCapturedByGrid(DeformableRef grid, NodeRef target) {
    if (!grid || !target) return false;
    for (auto children = grid->children; auto child : children)
        if (nodeTreeContains(child, target)) return true;
    return false;
}
DeformableRef activeArtDepthTarget(PuppetRef puppet, PartRef part) {
    if (!puppet || !puppet->root || !isActiveArtPart(part)) return {};
    if (auto target = containingDepthTarget(part)) return target;
    for (auto grid : puppet->findNodesType<GridDeformer>(puppet->root)) if (nodeCapturedByGrid(grid, part)) return grid;
    for (auto path : puppet->findNodesType<PathDeformer>(puppet->root)) if (nodeCapturedByGrid(path, part)) return path;
    return {};
}
bool targetHasArtCoverage(PuppetRef puppet, DeformableRef target) {
    if (!puppet || !puppet->root || !target) return false;
    for (auto part : puppet->findNodesType<Part>(puppet->root)) {
        if (!isActiveArtPart(part)) continue;
        if (part->textures.size() == 0 || !part->textures[0]) continue;
        if (containingDepthTarget(part) == target || nodeCapturedByGrid(target, part)) return true;
    }
    return false;
}
CandidateTargetResolution resolveCandidateTarget(PuppetRef puppet, compat::Slice<Candidate> candidates) {
    CandidateTargetResolution result;
    for (auto candidate : candidates) {
        auto target = containingDepthTarget(candidate.node);
        if (!target) if (auto part = std::dynamic_pointer_cast<Part>(candidate.node))
            target = activeArtDepthTarget(puppet, part);
        if (!target) { if (!result.node) result.node = candidate.node; continue; }
        if (result.target && result.target != target) {
            result.node.reset(); result.target.reset(); result.ambiguous = true; return result;
        }
        result.node = candidate.node;
        result.target = target;
    }
    return result;
}
std::size_t findAccum(compat::Slice<GridAccum>& accums, DeformableRef grid) {
    for (std::size_t i = 0; i < accums.size(); ++i) if (accums[i].grid == grid) return i;
    if (!grid) actions::node::detail::failActionObjectReference();
    const auto vertices = grid->getVertices();
    GridAccum accum;
    accum.grid = grid;
    accum.best.resize(vertices.size(), std::numeric_limits<float>::quiet_NaN());
    accum.baseBest.resize(vertices.size(), std::numeric_limits<float>::quiet_NaN());
    accum.has.resize(vertices.size());
    accum.winnerLayerPaths.resize(vertices.size());
    accums.append(accum);
    return accums.size() - 1;
}
std::size_t findLayerMask(GridAccum& accum, compat::RetainedString path, compat::RetainedString name) {
    std::size_t index = 0;
    for (auto masks = accum.layerMasks; auto& mask : masks) {
        if (mask.layerPath == path) return index;
        ++index;
    }
    PsdDepthGridLayerMask mask;
    mask.layerPath = path; mask.layerName = name;
    accum.layerMasks.append(mask);
    return accum.layerMasks.size() - 1;
}
void attachPartCoverage(DepthLayerImage& image, PartRef part) {
    CoverageSeen seen;
    attachPartCoverage(image, part, seen);
}
void attachPartCoverage(DepthLayerImage& image, PartRef part, CoverageSeen& seen) {
    if (!part || seen.contains(compat::ngRequireObject(part)->getUUID())) return;
    seen[compat::ngRequireObject(part)->getUUID()] = true;
    if (part->textures.size() == 0) return;
    const auto texture = part->textures[0];
    if (!texture || texture->width() <= 0 || texture->height() <= 0) return;
    const auto channels = texture->channels();
    if (channels <= 0) return;
    const auto data = texture->getTextureData(true);
    const auto expected = std::size_t(texture->width()) * std::size_t(texture->height()) * std::size_t(channels);
    if (!data || data->size() < expected) return;
    CoverageSource source;
    source.width = texture->width(); source.height = texture->height(); source.channels = channels;
    source.opacity = clamp01(part->opacity);
    source.data = compat::Slice<std::uint8_t>::retainedView(data, data->data(), data->size());
    source.worldToLocal = ext::nodes::excamera_detail::inverse(part->transform().matrix());
    source.localToDocumentWorld = part->transform().matrix();
    const auto mesh = part->nicxlive::core::nodes::Drawable::getMesh();
    source.vertices = mesh.vertices;
    source.uvs = mesh.uvs;
    if (!mesh.indices.empty())
        source.indices = compat::Slice<std::uint16_t>(mesh.indices.begin(), mesh.indices.end());
    source.origin = mesh.origin;
    const auto center = core::math::ngTransformHomogeneousPoint(source.localToDocumentWorld, {0, 0, 0});
    const auto documentLeft = compat::ngFloatToInt32(std::round(center.x + float(image.documentWidth) / 2.0f -
        float(source.width) / 2.0f));
    const auto documentTop = compat::ngFloatToInt32(std::round(center.y + float(image.documentHeight) / 2.0f -
        float(source.height) / 2.0f));
    source.hasDocumentRect = true;
    source.documentLeft = documentLeft;
    source.documentTop = documentTop;
    source.documentWidth = source.width; source.documentHeight = source.height;
    image.coverageSources.append(source);
}
void attachNodeCoverage(DepthLayerImage& image, NodeRef node, CoverageSeen& seen) {
    if (!node) return;
    if (auto part = std::dynamic_pointer_cast<Part>(node)) {
        if (isActiveArtPart(part)) { attachPartCoverage(image, part, seen); return; }
    }
    for (auto children = node->children; auto child : children) attachNodeCoverage(image, child, seen);
}
void attachGridCoverage(DepthLayerImage& image, PuppetRef puppet, DeformableRef grid,
    CoverageSeen& seen) {
    if (!puppet || !puppet->root || !grid) return;
    for (auto part : puppet->findNodesType<Part>(puppet->root)) {
        if (!isActiveArtPart(part)) continue;
        if (containingDepthTarget(part) == grid || nodeCapturedByGrid(grid, part)) attachPartCoverage(image, part, seen);
    }
}
void attachMatchedCoverage(DepthLayerImage& image, PuppetRef puppet, NodeRef node, DeformableRef grid) {
    CoverageSeen seen;
    if (auto part = std::dynamic_pointer_cast<Part>(node))
        if (isActiveArtPart(part)) { attachPartCoverage(image, part, seen); return; }
    attachGridCoverage(image, puppet, grid, seen);
    if (image.coverageSources.empty()) attachNodeCoverage(image, node, seen);
}
DeformableRef findGridByUuid(PuppetRef puppet, compat::RetainedString uuid) {
    if (!puppet) actions::node::detail::failActionObjectReference();
    for (auto grid : puppet->findNodesType<GridDeformer>(compat::ngRequireObject(puppet->root))) {
        if (retainImportUnsignedDecimal(compat::ngRequireObject(grid)->getUUID()).view() == uuid.view()) return grid;
    }
    for (auto path : puppet->findNodesType<PathDeformer>(compat::ngRequireObject(puppet->root))) {
        if (retainImportUnsignedDecimal(compat::ngRequireObject(path)->getUUID()).view() == uuid.view()) return path;
    }
    return {};
}
Vec2 gridVertexDocumentPosition(DeformableRef grid, Vec2 vertex, int width, int height) {
    if (!grid) actions::node::detail::failActionObjectReference();
    const auto world = core::math::ngTransformHomogeneousPoint(grid->transform().matrix(), {vertex.x, vertex.y, 0});
    return {world.x + float(width) / 2.0f, world.y + float(height) / 2.0f};
}
}
std::shared_ptr<nicxlive::core::nodes::GridDeformer> ngPsdDepthContainingGrid(detail::NodeRef node) {
    return std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(detail::containingDepthTarget(node));
}
detail::DeformableRef ngPsdDepthContainingTarget(detail::NodeRef node) { return detail::containingDepthTarget(node); }
compat::RetainedString ngPsdDepthGridLayerKey(std::uint64_t uuid, compat::RetainedString path) {
    const auto decimal = detail::retainImportUnsignedDecimal(uuid);
    return detail::allocateImportBytes([&] {
        return compat::RetainedString(std::string(decimal.view()) + "\n" + std::string(path.view()));
    });
}
bool ngPsdDepthGridEnabled(PsdDepthImportSettings& settings, std::uint64_t uuid) {
    const auto key = detail::retainImportUnsignedDecimal(uuid);
    const auto disabled = settings.disabledGridUuids.find(key.view());
    return disabled ? !*disabled : true;
}
bool ngPsdDepthGridLayerEnabled(PsdDepthImportSettings& settings, std::uint64_t uuid, compat::RetainedString path) {
    const auto key = ngPsdDepthGridLayerKey(uuid, path);
    const auto disabled = settings.disabledGridLayerKeys.find(key.view());
    return disabled ? !*disabled : true;
}
nicxlive::core::math::Vec2 ngPsdDepthGridVertexDocumentPosition(
    std::shared_ptr<nicxlive::core::nodes::GridDeformer> grid, nicxlive::core::math::Vec2 vertex, int width, int height) {
    return detail::gridVertexDocumentPosition(grid, vertex, width, height);
}
nicxlive::core::math::Vec2 ngPsdDepthTargetPointDocumentPosition(detail::DeformableRef target,
    nicxlive::core::math::Vec2 vertex, int width, int height) {
    return detail::gridVertexDocumentPosition(target, vertex, width, height);
}
}
