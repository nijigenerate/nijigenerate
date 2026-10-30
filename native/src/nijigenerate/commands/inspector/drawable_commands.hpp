#pragma once
#include "apply_node_commands.hpp"
#include "apply_node_table.hpp"
#include <nijigenerate/panels/inspector/drawable.hpp>
namespace nijigenerate::commands::inspector::apply_node {
using DrawableInspector = panels::inspector::drawable::NodeInspector<
    panels::inspector::ModelEditSubMode::Layout, nicxlive::core::nodes::Drawable>;
using OffsetXAccess = InspectorAttributeAccess<DrawableInspector,&DrawableInspector::offsetX>;
using OffsetYAccess = InspectorAttributeAccess<DrawableInspector,&DrawableInspector::offsetY>;
using OffsetXCommandBase = ApplyInspectorPropCommand<DrawableInspector,OffsetXAccess,"Drawable","float">;
using OffsetYCommandBase = ApplyInspectorPropCommand<DrawableInspector,OffsetYAccess,"Drawable","float">;
using OffsetXCommand = DefApply<"OffsetX",
    DrawableInspector, OffsetXAccess, "Drawable", "float">::Apply;
using OffsetYCommand = DefApply<"OffsetY",
    DrawableInspector, OffsetYAccess, "Drawable", "float">::Apply;
void ngInitDrawableOffsetCommands();
}
namespace nijigenerate::commands {
template<> struct CommandTraits<inspector::apply_node::OffsetXCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::OffsetXCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::OffsetYCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::OffsetYCommandBase>;
};
}
