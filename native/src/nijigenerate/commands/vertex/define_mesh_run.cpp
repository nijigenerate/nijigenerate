#include "define_mesh.hpp"
#include "define_mesh_detail.hpp"
#include "define_mesh_preflight.hpp"
#include "nijigenerate/commands/context.hpp"
#include "nijigenerate/commands/depth/detail/bone_dirty_providers.hpp"
#include "nijigenerate/core/actionstack.hpp"
#include "nijigenerate/compat/vec3u.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include <nijigenerate/compat/scope_exit.hpp>
#include "nijigenerate/project.hpp"
#include "nijigenerate/viewport/common/mesh.hpp"
#include "nijigenerate/viewport/vertex/package.hpp"
#include <nodes/deformable.hpp>
#include <nodes/drawable.hpp>
#include <nodes/grid_deformer.hpp>

namespace nijigenerate::commands::vertex::define_mesh {
using nicxlive::core::nodes::Deformable;
using nicxlive::core::nodes::Drawable;
using nicxlive::core::nodes::GridDeformer;

std::shared_ptr<CommandResult> DefineMeshCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!runnable(context)) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No drawable nodes available"));
    if (vertices.isNull() || indices.isNull())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("Vertices or indices not provided"));
    if (vertices.empty() || indices.empty()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("Vertices or indices empty"));
    if (vertices.size() % 2 != 0) detail::failInput("vertices length must be even (x,y pairs)", 96);
    if (indices.size() % 3 != 0)
        detail::failInput("indices length must be a multiple of 3 (triangles)", 97);
    auto points = detail::parseFlatVertices(vertices);
    for (auto index : indices) {
        if (index >= points.size()) detail::failInput("index out of range in indices array", 103);
    }
    compat::Slice<compat::Vec3u> triangles;
    triangles.resize(indices.size() / 3);
    for (std::size_t index = 0; index < triangles.size(); ++index)
        triangles[index] = compat::Vec3u{indices[index * 3], indices[index * 3 + 1], indices[index * 3 + 2]};

    auto nodes = compat::ngRequireObject(context)->hasNodes() ? context->nodes() : project::incSelectedNodes();
    compat::Slice<std::shared_ptr<Drawable>> targets;
    for (auto node : nodes) {
        if (std::dynamic_pointer_cast<Drawable>(node)) targets.append(std::dynamic_pointer_cast<Drawable>(node));
    }
    if (targets.empty()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No drawable targets"));
    for (auto target : targets) {
        compat::RetainedString reason;
        if (!detail::canApplyMeshTopology(target.get(), reason)) return CommandResult::opCall(false, reason);
    }
    for (auto target : targets) {
        auto base = target->Drawable::getMesh();
        auto mesh = viewport::common::mesh::IncMesh::create(base);
        mesh->vertices.resize(0);
        mesh->importVertsAndTris(points, triangles);
        mesh->refresh();
        compat::RetainedString message;
        if (!viewport::vertex::ngApplyDrawableMeshFromCommand(target, mesh, message))
            return CommandResult::opCall(false, message);
    }
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> DefineVerticesCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!runnable(context)) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No non-drawable deformable nodes available"));
    auto points = detail::parseFlatVertices(vertices);
    auto nodes = compat::ngRequireObject(context)->hasNodes() ? context->nodes() : project::incSelectedNodes();
    compat::Slice<std::shared_ptr<Deformable>> targets;
    for (auto node : nodes) {
        if (std::dynamic_pointer_cast<Deformable>(node) && !std::dynamic_pointer_cast<Drawable>(node))
            targets.append(std::dynamic_pointer_cast<Deformable>(node));
    }
    if (targets.empty()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No non-drawable deformable targets"));
    for (auto target : targets) {
        if (std::dynamic_pointer_cast<GridDeformer>(target)) {
            viewport::common::mesh::GridAxes gridAxes;
            if (!viewport::common::mesh::isGrid(points, gridAxes))
                detail::failInput("GridDeformer vertices must form a complete rectangular grid", 176);
        }
        compat::RetainedString message;
        if (!viewport::vertex::ngApplyDeformableVerticesFromCommand(target, points, message))
            return CommandResult::opCall(false, message);
        depth::bone::ngMarkDepthBoneDirtyForTarget(target,
            compat::RetainedString::fromStaticLiteral("Target Vertices"));
    }
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> DefineGridCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!runnable(context)) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No GridDeformer nodes available"));
    auto points = detail::makeGridVertices(axisX, axisY);
    auto nodes = compat::ngRequireObject(context)->hasNodes() ? context->nodes() : project::incSelectedNodes();
    compat::Slice<std::shared_ptr<GridDeformer>> targets;
    for (auto node : nodes) {
        if (std::dynamic_pointer_cast<GridDeformer>(node))
            targets.append(std::dynamic_pointer_cast<GridDeformer>(node));
    }
    if (targets.empty()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No GridDeformer targets"));
    core::actionstack::incActionPushGroup();
    std::shared_ptr<CommandResult> result;
    compat::ngRunWithScopeExit([&] {
        result = [&]() -> std::shared_ptr<CommandResult> {
            for (auto target : targets) {
                compat::RetainedString message;
                if (!viewport::vertex::ngApplyDeformableVerticesFromCommand(target, points, message))
                    return CommandResult::opCall(false, message);
                depth::bone::ngMarkDepthBoneDirtyForTarget(target,
                    compat::RetainedString::fromStaticLiteral("Target Vertices"));
            }
            depth::bone::ngFlushDepthBoneDirty();
            return CommandResult::opCall(true);
        }();
    }, [] {
        core::actionstack::incActionPopGroup();
    });
    return result;
}

} // namespace nijigenerate::commands::vertex::define_mesh
