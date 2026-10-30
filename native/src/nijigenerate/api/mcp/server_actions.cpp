#include "server.hpp"
#include "nijigenerate/core/actionstack.hpp"
#include "nijigenerate/project.hpp"
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <span>
#include <stdexcept>
#include <string_view>

namespace nijigenerate::api::mcp::server {
namespace {

bool _mcpNeedsActionScopeForCurrentMode(compat::RetainedStringSlice toolName) {
    const auto name = toolName.view();
    return name.starts_with("VertexCommand_") || name == "EditCommand_Undo" || name == "EditCommand_Redo";
}

} // namespace

void ngMcpPrepareActionScopeForCurrentMode(compat::RetainedStringSlice toolName) {
    if (!_mcpNeedsActionScopeForCurrentMode(toolName)) return;

    using namespace core::actionstack;
    using project::EditMode;
    ngFlushActionStackGroups();
    switch (project::incEditMode()) {
    case EditMode::VertexEdit: {
        const compat::Slice<ActionStackScopeUnit> allowed{ActionStackScopeUnit::VertexEdit};
        ngGuardActionStackScopes(std::span<const ActionStackScopeUnit>(allowed.data(),allowed.size()));
        break;
    }
    case EditMode::DepthEdit: {
        const compat::Slice<ActionStackScopeUnit> allowed{ActionStackScopeUnit::DepthEdit};
        ngGuardActionStackScopes(std::span<const ActionStackScopeUnit>(allowed.data(),allowed.size()));
        break;
    }
    case EditMode::ModelEdit:
    case EditMode::AnimEdit:
    case EditMode::ModelTest:
    case EditMode::ALL:
        ngGuardActionStackScopes();
        break;
    default:
        throw compat::ngStaticError<compat::SwitchError>(
            compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/api/mcp/server.d"),155);
    }
    ngFlushActionStackGroups();
}

void ngMcpFinishActionBoundary() {
    core::actionstack::ngFlushActionStackGroups();
}

void ngAcpLoadSettings() {
    // ACP settings are read on demand through incSettingsGet in the original source.
}

} // namespace nijigenerate::api::mcp::server
