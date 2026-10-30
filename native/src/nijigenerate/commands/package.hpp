#pragma once

// Original public imports, in source order, through their actual module facades.
#include <nijigenerate/commands/base.hpp>
#include <nijigenerate/commands/command_scope.hpp>
#include <nijigenerate/commands/binding/binding.hpp>
#include <nijigenerate/commands/node/node.hpp>
#include <nijigenerate/commands/node/dynamic.hpp>
#include <nijigenerate/commands/parameter/animedit.hpp>
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
#include <nijigenerate/commands/view/panel.hpp>
#include <nijigenerate/commands/inspector/apply_node.hpp>
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

#include <nijigenerate/commands/detail/map_selection.hpp>
#include <nijigenerate/commands/detail/map_validation.hpp>
#include <nijigenerate/commands/detail/cmd_map_adapters.hpp>
#include <nijigenerate/commands/detail/map_initialization.hpp>
#include <nijigenerate/commands/detail/automesh_forced_aliases.hpp>
#include <nijigenerate/commands/detail/apply_args.hpp>
#include <nijigenerate/commands/detail/command_by_id.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <nijigenerate/commands/puppet/tool_map_metadata.hpp>
#include <nijigenerate/commands/all_command_maps.hpp>
#include <nijigenerate/commands/detail/initializer_scan.hpp>
#include <nijigenerate/commands/detail/initializer_diagnostics.hpp>
#include <nijigenerate/commands/cmd.hpp>

namespace nijigenerate::commands {

void ngInitAllCommands();

}
