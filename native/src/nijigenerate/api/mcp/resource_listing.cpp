#include "resource_listing.hpp"
#include "detail/resource_entry.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <common/resource_key_map.hpp>
#include <compat/native_access_fault.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <functional>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::api::mcp::resource_listing {
using namespace core::settings;
namespace {
template<class Operation> auto allocateResourceBytes(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
std::string decimal(std::uint64_t value) {
    return allocateResourceBytes([&] { return std::to_string(value); });
}
void append(std::string& destination, std::string_view bytes) {
    allocateResourceBytes([&] { destination.append(bytes); });
}
compat::RetainedString retainBytes(std::string bytes) {
    return allocateResourceBytes([&] { return compat::RetainedString(std::move(bytes)); });
}
struct UuidHash {
    std::size_t operator()(std::uint32_t value) const noexcept { return value; }
};
std::string encodeComponent(std::string_view value) {
    // Phobos std.uri.encodeComponent first validates UTF-8 via toUTF32.
    for (std::size_t position = 0; position < value.size();)
        (void)core::selector::ngDecodeSelectorUtf8(value, position);
    return allocateResourceBytes([&] {
        constexpr char hex[] = "0123456789ABCDEF";
        std::string result;
        for (unsigned char byte : value) {
            const bool plain = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
                (byte >= '0' && byte <= '9') || std::string_view("-_.!~*'()").find(byte) != std::string_view::npos;
            if (plain) result.push_back(static_cast<char>(byte));
            else { result.push_back('%'); result.push_back(hex[byte >> 4]); result.push_back(hex[byte & 15]); }
        }
        return result;
    });
}
}
SettingsJsonValue buildCurrentResourceList() {
    using compat::RetainedString;
    SettingsJsonArray resources;
    resources.append(detail::resourceEntry(
        RetainedString::fromStaticLiteral("resource://nijigenerate/resources/find?selector=*"),
        RetainedString::fromStaticLiteral("Start Here: Explore Entire Model Tree")));
    resources.append(detail::resourceEntry(
        RetainedString::fromStaticLiteral("resource://nijigenerate/guides/resources"),
        RetainedString::fromStaticLiteral("Resources Guide")));
    resources.append(detail::resourceEntry(
        RetainedString::fromStaticLiteral("resource://nijigenerate/guides/selectors"),
        RetainedString::fromStaticLiteral("Selectors Guide")));
    resources.append(detail::resourceEntry(
        RetainedString::fromStaticLiteral("resource://nijigenerate/guides/find"),
        RetainedString::fromStaticLiteral("Find Guide")));
    const auto puppet = project::incActivePuppet();
    if (puppet) {
        nicxlive::core::common::ResourceKeyMap<std::uint32_t, bool, UuidHash,
            std::equal_to<std::uint32_t>> seen;
        const auto addNodeResource = [&](auto&& self, std::shared_ptr<nicxlive::core::nodes::Node> node) -> void {
            if (!node || seen.contains(node->getUUID())) return;
            seen[node->getUUID()] = true;
            const auto uuid = node->getUUID();
            const auto uri = allocateResourceBytes([&] {
                return RetainedString("resource://nijigenerate/resources/" + std::to_string(uuid));
            });
            const auto name = ext::nodes::ngRetainNodeName(node->getName());
            resources.append(detail::resourceEntry(uri, name));
            const auto children = node->childrenList();
            for (auto child : children) self(self, child);
        };
        addNodeResource(addNodeResource, puppet->root);
        const auto parametersForResources = puppet->parameters;
        for (auto parameter : parametersForResources) {
            if (!parameter || seen.contains(parameter->getUUID())) continue;
            seen[parameter->getUUID()] = true;
            const auto uuid = parameter->getUUID();
            const auto uri = allocateResourceBytes([&] {
                return RetainedString("resource://nijigenerate/resources/" + std::to_string(uuid));
            });
            const auto name = ext::nodes::ngRetainNodeName(parameter->getName());
            resources.append(detail::resourceEntry(uri, name));
        }
        const auto parametersForBindings = puppet->parameters;
        for (auto parameter : parametersForBindings) {
            if (!parameter) continue;
            const auto bindings = parameter->bindings;
            for (auto binding : bindings) {
                const auto bound = nicxlive::core::d_runtime::ngRequireObject(binding)->getTarget();
                const auto target = bound.getResource();
                const auto uuid = target ? target->getUUID() :
                    nicxlive::core::d_runtime::ngRequireObject(binding)->getNodeUUID();
                // Finish the first argument before evaluating the display name.
                auto uri = allocateResourceBytes([] {
                    return std::string("resource://nijigenerate/bindings/get?parameter=");
                });
                const auto parameterUuid = parameter->getUUID();
                append(uri, decimal(parameterUuid));
                append(uri, "&target=");
                append(uri, decimal(uuid));
                append(uri, "&name=");
                const auto uriBindingName = nicxlive::core::d_runtime::ngRequireObject(binding)->getTarget().name;
                append(uri, encodeComponent(uriBindingName.view()));
                const auto retainedUri = retainBytes(std::move(uri));
                const auto parameterName = parameter->getName();
                auto name = allocateResourceBytes([&] { return std::string(parameterName.view()); });
                append(name, " -> ");
                if (target) {
                    const auto targetName = target->getName();
                    append(name, targetName.view());
                } else {
                    append(name, decimal(uuid));
                }
                append(name, ".");
                const auto displayBindingName = nicxlive::core::d_runtime::ngRequireObject(binding)->getTarget().name;
                append(name, displayBindingName.view());
                resources.append(detail::resourceEntry(retainedUri, retainBytes(std::move(name))));
            }
        }
    }
    SettingsJsonObject result;
    result["resources"] = SettingsJsonValue(resources);
    return result;
}
void rewriteResourcesListResponse(SettingsJsonValue& response, SettingsJsonValue& request) {
    if (!request.is_object()) return;
    if (!request.sourceObjectRef().if_contains("method")) return;
    if (!request.sourceIndexValue("method").is_string()) return;
    if (request.sourceIndexValue("method").as_string().view() != "resources/list") return;
    if (!response.is_object()) return;
    if (!response.sourceObjectRef().if_contains("result")) return;
    const auto replacement = buildCurrentResourceList();
    // A virtual resource getter may have replaced response while building.
    // Phobos indexed assignment accepts a current object or null receiver.
    response.sourceIndexAssignSlot(SettingsString(
        compat::RetainedString::fromStaticLiteral("result"))) = replacement;
}
}
