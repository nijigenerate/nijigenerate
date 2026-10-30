#include "config_detail.hpp"
#include "detail/config_snapshot_capture.hpp"
#include "detail/config_snapshot_compare.hpp"
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <array>
#include <compat/source_allocator.hpp>
#include <utility>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::commands::automesh::config {
namespace detail {
compat::RetainedString configMixinName(std::string_view declaration, std::string_view child,
        compat::RetainedString argument) {
    auto parent = ::nijigenerate::commands::detail::composeCommandTemplateName(
        "nijigenerate.commands.automesh.config", declaration, argument, false);
    try {
        std::string name(parent.view());
        name += '.';
        name.append(child);
        return compat::RetainedString(std::move(name));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
nicxlive::core::d_runtime::SourceTypeSpellingNames configMixinTypeSpellings(
        std::string_view declaration, std::string_view child, compat::RetainedString argument) {
    return configMixinRetainedTypeSpellings(declaration,compat::RetainedString::fromStaticView(child),
        std::move(argument));
}
nicxlive::core::d_runtime::SourceTypeSpellingNames configMixinRetainedTypeSpellings(
        std::string_view declaration, compat::RetainedString unqualified, compat::RetainedString argument) {
    auto qualified = configMixinName(declaration, unqualified.view(), std::move(argument));
    return {::nijigenerate::commands::detail::commandTypeArgumentGroup(qualified),
        ::nijigenerate::commands::detail::commandTypeArgumentGroup(unqualified), qualified, unqualified};
}
String tr(std::string_view text) {
    // Every caller supplies an original static translation-key literal.
    return core::translation::ngTranslateRetained(String::fromStaticView(text));
}
ConfigCommandText commandText(std::string_view label, std::string_view description) {
    auto translatedLabel = tr(label);
    auto translatedDescription = tr(description);
    return {std::move(translatedLabel), std::move(translatedDescription)};
}
String format(std::string_view pattern, String text) {
    const std::array args{core::translation::ngFormatArgument(text)};
    auto formatted = core::translation::ngFormatTyped(pattern,args);
    try { return String(std::move(formatted)); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
ConfigCommandText formattedCommandText(std::string_view pattern, std::string_view processorName,
        std::string_view description) {
    auto translatedPattern = tr(pattern);
    // All callers pass the original static AMProcInfo name literal.
    auto retainedName = String::fromStaticView(processorName);
    auto translatedLabel = format(translatedPattern.view(), retainedName);
    auto translatedDescription = tr(description);
    return {std::move(translatedLabel), std::move(translatedDescription)};
}
AutoMeshConfigSnapshot snapshot(const std::shared_ptr<Reflect>& reflect) {
    return _autoMeshConfigSnapshot(reflect);
}
void restore(const std::shared_ptr<Processor>& processor, AutoMeshConfigSnapshot state) {
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) return;
    reflect->writeValues(String::fromStaticLiteral("Simple"),state.simple.retainedString());
    reflect->writeValues(String::fromStaticLiteral("Advanced"),state.advanced.retainedString());
}
void push(const std::shared_ptr<Processor>& processor,const std::shared_ptr<Reflect>& reflect,
        AutoMeshConfigSnapshot previous) {
    auto next=snapshot(reflect);
    if (!_sameAutoMeshConfig(previous,next))
        core::actionstack::incActionPush(std::allocate_shared<AutoMeshConfigChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<AutoMeshConfigChangeAction>{}, processor, previous, next));
}
std::shared_ptr<Processor> resolve(String id) {
    for (auto processor : viewport::vertex::ngAutoMeshProcessors())
        if (compat::ngRequireObject(processor)->procId()==id) return processor;
    return {};
}
compat::Slice<std::shared_ptr<Processor>> configProcessors() {
    return viewport::vertex::ngAutoMeshProcessors();
}
std::shared_ptr<CommandResult> runGetConfig(std::shared_ptr<Processor> processor, const String& level) {
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) {
        dialog("Error",[&] { return tr("Processor not reflectable"); });
        return result(false,"Not reflectable");
    }
    auto values=reflect->values(level.empty()?String::fromStaticLiteral("Simple"):level);
    dialog("Values",[&] { return values; });
    return result(true);
}
std::shared_ptr<CommandResult> runSetConfig(std::shared_ptr<Processor> processor,
        const String& level, const String& updates) {
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) {
        dialog("Error",[&] { return tr("Processor not reflectable"); });
        return result(false,"Not reflectable");
    }
    auto oldState=snapshot(reflect);
    if (!reflect->writeValues(level.empty()?String::fromStaticLiteral("Simple"):level,updates)) {
        dialog("Error",[&] { return tr("No values applied"); });
        return result(false,"No values applied");
    }
    push(processor,reflect,oldState);
    return result(true);
}
std::shared_ptr<CommandResult> runPresetConfig(std::shared_ptr<Processor> processor, const String& preset) {
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) {
        dialog("Error",[&] { return tr("Processor not reflectable"); });
        return result(false,"Not reflectable");
    }
    auto oldState=snapshot(reflect);
    if (!reflect->applyPreset(preset)) {
        dialog("Error",[&] { return format("Preset not found: %s",preset); });
        return result(false,"Preset not found");
    }
    push(processor,reflect,oldState);
    return result(true);
}
}
AutoMeshConfigChangeAction::AutoMeshConfigChangeAction(std::shared_ptr<detail::Processor> value,
        AutoMeshConfigSnapshot oldValue, AutoMeshConfigSnapshot newValue)
    : processor(value),processorName(value?value->displayName():detail::tr("AutoMesh")),
      oldState(oldValue),newState(newValue) {
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshConfigChangeAction>();
}
void AutoMeshConfigChangeAction::rollback() {
    const auto receiver = weak_from_this().lock();
    detail::restore(processor,oldState);
}
void AutoMeshConfigChangeAction::redo() {
    const auto receiver = weak_from_this().lock();
    detail::restore(processor,newState);
}
actions::ActionText AutoMeshConfigChangeAction::describe() {
    const auto receiver = weak_from_this().lock();
    return actions::ActionText(detail::format(detail::tr("Changed AutoMesh configuration: %s").view(),processorName));
}
actions::ActionText AutoMeshConfigChangeAction::describeUndo() {
    const auto receiver = weak_from_this().lock();
    return actions::ActionText(detail::format(detail::tr("Reverted AutoMesh configuration: %s").view(),processorName));
}
actions::ActionText AutoMeshConfigChangeAction::getName() {
    const auto receiver = weak_from_this().lock();
    return actions::ActionText(detail::String::fromStaticLiteral("AutoMeshConfigChangeAction"));
}
compat::RetainedString AutoMeshConfigChangeAction::toString() {
    const auto receiver = weak_from_this().lock();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
}
