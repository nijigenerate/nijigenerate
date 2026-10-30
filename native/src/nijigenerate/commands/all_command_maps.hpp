#pragma once

#include "all_maps.hpp"
#include "detail/map_selection.hpp"
#include "puppet/tool_map_metadata.hpp"
#include <tuple>

namespace nijigenerate::commands::detail {

// A D alias names the existing AA variable. Resolve its TLS identity at get(),
// without copying the descriptor or retaining another thread's object address.
#define NG_EXISTING_COMMAND_MAP(Name, Object) \
    struct Name { \
        using map_type = decltype(Object); \
        static map_type& get() { return Object; } \
    };

NG_EXISTING_COMMAND_MAP(ScopeCommandMapBinding, scopeCommands)
NG_EXISTING_COMMAND_MAP(BindingCommandMapBinding, binding::commands)
NG_EXISTING_COMMAND_MAP(NodeCommandMapBinding, node::node::commands)
NG_EXISTING_COMMAND_MAP(AnimeditCommandMapBinding, parameter::animedit::commands)
NG_EXISTING_COMMAND_MAP(GroupCommandMapBinding, parameter::group::commands)
NG_EXISTING_COMMAND_MAP(ParameterCommandMapBinding, parameter::param::commands)
NG_EXISTING_COMMAND_MAP(ParameterEditCommandMapBinding, parameter::paramedit::commands)
NG_EXISTING_COMMAND_MAP(ParameterPropCommandMapBinding, parameter::prop::commands)
NG_EXISTING_COMMAND_MAP(FileCommandMapBinding, puppet::file::commands)
NG_EXISTING_COMMAND_MAP(EditCommandMapBinding, puppet::edit::commands)
NG_EXISTING_COMMAND_MAP(ViewCommandMapBinding, puppet::view::commands)
// ToolCommandMapBinding is already published by tool_map_metadata.hpp.
NG_EXISTING_COMMAND_MAP(ViewportControlMapBinding, viewport::control::commands)
NG_EXISTING_COMMAND_MAP(ViewportPaletteMapBinding, viewport::palette::commands)
NG_EXISTING_COMMAND_MAP(SelectToolModeMapBinding, mesheditor::tool::selectToolModeCommands)
NG_EXISTING_COMMAND_MAP(TogglePanelMapBinding, view::panel::togglePanelCommands)
NG_EXISTING_COMMAND_MAP(AddNodeMapBinding, node::dynamic::addNodeCommands)
NG_EXISTING_COMMAND_MAP(InsertNodeMapBinding, node::dynamic::insertNodeCommands)
NG_EXISTING_COMMAND_MAP(ConvertNodeMapBinding, node::dynamic::convertNodeCommands)
NG_EXISTING_COMMAND_MAP(InspectorApplyNodeMapBinding, inspector::apply_node::commands)
NG_EXISTING_COMMAND_MAP(NodeMaskMapBinding, node::mask::commands)
NG_EXISTING_COMMAND_MAP(NodeWeldingMapBinding, node::welding::commands)
NG_EXISTING_COMMAND_MAP(NodeSimplePhysicsMapBinding, node::simplephysics::commands)
NG_EXISTING_COMMAND_MAP(AutoMeshApplyMapBinding, automesh::dynamic::autoMeshApplyCommands)
NG_EXISTING_COMMAND_MAP(AutoMeshConfigMapBinding, automesh::config::commands)
NG_EXISTING_COMMAND_MAP(AutoMeshGetConfigMapBinding, automesh::config::autoMeshGetConfigCommands)
NG_EXISTING_COMMAND_MAP(AutoMeshSetConfigMapBinding, automesh::config::autoMeshSetConfigCommands)
NG_EXISTING_COMMAND_MAP(AutoMeshTypedMapBinding, automesh::config::autoMeshTypedCommands)
NG_EXISTING_COMMAND_MAP(VertexDefineMeshMapBinding, vertex::define_mesh::commands)
NG_EXISTING_COMMAND_MAP(ModelDeformBindingMapBinding, model::set_deform_binding::commands)
NG_EXISTING_COMMAND_MAP(DepthBoneMapBinding, depth::bone::commands)
NG_EXISTING_COMMAND_MAP(DepthEditorMapBinding, depth::editor::commands)
NG_EXISTING_COMMAND_MAP(DepthMapMapBinding, depth::map::commands)
NG_EXISTING_COMMAND_MAP(PsdDepthDialogMapBinding, depth::psd_dialog::commands)

#undef NG_EXISTING_COMMAND_MAP

} // namespace nijigenerate::commands::detail

namespace nijigenerate::commands {

// Exact original AliasSeq order. This type alias creates no map or binding object.
using AllCommandMaps = std::tuple<
    detail::ScopeCommandMapBinding,
    detail::BindingCommandMapBinding,
    detail::NodeCommandMapBinding,
    detail::AnimeditCommandMapBinding,
    detail::GroupCommandMapBinding,
    detail::ParameterCommandMapBinding,
    detail::ParameterEditCommandMapBinding,
    detail::ParameterPropCommandMapBinding,
    detail::FileCommandMapBinding,
    detail::EditCommandMapBinding,
    detail::ViewCommandMapBinding,
    detail::ToolCommandMapBinding,
    detail::ViewportControlMapBinding,
    detail::ViewportPaletteMapBinding,
    detail::SelectToolModeMapBinding,
    detail::TogglePanelMapBinding,
    detail::AddNodeMapBinding,
    detail::InsertNodeMapBinding,
    detail::ConvertNodeMapBinding,
    detail::InspectorApplyNodeMapBinding,
    detail::NodeMaskMapBinding,
    detail::NodeWeldingMapBinding,
    detail::NodeSimplePhysicsMapBinding,
    detail::AutoMeshApplyMapBinding,
    detail::AutoMeshConfigMapBinding,
    detail::AutoMeshGetConfigMapBinding,
    detail::AutoMeshSetConfigMapBinding,
    detail::AutoMeshTypedMapBinding,
    detail::VertexDefineMeshMapBinding,
    detail::ModelDeformBindingMapBinding,
    detail::DepthBoneMapBinding,
    detail::DepthEditorMapBinding,
    detail::DepthMapMapBinding,
    detail::PsdDepthDialogMapBinding>;

} // namespace nijigenerate::commands
