#pragma once
#include <nijigenerate/providers/mcp/resources.hpp>
#include <param/parameter.hpp>
#include <param/binding.hpp>
#include <utility>
namespace nijigenerate::api::mcp::server::detail {
using Json = core::settings::SettingsJsonValue;
using String = compat::RetainedStringSlice;
using Parameter = nicxlive::core::param::Parameter;
using Binding = nicxlive::core::param::ParameterBinding;
template<class Value> void assignResourceMember(core::settings::SettingsJsonObject& object,
        String key, Value&& value) {
    Json entry(std::forward<Value>(value)());
    const auto destination = object.sourceAssignment(core::settings::SettingsString(key));
    *destination.value = std::move(entry);
}
String ngDecodeMcpResourceComponent(String);
String bindingResourceUri(std::shared_ptr<Parameter>, std::shared_ptr<Binding>);
bool parseBindingDescriptorPath(String, std::uint32_t&, std::uint32_t&, String&, String&);
Json bindingToJson(std::shared_ptr<Parameter>, std::shared_ptr<Binding>);
std::shared_ptr<Parameter> bindingOwnerParameter(std::shared_ptr<Binding>);
Json readBindingByDescriptor(std::uint32_t, std::uint32_t, String);
providers::mcp::resources::ResourceContents buildFindResource(String);
}
