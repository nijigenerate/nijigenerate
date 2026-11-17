#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/commands/automesh/dynamic.hpp>
#include <nijigenerate/viewport/vertex/automesh/grid.hpp>
#include <nodes/part.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
namespace {
Json frame(const nicxlive::core::math::Transform& transform) {
    return array({transform.translation.x, transform.translation.y, transform.translation.z,
        transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.scale.x, transform.scale.y});
}
template<class T> Json values(const T& source) {
    core::settings::SettingsJsonArray result;
    for (auto value : source) result.append(Json(value));
    return result;
}
}
Json meshParts(Json state, Json program, Task task,
    const std::shared_ptr<actions::AsyncGroupAction>& owner) {
    struct MeshGroup {
        compat::Slice<std::uint32_t> ids;
        float spacing{std::numeric_limits<float>::quiet_NaN()};
        float divisions{std::numeric_limits<float>::quiet_NaN()};
        compat::Slice<float> scales;
        bool shoulder{};
    };
    framework::Table<MeshGroup> groups;
    NativeIntegerMap<std::uint64_t, bool> shoulders;
    for (auto pair : get(state, "shoulder_pairs", Json(core::settings::SettingsJsonArray{})).sourceArrayValue())
        if (pair.sourceIndexValue("matching").as_bool()) shoulders[ngRigUnsigned(pair.sourceIndexValue("target"))]=true;
    // Both sides of a layered attachment need contour samples at welding scale.
    for (auto pair : get(state, "surface_pairs", Json(core::settings::SettingsJsonArray{})).sourceArrayValue())
        if (pair.sourceIndexValue("matching").as_bool()) {
            shoulders[ngRigUnsigned(pair.sourceIndexValue("source"))]=true;
            shoulders[ngRigUnsigned(pair.sourceIndexValue("target"))]=true;
        }
    NativeIntegerMap<std::uint32_t, Json> beforeFrames;
    compat::Slice<std::uint32_t> neckShadowParts;
    core::settings::SettingsJsonArray unavailable;
    const auto failPart = [&](std::uint32_t id, framework::Text message, framework::AutoRigTaskState status) {
        unavailable.append(id);
        task->recordUnit({{id}, status, std::move(message)});
    };
    task->runOnMainThread([&] {
        NativeIntegerMap<std::uint32_t, bool> welded;
        if (task->targetScope()) {
            const auto gather = [&](auto&& self, const NodeRef& node) -> void {
                if (auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(node))
                    for (const auto& link : part->weldedLinks) if (auto peer = link.target.lock()) {
                        welded[part->getUUID()] = true;
                        welded[peer->getUUID()] = true;
                    }
                for (auto child : node->childrenList()) self(self, child);
            };
            gather(gather, project::incActivePuppet()->root);
        }
        for (auto carrier : program.sourceIndexValue("carriers").sourceArrayValue()) {
            const auto id = uuid(carrier.sourceIndexValue("part"));
            if (!task->acceptsTarget(id)) continue;
            auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(
                project::incActivePuppet()->findNodeById(id));
            if (!part || !part->textures[0]) {
                failPart(id, "Mesh Part or texture is unavailable", framework::AutoRigTaskState::Blocked);
                continue;
            }
            auto texture = part->textures[0];
            if (task->targetScope()) {
                const auto puppet = project::incActivePuppet();
                const bool bound = std::ranges::any_of(puppet->parameters, [&](const auto& parameter) {
                    return bool(parameter->getBinding(part, "deform"));
                });
                if (bound || welded.contains(id) || !part->childrenList().empty()) {
                    failPart(id, "The mesh is referenced by deformation or Welding",
                        framework::AutoRigTaskState::Blocked);
                    continue;
                }
            }
            beforeFrames[id] = frame(part->localTransform);
            // Drawing overlapping triangles on a Multiply layer applies the
            // shadow twice. A rectangular grid covers the soft alpha once.
            if (nativeString(carrier.sourceIndexValue("role")).view() == "neck" &&
                part->blendMode == nicxlive::core::nodes::BlendMode::Multiply &&
                !shoulders.contains(id)) {
                neckShadowParts.append(id);
                continue;
            }
            const auto widthForSpacing = texture->width();
            const auto heightForSpacing = texture->height();
            float spacing = std::min(10.0f, std::max(1.0f,
                static_cast<float>(std::max(widthForSpacing, heightForSpacing)) / 12));
            const bool shoulder = shoulders.contains(part->getUUID());
            float divisions = 12;
            compat::Slice<float> scales{.5f, 0.f};
            if (shoulder) {
                spacing = 1;
                const auto widthForDivisions = texture->width();
                const auto heightForDivisions = texture->height();
                divisions = static_cast<float>(std::min(64., std::max(12.,
                    std::ceil(std::max(widthForDivisions, heightForDivisions) / 4.))));
                // Preserve all four virtual reads, including the width product
                // before either height read, as in the Source binary expression.
                const auto widthForRadius = static_cast<double>(texture->width());
                const auto widthForRadiusAgain = texture->width();
                const auto widthSquared = widthForRadius * widthForRadiusAgain;
                const auto heightForRadius = static_cast<double>(texture->height());
                const auto heightForRadiusAgain = texture->height();
                const auto heightSquared = heightForRadius * heightForRadiusAgain;
                const double radius = std::sqrt(widthSquared + heightSquared) / 2;
                const auto count = static_cast<std::size_t>(std::max(2., std::ceil(.2 * radius / 4) + 1));
                scales = {};
                for (std::size_t i = 0; i < count; ++i)
                    scales.append(static_cast<float>(.8 + .2 * i / (count - 1)));
                scales.append(.5f); scales.append(0.f);
            }
            const auto configuration = objectEvidence({{stringLiteralEvidence("spacing"), spacing}, {stringLiteralEvidence("divisions"), divisions},
                {stringLiteralEvidence("scales"), values(scales)}, {stringLiteralEvidence("shoulder"), shoulder}});
            const auto key = ngRigDigest(configuration).retainedString();
            if (auto existing = groups.find(key); existing != groups.end()) existing->second.ids.append(part->getUUID());
            else groups.try_emplace(key, MeshGroup{{part->getUUID()}, spacing, divisions, scales, shoulder});
        }
    });
    core::settings::SettingsJsonArray configurations;
    const auto applyMesh = [&]<class Processor>(const compat::Slice<std::uint32_t>& ids,
        std::shared_ptr<Processor> processor) {
        ngRigCheckpoint(task);
        std::shared_ptr<commands::CommandResult> result;
        compat::SharedDelegate<bool()> canceled = [task] { return task->isCanceled(); };
        task->runOnMainThread([&] {
            compat::Slice<NodeRef> parts;
            for (auto id : ids) parts.append(project::incActivePuppet()->findNodeById(id));
            using Apply = commands::automesh::dynamic::ApplyAutoMeshPT<Processor>;
            result = commandSource(
                [&] {
                    return createNativeObject<Apply>(processor, canceled);
                },
                [&] {
                    return editorContext(parts);
                });
        });
        result = result->waitForCompletion();
        enforceNativeLazy(result->succeeded,[&] { return result->message; },680);
        task->runOnMainThread([&] {
            for (const auto id : ids) {
                auto part = project::incActivePuppet()->findNodeById(id);
                enforceNative(beforeFrames[id] == frame(part->localTransform),
                    "Part AutoMesh changed the source coordinate frame",690);
            }
        });
    };
    const auto tryBatch = [&](const compat::Slice<std::uint32_t>& ids, auto makeProcessor) {
        const auto attempt = [&](const compat::Slice<std::uint32_t>& batch) -> framework::Text {
            actions::AsyncGroupAction::Savepoint saved;
            task->runOnMainThread([&] { saved = owner->savepoint(); });
            try {
                applyMesh(batch, makeProcessor());
                return {};
            } catch (const compat::ThrowableException& error) {
                ngRigCheckpoint(task);
                task->runOnMainThread([&] { owner->rollbackTo(saved); });
                return error.msg;
            }
        };
        compat::Slice<std::uint32_t> applied;
        const auto batchError = attempt(ids);
        if (batchError.empty()) applied = ids;
        else for (const auto id : ids) {
            const auto error = ids.size() > 1 ? attempt({id}) : batchError;
            if (error.empty()) applied.append(id);
            else failPart(id, error, framework::AutoRigTaskState::Failed);
        }
        for (const auto id : applied) task->recordUnit({{id}, framework::AutoRigTaskState::Succeeded, {}});
        return applied;
    };
    for (auto [key, group] : groups) {
        const auto applied = tryBatch(group.ids, [&] {
            return std::shared_ptr<viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor>(
                createNativeObject<RigPartMeshProcessor>(group.spacing, group.divisions, group.scales, group.shoulder));
        });
        if (applied.empty()) continue;
        configurations.append(objectEvidence({{stringLiteralEvidence("parts"), values(applied)}, {stringLiteralEvidence("min_distance"), group.spacing},
            {stringLiteralEvidence("div_per_part"), group.divisions}, {stringLiteralEvidence("scales"), values(group.scales)},
            {stringLiteralEvidence("proximal_shoulder_sampling"), group.shoulder}}));
    }
    if (!neckShadowParts.empty()) {
        const auto applied = tryBatch(neckShadowParts, [] {
            auto processor = createNativeObject<viewport::vertex::automesh::grid::GridAutoMeshProcessor>();
            processor->xSegments = 12;
            processor->ySegments = 12;
            processor->ngPostParamWrite(compat::RetainedString::fromStaticLiteral("x_segments"));
            return processor;
        });
        if (!applied.empty()) configurations.append(objectEvidence({{stringLiteralEvidence("parts"), values(applied)},
            {stringLiteralEvidence("processor"), "Grid"}}));
    }
    if (!unavailable.empty() && !task->targetScope())
        assignNative(state, "unmeshed_parts", unavailable);
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("part_mesh_generation")) = Json(configurations);
    return state;
}
}
