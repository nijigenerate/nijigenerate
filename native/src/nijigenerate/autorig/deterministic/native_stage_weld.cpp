#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "geometry.hpp"
#include "review.hpp"
#include "detail/evidence.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/core/actionstack.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
using namespace review;
Json weldShoulders(Json state, Task task) {
    const auto weldKind = compat::RetainedString::fromStaticLiteral("weld");
    auto pairs = get(state, "shoulder_pairs", core::settings::SettingsJsonArray{}).sourceArrayValue().duplicate();
    for (auto pair : get(state, "surface_pairs", core::settings::SettingsJsonArray{}).sourceArrayValue()) pairs.append(pair);
    core::settings::SettingsJsonArray outsideScope;
    if (task->targetScope() || task->eligibleTargets()) {
        // The existing algorithm refines both ends before creating links. A
        // crossing edge would also change vertex correspondence for other edges.
        NativeIntegerMap<std::uint32_t, bool> crossing;
        for (const auto& pair : pairs) {
            if (!indexNative(pair, "matching").as_bool()) continue;
            const auto src = uuid(indexNative(pair, "source")), dst = uuid(indexNative(pair, "target"));
            if (task->acceptsTarget(src) != task->acceptsTarget(dst)) {
                crossing[src] = true;
                crossing[dst] = true;
            }
        }
        bool expanded;
        do {
            expanded = false;
            for (const auto& pair : pairs) {
                if (!indexNative(pair, "matching").as_bool()) continue;
                const auto src = uuid(indexNative(pair, "source")), dst = uuid(indexNative(pair, "target"));
                if (crossing.contains(src) == crossing.contains(dst)) continue;
                crossing[src] = true;
                crossing[dst] = true;
                expanded = true;
            }
        } while (expanded);
        core::settings::SettingsJsonArray selected;
        for (const auto& pair : pairs) {
            const auto src = uuid(indexNative(pair, "source")), dst = uuid(indexNative(pair, "target"));
            if (get(pair, "status", "").as_string().view() == "applied_readback_verified") {
                outsideScope.append(pair);
                if (task->acceptsTarget(src) && task->acceptsTarget(dst))
                    task->recordUnit({{src, dst}, framework::AutoRigTaskState::NotApplicable,
                        "The verified Welding is already present"});
                continue;
            }
            if ((!task->targetScope() && !indexNative(pair, "matching").as_bool()) ||
                (task->acceptsTarget(src) && task->acceptsTarget(dst) && !crossing.contains(src))) selected.append(pair);
            else {
                outsideScope.append(pair);
                if (indexNative(pair, "matching").as_bool() && (task->acceptsTarget(src) || task->acceptsTarget(dst)))
                    task->recordUnit({{src, dst}, framework::AutoRigTaskState::Blocked,
                        "Welding requires the complete shared seam"});
            }
        }
        pairs = std::move(selected);
    }
    core::settings::SettingsJsonArray operations;
    auto settings = reviewSettings(task);
    for (auto pair : pairs) operations.append(objectEvidence({
        {stringLiteralEvidence("id"), core::settings::SettingsString(ngRigReviewOperationId(weldKind, pair))},
        {stringLiteralEvidence("enabled"), ngRigReviewEnabled(settings, ngRigReviewOperationId(weldKind, pair))},
        {stringLiteralEvidence("plan"), ngRigReviewCompact(pair)}}));
    task->previewJson("review-operations", operations);
    const auto overlapBounds = [](Json pair) -> std::optional<std::array<float, 4>> {
        if (stringGet(pair, "kind", "") == "hip_attachment") {
            const auto box = ngRigNumbers(indexNative(pair, "seam_region"));
            return std::array<float, 4>{float(box[0]), float(box[1]), float(box[2]), float(box[3])};
        }
        if (get(pair, "upper_arm_length", Json{}).is_null()) return std::nullopt;
        const auto origin = ngRigNumbers(indexNative(pair, "origin"));
        const auto radius = .30 * ngRigNumber(indexNative(pair, "upper_arm_length"));
        return std::array<float, 4>{float(origin[0]-radius), float(origin[1]-radius),
            float(origin[0]+radius), float(origin[1]+radius)};
    };
    using viewport::common::mesheditor::operations::impl::WeldingOverlap;
    // Each pair contributes its shared vertices once. Reapplying the overlay
    // to progressively refined meshes multiplies the cuts.
    std::vector<std::optional<WeldingOverlap>> seams(pairs.size());
    NativeIntegerMap<std::uint32_t, std::size_t> originalVertexCounts;
    const auto originalCount = [&](const auto& part) {
        const auto id = part->getUUID();
        if (!originalVertexCounts.contains(id)) originalVertexCounts[id] = part->getVertices().size();
        return originalVertexCounts.find(id)->second;
    };
    for (std::size_t i=0; i<pairs.size(); ++i) {
        auto& pair = pairs[i];
        if (!ngRigReviewEnabled(settings, ngRigReviewOperationId(weldKind, pair))) {
            assignNative(pair, "status", "disabled_by_user"); continue;
        }
        if (!indexNative(pair, "matching").as_bool()) { assignNative(pair, "status", "not_matching"); continue; }
        ngRigCheckpoint(task);
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet();
            auto source = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(indexNative(pair, "source"))));
            auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(indexNative(pair, "target"))));
            enforceNative(source && target,"Shoulder welding Part disappeared",1287);
            auto originalSource = textureMapping(source), originalTarget = textureMapping(target);
            auto prepared = viewport::common::mesheditor::operations::impl::ngPrepareWeldingOverlap(
                source, target, puppet->root->transform().matrix(), overlapBounds(pair),
                {originalCount(source), originalCount(target)});
            seams[i] = prepared;
            if (!prepared.addedVertices && !prepared.topologyChanged) return;
            assignNative(pair, "refined_vertices",
                ngRigUnsigned(get(pair, "refined_vertices", 0)) + prepared.addedVertices);
            auto beforeSource = textureMapping(source), beforeTarget = textureMapping(target);
            for (const auto& meshes : {std::array<Json, 2>{originalSource, beforeSource},
                std::array<Json, 2>{originalTarget, beforeTarget}}) {
                enforceNative(indexNative(meshes[1], "vertices").sourceArrayValue().size() >= indexNative(meshes[0], "vertices").sourceArrayValue().size(),"Shoulder refinement removed original vertices",1295);
                const auto before = ngRigPoints(indexNative(meshes[0], "vertices"));
                const auto after = ngRigPoints(indexNative(meshes[1], "vertices"));
                for (std::size_t i = 0; i < before.size(); ++i) {
                    enforceNative(std::hypot(before[i][0]-after[i][0], before[i][1]-after[i][1]) <= .004,
                        "Welding moved an original vertex beyond rounding tolerance", 1298);
                }
                geometry::ngRigVerifySourceUV(meshes[0], meshes[1]);
            }
            auto targets = indexNative(state, "targets").sourceArrayValue().duplicate();
            for (auto& entry : targets) for (auto part : {source, target}) if (uuid(indexNative(entry, "part")) == part->getUUID()) {
                auto original = part == source ? originalSource : originalTarget;
                enforceNative(sameTextureMapping(indexNative(entry, "mapping"), original),"Shoulder mesh changed before refinement",1305);
                assignNative(entry, "mapping", part == source ? beforeSource : beforeTarget);
                compat::Slice<Point2> world;
                for (auto p : ngRigPoints(indexNative(indexNative(entry, "mapping"), "vertices"))) world.append(toRoot(part, p[0], p[1]));
                assignNative(entry, "world", ngRigPointsJson(world));
            }
            assignNative(state, "targets", targets);
        });
    }
    // Finish every shared mesh edit before predicting or creating any link.
    for (std::size_t i=0; i<pairs.size(); ++i) {
        auto& pair=pairs[i];
        if (!ngRigReviewEnabled(settings, ngRigReviewOperationId(weldKind, pair)) || !indexNative(pair, "matching").as_bool()) continue;
        ngRigCheckpoint(task);
        compat::Slice<std::ptrdiff_t> indices;
        std::size_t count = 0;
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet();
            auto source = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(indexNative(pair, "source"))));
            auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(indexNative(pair, "target"))));
            enforceNative(source && target, "Welding Part disappeared", 1326);
            enforceNative(seams[i].has_value(), "Missing Welding vertex correspondence", 1334);
            indices = seams[i]->indices.duplicate();
            indices.resize(source->getVertices().size(), -1);
            const auto beforeSource = textureMapping(source), beforeTarget = textureMapping(target);
            const auto src = ngRigPoints(indexNative(beforeSource, "vertices"));
            const auto dst = ngRigPoints(indexNative(beforeTarget, "vertices"));
            compat::Slice<std::ptrdiff_t> reverse;
            reverse.resize(dst.size(), -1);
            for (std::size_t i = 0; i < indices.size(); ++i) {
                const auto j=indices[i];
                if (j < 0) continue;
                enforceNative(static_cast<std::size_t>(j)<reverse.size() && reverse[j]<0,
                    "Welding overlap is not one-to-one", 1341);
                reverse[j]=static_cast<std::ptrdiff_t>(i);
                const auto a = toRoot(source, src[i][0], src[i][1]);
                const auto b = toRoot(target, dst[j][0], dst[j][1]);
                enforceNative(std::hypot(a[0]-b[0], a[1]-b[1]) <= .0021,
                    "Welding overlap vertices do not coincide", 1344);
                ++count;
            }
            if (count == 0) return;
            const auto retained=actions::node::Indices::retainedView(
                indices.retentionOwner(), indices.data(), indices.size());
            core::actionstack::incActionPush(std::allocate_shared<actions::node::DrawableAddWeldingAction>(
                nicxlive::core::d_runtime::SourceAllocator<actions::node::DrawableAddWeldingAction>{},
                source, target, retained, 0.f));
            const auto otherLinks=target->weldedLinks;
            enforceNative(std::ranges::any_of(otherLinks,[&](const auto& link) {
                return link.target.lock()==source && link.weight==1.f &&
                    std::ranges::equal(link.indices,reverse);
            }), "Welding inverse differs from the shared surface", 1361);
            bool found = false;
            const auto links = source->weldedLinks;
            for (auto link : links) if (link.target.lock() == target) {
                const auto sameIndices = [&] {
                    if (link.indices.size() != indices.size()) return false;
                    for (std::size_t i = 0; i < indices.size(); ++i)
                        if (link.indices[i] != indices[i]) return false;
                    return true;
                };
                if (!sameIndices() || link.weight != 0) {
                    core::settings::SettingsJsonArray predicted, actual;
                    for (auto i : indices) predicted.append(i);
                    for (auto i : link.indices) actual.append(i);
                    task->previewJson("welding-correspondence-mismatch", objectEvidence({
                        {stringLiteralEvidence("source"), indexNative(pair, "source")}, {stringLiteralEvidence("target"), indexNative(pair, "target")},
                        {stringLiteralEvidence("predicted"), predicted}, {stringLiteralEvidence("actual"), actual},
                        {stringLiteralEvidence("source_vertices_before"), indexNative(beforeSource, "vertices").sourceArrayValue().size()},
                        {stringLiteralEvidence("source_vertices_after"), source->getVertices().size()},
                        {stringLiteralEvidence("target_vertices_before"), indexNative(beforeTarget, "vertices").sourceArrayValue().size()},
                        {stringLiteralEvidence("target_vertices_after"), target->getVertices().size()}}));
                }
                enforceNative(sameIndices() && link.weight == 0,"Native shoulder correspondence differs from prediction",1366); found = true;
            }
            enforceNative(found && textureMapping(source) == beforeSource && textureMapping(target) == beforeTarget,"Shoulder welding changed native mesh arrays or failed to create its link",1369);
        });
        if (count == 0) {
            assignNative(pair, "status", "no_shared_surface");
            continue;
        }
        core::settings::SettingsJsonArray indexValues;
        for (auto i : indices) indexValues.append(i);
        assignNative(pair, "indices", indexValues); assignNative(pair, "weight", 0.); assignNative(pair, "paired_vertices", count);
        assignNative(pair, "mesh_arrays_unchanged", true); assignNative(pair, "status", "applied_readback_verified");
        if (task->targetScope()) task->recordUnit({
            {uuid(indexNative(pair, "source")), uuid(indexNative(pair, "target"))},
            framework::AutoRigTaskState::Succeeded, {}});
    }
    for (const auto& pair : outsideScope) pairs.append(pair);
    core::settings::SettingsJsonArray shoulders, surfaces;
    for (auto pair : pairs) {
        const auto kind=get(pair,"kind","").as_string().view();
        if (kind == "layered_ear" || kind == "hip_attachment") surfaces.append(pair);
        else shoulders.append(pair);
    }
    assignNative(state, "shoulder_pairs", shoulders);
    assignNative(state, "surface_pairs", surfaces);
    return state;
}
}
