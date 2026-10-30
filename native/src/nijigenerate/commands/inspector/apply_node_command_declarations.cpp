#include "apply_node_commands.hpp"
#include <nijigenerate/panels/inspector/pathdeform.hpp>

namespace nijigenerate::commands::inspector::apply_node {

using Path = panels::inspector::pathdeform::PathDeformer;
using PathInspector = panels::inspector::pathdeform::NodeInspector<
    panels::inspector::ModelEditSubMode::Layout, Path>;
using PathState = panels::inspector::pathdeform::detail::NodeInspectorState<
    panels::inspector::ModelEditSubMode::Layout, Path>;

// Instantiate the real declaration surfaces without constructing a command or
// substituting an inherited successful run for the unresolved source body.
template<auto member, CommandLiteral ValueName> constexpr bool pathApplyDeclaration() {
    using Access = InspectorAttributeAccess<PathInspector, member>;
    using Storage = InspectorApplyFields<Access, ValueName>;
    using CommandT = ApplyInspectorPropCommand<PathInspector, Access, "PathDeformer", ValueName>;
    static_assert(std::derived_from<CommandT, Command>);
    static_assert(std::same_as<decltype(&CommandT::run),
        std::shared_ptr<CommandResult> (CommandT::*)(std::shared_ptr<Context>)>);
    static_assert(std::tuple_size_v<decltype(CommandFields<Storage>::descriptors)> == 1);
    static_assert(::nijigenerate::commands::detail::validCommandFields<Storage>());
    static_assert(::nijigenerate::commands::detail::firstAttribute<ShortcutVisibility, CommandT>(
        ShortcutVisible).value == CommandShortcutPolicy::hidden);
    static_assert(::nijigenerate::commands::detail::firstAttribute<IrreversibleEffectMeta, CommandT>(
        EffectApply).value == CommandIrreversibleEffect::structuralEdit);
    return true;
}

static_assert(pathApplyDeclaration<&PathState::dynamic, "bool">());
static_assert(pathApplyDeclaration<&PathState::physicsEnabled, "bool">());
static_assert(pathApplyDeclaration<&PathState::curveType, "CurveType">());
static_assert(pathApplyDeclaration<&PathState::gravity, "float">());
static_assert(pathApplyDeclaration<&PathState::restoreConstant, "float">());
static_assert(pathApplyDeclaration<&PathState::damping, "float">());
static_assert(pathApplyDeclaration<&PathState::inputScale, "float">());
static_assert(pathApplyDeclaration<&PathState::propagateScale, "float">());

using DynamicToggle = ToggleInspectorPropCommand<PathInspector,
    InspectorAttributeAccess<PathInspector, &PathState::dynamic>, "PathDeformer">;
using PhysicsToggle = ToggleInspectorPropCommand<PathInspector,
    InspectorAttributeAccess<PathInspector, &PathState::physicsEnabled>, "PathDeformer">;
static_assert(std::derived_from<DynamicToggle, Command>);
static_assert(std::derived_from<PhysicsToggle, Command>);
static_assert(std::tuple_size_v<decltype(CommandFields<InspectorToggleFields>::descriptors)> == 0);

} // namespace nijigenerate::commands::inspector::apply_node
