#pragma once

#include "map_selection.hpp"
#include <nijigenerate/commands/view/panel_command_map.hpp>

namespace nijigenerate::commands::detail {

// PanelCommandMap is the existing registry for the distinct source PanelKey domain.
template<> struct AssociativeMapKey<view::panel::PanelCommandMap> {
    using type = view::panel::PanelKey;
};

} // namespace nijigenerate::commands::detail
