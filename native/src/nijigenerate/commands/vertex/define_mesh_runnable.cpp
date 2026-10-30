#include "define_mesh.hpp"
#include "nijigenerate/commands/context.hpp"
#include "nijigenerate/project.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include <nodes/deformable.hpp>
#include <nodes/drawable.hpp>
#include <nodes/grid_deformer.hpp>

namespace nijigenerate::commands::vertex::define_mesh {

bool DefineMeshCommand::runnable(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto nodes = compat::ngRequireObject(context)->hasNodes() ? context->nodes() : project::incSelectedNodes();
    for (auto node : nodes) {
        if (std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(node)) return true;
    }
    return false;
}

bool DefineVerticesCommand::runnable(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto nodes = compat::ngRequireObject(context)->hasNodes() ? context->nodes() : project::incSelectedNodes();
    for (auto node : nodes) {
        if (std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node) &&
            !std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(node)) return true;
    }
    return false;
}

bool DefineGridCommand::runnable(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto nodes = compat::ngRequireObject(context)->hasNodes() ? context->nodes() : project::incSelectedNodes();
    for (auto node : nodes) {
        if (std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(node)) return true;
    }
    return false;
}

} // namespace nijigenerate::commands::vertex::define_mesh
