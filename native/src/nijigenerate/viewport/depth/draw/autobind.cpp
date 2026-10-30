#include "autobind.hpp"
#include "nijigenerate/ext/nodes/exdepthmapped.hpp"
#include "nijigenerate/ext/nodes/expart.hpp"
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <bit>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nijigenerate::viewport::depth::draw {
using namespace nicxlive::core;
using namespace nicxlive::core::nodes;
using ext::nodes::DepthMappedNode;
using ext::nodes::ExPart;
namespace {
struct Candidate { std::shared_ptr<Node> node; int priority = 0; };
using Candidates = compat::Slice<Candidate>;
std::string_view view(const DepthDrawString& text) { return text.view(); }
using AutoBindString = DepthDrawString;
AutoBindString descriptor(const DepthDrawString& text) { return text; }
AutoBindString layerBaseName(DepthDrawString path) {
    if (path.empty()) return {};
    const auto pos = path.view().find_last_of("/\\");
    return pos == std::string_view::npos ? path : path.subslice(pos + 1, path.size());
}
std::shared_ptr<Deformable> containingDepthTarget(std::shared_ptr<Node> node) {
    for (auto cursor = node; cursor; cursor = cursor->parentRef()) {
        auto target = std::dynamic_pointer_cast<Deformable>(cursor);
        if (target && std::dynamic_pointer_cast<DepthMappedNode>(target)) return target;
    }
    return {};
}
Candidates bestCandidates(Candidates candidates) {
    if (candidates.empty()) return {};
    auto bestPriority = candidates[0].priority;
    for (const auto& candidate : candidates)
        if (candidate.priority < bestPriority) bestPriority = candidate.priority;
    Candidates result;
    for (const auto& candidate : candidates)
        if (candidate.priority == bestPriority) result.append(candidate);
    return result;
}
Candidates matchCandidates(std::shared_ptr<Puppet> puppet,
    DepthDrawString layerPath, AutoBindString layerName, bool matchDirectGridName) {
    Candidates candidates;
    if (!puppet || !puppet->root) return candidates;
    for (const auto part : puppet->findNodesType<ExPart>(puppet->root)) {
        AutoBindString path;
        if (!part->layerPath.empty()) path = part->layerPath;
        else {
            const AutoBindString name = part->getName();
            try {
                std::string bytes;
                bytes.reserve(1 + name.size());
                bytes.push_back('/');
                if (!name.empty()) bytes.append(name.data(), name.size());
                path = AutoBindString(std::move(bytes));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }
        if (path.view() == view(layerPath)) candidates.append(Candidate{part, 0});
        else if (layerBaseName(path).view() == layerName.view()) candidates.append(Candidate{part, 1});
        else if (part->getName() == layerName.view()) candidates.append(Candidate{part, 2});
    }
    if (matchDirectGridName) {
        auto direct = [&](const auto& node) {
            if (node->getName() == layerName.view() && std::dynamic_pointer_cast<DepthMappedNode>(node))
                candidates.append(Candidate{node, 3});
        };
        for (const auto grid : puppet->findNodesType<GridDeformer>(puppet->root)) direct(grid);
        for (const auto path : puppet->findNodesType<PathDeformer>(puppet->root)) direct(path);
    }
    return bestCandidates(candidates);
}
}
DepthDrawAutoBindResult ngDepthDrawAutoBindLayer(std::shared_ptr<Puppet> puppet,
    DepthDrawLayer layer, bool matchDirectGridName) {
    DepthDrawAutoBindResult result;
    result.layerId = layer.id; result.layerPath = layer.layerPath;
    result.layerName = view(layer.displayName).empty() ? layerBaseName(layer.layerPath) : descriptor(layer.displayName);
    const auto candidates = matchCandidates(puppet, result.layerPath, result.layerName, matchDirectGridName);
    if (candidates.empty()) { result.status = AutoBindString::fromStaticLiteral("Unmatched"); return result; }
    result.ambiguous = candidates.size() > 1;
    if (result.ambiguous) { result.status = AutoBindString::fromStaticLiteral("Ambiguous"); return result; }
    const auto matchedNode = candidates[0].node;
    const auto target = containingDepthTarget(matchedNode);
    result.matchedNodeName = matchedNode->getName(); result.matchedNodeUuid = matchedNode->getUUID();
    if (!target) { result.status = AutoBindString::fromStaticLiteral("UnmatchedWithoutGrid"); return result; }
    result.matched = true; result.targetGridName = target->getName(); result.targetGridUuid = target->getUUID();
    result.status = AutoBindString::fromStaticLiteral("Matched");
    return result;
}
DepthDrawBinding ngDepthDrawBindingFromAutoBind(DepthDrawAutoBindResult result, int order) {
    DepthDrawBinding binding;
    binding.layerId = result.layerId; binding.targetNodeUuid = result.matchedNodeUuid;
    binding.targetGridUuid = result.targetGridUuid; binding.order = order; binding.enabled = result.matched;
    return binding;
}
compat::Slice<DepthDrawAutoBindResult> ngDepthDrawAutoBindSession(std::shared_ptr<DepthDrawSession> session,
    std::shared_ptr<Puppet> puppet, bool matchDirectGridName) {
    compat::Slice<DepthDrawAutoBindResult> results;
    if (!session) return results;
    const auto layers = session->layers;
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto layer = layers[i];
        const auto result = ngDepthDrawAutoBindLayer(puppet, layer, matchDirectGridName);
        results.append(result);
        if (!result.matched) continue;
        bool foundBinding = false;
        std::ptrdiff_t reusableBinding = -1;
        auto bindings = session->bindings;
        for (std::size_t bindingIndex = 0; bindingIndex < bindings.size(); ++bindingIndex) {
            auto& binding = bindings[bindingIndex];
            if (view(binding.layerId) != view(layer.id)) continue;
            if (binding.targetGridUuid == result.targetGridUuid) {
                binding.targetNodeUuid = result.matchedNodeUuid; binding.enabled = true; foundBinding = true;
                break;
            }
            auto target = puppet ? puppet->findNodeById(static_cast<std::uint32_t>(binding.targetGridUuid)) : nullptr;
            if (reusableBinding < 0 && (!binding.enabled || !std::dynamic_pointer_cast<DepthMappedNode>(target)))
                reusableBinding = static_cast<std::ptrdiff_t>(bindingIndex);
        }
        if (foundBinding) continue;
        const auto binding = ngDepthDrawBindingFromAutoBind(result,
            std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i)));
        if (reusableBinding >= 0) {
            const auto index = static_cast<std::size_t>(reusableBinding);
            auto& currentBindings = session->bindings;
            if (index >= currentBindings.size())
                throw compat::ngStaticError<compat::ArrayIndexError>(index, currentBindings.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/autobind.d"), 165);
            currentBindings[index] = binding;
        } else session->bindings.append(binding);
    }
    return results;
}
} // namespace nijigenerate::viewport::depth::draw

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::Candidate> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.draw.autobind.Candidate)";
    inline static constexpr char unqualifiedArgument[] = "(Candidate)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.draw.autobind.Candidate";
    inline static constexpr char unqualifiedElement[] = "Candidate";
};
} // namespace nicxlive::core::d_runtime
