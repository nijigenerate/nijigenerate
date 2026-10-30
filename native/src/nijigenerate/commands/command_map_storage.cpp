#include "depth/editor_table.hpp"
#include "inspector/apply_node_table.hpp"
#include "node/node_table.hpp"
#include "parameter/group_table.hpp"
#include "puppet/edit_table.hpp"
#include "puppet/tool_table.hpp"
#include "viewport/control_table.hpp"
#include "viewport/palette_table.hpp"
#include "binding/binding_table.hpp"
#include "parameter/param_table.hpp"
#include "parameter/paramedit_table.hpp"
#include "parameter/prop_table.hpp"
#include "detail/package_log_disabled.hpp"
#include "detail/base_ex_args.hpp"
#include "detail/command_by_id.hpp"

namespace nijigenerate::commands::depth::editor {
thread_local CommandTable commands;
}
namespace nijigenerate::commands::inspector::apply_node {
thread_local CommandTable commands;
}
namespace nijigenerate::commands::node::node {
thread_local CommandTable commands;
}
namespace nijigenerate::commands::parameter::group {
thread_local CommandTable commands;
}
namespace nijigenerate::commands::puppet::edit {
thread_local CommandTable commands;
}
namespace nijigenerate::commands::puppet::tool {
thread_local CommandTable commands;
}
namespace nijigenerate::commands::viewport::control {
thread_local CommandTable commands;
}
namespace nijigenerate::commands::viewport::palette {
thread_local CommandTable commands;
}
namespace nijigenerate::commands::binding {
thread_local CommandTable commands;
}
namespace nijigenerate::commands::parameter::param {
thread_local CommandTable commands;
}
namespace nijigenerate::commands::parameter::paramedit {
thread_local CommandTable commands;
}
namespace nijigenerate::commands::parameter::prop {
thread_local CommandTable commands;
}
