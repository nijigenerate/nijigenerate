#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/automesh/dynamic.hpp>
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
Json meshParts(Json state, Json program, Task task) {
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
    NativeIntegerMap<std::uint32_t, Json> beforeFrames;
    task->runOnMainThread([&] {
        for (auto carrier : program.sourceIndexValue("carriers").sourceArrayValue()) {
            auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(
                project::incActivePuppet()->findNodeById(uuid(carrier.sourceIndexValue("part"))));
            enforceNative(bool(part),"Rig material disappeared before meshing",647);
            auto texture = part->textures[0];
            enforceNative(bool(texture),"Rig Part texture is missing",648);
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
            const auto frameKey = part->getUUID();
            const auto before = frame(part->localTransform);
            beforeFrames[frameKey] = before;
        }
    });
    core::settings::SettingsJsonArray configurations;
    for (auto [key, group] : groups) {
        ngRigCheckpoint(task);
        std::shared_ptr<commands::CommandResult> result;
        auto processor = createNativeObject<RigPartMeshProcessor>(group.spacing, group.divisions,
            group.scales, group.shoulder);
        compat::SharedDelegate<bool()> canceled = [task] { return task->isCanceled(); };
        task->runOnMainThread([&] {
            compat::Slice<NodeRef> parts;
            for (auto id : group.ids) parts.append(project::incActivePuppet()->findNodeById(id));
            using Apply = commands::automesh::dynamic::ApplyAutoMeshPT<
                viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor>;
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
        configurations.append(objectEvidence({{stringLiteralEvidence("parts"), values(group.ids)}, {stringLiteralEvidence("min_distance"), group.spacing},
            {stringLiteralEvidence("div_per_part"), group.divisions}, {stringLiteralEvidence("scales"), values(group.scales)},
            {stringLiteralEvidence("proximal_shoulder_sampling"), group.shoulder}}));
    }
    task->runOnMainThread([&] {
        for (auto [id, before] : beforeFrames) {
            auto part = project::incActivePuppet()->findNodeById(id);
            enforceNative(before == frame(part->localTransform),"Part AutoMesh changed the source coordinate frame",690);
        }
    });
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("part_mesh_generation")) = Json(configurations);
    return state;
}
}
