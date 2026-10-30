#include "helpers.hpp"
#include "detail/helper_number.hpp"
#include <nijigenerate/core/shortcut/base.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/project.hpp>
#include <param/binding_impl.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::api::mcp::helpers {
using namespace core::settings;
using Context = commands::Context;
namespace {
using Text=compat::RetainedString;
template<class Allocate> decltype(auto) allocateContextBytes(Allocate&& allocate) {
    try { return std::forward<Allocate>(allocate)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
void appendContextBytes(std::string& destination,std::string_view bytes) {
    allocateContextBytes([&] { destination.append(bytes); });
}
[[noreturn]] void fail(compat::RetainedStringSlice message,std::size_t line) {
    throw compat::ThrowableException(message.retainedString(),
        Text::fromStaticLiteral("source/nijigenerate/api/mcp/helpers.d"),line);
}
core::settings::SettingsJsonObject objectRef(SettingsJsonValue value) {
    return value.sourceObjectRef();
}
SettingsJsonValue& aaAt(SettingsJsonObject& object,std::string_view key,std::size_t line) {
    auto* found=object.if_contains(key);
    if(!found) throw compat::ngStaticError<compat::RangeError>(
        Text::fromStaticLiteral("source/nijigenerate/api/mcp/helpers.d"),line);
    return *found;
}
nicxlive::core::common::NullableString bindingName(const SettingsString& value) {
    return value.retainedString().toNullableString();
}
bool member(const SettingsJsonValue& value, std::string_view key) {
    const auto object = value.sourceObjectNoRef();
    return object.if_contains(key) != nullptr;
}
bool readBindingDescriptors(SettingsJsonValue value, std::shared_ptr<nicxlive::core::Puppet> puppet,
        std::shared_ptr<nicxlive::core::param::Parameter> parameter,
        Context::Bindings& bindings, compat::RetainedStringSlice& message) {
    bindings = {}; message.reset();
    const auto reject = [&]<class T>(T&& text) {
        if constexpr(std::is_array_v<std::remove_reference_t<T>>) message=Text::fromStaticLiteral(text);
        else message=allocateContextBytes([&] { return Text(std::forward<T>(text)); });
        return false;
    };
    if (!value.is_array()) return reject("context.bindings must be an array of {target, name}");
    if (!parameter) return reject("context.bindings requires context.parameters[0]");
    const auto entries = value.as_array();
    for (auto entry : entries) {
        if (!entry.is_object()) return reject("context.bindings entries must be objects");
        auto object = objectRef(entry);
        const std::string_view targetKey=object.if_contains("target")?"target":object.if_contains("targetUuid")?"targetUuid":"";
        const std::string_view nameKey=object.if_contains("name")?"name":object.if_contains("bindingName")?"bindingName":"";
        if (targetKey.empty() || nameKey.empty()) return reject("context.bindings entries require target and name");
        if (!aaAt(object,targetKey,201).is_int64() || aaAt(object,targetKey,201).as_int64() < 0)
            return reject("context.bindings target must be a non-negative integer UUID");
        if (!aaAt(object,nameKey,205).is_string() || aaAt(object,nameKey,205).as_string().empty())
            return reject("context.bindings name must be a non-empty string");
        const auto uuid = static_cast<std::uint32_t>(aaAt(object,targetKey,210).as_int64());
        std::shared_ptr<nicxlive::core::Resource> target=compat::ngRequireObject(puppet)->findNodeById(uuid);
        if(!target) target=compat::ngRequireObject(puppet)->findParameter(uuid);
        if(!target) return reject(allocateContextBytes([&] {
            return "context.bindings target UUID was not found: " + std::to_string(uuid);
        }));
        const auto name=bindingName(aaAt(object,nameKey,218).as_string());
        const auto binding=compat::ngRequireObject(parameter)->getBinding(target,name);
        if(!binding) {
            // The Source name operand is evaluated before its first concatenation.
            const auto parameterName=compat::ngRequireObject(parameter)->getName();
            auto failure=allocateContextBytes([] { return std::string("Binding was not found on parameter '"); });
            appendContextBytes(failure,parameterName.view());
            appendContextBytes(failure,"': target=");
            const auto number=allocateContextBytes([&] { return std::to_string(uuid); });
            appendContextBytes(failure,number);
            appendContextBytes(failure,", name=");
            const auto requestedName=aaAt(object,nameKey,221).as_string();
            appendContextBytes(failure,requestedName.view());
            return reject(std::move(failure));
        }
        bindings.append(binding);
    }
    if (bindings.empty()) return reject("context.bindings must contain at least one binding descriptor");
    return true;
}
}
std::shared_ptr<Context> buildContextFromPayload(SettingsJsonValue payload) {
    const auto context = core::shortcut::ngBuildExecutionContext();
    if (!member(payload, "context") || !payload.sourceIndexValue("context").is_object()) return context;
    auto object = payload.sourceIndexValue("context");
    const bool explicitParameters = member(object, "parameters") || member(object, "armedParameters");
    const auto puppet = project::incActivePuppet();
    if (puppet) {
        if (member(object, "parameters") && object.sourceIndexValue("parameters").is_array()) {
            compat::ngRequireObject(context)->hasParameters(false);
            Context::Parameters parameters;
            const auto entries = object.sourceIndexValue("parameters").sourceArrayValue();
            for (const auto entry : entries) if (entry.is_int64())
                if (auto parameter = puppet->findParameter(static_cast<std::uint32_t>(entry.as_int64())))
                    parameters.append(parameter);
            if (!parameters.empty()) compat::ngRequireObject(context)->parameters(parameters);
        }
        if (member(object, "armedParameters") && object.sourceIndexValue("armedParameters").is_array()) {
            compat::ngRequireObject(context)->hasArmedParameters(false);
            Context::Parameters parameters;
            const auto entries = object.sourceIndexValue("armedParameters").sourceArrayValue();
            for (const auto entry : entries) if (entry.is_int64())
                if (auto parameter = puppet->findParameter(static_cast<std::uint32_t>(entry.as_int64())))
                    parameters.append(parameter);
            if (!parameters.empty()) compat::ngRequireObject(context)->armedParameters(parameters);
        } else if (member(object, "parameters")) compat::ngRequireObject(context)->hasArmedParameters(false);
        if (member(object, "nodes") && object.sourceIndexValue("nodes").is_array()) {
            compat::ngRequireObject(context)->hasNodes(false);
            Context::Nodes nodes;
            const auto entries = object.sourceIndexValue("nodes").sourceArrayValue();
            for (const auto entry : entries) if (entry.is_int64())
                if (auto node = puppet->findNodeById(static_cast<std::uint32_t>(entry.as_int64()))) nodes.append(node);
            if (!nodes.empty()) compat::ngRequireObject(context)->nodes(nodes);
        }
        if (member(object, "bindings") || member(object, "activeBindings")) {
            const auto value = member(object, "bindings") ? object.sourceIndexValue("bindings") :
                object.sourceIndexValue("activeBindings");
            const auto parameter = compat::ngRequireObject(context)->hasParameters() && !compat::ngRequireObject(context)->parameters().empty() ?
                compat::ngRequireObject(context)->parameters()[0] : nullptr;
            Context::Bindings bindings;
            compat::RetainedStringSlice message;
            if (!readBindingDescriptors(value, puppet, parameter, bindings, message)) fail(message,283);
            compat::ngRequireObject(context)->activeBindings(bindings);
        }
    }
    if (member(object, "keyPoint")) fail(Text::fromStaticLiteral("context.keyPoint is not supported by MCP. Use context.parameterValue with parameter-axis values."),288);
    if (member(object, "parameterValue") && member(object, "paramValue"))
        fail(Text::fromStaticLiteral("Use only one of context.parameterValue or deprecated context.paramValue."),290);
    if (explicitParameters && !member(object, "parameterValue") && !member(object, "paramValue")) {
        compat::ngRequireObject(context)->hasKeyPoint(false); compat::ngRequireObject(context)->hasExplicitKeyPoint(false);
    }
    if (member(object, "parameterValue") || member(object, "paramValue")) {
        const auto value = member(object, "parameterValue") ? object.sourceIndexValue("parameterValue") :
            object.sourceIndexValue("paramValue");
        nicxlive::core::math::Vec2 parameterValue;
        compat::RetainedStringSlice message;
        if (!detail::_readParamValue(value, parameterValue, message)) fail(message,301);
        compat::ngRequireObject(context)->parameterValue(parameterValue);
        compat::ngRequireObject(context)->hasKeyPoint(false); compat::ngRequireObject(context)->hasExplicitKeyPoint(false);
    }
    return context;
}
}
