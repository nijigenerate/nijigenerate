#include "server_resources.hpp"
#include "server_internal.hpp"
#include "task.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/selector/query.hpp>
#include <nijigenerate/core/selector/treestore.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/uri_exception.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <param/binding_impl.hpp>
#include <serde_json.hpp>
#include <common/string_key_map.hpp>
#include <new>
#include <stdexcept>
#include <vector>

namespace nijigenerate::api::mcp::server::detail {
using Object = core::settings::SettingsJsonObject;
using Array = core::settings::SettingsJsonArray;
using nicxlive::core::nodes::Node;
namespace {
template<std::size_t N> String literal(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
template<std::size_t N> [[noreturn]] void uriError(const char (&message)[N], std::size_t sourceLine) {
    // Every call supplies the corresponding authored static message literal.
    throw compat::URIException(compat::RetainedString::fromStaticLiteral(message),
        compat::RetainedString::fromStaticLiteral("std/uri.d"),sourceLine);
}
void appendScalar(std::string& out, std::uint32_t scalar) {
    if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) {
        auto error = compat::UTFException(scalar > 0x10ffff
            ? compat::RetainedString::fromStaticLiteral("Encoding an invalid code point in UTF-8")
            : compat::RetainedString::fromStaticLiteral("Encoding a surrogate code point in UTF-8"),
            compat::RetainedString::fromStaticLiteral("std/utf.d"),2311);
        const std::uint32_t sequence[] = {scalar};
        error.setSequence(sequence);
        throw error;
    }
    char bytes[4];
    std::size_t length;
    if (scalar < 0x80) {
        bytes[0] = static_cast<char>(scalar);
        length = 1;
    } else if (scalar < 0x800) {
        bytes[0] = static_cast<char>(0xc0 | (scalar >> 6));
        bytes[1] = static_cast<char>(0x80 | (scalar & 63));
        length = 2;
    } else if (scalar < 0x10000) {
        bytes[0] = static_cast<char>(0xe0 | (scalar >> 12));
        bytes[1] = static_cast<char>(0x80 | ((scalar >> 6) & 63));
        bytes[2] = static_cast<char>(0x80 | (scalar & 63));
        length = 3;
    } else {
        bytes[0] = static_cast<char>(0xf0 | (scalar >> 18));
        bytes[1] = static_cast<char>(0x80 | ((scalar >> 12) & 63));
        bytes[2] = static_cast<char>(0x80 | ((scalar >> 6) & 63));
        bytes[3] = static_cast<char>(0x80 | (scalar & 63));
        length = 4;
    }
    try {
        out.append(bytes, length);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}
String decodeComponent(std::string_view input) {
    // Independent Phobos URI_Decode indexes char input as bytes, then UTF-8
    // encodes each decoded scalar. In particular '+' is never a space.
    const auto hex = [](unsigned char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    const auto byte = [&](std::size_t k, std::size_t hexErrorLine) {
        if (k + 2 >= input.size()) uriError("Unexpected end of URI",226);
        const auto high = hex(input[k + 1]), low = hex(input[k + 2]);
        if (high < 0 || low < 0) uriError("Expected two hexadecimal digits after '%'",hexErrorLine);
        return static_cast<unsigned>((high << 4) | low);
    };
    auto decoded = [&] {
        try {
            return std::vector<std::uint32_t>(input.size());
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 519);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 519);
        }
    }();
    std::size_t decodedCount = 0;
    for (std::size_t k = 0; k < input.size(); ++k) {
        std::uint32_t scalar = static_cast<unsigned char>(input[k]);
        if (scalar == '%') {
            const auto first = byte(k,228); k += 2; scalar = first;
            if (first & 0x80) {
                unsigned count = 1;
                for (;; ++count) {
                    if (count > 4) uriError("UTF-32 code point size too large",242);
                    if (!((first << count) & 0x80)) {
                        if (count == 1) uriError("UTF-32 code point size too small",246);
                        break;
                    }
                }
                scalar = first & ((1u << (7 - count)) - 1);
                if (k + 3 * (count - 1) >= input.size()) uriError("UTF-32 unaligned String",255);
                for (unsigned j = 1; j < count; ++j) {
                    if (input[++k] != '%') uriError("Expected: '%'",260);
                    const auto next = byte(k,262); k += 2;
                    if ((next & 0xc0) != 0x80) uriError("Incorrect UTF-32 multi-byte sequence",265);
                    scalar = (scalar << 6) | (next & 63);
                }
                if (scalar > 0x10ffff) uriError("Unknown UTF-32 code point",270);
            }
        }
        decoded[decodedCount++] = scalar;
    }
    const auto scalars = [&] {
        if (!decodedCount) return std::vector<std::uint32_t>{};
        try {
            return std::vector<std::uint32_t>(decoded.begin(), decoded.begin() + decodedCount);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    }();
    // URI_Decode completes before decodeComponent starts UTF-8 encoding.
    std::string output;
    for (auto scalar : scalars) appendScalar(output, scalar);
    if(output.empty()) return {};
    return allocateMcpText([&] {return compat::RetainedString(std::move(output));});
}
std::string encodeComponent(std::string_view input) {
    compat::phobos::detail::UnicodeCursor<std::string_view, char> cursor(input);
    while (!cursor.empty()) { (void)cursor.front(); cursor.popFront(); }
    constexpr char hex[] = "0123456789ABCDEF";
    return allocateMcpText([&] {
        std::string out;
        for (unsigned char c : input) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    std::string_view("-_.!~*'()").find(c) != std::string_view::npos) out.push_back(static_cast<char>(c));
            else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
        }
        return out;
    });
}
Json parse(std::string_view input) { return autorig::json::detail::parseJSON(input); }
struct TargetDescription {
    std::shared_ptr<nicxlive::core::Resource> resource;
    std::uint32_t uuid;
    String typeId;
};
TargetDescription describeTarget(const std::shared_ptr<Binding>& binding) {
    auto resource = compat::ngRequireObject(binding)->getTarget().getResource();
    const auto uuid = resource ? resource->getUUID() : binding->getNodeUUID();
    String typeId=compat::RetainedString::emptyLiteral();
    if (auto node = std::dynamic_pointer_cast<Node>(resource)) {
        const auto sourceTypeId=node->typeId();
        typeId=allocateMcpText([&] {return compat::RetainedString(sourceTypeId);});
    }
    else if (std::dynamic_pointer_cast<Parameter>(resource))
        typeId=compat::RetainedString::fromStaticLiteral("Parameter");
    return {resource, uuid, typeId};
}
Json targetJson(const TargetDescription& target) {
    Object result;
    result[literal("uuid")] = static_cast<std::int64_t>(target.uuid);
    if (target.resource) assignResourceMember(result, literal("name"), [&] { return target.resource->getName(); });
    else result[literal("name")] = core::settings::SettingsString(compat::RetainedString::emptyLiteral());
    result[literal("typeId")] = core::settings::SettingsString(target.typeId);
    return result;
}
Json parameterJson(const std::shared_ptr<Parameter>& parameter) {
    Object result;
    assignResourceMember(result, literal("uuid"), [&] {
        return static_cast<std::int64_t>(compat::ngRequireObject(parameter)->getUUID());
    });
    assignResourceMember(result, literal("name"), [&] { return parameter->getName(); });
    return result;
}
String modeName(nicxlive::core::common::InterpolateMode mode) {
    using Mode = nicxlive::core::common::InterpolateMode;
    switch (mode) {
    case Mode::Nearest: return literal("Nearest");
    case Mode::Linear: return literal("Linear");
    case Mode::Step: return literal("Stepped");
    case Mode::Cubic: return literal("Cubic");
    case Mode::Bezier: return literal("Bezier");
    case static_cast<Mode>(5): return literal("COUNT");
    }
    return allocateMcpText([&] {
        return compat::RetainedString("cast(InterpolateMode)" + std::to_string(static_cast<std::int32_t>(mode)));
    });
}
}
String ngDecodeMcpResourceComponent(String value) { return decodeComponent(value.view()); }
String bindingResourceUri(std::shared_ptr<Parameter> parameter, std::shared_ptr<Binding> binding) {
    const auto target = compat::ngRequireObject(binding)->getTarget().getResource();
    const auto uuid = target ? target->getUUID() : binding->getNodeUUID();
    // Preserve left-to-right source concatenation operands across virtual calls.
    const auto parameterUuid=compat::ngRequireObject(parameter)->getUUID();
    const auto parameterText=allocateMcpText([&] {return std::to_string(parameterUuid);});
    const auto targetText=allocateMcpText([&] {return std::to_string(uuid);});
    const auto nameText=encodeComponent(binding->getTarget().name);
    return allocateMcpText([&] {
        return compat::RetainedString("resource://nijigenerate/bindings/get?parameter="+parameterText+
            "&target="+targetText+"&name="+nameText);
    });
}
bool parseBindingDescriptorPath(String path, std::uint32_t& parameterUuid, std::uint32_t& targetUuid,
        String& bindingName, String& message) {
    parameterUuid = 0; targetUuid = 0; bindingName.reset(); message.reset();
    nicxlive::core::common::StringKeyMap<String> query;
    const auto input = allocateMcpText([&] {return "parameter=" + std::string(path.view());});
    for (std::size_t start = 0; start <= input.size();) {
        auto end = input.find('&', start); if (end == std::string::npos) end = input.size();
        const auto part = std::string_view(input).substr(start, end - start);
        if (const auto equals = part.find('='); equals != std::string_view::npos) {
            const auto key = decodeComponent(part.substr(0, equals));
            const auto value = decodeComponent(part.substr(equals + 1));
            query[key.retainedString()] = value;
        }
        if (end == input.size()) break; start = end + 1;
    }
    if (!query.contains("parameter") || !query.contains("target") || !query.contains("name")) {
        message = compat::RetainedString::fromStaticLiteral("Binding read requires parameter, target, and name");
        return false;
    }
    try {
        const auto convert = [](std::string_view input) {
            const auto result = core::selector::ngParseSelectorUInt(input);
            // A suffix after the decimal prefix starts with a non-digit. Reuse
            // the actual uint convError/UTF path for to!uint's all-input check.
            if (!input.empty()) (void)core::selector::ngParseSelectorUInt(input);
            return result;
        };
        parameterUuid = convert(query.get("parameter").view());
        targetUuid = convert(query.get("target").view());
        bindingName = query.get("name");
    } catch (const compat::ThrowableException&) {
        message = compat::RetainedString::fromStaticLiteral(
            "Binding read parameter and target must be integer UUIDs");
        return false;
    }
    if (bindingName.empty()) {
        message = compat::RetainedString::fromStaticLiteral("Binding read name must not be empty");
        return false;
    }
    return true;
}
Json bindingToJson(std::shared_ptr<Parameter> parameter, std::shared_ptr<Binding> binding) {
    const auto target = describeTarget(binding);
    Object result; result[literal("typeId")] = core::settings::SettingsString(literal("Binding"));
    assignResourceMember(result, literal("parameter"), [&] { return parameterJson(parameter); });
    assignResourceMember(result, literal("target"), [&] { return targetJson(target); });
    assignResourceMember(result, literal("name"), [&] { return binding->getTarget().name; });
    assignResourceMember(result, literal("interpolateMode"), [&] {
        return core::settings::SettingsString(modeName(binding->interpolateMode()));
    });
    assignResourceMember(result, literal("setCount"), [&] { return static_cast<std::int64_t>(binding->getSetCount()); });
    assignResourceMember(result, literal("uri"), [&] {
        return core::settings::SettingsString(bindingResourceUri(parameter, binding));
    });
    Array axisOffsets, axisValues;
    for (std::uint32_t axis = 0; axis != 2; ++axis) {
        Array offsets, values; const auto points = parameter->axisPoints[axis];
        for (auto offset : points) { offsets.append(static_cast<double>(offset)); values.append(static_cast<double>(parameter->unmapAxis(axis, offset))); }
        axisOffsets.append(offsets); axisValues.append(values);
    }
    result[literal("axisOffsets")] = axisOffsets; result[literal("axisValues")] = axisValues;
    nicxlive::core::serde::TypedJsonWriter writer;
    (void)binding->serializeSelf(writer); writer.flush();
    assignResourceMember(result, literal("data"), [&] { return parse(writer.str()); });
    return result;
}
std::shared_ptr<Parameter> bindingOwnerParameter(std::shared_ptr<Binding> binding) {
    using namespace nicxlive::core::param;
    if (auto value = std::dynamic_pointer_cast<ValueParameterBinding>(binding)) return value->getParameter();
    if (auto value = std::dynamic_pointer_cast<DeformationParameterBinding>(binding)) return value->getParameter();
    if (auto value = std::dynamic_pointer_cast<ParameterParameterBinding>(binding)) return value->getParameter();
    return nullptr;
}
Json readBindingByDescriptor(std::uint32_t parameterUuid, std::uint32_t targetUuid, String bindingName) {
    Object result;
    const auto error = [&](core::settings::SettingsString value) -> Json {
        result[literal("item")] = nullptr; result[literal("error")] = value; return result;
    };
    const auto puppet = project::incActivePuppet();
    if (!puppet) return error(compat::RetainedString::fromStaticLiteral("No active puppet"));
    const auto parameter = puppet->findParameter(parameterUuid);
    if (!parameter) return error(allocateMcpText([&] {
        return compat::RetainedString("Parameter not found: " + std::to_string(parameterUuid));
    }));
    std::shared_ptr<nicxlive::core::Resource> target=puppet->findNodeById(targetUuid);
    if (!target) target=puppet->findParameter(targetUuid);
    if (!target) return error(allocateMcpText([&] {
        return compat::RetainedString("Target not found: " + std::to_string(targetUuid));
    }));
    const auto binding=parameter->getBinding(target,bindingName.retainedString().toNullableString());
    if (!binding) return error(allocateMcpText([&] {
        return compat::RetainedString("Binding not found: parameter=" + std::to_string(parameterUuid) + ", target=" +
            std::to_string(targetUuid) + ", name=" + std::string(bindingName.view()));
    }));
    assignResourceMember(result, literal("item"), [&] { return bindingToJson(parameter, binding); });
    return result;
}
providers::mcp::resources::ResourceContents buildFindResource(String selectorParam) {
    auto result = task::ngRunInMainThread(task::MainThreadAction<Json>([selectorParam] {
        using namespace core::selector;
        auto selector = createMcpObject<Selector>();
        if (!selectorParam.empty()) selector->build(SelectorString(selectorParam));
        const auto results=selector->run();
        auto tree = createMcpObject<TreeStore_<false>>(); tree->setResources(results);
        const auto makeTree = [&](auto&& self,ResourcePtr resource) -> Json {
            Object value;
            assignResourceMember(value, literal("typeId"), [&] { return compat::ngRequireObject(resource)->typeId(); });
            assignResourceMember(value, literal("uuid"), [&] { return static_cast<std::int64_t>(resource->uuid()); });
            assignResourceMember(value, literal("name"), [&] { return resource->name(); });
            assignResourceMember(value, literal("uri"), [&] {
                const auto uuid=resource->uuid();
                return allocateMcpText([&] {
                    return compat::RetainedString("resource://nijigenerate/resources/" + std::to_string(uuid));
                });
            });
            if (auto node = to<Node>(resource)) {
                nicxlive::core::serde::TypedJsonWriter writer; const auto scope = writer.structBegin();
                node->serializePartial(writer, nicxlive::core::nodes::SerializeNodeFlags::Basics, true);
                writer.structEnd(scope); writer.flush();
                assignResourceMember(value, literal("data"), [&] { return parse(writer.str()); });
            }
            if (auto binding = to<Binding>(resource)) {
                std::shared_ptr<Parameter> parameter;
                auto source = resource->source;
                while (source) { parameter = to<Parameter>(source); if (parameter) break; source = source->source; }
                if (!parameter) parameter = bindingOwnerParameter(binding);
                if (parameter) {
                    assignResourceMember(value, literal("uri"), [&] {
                        return core::settings::SettingsString(bindingResourceUri(parameter, binding));
                    });
                    assignResourceMember(value, literal("parameter"), [&] { return parameterJson(parameter); });
                    assignResourceMember(value, literal("target"), [&] { return targetJson(describeTarget(binding)); });
                    assignResourceMember(value, literal("bindingName"), [&] { return binding->getTarget().name; });
                }
            }
            Array children;
            if (tree->children.contains(resource)) {
                const auto found = tree->children.find(resource);
                if (found == tree->children.end()) throw compat::ngStaticError<compat::RangeError>(
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/api/mcp/server.d"), 495);
                const auto entries = found->second;
                for (auto child : entries) children.append(self(self,child));
            }
            value[literal("children")] = children; return value;
        };
        Array items; const auto roots = tree->roots;
        for (auto root : roots) items.append(makeTree(makeTree,root));
        Object value; value[literal("items")] = items;
        value[literal("selectorSyntax")] = core::settings::SettingsString(literal("nijigenerate.core.selector"));
        value[literal("recommendedDiscoveryUri")] = core::settings::SettingsString(
            literal("resource://nijigenerate/resources/find?selector=*"));
        Array guides{
            Json(core::settings::SettingsString(literal("resource://nijigenerate/guides/find"))),
            Json(core::settings::SettingsString(literal("resource://nijigenerate/guides/selectors"))),
            Json(core::settings::SettingsString(literal("resource://nijigenerate/guides/resources")))};
        value[literal("guideUris")] = guides; return Json(value);
    }));
    return providers::mcp::resources::ResourceContents::makeText(literal("application/json"), compat::ngJsonStringify(result));
}
}
