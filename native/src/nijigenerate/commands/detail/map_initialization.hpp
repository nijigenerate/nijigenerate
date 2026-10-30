#pragma once

#include "map_selection.hpp"
#include "initializer_body.hpp"
#include "cmd_map_adapters.hpp"
#include <nijigenerate/commands/source_diagnostic_text.hpp>
#include <nijigenerate/commands/binding/binding.hpp>
#include <nijigenerate/commands/command_scope_command.hpp>
#include <nijigenerate/commands/node/node.hpp>
#include <nijigenerate/commands/node/dynamic.hpp>
#include <nijigenerate/commands/parameter/animedit_commands.hpp>
#include <nijigenerate/commands/parameter/group.hpp>
#include <nijigenerate/commands/parameter/param.hpp>
#include <nijigenerate/commands/parameter/paramedit.hpp>
#include <nijigenerate/commands/parameter/prop.hpp>
#include <nijigenerate/commands/puppet/file.hpp>
#include <nijigenerate/commands/puppet/edit.hpp>
#include <nijigenerate/commands/puppet/view.hpp>
#include <nijigenerate/commands/puppet/tool.hpp>
#include <nijigenerate/commands/viewport/control.hpp>
#include <nijigenerate/commands/viewport/palette.hpp>
#include <nijigenerate/commands/mesheditor/tool.hpp>
#include <nijigenerate/commands/view/panel_registry.hpp>
#include <nijigenerate/commands/inspector/apply_node.hpp>
#include <nijigenerate/commands/inspector/apply_node_initialization.hpp>
#include <nijigenerate/commands/node/mask.hpp>
#include <nijigenerate/commands/node/welding.hpp>
#include <nijigenerate/commands/node/simplephysics.hpp>
#include <nijigenerate/commands/automesh/dynamic.hpp>
#include <nijigenerate/commands/automesh/config.hpp>
#include <nijigenerate/commands/vertex/define_mesh.hpp>
#include <nijigenerate/commands/model/set_deform_binding.hpp>
#include <nijigenerate/commands/depth/bone.hpp>
#include <nijigenerate/commands/depth/editor.hpp>
#include <nijigenerate/commands/depth/map.hpp>
#include <nijigenerate/commands/depth/psd_dialog.hpp>
#include <concepts>

namespace nijigenerate::commands::detail {

template<ExistingMapBinding Binding, auto KeyOrigin = nullptr> void ngInitCommandMap() {
    using K = KeyTypeOfCommandMapBinding<Binding>;
    if constexpr (std::same_as<K, binding::BindingCommand>)
        binding::ngInitCommands<K>();
    else if constexpr (std::same_as<K, CommandScopeCommand>)
        ::nijigenerate::commands::ngInitCommands<K>();
    else if constexpr (std::same_as<K, node::node::NodeCommand>)
        node::node::ngInitCommands<K>();
    else if constexpr (std::same_as<K, parameter::animedit::AnimeditCommand>)
        parameter::animedit::ngInitCommands<K>();
    else if constexpr (std::same_as<K, parameter::group::GroupCommand>)
        parameter::group::ngInitCommands<K>();
    else if constexpr (std::same_as<K, parameter::param::ParamCommand>)
        parameter::param::ngInitCommands<K>();
    else if constexpr (std::same_as<K, parameter::paramedit::ParameditCommand>)
        parameter::paramedit::ngInitCommands<K>();
    else if constexpr (std::same_as<K, parameter::prop::ParamPropCommand>)
        parameter::prop::ngInitCommands<K>();
    else if constexpr (std::same_as<K, puppet::file::FileCommand>)
        puppet::file::ngInitCommands<K>();
    else if constexpr (std::same_as<K, puppet::edit::EditCommand>)
        puppet::edit::ngInitCommands<K>();
    else if constexpr (std::same_as<K, puppet::view::ViewCommand>)
        puppet::view::ngInitCommands<K>();
    else if constexpr (std::same_as<K, puppet::tool::ToolCommand>)
        puppet::tool::ngInitCommands<K>();
    else if constexpr (std::same_as<K, viewport::control::ViewportCommand>)
        viewport::control::ngInitCommands<K>();
    else if constexpr (std::same_as<K, viewport::palette::PaletteCommand>)
        viewport::palette::ngInitCommands<K>();
    else if constexpr (std::same_as<K, ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode>)
        mesheditor::tool::ngInitCommands<K>();
    else if constexpr (std::same_as<K, view::panel::PanelKey>)
        view::panel::ngInitCommands<K>();
    else if constexpr (std::same_as<K, node::dynamic::AddNodeKey>)
        node::dynamic::ngInitCommands<K>();
    else if constexpr (std::same_as<K, node::dynamic::InsertNodeKey>)
        node::dynamic::ngInitCommands<K>();
    else if constexpr (std::same_as<K, node::dynamic::ConvertToKey>)
        node::dynamic::ngInitCommands<K>();
    else if constexpr (std::same_as<K, inspector::apply_node::InspectorNodeApplyCommand>)
        inspector::apply_node::ngInitCommands<K>();
    else if constexpr (std::same_as<K, node::mask::NodeMaskCommand>)
        node::mask::ngInitCommands<K>();
    else if constexpr (std::same_as<K, node::welding::NodeWeldingCommand>)
        node::welding::ngInitCommands<K>();
    else if constexpr (std::same_as<K, node::simplephysics::NodeSimplePhysicsCommand>)
        node::simplephysics::ngInitCommands<K>();
    else if constexpr (std::same_as<K, automesh::dynamic::AutoMeshKey>)
        automesh::dynamic::ngInitCommands<K>();
    else if constexpr (std::same_as<K, automesh::config::AutoMeshConfigCommand>)
        automesh::config::ngInitCommands<K>();
    else if constexpr (std::same_as<K, automesh::config::AutoMeshGetConfigKey>)
        automesh::config::ngInitCommands<K>();
    else if constexpr (std::same_as<K, automesh::config::AutoMeshSetConfigKey>)
        automesh::config::ngInitCommands<K>();
    else if constexpr (std::same_as<K, automesh::config::AutoMeshSetSimpleKey>)
        automesh::config::ngInitCommands<K>();
    else if constexpr (std::same_as<K, automesh::config::AutoMeshSetAdvancedKey>)
        automesh::config::ngInitCommands<K>();
    else if constexpr (std::same_as<K, automesh::config::AutoMeshSetPresetKey>)
        automesh::config::ngInitCommands<K>();
    else if constexpr (std::same_as<K, automesh::config::AutoMeshTypedCommand>)
        automesh::config::ngInitCommands<K>();
    else if constexpr (std::same_as<K, vertex::define_mesh::VertexCommand>)
        vertex::define_mesh::ngInitCommands<K>();
    else if constexpr (std::same_as<K, model::set_deform_binding::ModelCommand>)
        model::set_deform_binding::ngInitCommands<K>();
    else if constexpr (std::same_as<K, depth::bone::DepthBoneCommand>)
        depth::bone::ngInitCommands<K>();
    else if constexpr (std::same_as<K, depth::editor::DepthEditorOperationCommand>)
        depth::editor::ngInitCommands<K>();
    else if constexpr (std::same_as<K, depth::map::DepthMapCommand>)
        depth::map::ngInitCommands<K>();
    else if constexpr (std::same_as<K, depth::psd_dialog::PsdDepthDialogCommand>)
        depth::psd_dialog::ngInitCommands<K>();
    else
        static_assert(ngRequireSourceInitializerDispatch<K, KeyOrigin>(),
            "Missing defining Source key diagnostic origin for initializer dispatch");
}

} // namespace nijigenerate::commands::detail
