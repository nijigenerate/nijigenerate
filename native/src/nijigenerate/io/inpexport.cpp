#include "inpexport.hpp"
#include "project_reader.hpp"
#include "project_writer.hpp"
#include <nijigenerate/atlas/atlas.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/ext/nodes/excamera.hpp>
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/phobos_file_write.hpp>
#include <nijigenerate/compat/phobos_file_rename.hpp>
#include <nijigenerate/viewport/model/detail/phobos_stable_part_sort.hpp>
#include <nodes/projectable.hpp>
#include <puppet.hpp>
#include <common/resource_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <resource.hpp>
#include <algorithm>
#include <bit>
#include <new>
#include <stdexcept>
#include <string_view>
#include <source_location>

namespace nijigenerate::io::inpexport {
namespace {
using namespace nicxlive::core;
using Node = nodes::Node;
using NodeRef = std::shared_ptr<Node>;
using ParameterRef = std::shared_ptr<param::Parameter>;
using Bindings = common::SharedArray<std::shared_ptr<param::ParameterBinding>>;
using Atlas = atlas::atlas::Atlas;
using nodes::Vec2;
struct NodeUuidHash {
    std::size_t operator()(std::uint32_t value) const { return value; }
};
using RemovedNodeMap = common::ResourceKeyMap<std::uint32_t, bool,
    NodeUuidHash, std::equal_to<std::uint32_t>>;
using ExportPartPolicy = common::ObjectClassKeyPolicy<ExportPart>;
using TakenPartMap = common::ResourceKeyMap<ExportPart, bool, ExportPartPolicy::Hash, ExportPartPolicy::Equal>;
float clampUnit(float value) {
    const float lower = value < 0 ? 0.0f : value;
    return 1 < lower ? 1.0f : lower;
}
Vec2 mapUVCoord(Vec2 value, Vec2 minimum, Vec2 maximum) {
    const Vec2 range{maximum.x - minimum.x, maximum.y - minimum.y};
    const Vec2 offset{value.x - minimum.x, value.y - minimum.y};
    const Vec2 off{offset.x / range.x, offset.y / range.y};
    return {clampUnit(off.x), clampUnit(off.y)};
}
Vec2 partSize(ExportPart part, IncINPExportSettings settings) {
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto bounds = actions::node::detail::requireActionObject(part)->bounds.value_or(
        std::array<float, 4>{nan, nan, nan, nan});
    return {(bounds[2] - bounds[0]) + settings.padding, (bounds[3] - bounds[1]) + settings.padding};
}
Vec2 packingRatio(Vec2 size, IncINPExportSettings settings) {
    return {static_cast<float>((size.x * settings.scale) / static_cast<float>(settings.atlasResolution) + .01),
        static_cast<float>((size.y * settings.scale) / static_cast<float>(settings.atlasResolution) + .01)};
}
bool packPart(ExportPart part, IncINPExportSettings settings, float scale, ExportAtlas& atlas) {
    if (settings.nonLinearScaling) {
        const auto size = partSize(part, settings), ratio = packingRatio(size, settings);
        if (ratio.x > 1.0) scale = static_cast<float>(static_cast<float>(settings.atlasResolution) / size.x - .01);
        if (ratio.y > 1.0) scale = static_cast<float>(static_cast<float>(settings.atlasResolution) / size.y - .01);
    }
    const auto receiver = actions::node::detail::requireActionObject(atlas);
    return receiver->pack(part, scale);
}
float possiblePackingRatio(ExportParts parts, IncINPExportSettings settings) {
    if (settings.nonLinearScaling) return 1;
    float scale = 1;
    for (auto part : parts) {
        const auto size = partSize(part, settings), ratio = packingRatio(size, settings);
        if (ratio.x > 1.0) scale = static_cast<float>(static_cast<float>(settings.atlasResolution) / size.x - .015);
        if (ratio.y > 1.0) scale = static_cast<float>(static_cast<float>(settings.atlasResolution) / size.y - .015);
    }
    return scale;
}
bool exclude(NodeRef node) {
    return std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(node) ||
        std::dynamic_pointer_cast<ext::nodes::ExDepthBone>(node);
}
Node::ChildArray duplicateChildren(Node::ChildArray children) {
    try {
        return children.duplicate();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
}
template<class F> void eachParameter(const ExportPuppet& puppet, F visit) {
    auto parameters = actions::node::detail::requireActionObject(puppet)->parameters;
    for (auto parameter : parameters) {
        if (auto group = std::dynamic_pointer_cast<ext::ExParameterGroup>(parameter)) {
            auto children = group->children;
            for (auto child : children) visit(child);
        } else visit(parameter);
    }
}
void pruneDepthRig(ExportPuppet puppet) {
    RemovedNodeMap removed;
    const auto prune = [&](auto&& pruneSelf, NodeRef& node) -> void {
        const auto children = duplicateChildren(actions::node::detail::requireActionObject(node)->children);
        for (auto child : children) {
            if (exclude(child)) {
                auto offset = child->getIndexInParentSource();
                const auto promote = [&](auto&& promoteSelf, NodeRef excluded) -> void {
                    removed[excluded->getUUID()] = true;
                    const auto descendants = duplicateChildren(excluded->children);
                    for (auto descendant : descendants) {
                        if (exclude(descendant)) promoteSelf(promoteSelf, descendant);
                        else {
                            {
                                const auto parent = node;
                                actions::node::detail::requireActionObject(descendant)->reparent(parent, offset++, false);
                            }
                            actions::node::detail::requireActionObject(descendant)->transformChanged();
                            pruneSelf(pruneSelf, descendant);
                        }
                    }
                };
                promote(promote, child); child->insertInto({}, Node::OFFSET_END);
            } else pruneSelf(pruneSelf, child);
        }
    };
    prune(prune, actions::node::detail::requireActionObject(puppet)->root);
    if (removed.empty()) return;
    eachParameter(puppet, [&](ParameterRef parameter) {
        Bindings bindings;
        auto previous = actions::node::detail::requireActionObject(parameter)->bindings;
        for (auto binding : previous) {
            const auto target = actions::node::detail::requireActionObject(binding)->getTarget().target.lock();
            if (target && removed.contains(target->getUUID())) continue;
            bindings.push_back(binding);
        }
        parameter->bindings = bindings;
    });
    puppet->rescanNodes();
}
std::vector<std::uint8_t> writeWithoutDepthRig(ExportPuppet puppet) {
    struct DetachedNode {
        NodeRef node, parent;
        std::ptrdiff_t index{};
        decltype(Node::localTransform) localTransform;
    };
    struct PromotedNode {
        NodeRef node, parent;
        std::ptrdiff_t index{};
        decltype(Node::localTransform) localTransform;
    };
    struct BindingSnapshot { ParameterRef parameter; Bindings bindings; };
    compat::Slice<DetachedNode> detached;
    compat::Slice<PromotedNode> promoted;
    compat::Slice<BindingSnapshot> snapshots;
    RemovedNodeMap removed;
    const auto prepare = [&](auto&& prepareSelf, NodeRef parent) -> void {
        const auto children = duplicateChildren(actions::node::detail::requireActionObject(parent)->children);
        for (std::size_t index = 0; index < children.size(); ++index) {
            auto child = children[index];
            if (exclude(child)) {
                auto offset = child->getIndexInParentSource();
                detached.append({child, parent, static_cast<std::ptrdiff_t>(index), child->localTransform});
                const auto promote = [&](auto&& promoteSelf, NodeRef excluded) -> void {
                    removed[excluded->getUUID()] = true;
                    const auto descendants = duplicateChildren(excluded->children);
                    for (std::size_t i = 0; i < descendants.size(); ++i) {
                        auto descendant = descendants[i];
                        if (exclude(descendant)) promoteSelf(promoteSelf, descendant);
                        else {
                            promoted.append({descendant, excluded, static_cast<std::ptrdiff_t>(i),
                                actions::node::detail::requireActionObject(descendant)->localTransform});
                            descendant->reparent(parent, offset++, false); descendant->transformChanged();
                            prepareSelf(prepareSelf, descendant);
                        }
                    }
                };
                promote(promote, child);
            } else prepareSelf(prepareSelf, child);
        }
    };
    const auto restore = [&] {
        const auto bindingSnapshots = snapshots;
        for (auto saved : bindingSnapshots) saved.parameter->bindings = saved.bindings;
        const auto promotedNodes = promoted;
        for (auto saved : promotedNodes) {
            saved.node->reparent(saved.parent, static_cast<std::size_t>(saved.index), true);
            saved.node->localTransform = saved.localTransform; saved.node->transformChanged();
        }
        const auto detachedNodes = detached;
        for (auto saved : detachedNodes) {
            saved.node->reparent(saved.parent, static_cast<std::size_t>(saved.index), true);
            saved.node->localTransform = saved.localTransform; saved.node->transformChanged();
        }
        puppet->rescanNodes();
    };
    prepare(prepare, actions::node::detail::requireActionObject(puppet)->root);
    if (detached.empty()) return ngWriteProjectPuppetMemory(*puppet);
    const auto detachedNodes = detached;
    for (auto node : detachedNodes) node.node->reparent({}, 0, true);
    eachParameter(puppet, [&](ParameterRef parameter) {
        Bindings bindings; bool changed = false;
        auto previous = actions::node::detail::requireActionObject(parameter)->bindings;
        for (auto binding : previous) {
            const auto target = actions::node::detail::requireActionObject(binding)->getTarget().target.lock();
            if (target && removed.contains(target->getUUID())) { changed = true; continue; }
            bindings.push_back(binding);
        }
        if (changed) { snapshots.append({parameter, parameter->bindings}); parameter->bindings = bindings; }
    });
    std::vector<std::uint8_t> result;
    compat::ngRunWithScopeExit([&] {
        puppet->rescanNodes();
        puppet->populateTextureSlots();
        result = ngWriteProjectPuppetMemory(*puppet);
    }, restore);
    return result;
}
}
void ngINPExportPruneEditorOnlyNodes(ExportPuppet puppet) { pruneDepthRig(puppet); }
ExportParts incINPExportGetBestSort(ExportPuppet puppet) {
    if (!puppet) actions::node::detail::failActionObjectReference();
    const auto originalParts = puppet->getAllParts();
    ExportParts parts = [&] {
        if (originalParts.empty()) return ExportParts{};
        try {
            return ExportParts(originalParts.begin(), originalParts.end());
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    }();
    ::nijigenerate::viewport::model::detail::phobosStablePartSort(parts, [](ExportPart a, ExportPart b) {
        if (!a) actions::node::detail::failActionObjectReference();
        if (compat::ngArrayAt(a->textures, 0)) {
            if (!b) actions::node::detail::failActionObjectReference();
            if (compat::ngArrayAt(b->textures, 0)) {
                const auto leftWidth = actions::node::detail::requireActionObject(
                    compat::ngArrayAt(a->textures, 0))->width();
                const auto leftHeight = actions::node::detail::requireActionObject(
                    compat::ngArrayAt(a->textures, 0))->height();
                const auto leftSize = std::bit_cast<std::int32_t>(
                    std::uint32_t(leftWidth) + std::uint32_t(leftHeight));
                const auto rightWidth = actions::node::detail::requireActionObject(
                    compat::ngArrayAt(b->textures, 0))->width();
                const auto rightHeight = actions::node::detail::requireActionObject(
                    compat::ngArrayAt(b->textures, 0))->height();
                const auto rightSize = std::bit_cast<std::int32_t>(
                    std::uint32_t(rightWidth) + std::uint32_t(rightHeight));
                return leftSize > rightSize;
            }
        }
        return compat::ngArrayAt(a->textures, 0) != nullptr;
    });
    return parts;
}
ExportPuppet incINPExportGenPuppet(ExportPuppet puppet, IncINPExportSettings settings, bool optimize) {
    auto result = project_reader::ngLoadProjectInp<Puppet>(compat::Slice<std::uint8_t>(writeWithoutDepthRig(puppet)));
    if (optimize) incINPExportOptimizePrune(result, settings);
    return result;
}
void incINPExportOptimizePrune(ExportPuppet puppet, IncINPExportSettings settings) {
    if (!settings.optimizePruneUnused) return;
    const auto prune = [&](auto&& pruneSelf, NodeRef& node) -> void {
        auto children = actions::node::detail::requireActionObject(node)->children;
        for (std::size_t i = 0; i < children.size(); ++i) {
            auto& child = children[i];
            if (!actions::node::detail::requireActionObject(child)->getEnabled()) {
                actions::node::detail::requireActionObject(child)->insertInto({}, Node::OFFSET_END);
            } else pruneSelf(pruneSelf, child);
        }
    };
    actions::node::detail::requireActionObject(puppet)->populateTextureSlots();
    prune(prune, actions::node::detail::requireActionObject(puppet)->root);
}
void incINPExportDecorateWatermark(ExportAtlas& atlas, IncINPExportSettings& settings) {
    if (settings.decorateWatermark && !actions::node::detail::requireActionObject(atlas)->textures.empty()) {
        const float loops = std::max(static_cast<float>(settings.decorateWatermarkLoops), 1.f);
        const auto receiver = actions::node::detail::requireActionObject(atlas);
        const auto watermark = settings.decorateWatermark;
        const float width = static_cast<float>(settings.atlasResolution);
        const float height = static_cast<float>(settings.atlasResolution);
        const auto blendMode = settings.decorateWatermarkBlendMode;
        const auto opacity = settings.decorateWatermarkOpacity;
        receiver->renderOnTop(0, watermark, {0, 0, width, height}, {0, 0, loops, loops}, blendMode, opacity);
    }
}
IncINPPreviewInfo incINPExportGenPreview(ExportPuppet puppet, IncINPExportSettings settings) {
    auto atlas = Atlas::create(settings.atlasResolution, settings.padding, settings.scale);
    const auto parts = incINPExportGetBestSort(puppet);
    const float scale = possiblePackingRatio(parts, settings);
    for (auto part : parts) if (!packPart(part, settings, scale, atlas)) break;
    incINPExportDecorateWatermark(atlas, settings); atlas->finalize();
    return {scale, atlas->textures[0]};
}
ExportAtlases incINPExportGenAtlasses(ExportPuppet puppet, IncINPExportSettings settings) {
    ExportAtlases atlases;
    atlases.resize(1);
    atlases[0] = Atlas::create(settings.atlasResolution, settings.padding, settings.scale);
    const auto parts = incINPExportGetBestSort(puppet);
    auto left = parts.size(); TakenPartMap taken; bool failed = false;
    const float scale = possiblePackingRatio(parts, settings);
    for (auto part : parts) taken[part] = false;
    while (left > 0) {
        bool packed = false;
        for (auto part : parts) {
            {
                const auto flag = taken.find(part);
                if (flag == taken.end()) throw compat::ngStaticError<compat::RangeError>(
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/inpexport.d"), 437);
                if (flag->second) continue;
            }
            if (packPart(part, settings, scale, atlases[atlases.size() - 1])) {
                taken[part] = true; --left; failed = false; packed = true; break;
            }
        }
        if (packed) continue;
        if (failed) {
            const auto message = core::translation::ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("A texture is too big for the atlas."));
            auto copiedMessage = compat::RetainedString::fromStaticLiteral("Enforcement failed");
            if (!message.empty()) {
                try {
                    copiedMessage = compat::RetainedString(message.view());
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
                }
            }
            throw compat::ThrowableException(copiedMessage,
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/inpexport.d"), 449);
        }
        incINPExportDecorateWatermark(atlases[atlases.size() - 1], settings);
        actions::node::detail::requireActionObject(atlases[atlases.size() - 1])->finalize();
        atlases.append(Atlas::create(settings.atlasResolution, settings.padding, settings.scale));
        failed = true;
    }
    return atlases;
}
void incINPExportFlatten(ExportPuppet& target) {
    decltype(target->parameters) parameters;
    auto previous = actions::node::detail::requireActionObject(target)->parameters;
    for (const auto& parameter : previous) {
        if (auto group = std::dynamic_pointer_cast<ext::ExParameterGroup>(parameter)) {
            const auto children = group->children;
            parameters.appendRange(children);
        } else parameters.push_back(parameter);
    }
    actions::node::detail::requireActionObject(target)->parameters = parameters;
    actions::node::detail::requireActionObject(target)->applyDeformToChildren();
    auto animationTraversal = actions::node::detail::requireActionObject(target)->getAnimations();
    for (auto entry = animationTraversal.begin(); entry != animationTraversal.end(); ++entry) {
        const auto currentAnimation = animationTraversal.retain(entry);
        auto& animation = *currentAnimation;
        {
            const auto animationPuppet = target;
            animation.finalize(animationPuppet);
        }
        decltype(animation.lanes) lanes;
        auto laneTraversal = animation.lanes;
        for (auto& lane : laneTraversal)
            if (actions::node::detail::requireActionObject(lane.paramRef)->targetParam) lanes.push_back(lane);
        animation.lanes = lanes;
    }
    auto cameras = [&] {
        const auto cameraReceiver = actions::node::detail::requireActionObject(target);
        return cameraReceiver->findNodesType<ext::nodes::ExCamera>(target->root);
    }();
    for (auto& camera : cameras)
        actions::node::detail::requireActionObject(camera)->setParentSource({});
}
void incINPExportFinalizePacking(ExportPuppet& source, ExportAtlases atlases) {
    for (auto part : compat::ngRequireObject(source)->getAllParts()) {
        if (std::dynamic_pointer_cast<nodes::Projectable>(part)) continue;
        for (auto atlas : atlases) {
            const auto membershipKey = compat::ngRequireObject(part)->getUUID();
            if (!compat::ngRequireObject(atlas)->mappings.contains(membershipKey)) continue;
            auto uvs = compat::ngRequireObject(part)->nodes::Drawable::getMesh().uvs.dup().toArray();
            nodes::Vec4 area{1, 1, 0, 0};
            for (auto uv : uvs) {
                if (uv.x < area.x) area.x = uv.x; if (uv.y < area.y) area.y = uv.y;
                if (uv.x > area.z) area.z = uv.x; if (uv.y > area.w) area.w = uv.y;
            }
            for (std::size_t i=0; i<uvs.size(); ++i)
                uvs[i] = mapUVCoord(uvs[i], {area.x, area.y}, {area.z, area.w});
            const float size = [&] {
                const auto texture = compat::ngRequireObject(atlas)->textures[0];
                return static_cast<float>(compat::ngRequireObject(texture)->width());
            }();
            const auto currentMappings = compat::ngRequireObject(atlas)->mappings;
            const auto mappingKey = compat::ngRequireObject(part)->getUUID();
            const auto mapping = currentMappings.find(mappingKey);
            if (!mapping) throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/inpexport.d"), 529);
            const auto rect = *mapping;
            for (auto& uv : uvs) {
                uv.x = (rect.x + uv.x * rect.w) / size; uv.y = (rect.y + uv.y * rect.h) / size;
            }
            compat::ngRequireObject(part)->nodes::Drawable::getMesh().uvs = nodes::Vec2Array(uvs);
            for (std::size_t i = 0; i < static_cast<std::size_t>(nodes::TextureUsage::COUNT); ++i)
                compat::ngRequireObject(part)->textures[i] = compat::ngRequireObject(atlas)->packedIndices[i] == 0 ?
                    nullptr : compat::ngRequireObject(atlas)->textures[i];
            break;
        }
    }
    compat::ngRequireObject(source)->populateTextureSlots();
}
void incINPExport(ExportPuppet puppet, IncINPExportSettings settings, compat::RetainedStringSlice file) {
    auto source = incINPExportGenPuppet(puppet, settings, true);
    auto atlases = incINPExportGenAtlasses(puppet, settings);
    incINPExportFlatten(source); incINPExportFinalizePacking(source, atlases);
    const auto swap = [&] {
        try {
            return compat::RetainedString(std::string(file.view()) + ".export.swp");
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    {
        const auto bytes = ngWriteProjectPuppetMemory(*source);
        compat::phobos::ngFileWrite(swap,
            std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    }
    compat::phobos::ngFileRename(swap, file);
}
}
