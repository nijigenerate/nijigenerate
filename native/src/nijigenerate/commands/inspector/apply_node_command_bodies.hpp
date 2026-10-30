// Implements the original generic Apply and Toggle command bodies against real inspector properties.
#pragma once
#include <nijigenerate/commands/inspector/apply_node_commands.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::commands::inspector::apply_node {
namespace detail {
template<class Access, CommandLiteral TargetName, std::size_t PlainSize, std::size_t TypedSize>
compat::RetainedString inspectorCommandText(const char (&plain)[PlainSize], const char (&typed)[TypedSize]) {
    if constexpr (std::same_as<typename Access::Target, nicxlive::core::nodes::Node>) {
        const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(plain));
        const std::array<core::translation::TypedFormatArgument, 1> args{
            core::translation::ngFormatArgument(compat::RetainedString::fromStaticLiteral(Access::id.text))};
        auto formatted = core::translation::ngFormatTyped(pattern.view(), args);
        return retainInspectorText(std::move(formatted));
    } else {
        const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(typed));
        const std::array<core::translation::TypedFormatArgument, 2> args{
            core::translation::ngFormatArgument(compat::RetainedString::fromStaticLiteral(Access::id.text)),
            core::translation::ngFormatArgument(compat::RetainedString::fromStaticLiteral(TargetName.value))};
        auto formatted = core::translation::ngFormatTyped(pattern.view(), args);
        return retainInspectorText(std::move(formatted));
    }
}
template<class Access, CommandLiteral TargetName>
InspectorToggleText inspectorToggleText() {
    InspectorToggleText result;
    result.label = inspectorCommandText<Access, TargetName>("Toggle %s", "Toggle %s (%s)");
    result.description = inspectorCommandText<Access, TargetName>(
        "Toggles the value of %s", "Toggles the value of %s for %s");
    return result;
}
}
template<class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>::ApplyInspectorPropCommand(typename Access::Value value)
    : ApplyInspectorPropCommand(SourceConstructorFrame::capture(value)) {}

template<class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
auto ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>::SourceConstructorFrame::capture(ValT& value)
        -> SourceConstructorFrame {
    // The reference keeps the original named parameter live. Only the actual
    // ExCommand parameter operation copies it, after formatting the description.
    return {detail::inspectorCommandText<Access, TargetName>("Apply %s", "Apply %s (%s)"), value};
}

template<class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>::ApplyInspectorPropCommand(SourceConstructorFrame frame)
    : ExCommand<InspectorApplyFields<Access, DTypeName>>(compat::RetainedString{},
        std::move(frame.description), frame.value) {}

template<class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
std::shared_ptr<CommandResult> ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>::run(
    std::shared_ptr<Context> context) {
    const auto receiver = this->weak_from_this().lock();
    std::shared_ptr<I> inspector;
    if (compat::ngRequireObject(context)->hasInspectors()) for (auto candidate : context->inspectors()) {
        inspector = std::dynamic_pointer_cast<I>(candidate);
        if (inspector) break;
    }
    if (!inspector) return CommandResult::opCall(false, "Inspector not available");
    inspector->subMode(project::ngModelEditSubMode());
    if (context->hasNodes()) inspector->capture(context->nodes());
    using Action = InspectorAttributeAction<I, Access>;
    typename Action::Nodes nodes;
    typename Action::Values oldValues;
    const bool deform = inspector->subMode() == panels::inspector::ModelEditSubMode::Deform;
    if (!deform && context->hasNodes()) {
        for (auto node : context->nodes())
            if (auto target = std::dynamic_pointer_cast<typename Access::Target>(node)) nodes.append(target);
        for (auto node : nodes) oldValues.append(Access::get(inspector)->get(node));
    }
    Access::get(inspector)->value = this->value;
    Access::get(inspector)->apply();
    if (!deform && context->hasNodes() && !oldValues.empty() && panels::inspector::ngInspectorValuesDiffer(typename Access::Value(oldValues[0]), this->value))
        core::actionstack::incActionPush(std::allocate_shared<Action>(
            nicxlive::core::d_runtime::SourceAllocator<Action>{}, inspector, nodes, oldValues, this->value));
    if (context->hasNodes()) inspector->capture(context->nodes());
    return CommandResult::opCall(true);
}

template<class I, class Access, CommandLiteral TargetName>
ToggleInspectorPropCommand<I, Access, TargetName>::ToggleInspectorPropCommand()
    : ToggleInspectorPropCommand(sourceConstructorText()) {}

template<class I, class Access, CommandLiteral TargetName>
detail::InspectorToggleText ToggleInspectorPropCommand<I, Access, TargetName>::sourceConstructorText() {
    return detail::inspectorToggleText<Access, TargetName>();
}

template<class I, class Access, CommandLiteral TargetName>
ToggleInspectorPropCommand<I, Access, TargetName>::ToggleInspectorPropCommand(detail::InspectorToggleText text)
    : ExCommand<InspectorToggleFields>(std::move(text.label), std::move(text.description)) {}

template<class I, class Access, CommandLiteral TargetName>
std::shared_ptr<CommandResult> ToggleInspectorPropCommand<I, Access, TargetName>::run(
    std::shared_ptr<Context> context) {
    const auto receiver = this->weak_from_this().lock();
    std::shared_ptr<I> inspector;
    if (compat::ngRequireObject(context)->hasInspectors()) for (auto candidate : context->inspectors()) {
        inspector = std::dynamic_pointer_cast<I>(candidate);
        if (inspector) break;
    }
    if (!inspector) return CommandResult::opCall(false, "Inspector not available");
    inspector->subMode(project::ngModelEditSubMode());
    if (context->hasNodes()) inspector->capture(context->nodes());
    using Action = InspectorToggleAttributeAction<I, Access>;
    typename Action::Nodes nodes;
    typename Action::Values oldValues;
    const bool deform = inspector->subMode() == panels::inspector::ModelEditSubMode::Deform;
    if (!deform && context->hasNodes()) {
        for (auto node : context->nodes())
            if (auto target = std::dynamic_pointer_cast<typename Access::Target>(node)) nodes.append(target);
        for (auto node : nodes) oldValues.append(Access::get(inspector)->get(node));
    }
    Access::get(inspector)->value = !Access::get(inspector)->value;
    Access::get(inspector)->apply();
    if (!deform && context->hasNodes() && !nodes.empty())
        core::actionstack::incActionPush(std::allocate_shared<Action>(
            nicxlive::core::d_runtime::SourceAllocator<Action>{}, inspector, nodes, oldValues));
    if (context->hasNodes()) inspector->capture(context->nodes());
    return CommandResult::opCall(true);
}
}

