#include "commands.hpp"
#include "http_transport.hpp"
#include "jsonrpc.hpp"
#include "detail/utf8_replacement.hpp"
#include "detail/phobos_string_strip.hpp"
#include "detail/stdio_writer.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/autorig/deterministic/contracts.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace njc::commands {
namespace {
using Value = nijigenerate::core::settings::SettingsJsonValue;
using Text = config::Text;
using Args = nijigenerate::compat::Slice<Text>;
using http_transport::HttpTransport;
namespace contract = nijigenerate::autorig::deterministic::contracts;
template<class Operation> auto allocateMemory(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
            std::source_location::current());
    } catch (const std::length_error&) {
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
            std::source_location::current());
    }
}
template<std::size_t N> void enforce(bool condition, const char (&message)[N], std::size_t sourceLine) {
    if (!condition) {
        const auto copiedMessage = allocateMemory([&] {
            return nijigenerate::compat::RetainedString(std::string_view(message, N - 1));
        });
        throw CommandException(copiedMessage,
            nijigenerate::compat::RetainedString::fromStaticLiteral("source/njc/commands.d"), sourceLine);
    }
}
auto& object(Value& value) {
    auto& result = value.as_object();
    if (result.isOrdered()) throw nijigenerate::core::settings::SettingsJsonError::fromStaticLiteral(
        "JSONValue object is ordered, cannot return by ref");
    return result;
}
Value get(Value value, std::string_view key) {
    const auto& members=value.as_object();
    if (const auto* found=members.if_contains(key)) return *found;
    auto message = allocateMemory([&] {
        return nijigenerate::compat::RetainedString("Key not found: " + std::string(key));
    });
    throw nijigenerate::core::settings::SettingsJsonError(std::move(message));
}
auto array(Value value, std::string_view key) { return get(value, key).as_array(); }
Text string(Value value, std::string_view key) { return get(value, key).as_string(); }
std::string serialize(Value value) {
    if (value.is_object()) {
        auto result = allocateMemory([] { return std::string("{"); });
        bool first = true;
        auto emit = [&](std::string_view name, Value entry) {
            if (!first) allocateMemory([&] { result += ','; });
            first = false;
            allocateMemory([&] { contract::detail::appendJsonString(result, name); });
            allocateMemory([&] { result += ':'; });
            const auto serialized = serialize(entry);
            allocateMemory([&] { result += serialized; });
        };
        const auto& members = value.as_object();
        if (members.isOrdered()) {
            for (const auto& member : members) emit(member.name.view(), member.item);
        } else {
            std::vector<nijigenerate::core::settings::SettingsString> keys;
            for (const auto& member : members) allocateMemory([&] { keys.push_back(member.name); });
            std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.view() < b.view(); });
            for (const auto& key : keys) emit(key.view(), members.at(key.view()));
        }
        return allocateMemory([&] { return result + '}'; });
    }
    if (value.is_array()) {
        auto result = allocateMemory([] { return std::string("["); });
        bool first = true;
        for (auto entry : value.as_array()) {
            if (!first) allocateMemory([&] { result += ','; });
            first = false;
            const auto serialized = serialize(entry);
            allocateMemory([&] { result += serialized; });
        }
        return allocateMemory([&] { return result + ']'; });
    }
    if (value.is_double()) {
        if (std::isnan(value.as_double()))
            throw nijigenerate::core::settings::SettingsJsonError::fromStaticLiteral(
                "Cannot encode NaN. Consider passing the specialFloatLiterals flag.");
        if (std::isinf(value.as_double()))
            throw nijigenerate::core::settings::SettingsJsonError::fromStaticLiteral(
                "Cannot encode Infinity. Consider passing the specialFloatLiterals flag.");
    }
    const auto serialized = allocateMemory([&] { return contract::ngRigCanonical(value); });
    return allocateMemory([&] { return std::string(serialized.view()); });
}
std::string parseJsonOption(Args args, std::string_view defaultJson) {
    if (!args.size()) return allocateMemory([&] { return std::string(defaultJson); });
    enforce(args.size() == 2 && args[0].view() == "--json", "expected --json <json>", 100);
    nijigenerate::autorig::json::detail::FirstPassReader reader(args[1]);
    return serialize(reader.parseRoot());
}
Value call(HttpTransport& transport, std::int64_t& nextId, std::string_view method,
    std::string_view paramsJson = "{}") {
    const auto id = nextId;
    nextId = std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(nextId) + 1u);
    auto response = transport.postJson(rpcRequest(id, method, paramsJson));
    enforce(response.is_object(), "MCP response is not a JSON object", 86);
    if (object(response).if_contains("error")) {
        const auto serialized = serialize(get(response, "error"));
        auto message = allocateMemory([&] {
            return nijigenerate::compat::RetainedString("MCP error: " + serialized);
        });
        throw CommandException(std::move(message),
            nijigenerate::compat::RetainedString::fromStaticLiteral("source/njc/commands.d"), 88);
    }
    return response;
}
void initialize(HttpTransport& transport, std::int64_t& nextId) {
    auto response = call(transport, nextId, "initialize",
        R"({"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"njc","version":"0.1.0"}})");
    enforce(object(response).if_contains("result") != nullptr, "initialize response has no result", 81);
}
Value readUri(HttpTransport& transport, std::int64_t& nextId, std::string_view uri) {
    const auto escaped = escapeJson(uri);
    const auto params = allocateMemory([&] { return "{\"uri\":\"" + std::string(escaped.view()) + "\"}"; });
    return call(transport, nextId, "resources/read", params);
}
std::string encodeComponent(std::string_view value) {
    // Actual Phobos std.uri -> toUTF32 -> byUTF!dchar uses replacement decoding.
    // Copyright The D Language Foundation 2000-2009. BSL-1.0: detail/phobos.LICENSE.
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (std::size_t position = 0; position < value.size();) {
        const auto scalar = njc::detail::ngNextReplacementScalar(value, position);
        if ((scalar >= U'a' && scalar <= U'z') || (scalar >= U'A' && scalar <= U'Z') ||
            (scalar >= U'0' && scalar <= U'9') ||
            (scalar < 128 && std::string_view("-_.!~*'()").find(static_cast<char>(scalar)) != std::string_view::npos)) {
            allocateMemory([&] { result += static_cast<char>(scalar); });
            continue;
        }
        std::array<unsigned char, 4> bytes{};
        std::size_t count;
        if (scalar < 0x80) { bytes[0] = static_cast<unsigned char>(scalar); count = 1; }
        else if (scalar < 0x800) {
            bytes[0] = static_cast<unsigned char>(0xc0 | (scalar >> 6));
            bytes[1] = static_cast<unsigned char>(0x80 | (scalar & 0x3f)); count = 2;
        } else if (scalar < 0x10000) {
            bytes[0] = static_cast<unsigned char>(0xe0 | (scalar >> 12));
            bytes[1] = static_cast<unsigned char>(0x80 | ((scalar >> 6) & 0x3f));
            bytes[2] = static_cast<unsigned char>(0x80 | (scalar & 0x3f)); count = 3;
        } else {
            bytes[0] = static_cast<unsigned char>(0xf0 | (scalar >> 18));
            bytes[1] = static_cast<unsigned char>(0x80 | ((scalar >> 12) & 0x3f));
            bytes[2] = static_cast<unsigned char>(0x80 | ((scalar >> 6) & 0x3f));
            bytes[3] = static_cast<unsigned char>(0x80 | (scalar & 0x3f)); count = 4;
        }
        for (std::size_t i = 0; i < count; ++i) {
            const auto byte = bytes[i];
            allocateMemory([&] {
                result += '%'; result += hex[byte >> 4]; result += hex[byte & 15];
            });
        }
    }
    return result;
}
Value dispatch(HttpTransport& transport, std::int64_t& nextId, Args args) {
    enforce(args.size() > 0, "missing command", 23);
    const auto command = args[0].view();
    if (command == "tools") {
        enforce(args.size() >= 2, "tools requires list or call", 26);
        if (args[1].view() == "list") {
            enforce(args.size() == 2, "tools list takes no arguments", 28);
            return call(transport, nextId, "tools/list");
        }
        if (args[1].view() == "call") {
            enforce(args.size() >= 3, "tools call requires a tool name", 32);
            const auto name = escapeJson(args[2].view());
            const auto arguments = parseJsonOption(args.slice(3, args.size()), "{}");
            const auto params = allocateMemory([&] {
                return "{\"name\":\"" + std::string(name.view()) + "\",\"arguments\":" + arguments + '}';
            });
            return call(transport, nextId, "tools/call", params);
        }
    }
    if (command == "resources") {
        enforce(args.size() >= 2, "resources requires list, templates, or read", 39);
        if (args[1].view() == "list") {
            enforce(args.size() == 2, "resources list takes no arguments", 41);
            return call(transport, nextId, "resources/list");
        }
        if (args[1].view() == "templates") {
            enforce(args.size() == 2, "resources templates takes no arguments", 45);
            return call(transport, nextId, "resources/templates/list");
        }
        if (args[1].view() == "read") {
            enforce(args.size() == 3, "resources read requires a URI", 49);
            return readUri(transport, nextId, args[2].view());
        }
    }
    if (command == "find") {
        enforce(args.size() == 2, "find requires one selector", 55);
        const auto encoded = encodeComponent(args[1].view());
        const auto uri = allocateMemory([&] { return "resource://nijigenerate/resources/find?selector=" + encoded; });
        return readUri(transport, nextId, uri);
    }
    if (command == "read") {
        enforce(args.size() == 2, "read requires a resource UUID or URI", 61);
        auto uri = args[1];
        if (!uri.view().starts_with("resource://")) {
            const auto encoded = encodeComponent(uri.view());
            uri = allocateMemory([&] {
                return Text(nijigenerate::compat::RetainedString("resource://nijigenerate/resources/" + encoded));
            });
        }
        return readUri(transport, nextId, uri.view());
    }
    if (command == "rpc") {
        enforce(args.size() >= 2, "rpc requires a method", 70);
        return call(transport, nextId, args[1].view(), parseJsonOption(args.slice(2, args.size()), "{}"));
    }
    auto message = allocateMemory([&] {
        return nijigenerate::compat::RetainedString("unknown command: " + std::string(command));
    });
    throw CommandException(std::move(message),
        nijigenerate::compat::RetainedString::fromStaticLiteral("source/njc/commands.d"), 75);
}
Text firstLine(Text input) {
    auto bytes = input.view();
    const auto newline = bytes.find_first_of("\r\n");
    if (newline != std::string_view::npos) bytes = bytes.substr(0, newline);
    const auto [first, last] = detail::ngStripStringBounds(bytes);
    return input.slice(first, last);
}
void printResourceList(Value response) {
    const auto resources = array(get(response, "result"), "resources");
    const auto count = allocateMemory([&] { return std::to_string(resources.size()); });
    detail::ngWriteStdioLine(stdout, {"resources: ", count});
    for (auto resource : resources) {
        const auto name = string(resource, "name"), uri = string(resource, "uri");
        const auto description = object(resource).if_contains("description") ? string(resource, "description") :
            Text(nijigenerate::compat::RetainedString::emptyLiteral());
        detail::ngWriteStdioLine(stdout, {"- ", name.view()});
        detail::ngWriteStdioLine(stdout, {"  uri: ", uri.view()});
        if (!description.empty()) detail::ngWriteStdioLine(stdout, {"  ", firstLine(description).view()});
    }
}
void printToolList(Value response) {
    const auto tools = array(get(response, "result"), "tools");
    const auto count = allocateMemory([&] { return std::to_string(tools.size()); });
    detail::ngWriteStdioLine(stdout, {"tools: ", count});
    for (auto tool : tools) {
        const auto name = string(tool, "name");
        const auto description = object(tool).if_contains("description") ? string(tool, "description") :
            Text(nijigenerate::compat::RetainedString::emptyLiteral());
        detail::ngWriteStdioLine(stdout, {"- ", name.view()});
        if (!description.empty()) detail::ngWriteStdioLine(stdout, {"  ", firstLine(description).view()});
    }
}
void printTemplateList(Value response) {
    const auto templates = array(get(response, "result"), "resourceTemplates");
    const auto count = allocateMemory([&] { return std::to_string(templates.size()); });
    detail::ngWriteStdioLine(stdout, {"resource templates: ", count});
    for (auto entry : templates) {
        const auto name = string(entry, "name");
        detail::ngWriteStdioLine(stdout, {"- ", name.view()});
        const auto uriTemplate = string(entry, "uriTemplate");
        detail::ngWriteStdioLine(stdout, {"  uriTemplate: ", uriTemplate.view()});
        if (object(entry).if_contains("description")) {
            const auto description = string(entry, "description");
            const auto line = firstLine(description);
            detail::ngWriteStdioLine(stdout, {"  ", line.view()});
        }
    }
}
void printResponse(const config::Options& options, Value response) {
    if (!options.summary || options.rawJson || !options.command.size()) {
        detail::ngWriteStdioLine(stdout, {serialize(response)});
        return;
    }
    if (options.command.size() >= 2) {
        const auto command = options.command[0].view(), subcommand = options.command[1].view();
        if (command == "resources" && subcommand == "list") { printResourceList(response); return; }
        if (command == "tools" && subcommand == "list") { printToolList(response); return; }
        if (command == "resources" && subcommand == "templates") { printTemplateList(response); return; }
    }
    detail::ngWriteStdioLine(stdout, {serialize(response)});
}
} // namespace
int runCommand(config::Options options) {
    auto transport = std::allocate_shared<HttpTransport>(
        nicxlive::core::d_runtime::SourceAllocator<HttpTransport>{}, options.endpoint);
    std::int64_t nextId = 1;
    initialize(*transport, nextId);
    printResponse(options, dispatch(*transport, nextId, options.command));
    return 0;
}
} // namespace njc::commands
