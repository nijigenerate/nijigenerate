#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "geometry.hpp"
#include "review.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nijigenerate/commands/node/welding_commands.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
using namespace review;
Json weldShoulders(Json state, Task task) {
    auto pairs = get(state, "shoulder_pairs", core::settings::SettingsJsonArray{}).sourceArrayValue().duplicate();
    core::settings::SettingsJsonArray operations;
    auto settings = reviewSettings(task);
    for (auto pair : pairs) operations.append(objectEvidence({
        {stringLiteralEvidence("id"), core::settings::SettingsString(ngRigReviewOperationId("weld", pair))},
        {stringLiteralEvidence("enabled"), ngRigReviewEnabled(settings, ngRigReviewOperationId("weld", pair))},
        {stringLiteralEvidence("plan"), ngRigReviewCompact(pair)}}));
    task->previewJson("review-operations", operations);
    for (auto& pair : pairs) {
        if (!ngRigReviewEnabled(settings, ngRigReviewOperationId("weld", pair))) {
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
            auto added = viewport::common::mesheditor::operations::impl::ngRefineWeldingSeams(source, target);
            assignNative(pair, "refined_vertices", added);
            auto beforeSource = textureMapping(source), beforeTarget = textureMapping(target);
            for (const auto& meshes : {std::array<Json, 2>{originalSource, beforeSource},
                std::array<Json, 2>{originalTarget, beforeTarget}}) {
                enforceNative(indexNative(meshes[1], "vertices").sourceArrayValue().size() >= indexNative(meshes[0], "vertices").sourceArrayValue().size(),"Shoulder refinement removed original vertices",1295);
                for (const auto field : {"vertices", "uv"}) {
                    const auto samples = indexNative(meshes[0], field).sourceArrayValue();
                    for (std::size_t i = 0; i < samples.size(); ++i)
                        enforceNative(samples[i] == readNativeArray(indexNative(meshes[1], field).sourceArrayValue(),i,1298),"Shoulder refinement changed an original sample",1298);
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
    for (auto& pair : pairs) {
        if (!ngRigReviewEnabled(settings, ngRigReviewOperationId("weld", pair)) || !indexNative(pair, "matching").as_bool()) continue;
        ngRigCheckpoint(task);
        compat::Slice<Point2> sourcePoints, targetPoints;
        Json beforeSource, beforeTarget;
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet();
            auto source = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(indexNative(pair, "source"))));
            auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(indexNative(pair, "target"))));
            enforceNative(source && target,"Shoulder welding Part disappeared",1326);
            beforeSource = textureMapping(source); beforeTarget = textureMapping(target);
            for (auto p : ngRigPoints(indexNative(beforeSource, "vertices"))) {
                auto point = core::math::ngTransformHomogeneousPoint(source->transform().matrix(),
                    {static_cast<float>(p[0]), static_cast<float>(p[1]), 0});
                sourcePoints.append(Point2{point.x, point.y});
            }
            for (auto p : ngRigPoints(indexNative(beforeTarget, "vertices"))) {
                auto point = core::math::ngTransformHomogeneousPoint(target->transform().matrix(),
                    {static_cast<float>(p[0]), static_cast<float>(p[1]), 0});
                targetPoints.append(Point2{point.x, point.y});
            }
        });
        compat::Slice<std::ptrdiff_t> indices;
        std::size_t count = 0;
        for (std::size_t i = 0; i < sourcePoints.size(); ++i) {
            if (i % 256 == 0) ngRigCheckpoint(task);
            auto p = sourcePoints[i]; double nearest = INFINITY; std::ptrdiff_t index = -1;
            for (std::size_t j = 0; j < targetPoints.size(); ++j) {
                auto q = targetPoints[j]; auto dx = p[0] - q[0], dy = p[1] - q[1];
                double distance = dx * dx + dy * dy;
                if (distance < nearest) { nearest = distance; index = static_cast<std::ptrdiff_t>(j); }
            }
            indices.append(nearest < 16 ? index : -1);
            if (nearest < 16) ++count;
        }
        if (!count) { assignNative(pair, "status", "no_native_vertex_pairs"); continue; }
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet();
            auto source = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(indexNative(pair, "source"))));
            auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(uuid(indexNative(pair, "target"))));
            commandSource(
                [&] {
                    return createNativeObject<commands::node::welding::AddWeldingCommand>(target, 0);
                },
                [&] {
                    return editorContext({source});
                });
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
        core::settings::SettingsJsonArray indexValues;
        for (auto i : indices) indexValues.append(i);
        assignNative(pair, "indices", indexValues); assignNative(pair, "weight", 0.); assignNative(pair, "paired_vertices", count);
        assignNative(pair, "mesh_arrays_unchanged", true); assignNative(pair, "status", "applied_readback_verified");
    }
    assignNative(state, "shoulder_pairs", pairs);
    return state;
}
}
