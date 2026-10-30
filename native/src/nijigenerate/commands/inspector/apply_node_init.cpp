#include "apply_node.hpp"
#include "apply_node_initialization.hpp"
#include "node_commands.hpp"
#include "drawable_commands.hpp"
#include "camera_command.hpp"
#include "composite_commands.hpp"
#include "part_commands.hpp"
#include "meshgroup_commands.hpp"
#include "griddeform_commands.hpp"
#include "path_commands.hpp"
#include "simplephysics_commands.hpp"
namespace nijigenerate::commands::inspector::apply_node {
void ngInitInspectorNodeApplyCommands() {
    initialization::Program::run<initialization::Table>();
}
}
