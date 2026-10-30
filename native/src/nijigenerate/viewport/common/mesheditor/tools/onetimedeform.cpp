#include "onetimedeform_scope.hpp"
#include "grid.hpp"
#include "package.hpp"
#include <nijigenerate/actions/mesheditor.hpp>
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/actions/binding_add_remove_action.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/widgets/statusbar.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/viewport/model/mesheditor/deformable.hpp>
#include <nijigenerate/viewport/vertex/mesheditor/deformable.hpp>
#include <algorithm>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::common::mesheditor::tools::onetimedeform {
using namespace project;
using namespace core::actionstack;
using detail::ngOneTimeDeformScope;
using detail::ngActiveOneTimeDeformScope;
using detail::ngEnsureOneTimeDeformCallbacks;
using detail::BindingRef;
using detail::ParameterRef;
using template_detail::translated;
bool detail::ngDisplayOneTimeDeformToolOptions(bool grid, base::Editors editors) {
    if (editors.empty()) return false;
    const auto editor = [&] {
        const auto values = [&] {
            try {
                // This concrete array contains passive real editor handles.
                return editors.values();
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }();
        return compat::ngArrayAt(values, 0);
    }();
    auto tool = std::dynamic_pointer_cast<OneTimeDeformBase>(compat::ngRequireObject(editor)->getTool());
    auto session = ngActiveOneTimeDeformScope(); if (!tool || !session || !session->active()) return false;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    struct Pop { ~Pop() { ImGui::PopStyleVar(2); } } pop;
    if (widgets::ngButtonColored(grid ? "\xee\x8f\xac" : "\xee\x9d\x86", ImVec2(0, 0), tool->mode == SubToolMode::Vertex ?
        widgets::colorUndefined : ImVec4(0.6f, 0.6f, 0.6f, 1))) tool->setSubToolMode(SubToolMode::Vertex, editor);
    widgets::ngTooltip(compat::RetainedStringSlice(grid ? translated("Grid Vertex Tool") : translated("Define paths")));
    ImGui::SameLine(0, 0);
    if (widgets::ngButtonColored("\xee\xa7\xac", ImVec2(0, 0), tool->mode == SubToolMode::Deform ?
        widgets::colorUndefined : ImVec4(0.6f, 0.6f, 0.6f, 1))) tool->setSubToolMode(SubToolMode::Deform, editor);
    widgets::ngTooltip(compat::RetainedStringSlice(grid ? translated("Grid deformation") : translated("Transform path")));
    if (!grid && tool->mode == SubToolMode::Deform && session->defImpl) {
        ImGui::SameLine(0, 4);
        base::Editors subEditors;
        subEditors[compat::ngRequireObject(session->defImpl)->getTarget()] = session->defImpl;
        if (auto info = ngGetToolInfoOf(base::VertexToolMode::BezierDeform))
            info->displayToolOptions(compat::ngRequireObject(session->defImpl)->deformOnly,
                base::VertexToolMode::BezierDeform, subEditors);
    }
    return false;
}
template class OneTimeDeform<GridDeformer>;
template class OneTimeDeform<PathDeformer>;
}
