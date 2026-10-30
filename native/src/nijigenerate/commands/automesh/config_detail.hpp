#pragma once
#include "config.hpp"
#include <nijigenerate/viewport/vertex/package.hpp>
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <utility>

namespace nijigenerate::commands::automesh::config {
namespace detail {
using String=compat::RetainedString;
using Processor=viewport::vertex::automesh::AutoMeshProcessor;
using Reflect=viewport::vertex::automesh::meta::IAutoMeshReflect;
String tr(std::string_view);
ConfigCommandText commandText(std::string_view label, std::string_view description);
ConfigCommandText formattedCommandText(std::string_view pattern, std::string_view processorName,
    std::string_view description);
String format(std::string_view, String);
template<class Producer> void dialog(std::string_view title, Producer&& produce) {
    auto translatedTitle = core::translation::ngTranslateCString(title);
    String text(std::forward<Producer>(produce)());
    widgets::dialog::ngDialog(
        widgets::dialog::DialogTag::retained(translatedTitle.data(), std::move(translatedTitle.owner)),
        widgets::dialog::DialogText(std::move(text)));
}
AutoMeshConfigSnapshot snapshot(const std::shared_ptr<Reflect>&);
void restore(const std::shared_ptr<Processor>&, AutoMeshConfigSnapshot);
void push(const std::shared_ptr<Processor>&, const std::shared_ptr<Reflect>&, AutoMeshConfigSnapshot);
std::shared_ptr<Processor> resolve(String);
template<class PT> std::shared_ptr<PT> resolveInstance() {
    for (auto processor : viewport::vertex::ngAutoMeshProcessors())
        if (auto result=std::dynamic_pointer_cast<PT>(processor)) return result;
    return {};
}
inline std::shared_ptr<CommandResult> result(bool success, String message=String::emptyLiteral()) {
    return CommandResult::opCall(success,message);
}
template<std::size_t N> std::shared_ptr<CommandResult> result(bool success,const char (&message)[N]) {
    return CommandResult::opCall(success,String::fromStaticLiteral(message));
}

template<class PT,class Writes> std::shared_ptr<CommandResult> runConfigLevel(Writes&& write) {
    auto processor=resolveInstance<PT>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    auto oldState=reflect?snapshot(reflect):AutoMeshConfigSnapshot{};
    std::forward<Writes>(write)(processor);
    if (reflect) push(processor,reflect,oldState);
    return result(true);
}
}
}
