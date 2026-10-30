// URI decoding derives from Phobos std.uri (Walter Bright).
// Copyright The D Language Foundation 2000 - 2009. Boost Software License 1.0.
// Form decoding follows vibe.textfilter.urlencode (Jan Krueger, Soenke Ludwig).
// Copyright 2012-2015 Soenke Ludwig. MIT license.
// HTTP/1 request integration: Copyright 2012-2024 Sönke Ludwig.
// Authors: Sönke Ludwig, Jan Krüger, Ilya Shipunov. MIT license.
// See port/licenses/mcp-http-transport.txt for the retained license texts.
#include "http_transport.hpp"
#include "http_request_loop.hpp"
#include "http_error_body.hpp"
#include "http_auth_time.hpp"
#include "http_route_match.hpp"
#include "http_event_runtime.hpp"
#include "http_source_log.hpp"
#include <nijigenerate/api/mcp/auth.hpp>
#include <nijigenerate/api/mcp/auth_wait.hpp>
#include <nijigenerate/api/mcp/resource_listing.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/default_random.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/uri_exception.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/phobos_unicode_exception.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/compat/phobos_unsigned_conversion.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <common/resource_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <compat/source_mutex.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/autorig/deterministic/detail/sha256.hpp>
#include <boost/context/detail/exception.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <atomic>
#include <functional>
#include <optional>
#include <source_location>
#include <thread>
#include <vector>

namespace nijigenerate::api::mcp::http_transport {
namespace {
using Json = JsonValue;
using Object = core::settings::SettingsJsonObject;
using Array = core::settings::SettingsJsonArray;
using Clock = detail::SourceAuthClock;
using Connection = detail::Connection;
using RequestTask = detail::RequestTask;
using Error = detail::Error;
using detail::SourceLogLevel;
using detail::httpSourceLog;
using Tcp = detail::Tcp;
namespace asio = boost::asio;
namespace http = boost::beast::http;
void prepareHttpClassInfo() {
    static const bool registered = [] {
        using nicxlive::core::d_runtime::ngRegisterObjectClassName;
        ngRegisterObjectClassName(typeid(HttpTransport), compat::RetainedString::fromStaticLiteral(
            "nijigenerate.api.mcp.http_transport.HttpTransport"));
        ngRegisterObjectClassName(typeid(ExtMCPServer), compat::RetainedString::fromStaticLiteral(
            "nijigenerate.api.mcp.http_transport.ExtMCPServer"));
        return true;
    }();
    (void)registered;
}
struct StringHash {
    std::size_t operator()(const String& value) const noexcept {
        return compat::ngPhobosStringHash(value.view());
    }
};
struct StringEqual {
    bool operator()(const String& left, const String& right) const noexcept {
        return left.view() == right.view();
    }
};
template<class V> using StringMap = nicxlive::core::common::ResourceKeyMap<String, V,
    StringHash, StringEqual>;
using Form = StringMap<String>;
template<std::size_t N> String literal(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
template<class F> bool attempt(F&& fn) {
    try { fn(); return true; }
    catch (const std::bad_alloc&) { throw; }
    catch (const std::exception&) { return false; }
}
struct JsonMember {
    String key;
    Json value;
    template<std::size_t N> JsonMember(const char (&name)[N], Json content)
        : key(literal(name)), value(std::move(content)) {}
};
struct JsonLiteralString {
    String value;
    template<std::size_t N> JsonLiteralString(const char (&text)[N]) : value(literal(text)) {}
};
Json object(std::initializer_list<JsonMember> values) {
    Object result;
    for (auto& [key, value] : values) result[key] = value;
    return Json(result);
}
Json strings(std::initializer_list<JsonLiteralString> values) {
    Array result;
    for (const auto& value : values) result.append(Json(value.value));
    return Json(result);
}
template<std::size_t N, std::size_t M = 1>
String get(const Form& form, const char (&key)[N], const char (&fallback)[M] = "") {
    auto found = form.find(literal(key));
    return found == form.end() ? literal(fallback) : found->second;
}
String header(const Connection& c, std::string_view name) {
    return c.request.headers.get(name);
}
std::string stringify(Json value) { return std::string(compat::ngJsonStringify(value).view()); }
String readBodyUTF8(Connection& connection) {
    connection.readBody();
    auto bytes = std::move(connection.body);
    connection.body.clear();
    const auto text = std::string_view(bytes);
    compat::phobos::detail::UnicodeCursor<std::string_view, char> cursor(text);
    while (!cursor.empty()) { (void)cursor.front(); cursor.popFront(); }
    if (text.empty()) return String{};
    // readAllUTF8 owns immutable stream bytes; a later keep-alive request must
    // not replace storage retained by an approval request or a parsed value.
    const bool hasBom = text.starts_with("\xef\xbb\xbf");
    String result(std::move(bytes));
    return hasBom ? result.slice(3, result.size()) : result;
}
void writeDefaultError(Connection& connection, unsigned status,
    std::optional<compat::RetainedString> message = std::nullopt,
    compat::RetainedString debugMessage = {}) {
#ifndef NDEBUG
    if (connection.responseStarted) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Assertion failure"),
        compat::RetainedString::fromStaticLiteral("vibe/http/internal/http1/server.d"), 123);
#endif
    const auto bodyMessage = message ? std::move(*message)
        : detail::sourceHTTPStatusText(static_cast<std::int32_t>(status));
    connection.status = status;
    const auto body = detail::sourceHTTPErrorBody(static_cast<std::int32_t>(status),
        bodyMessage, std::move(debugMessage));
    connection.writeBody(std::string(body.view()));
#ifndef NDEBUG
    if (!connection.responseStarted) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Assertion failure"),
        compat::RetainedString::fromStaticLiteral("vibe/http/internal/http1/server.d"), 138);
#endif
}
compat::RetainedString exceptionDebug(const std::exception& error) {
#ifndef NDEBUG
    if (const auto* original = dynamic_cast<const compat::Throwable*>(&error))
        return detail::sourceHTTPThrowableDebug(*original);
    // Native I/O exceptions keep their actual diagnostic; do not invent a D
    // source type or a captured stack trace that this provider does not supply.
    return detail::sourceSanitizeUTF8(compat::RetainedString(error.what()));
#else
    (void)error;
    return {};
#endif
}
String exceptionMessage(const std::exception& error) {
    if (const auto* original = dynamic_cast<const compat::Throwable*>(&error)) return original->msg;
    return String(error.what());
}
template<std::size_t N>
compat::URIException uriException(const char (&message)[N], std::size_t line) {
    return compat::URIException(compat::RetainedString::fromStaticLiteral(message),
        compat::RetainedString::fromStaticLiteral("std/uri.d"), line);
}
std::string decodeComponent(std::string_view text, bool formPlus = false) {
    // std.uri.URI_Decode(char[], 0), then std.utf.encode. The source decodes
    // percent-encoded scalars separately and treats raw bytes as scalar values.
    auto hex = [](unsigned char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    if (formPlus) {
        const auto invalidPercent = [](std::size_t line) {
            throw compat::ThrowableException(
                compat::RetainedString::fromStaticLiteral("invalid percent encoding"),
                compat::RetainedString::fromStaticLiteral("vibe/textfilter/urlencode.d"), line);
        };
        std::string out;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '%') {
                if (text.size() - i < 3)
                    invalidPercent(169);
                auto high = hex(text[i + 1]), low = hex(text[i + 2]);
                if (high < 0) {
                    // filterURLDecode first calls parse!int(hex, 16). No digit
                    // produces ConvException; a parsed prefix followed by a
                    // non-digit instead fails the subsequent enforce.
                    const auto input = text.substr(i + 1, 2);
                    const auto length = compat::phobos::ngUnexpectedScalarLength(input);
                    const auto shown = input.front() == '\n' ? std::string("\\n")
                        : std::string(input.substr(0, length));
                    throw compat::phobos::ConvException(
                        compat::RetainedString("Unexpected '" + shown + "' when converting from type string to type int"),
                        compat::RetainedString::fromStaticLiteral("std/conv.d"), 2915);
                }
                if (low < 0)
                    invalidPercent(172);
                out += char((high << 4) | low); i += 2;
            } else out += text[i] == '+' ? ' ' : text[i];
        }
        return out;
    }
    auto appendScalar = [](std::string& out, std::uint32_t c) {
        if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) {
            compat::UTFException failure(compat::RetainedString(c > 0x10ffff
                ? "Encoding an invalid code point in UTF-8" : "Encoding a surrogate code point in UTF-8"),
                compat::RetainedString::fromStaticLiteral("std/utf.d"), 2311);
            failure.setSequence(std::span<const std::uint32_t>(&c, 1));
            throw failure;
        }
        if (c < 0x80) out += char(c);
        else if (c < 0x800) { out += char(0xc0 | (c >> 6)); out += char(0x80 | (c & 63)); }
        else if (c < 0x10000) {
            out += char(0xe0 | (c >> 12)); out += char(0x80 | ((c >> 6) & 63)); out += char(0x80 | (c & 63));
        } else {
            out += char(0xf0 | (c >> 18)); out += char(0x80 | ((c >> 12) & 63));
            out += char(0x80 | ((c >> 6) & 63)); out += char(0x80 | (c & 63));
        }
    };
    auto uriByte = [&](std::size_t offset, std::size_t line) {
        auto high = hex(text[offset]), low = hex(text[offset + 1]);
        if (high < 0 || low < 0)
            throw uriException("Expected two hexadecimal digits after '%'", line);
        return (high << 4) | low;
    };
    std::vector<std::uint32_t> scalars;
    scalars.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        std::uint32_t scalar = static_cast<unsigned char>(text[i]);
        if (text[i] == '%') {
            if (text.size() - i < 3) throw uriException("Unexpected end of URI", 226);
            auto byte = uriByte(i + 1, 228); i += 2;
            scalar = byte;
            if (byte & 0x80) {
                unsigned count = 1;
                while (count <= 4 && ((byte << count) & 0x80)) ++count;
                if (count > 4) throw uriException("UTF-32 code point size too large", 242);
                if (count < 2) throw uriException("UTF-32 code point size too small", 246);
                scalar = byte & ((1u << (7 - count)) - 1);
                if (text.size() - i <= 3 * (count - 1))
                    throw uriException("UTF-32 unaligned String", 255);
                for (unsigned j = 1; j < count; ++j) {
                    if (text[i + 1] != '%') throw uriException("Expected: '%'", 260);
                    byte = uriByte(i + 2, 262); i += 3;
                    if ((byte & 0xc0) != 0x80)
                        throw uriException("Incorrect UTF-32 multi-byte sequence", 265);
                    scalar = (scalar << 6) | (byte & 63);
                }
                if (scalar > 0x10ffff) throw uriException("Unknown UTF-32 code point", 270);
            }
        }
        scalars.push_back(scalar);
    }
    // URI_Decode completes before std.utf.encode starts. A later malformed
    // escape therefore wins over an earlier surrogate scalar encoding error.
    std::string out;
    for (auto scalar : scalars) appendScalar(out, scalar);
    return out;
}
Form parseForm(String storage, bool query = false) {
    Form out;
    auto body = storage.view();
    std::vector<std::string_view> tokenPairs;
    if (!query && !body.empty()) {
        // std.array.split builds the complete descriptor array before the
        // token endpoint starts decoding. A trailing separator retains an
        // empty final element, unlike an initially empty input.
        auto remaining = body;
        for (;;) {
            const auto separator = remaining.find('&');
            tokenPairs.push_back(remaining.substr(0, separator));
            if (separator == remaining.npos) break;
            remaining.remove_prefix(separator + 1);
        }
    }
    std::size_t tokenIndex = 0;
    std::size_t offset = 0;
    while (query ? !body.empty() : tokenIndex < tokenPairs.size()) {
        auto amp = query ? body.find_first_of("&;") : body.npos;
        auto pair = query ? body.substr(0, amp) : tokenPairs[tokenIndex++];
#ifndef NDEBUG
        if (!query && pair.empty()) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Attempting to fetch the front of an empty splitter."),
            compat::RetainedString::fromStaticLiteral("std/algorithm/iteration.d"), 5972);
#endif
        auto eq = pair.find('=');
        auto key = pair.substr(0, eq);
        auto value = eq == pair.npos ? std::string_view{} : pair.substr(eq + 1);
        // The D token endpoint uses splitter("=").front, not the entire tail.
        if (!query) value = value.substr(0, value.find('='));
        const auto decode = [&](std::string_view input) -> String {
            // Vibe formDecode returns its input descriptor unchanged when it
            // contains neither percent nor plus; preserve the actual subview.
            if (query && input.find_first_of("%+") == input.npos) {
                if (!input.data()) return literal("");
                const auto first = offset + static_cast<std::size_t>(input.data() - body.data());
                return storage.slice(first, first + input.size());
            }
            if (!query && input.empty()) return String{};
            auto decoded = decodeComponent(input, query);
            // StringSliceAppender keeps a prefix view while every emitted byte
            // matches the original prefix, even when an escape was consumed:
            // formDecode("%25") retains the original first '%' byte.
            if (query && !input.empty() && input.starts_with(decoded)) {
                const auto first = offset + static_cast<std::size_t>(input.data() - body.data());
                return storage.slice(first, first + decoded.size());
            }
            return String(std::move(decoded));
        };
        if (query) {
            auto decodedKey = decode(key), decodedValue = decode(value);
            if (!out.contains(decodedKey)) out[decodedKey] = decodedValue;
        } else {
            // Selected LDC 1.41 extracts AA key calls, then value calls, before
            // insertion. C++ assignment spelling alone evaluates the RHS first.
            auto decodedKey = decode(key);
            auto decodedValue = decode(value);
            out[decodedKey] = decodedValue;
        }
        if (!query) continue;
        if (amp == body.npos) break;
        offset += amp + 1;
        body.remove_prefix(amp + 1);
    }
    return out;
}
String b64url(std::span<const std::uint8_t> data) {
    // Phobos std.base64 array encoding, Copyright Masahiro Nakagawa 2010-.
    // BSL-1.0 permission/disclaimer: port/licenses/mcp-http-transport.txt.
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    const auto blocks = data.size() / 3, remain = data.size() % 3;
    std::string out(blocks * 4 + (remain == 0 ? 0 : remain == 1 ? 2 : 3), static_cast<char>(0xff));
    if (data.empty()) return {};
    std::size_t input = 0, output = 0;
    for (std::size_t block = 0; block < blocks; ++block) {
        const auto bits = std::uint32_t(data[input]) << 16
            | std::uint32_t(data[input + 1]) << 8 | data[input + 2];
        out[output++] = alphabet[bits >> 18];
        out[output++] = alphabet[(bits >> 12) & 63];
        out[output++] = alphabet[(bits >> 6) & 63];
        out[output++] = alphabet[bits & 63];
        input += 3;
    }
    if (remain) {
        const auto bits = std::uint32_t(data[input]) << 16
            | (remain == 2 ? std::uint32_t(data[input + 1]) << 8 : 0);
        out[output++] = alphabet[bits >> 18];
        out[output++] = alphabet[(bits >> 12) & 63];
        if (remain == 2) out[output++] = alphabet[(bits >> 6) & 63];
    }
    return String(std::move(out));
}
String sha256_b64url(String input) {
    std::array<std::uint8_t, 32> result{};
    autorig::deterministic::contracts::detail::Sha256 hash;
    hash.put({reinterpret_cast<const std::uint8_t*>(input.data()), input.size()});
    result = hash.finish();
    return b64url(result);
}
String randToken(String prefix) {
    std::array<std::uint8_t, 32> data{};
    auto& random = compat::ngDefaultRandom();
    for (auto& byte : data) { byte = static_cast<std::uint8_t>(random.front()); random.popFront(); }
    const auto encoded = b64url(data);
    std::string combined(prefix.size() + encoded.size(), static_cast<char>(0xff));
    if (!prefix.empty()) std::copy_n(prefix.data(), prefix.size(), combined.data());
    std::copy_n(encoded.data(), encoded.size(), combined.data() + prefix.size());
    return String(std::move(combined));
}
Object& resourceDescriptionObject(Json& value) {
    if (!value.is_object()) throw core::settings::SettingsJsonError(
        literal("JSONValue is not an object").retainedString(), literal("std/json.d").retainedString(), 305);
    auto& object = value.as_object();
    if (object.isOrdered()) throw core::settings::SettingsJsonError(
        literal("JSONValue object is ordered, cannot return by ref").retainedString(),
        literal("std/json.d").retainedString(), 307);
    return object;
}
// opIndex retains objectNoRef's actual AA, including its temporary ordered conversion.
struct ResourceDescriptionIndex {
    Object object;
    Json* value;
    ResourceDescriptionIndex(Json& owner, String key) {
        if (!owner.is_object()) throw core::settings::SettingsJsonError(
            literal("JSONValue is not an object").retainedString(), literal("std/json.d").retainedString(), 337);
        const auto source = owner.as_object();
        if (source.isOrdered()) {
            const auto pairs = source.orderedMembers();
            for (const auto pair : pairs) object[pair.name] = pair.item;
        } else object = source;
        value = object.if_contains(key.view());
        if (!value) {
            std::string message("Key not found: ");
            message.append(key.view());
            throw core::settings::SettingsJsonError(
                compat::RetainedString(std::move(message)), literal("std/json.d").retainedString(), 806);
        }
    }
};
void stripResourceMetadataDescriptions(Json& response, Json& request) {
    const auto methodKey = literal("method");
    const auto resultKey = literal("result");
    if (!request.is_object() || !resourceDescriptionObject(request).if_contains(methodKey.view())) return;
    if (!ResourceDescriptionIndex(request, methodKey).value->is_string()) return;
    String listKey;
    {
        const auto method = ResourceDescriptionIndex(request, methodKey).value->as_string();
        if (method.view() == "resources/list") listKey = literal("resources");
        else if (method.view() == "resources/templates/list") listKey = literal("resourceTemplates");
        else return;
    }
    if (!response.is_object() || !resourceDescriptionObject(response).if_contains(resultKey.view())) return;
    if (!ResourceDescriptionIndex(response, resultKey).value->is_object()) return;
    {
        auto result = ResourceDescriptionIndex(response, resultKey);
        if (!resourceDescriptionObject(*result.value).if_contains(listKey.view())) return;
    }
    {
        auto result = ResourceDescriptionIndex(response, resultKey);
        if (!ResourceDescriptionIndex(*result.value, listKey).value->is_array()) return;
    }
    auto entries = [&] {
        auto result = ResourceDescriptionIndex(response, resultKey);
        auto list = ResourceDescriptionIndex(*result.value, listKey);
        return list.value->as_array();
    }();
    for (auto& entry : entries) {
        if (!entry.is_object()) continue;
        auto object = resourceDescriptionObject(entry);
        object.erase(literal("description").view());
        entry = Json(object);
    }
    auto result = ResourceDescriptionIndex(response, resultKey);
    const auto assigned = Json(entries);
    // opIndexAssign accepts null or ordered receivers; it is not the strict ref getter.
    if (!result.value->is_object() && !result.value->is_null())
        throw core::settings::SettingsJsonError(
            literal("JSONValue must be object or null").retainedString(), literal("std/json.d").retainedString(), 829);
    Object object;
    if (result.value->is_object()) object = result.value->as_object();
    object[listKey] = assigned;
    *result.value = Json(object);
}
} // namespace

struct HttpTransport::Impl {
    static void* operator new(std::size_t bytes) {
        try { return ::operator new(bytes); }
        catch (const std::bad_alloc&) {
            throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                std::source_location::current());
        }
    }
    static void operator delete(void* memory) noexcept { ::operator delete(memory); }
    struct AuthCode {
        String clientId, redirectUri, codeChallenge, scopeId, resource;
        Clock::time_point expiresAt;
    };
    struct AccessToken {
        String clientId, subject, scopeId, resource;
        Clock::time_point expiresAt;
    };
    HttpTransport& owner;
    MessageHandler messageHandler;
    std::shared_ptr<detail::SourceHTTPEventRuntime> runtime;
    detail::SourceHTTPListener listener;
    asio::io_context& loop() { return runtime->context(); }
    using ClientStream = std::shared_ptr<detail::SourceHTTPOutputStream>;
    compat::Slice<ClientStream> clients;
    // D's Fiber-keyed associative array retains its object keys, including a
    // slot left behind when the message handler throws. A raw address loses
    // that lifetime and may be recycled for another execution identity.
    nicxlive::core::common::PolicyKeyMap<std::shared_ptr<RequestTask>, std::shared_ptr<Json>,
        nicxlive::core::common::ObjectClassKeyPolicy<std::shared_ptr<RequestTask>>> responseSlots;
    nicxlive::core::d_runtime::SourceMutex monitor;
    template<class Body> void withMonitor(Body&& body) {
        monitor.lock_nothrow();
        compat::ngRunWithScopeExit(std::forward<Body>(body), [&] { monitor.unlock_nothrow(); });
    }
    String host;
    std::uint16_t port = 0;
    std::atomic_bool running{false}, shouldExit{false}, loopExited{false};
    bool auth = true;
    String issuer, canonicalResource;
    StringMap<AccessToken> atDb;
    StringMap<AuthCode> codeDb;
    Form rtDb;
    Impl(HttpTransport& value, String address, std::uint16_t number)
        : owner(value), host(address), port(number) {
        issuer = selfBase() + "/auth"; canonicalResource = selfBase() + "/mcp";
    }
    std::string selfBase() const {
        const auto currentHost = host;
        const auto portText = std::to_string(port);
        std::string result;
        result.reserve(sizeof("http://") - 1 + currentHost.size() + 1 + portText.size());
        result.append("http://");
        result.append(currentHost.view());
        result.append(":");
        result.append(portText);
        return result;
    }
    std::string protectedResourceMetadataUrl() const {
        return selfBase() + "/.well-known/oauth-protected-resource/mcp";
    }
    void performStopListening() {
        attempt([&] {
            listener.stopListening();
        });
    }
    void removeClient(std::size_t index, std::size_t sourceLine) {
        // Source re-evaluates both slices after a failed, possibly suspended
        // write. Preserve its checked bounds and fresh concatenation rather
        // than erasing through a potentially invalidated vector iterator.
        const auto left = [&] {
            if (index > clients.size()) throw compat::ngStaticError<compat::ArraySliceError>(
                0, index, clients.size(), compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/api/mcp/http_transport.d"), sourceLine);
            return clients.slice(0, index);
        }();
        const auto right = [&] {
            const auto first = index + 1;
            const auto last = clients.size();
            if (first > last) throw compat::ngStaticError<compat::ArraySliceError>(
                first, last, last, compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/api/mcp/http_transport.d"), sourceLine);
            return clients.slice(first, last);
        }();
        compat::Slice<ClientStream> joined;
        const auto count = left.size() + right.size();
        if (count) {
            joined = compat::Slice<ClientStream>(std::vector<ClientStream>(count));
            std::size_t output = 0;
            for (const auto& stream : left) joined[output++] = stream;
            for (const auto& stream : right) joined[output++] = stream;
        }
        clients = joined;
    }
    void deliver(Json message, RequestTask* origin) {
        withMonitor([&] {
            const auto execution = origin ? origin->shared_from_this() : std::shared_ptr<RequestTask>{};
            if (auto found = responseSlots.find(execution); found != responseSlots.end()) {
                *responseSlots.at(execution) = message;
                responseSlots.erase(execution);
            }
            for (std::size_t i = 0; i < clients.size();) {
                auto client = clients[i];
                if (attempt([&] { client->writeEvent("data: " + stringify(message) + "\n\n"); })) ++i;
                else removeClient(i, 144);
            }
        });
    }
    void performShutdownOnEventLoop() {
        running.store(false);
        withMonitor([&] {
            std::size_t i = 0;
            while (i < clients.size()) {
                auto client = clients[i];
                attempt([&] { client->writeEvent("event: shutdown\n\n"); }); ++i;
            }
            clients.resize(0);
        });
        performStopListening();
        attempt([&] {
            runtime->exitEventLoop();
        });
    }
    bool checkToken(String token, String resource) {
        if (atDb.find(token) == atDb.end()) return false;
        const auto snapshot = atDb.find(token)->second;
        return Clock::now() < snapshot.expiresAt && resource.view().starts_with(snapshot.resource.view());
    }
    void setUnauthorized(Connection& response) {
        response.status = 401;
        response.headers.set(http::field::www_authenticate,
            "Bearer resource_metadata=\"" + protectedResourceMetadataUrl() + "\"");
        response.headers.set(http::field::content_type, "application/json");
        response.writeBody(stringify(object({{"error", Json(literal("invalid_token"))}})));
    }
    bool authorized(Connection& c, bool logEventAuthorization = false) {
        if (!owner.authEnabled()) return true;
        auto value = header(c, "Authorization");
        if (logEventAuthorization) detail::httpLog("[MCP/HTTP] check token=%s", value);
        if (value.view().starts_with("Bearer ") && checkToken(value.slice(7, value.size()), canonicalResource)) return true;
        setUnauthorized(c); return false;
    }
    void handlePost(Connection& c) {
        if (!authorized(c)) return;
        auto body = readBodyUTF8(c);
        Json message;
        try {
            autorig::json::detail::FirstPassReader reader{body.retainedString()};
            // Source parseJSON uses default options, not strictParsing.
            message = reader.parseRoot();
        } catch (const core::settings::SettingsJsonError&) {
            c.headers.set(http::field::content_type, "application/json");
            c.writeBody(stringify(object({{"jsonrpc", Json(literal("2.0"))}, {"error",
                object({{"code", Json(-32700)}, {"message", Json(literal("Parse error"))}})}})));
            return;
        }
        auto response = std::allocate_shared<Json>(nicxlive::core::d_runtime::SourceAllocator<Json>{});
        const auto fiber = RequestTask::current().shared_from_this();
        withMonitor([&] { responseSlots[fiber] = response; });
        owner.handleMessage(message);
        withMonitor([&] { responseSlots.erase(fiber); });
        c.headers.set(http::field::content_type, "application/json");
        const auto idKey = literal("id");
        const auto hasId = [&] {
            // JSONValue's `in` uses objectNoRef, including ordered conversion;
            // it is not the strict mutable-ref object getter.
            if (!message.is_object()) throw core::settings::SettingsJsonError(
                literal("JSONValue is not an object").retainedString(), literal("std/json.d").retainedString(), 337);
            auto object = message.as_object();
            if (object.isOrdered()) {
                Object converted;
                const auto pairs = object.orderedMembers();
                for (const auto pair : pairs) converted[pair.name] = pair.item;
                object = converted;
            }
            return object.if_contains(idKey.view()) != nullptr;
        }();
        // Keep the source's separate second lookup instead of reusing `in`'s
        // temporary AA cell or treating a non-object as a notification.
        if (!hasId || ResourceDescriptionIndex(message, idKey).value->is_null()) {
            c.status = 204; c.writeBody("");
        }
        else {
            resource_listing::rewriteResourcesListResponse(*response, message);
            stripResourceMetadataDescriptions(*response, message); c.writeBody(stringify(*response));
        }
    }
    void handleEvents(std::shared_ptr<Connection> c) {
        if (!authorized(*c, true)) return;
        c->headers.set(http::field::content_type, "text/event-stream");
        c->headers.set(http::field::cache_control, "no-cache");
        c->headers.set(http::field::connection, "keep-alive");
        // Source acquires res.bodyWriter outside the prelude's catch(Exception).
        // A failed header write must not be swallowed or register a new client.
        c->startStream();
        const auto stream = c->bodyWriter();
        attempt([&] { stream->writeEvent(":\n\n"); });
        withMonitor([&] { clients.append(stream); });
        compat::ngRunWithScopeExit([&] {
            while (running.load()) {
                ngSleep(std::chrono::seconds(10));
                if (!running.load()) break;
                if (!attempt([&] { stream->writeEvent(": heartbeat\n\n"); })) break;
            }
        }, [&] {
            withMonitor([&] {
                auto found = std::find(clients.begin(), clients.end(), stream);
                if (found != clients.end()) removeClient(static_cast<std::size_t>(found - clients.begin()), 291);
            });
        });
    }
    void handleProtectedResourceMetadata(Connection& c) {
        detail::httpLog("[MCP/HTTP] Protected Resource Metadata");
        c.headers.set(http::field::content_type, "application/json");
        c.writeBody(stringify(object({{"resource", Json(canonicalResource)},
            {"authorization_servers", Json(Array{Json(issuer)})}, {"bearer_methods_supported", strings({"header"})},
            {"scopes_supported", strings({"mcp:use"})}, {"resource_name", Json(literal("Local MCP Server"))}})));
    }
    void handleASMetadata(Connection& c) {
        detail::httpLog("[MCP/HTTP] AS Metadata");
        c.headers.set(http::field::content_type, "application/json");
        c.writeBody(stringify(object({{"issuer", Json(issuer)},
            {"authorization_endpoint", Json(std::string(issuer.view()) + "/authorize")},
            {"token_endpoint", Json(std::string(issuer.view()) + "/token")}, {"response_types_supported", strings({"code"})},
            {"grant_types_supported", strings({"authorization_code", "refresh_token"})},
            {"code_challenge_methods_supported", strings({"S256"})}, {"token_endpoint_auth_methods_supported", strings({"none"})},
            {"scopes_supported", strings({"mcp:use"})}})));
    }
    void handleAuthorize(Connection& c) {
        detail::httpLog("[MCP/HTTP] Authorization.");
        const auto query = c.url.query.isNull() ? Form{} : parseForm(c.url.query, true);
        auto client = get(query, "client_id", "mcp-local"), redirect = get(query, "redirect_uri");
        auto response = get(query, "response_type"), scope = get(query, "scope", "mcp:use");
        auto state = get(query, "state"), challenge = get(query, "code_challenge");
        auto method = get(query, "code_challenge_method", "S256"), resource = get(query, "resource");
        detail::httpLog("[MCP/HTTP] responseType=%s, redirectUri=%s resource=%s, canonicalResource=%s, chalMethod=%s",
            response, redirect, resource, canonicalResource, method);
        if (response.view() != "code" || redirect.empty() || !canonicalResource.view().starts_with(resource.view())
                || method.view() != "S256") {
            c.status = 400; c.writeBody("invalid_request"); return;
        }
        if (!redirect.view().starts_with("http://127.0.0.1") && !redirect.view().starts_with("http://localhost") &&
                !redirect.view().starts_with("https://")) {
            c.status = 400; c.writeBody("invalid_redirect_uri"); return;
        }
        auto decision = nijigenerate::api::mcp::auth::ngSimpleAuth({randToken(literal("req_")), client, scope,
            resource, state, redirect});
        if (decision.empty()) decision = literal("deny");
        const auto concatenate = [](std::initializer_list<String> parts) {
            std::size_t length = 0;
            for (const auto& part : parts) length += part.size();
            std::string joined(length, static_cast<char>(0xff));
            std::size_t offset = 0;
            for (const auto& part : parts) {
                if (!part.empty()) std::copy_n(part.data(), part.size(), joined.data() + offset);
                offset += part.size();
            }
            return String(std::move(joined));
        };
        String location;
        if (decision.view() == "approve") {
            auto code = randToken(literal("code_"));
            codeDb[code] = {client, redirect, challenge, scope, resource, Clock::now() + std::chrono::hours(365 * 24)};
            location = concatenate({redirect, literal("?code="), code, literal("&state="), state});
        } else location = concatenate({redirect, literal("?error=access_denied&state="), state});
        c.status = 302; c.headers.set(http::field::location, std::string(location.view())); c.writeBody("");
    }
    void handleToken(Connection& c) {
        detail::httpLog("[MCP/HTTP] Token.");
        const auto body = readBodyUTF8(c);
        auto form = parseForm(body); auto grant = get(form, "grant_type");
        c.headers.set(http::field::content_type, "application/json");
        auto fail = [&]<std::size_t N>(const char (&body)[N]) {
            c.status = 400;
            c.writeBody(std::string(body, N - 1));
        };
        if (grant.view() == "authorization_code") {
            auto code = get(form, "code"), redirect = get(form, "redirect_uri"), client = get(form, "client_id", "mcp-local");
            auto verifier = get(form, "code_verifier"), resource = get(form, "resource");
            auto found = codeDb.find(code);
            if (found == codeDb.end()) { fail(R"({"error":"invalid_grant"})"); return; }
            auto ac = codeDb.at(code); codeDb.erase(code);
            if (Clock::now() > ac.expiresAt || ac.redirectUri != redirect || ac.clientId != client) {
                fail(R"({"error":"invalid_grant"})"); return;
            }
            if (sha256_b64url(verifier) != ac.codeChallenge) { fail(R"({"error":"invalid_grant"})"); return; }
            if (resource != ac.resource) { fail(R"({"error":"invalid_target"})"); return; }
            auto at = randToken(literal("at_")), rt = randToken(literal("rt_"));
            atDb[at] = {ac.clientId, literal("local-user"), ac.scopeId, ac.resource,
                Clock::now() + std::chrono::seconds(3600)};
            rtDb[rt] = at;
            const auto result = object({{"access_token", Json(at)}, {"token_type", Json(literal("Bearer"))},
                {"expires_in", Json(3600)}, {"refresh_token", Json(rt)}, {"scope", Json(ac.scopeId)}});
            c.status = 200; c.writeBody(stringify(result)); return;
        }
        if (grant.view() == "refresh_token") {
            auto rt = get(form, "refresh_token"), resource = get(form, "resource");
            auto found = rtDb.find(rt);
            if (found == rtDb.end()) { fail(R"({"error":"invalid_grant"})"); return; }
            const auto oldAt = rtDb.at(rt);
            auto existing = atDb.find(oldAt);
            if (existing == atDb.end()) { fail(R"({"error":"invalid_grant"})"); return; }
            auto old = atDb.at(oldAt);
            if (old.resource != resource) { fail(R"({"error":"invalid_target"})"); return; }
            auto at = randToken(literal("at_"));
            atDb[at] = {old.clientId, old.subject, old.scopeId, old.resource, Clock::now() + std::chrono::seconds(3600)};
            rtDb[rt] = at;
            const auto result = object({{"access_token", Json(at)}, {"token_type", Json(literal("Bearer"))},
                {"expires_in", Json(3600)}, {"refresh_token", Json(rt)}, {"scope", Json(old.scopeId)}});
            c.status = 200; c.writeBody(stringify(result)); return;
        }
        fail(R"({"error":"unsupported_grant_type"})");
    }
    void dispatch(std::shared_ptr<Connection> c) {
        const auto path = c->url.path.view();
        const auto method = c->request.method;
        using Route = detail::SourceLiteralRoute;
        if (detail::sourceHTTPRouteMatches(Route::mcp, method, path)) handlePost(*c);
        else if (detail::sourceHTTPRouteMatches(Route::events, method, path)) handleEvents(c);
        else if (detail::sourceHTTPRouteMatches(Route::protectedResourceMetadata, method, path)) handleProtectedResourceMetadata(*c);
        else if (detail::sourceHTTPRouteMatches(Route::authorizationServerMetadata, method, path)) handleASMetadata(*c);
        else if (detail::sourceHTTPRouteMatches(Route::authorize, method, path)) {
            // The handler logs its entry before evaluating the lazy query.
            handleAuthorize(*c);
        }
        else if (detail::sourceHTTPRouteMatches(Route::token, method, path)) handleToken(*c);
        else {
            compat::RetainedString debugMessage;
            httpSourceLog(SourceLogLevel::diagnostic, "No response written for %s",
                [&] { return std::tuple(c->request.requestURI); });
#ifndef NDEBUG
            debugMessage = detail::sourceHTTPNotFoundDebug(c->request.requestURI.retainedString());
#endif
            writeDefaultError(*c, 404, std::nullopt, std::move(debugMessage));
        }
    }
    static bool waitForData(const std::shared_ptr<Connection>& connection) {
        if (!connection->isOpen()) return false;
        if (connection->input.size()) return true;
        struct InitialRead {
            bool armed = true;
            bool expired = false;
            bool finished = false;
            bool abandoned = false;
            bool timerFired = false;
            std::size_t bytes = 0;
            asio::cancellation_signal cancelRead;
            compat::SharedDelegate<void(Error)> completion;
        };
        const auto state = std::make_shared<InitialRead>();
        const auto timer = std::make_shared<asio::steady_timer>(connection->socket.get_executor());
        timer->expires_after(std::chrono::seconds(10));
        const auto cancelWait = [&] {
            state->armed = false;
            state->completion = {};
            if (!state->finished) {
                state->abandoned = true;
                state->cancelRead.emit(asio::cancellation_type::total);
            }
            if (!state->timerFired) { Error ignored; timer->cancel(ignored); }
        };
        bool available = false;
        connection->withReadSocketReference([&] { compat::ngRunWithScopeExit([&] {
            try {
                RequestTask::current().await([&](auto done) {
                    state->completion = std::move(done);
                    timer->async_wait([state](Error error) {
                        if (!error) state->timerFired = true;
                        if (!error && state->armed) {
                            state->expired = true;
                            state->armed = false;
                            auto complete = std::exchange(state->completion, {});
                            if (complete) complete(asio::error::timed_out);
                        }
                    });
                    connection->readSome(asio::bind_cancellation_slot(state->cancelRead.slot(),
                        [state](Error error, std::size_t bytes) {
                            if (state->abandoned) return;
                            state->armed = false;
                            state->finished = true;
                            state->bytes = bytes;
                            auto complete = std::exchange(state->completion, {});
                            if (complete) complete(error);
                        }));
                });
            } catch (const boost::system::system_error&) {
                // Source waitForData returns false for an empty disconnected/error
                // result; an actual start failure still propagates separately.
                if (!state->finished && !state->expired) throw;
            }
            // Like asyncAwaitAny, a read that fires before the task resumes wins
            // over an already-fired timer. Otherwise scope exit cancels only this
            // read; late Asio cancellation completion cannot call stack locals.
            // TCPConnection.waitForDataEx rejects an invalidated connection
            // context before exposing even a previously completed read.
            available = connection->isOpen() && state->finished && state->bytes != 0;
        }, cancelWait);
        });
        return available;
    }
    static void processConnection(const std::shared_ptr<Connection>& c,
        const std::shared_ptr<detail::SourceHTTPServerContext>& server) {
        compat::ngRunWithScopeExit([&] {
            Error ignored;
            c->socket.set_option(Tcp::no_delay(true), ignored);
            if (!waitForData(c)) {
                httpSourceLog(SourceLogLevel::debug_,
                    "Client didn't send the initial request in a timely manner. Closing connection.");
                return;
            }
            while (true) {
                // handleRequest captures its real UTC instant before inspecting
                // virtual hosts, even when the listener has just become empty.
                const auto requestTime = detail::SourceAuthClock::now();
                auto requestHandler = server->defaultRequestHandler();
                if (!requestHandler) {
                    httpSourceLog(SourceLogLevel::warn,
                        "Didn't find a HTTP listening context for incoming connection. Dropping.");
                    break;
                }
                c->request = {}; c->headers.clear(); c->status = 200; c->streaming = false;
                bool parsed = false;
                bool keepAlive = false;
                try {
                    c->read(requestTime, [&] {
                        requestHandler = server->selectRequestHandler(c->request.headers.get("Host").view(), requestHandler);
                    });
                    parsed = true;
                    keepAlive = c->persistent();
                    c->logRequestEntry();
                    requestHandler(c);
                } catch (const boost::context::detail::forced_unwind&) { throw; }
                catch (const detail::IdleTimeout&) { break; }
                catch (const detail::HTTPStatusException& error) {
                    if (!c->responseStarted)
                        writeDefaultError(*c, static_cast<unsigned>(error.status()), error.msg,
                            error.debugMessage.retainedString());
                    else httpSourceLog(SourceLogLevel::diagnostic,
                        "HTTPStatusException while writing the response: %s", [&] { return std::tuple(error.msg); });
#ifndef NDEBUG
                    httpSourceLog(SourceLogLevel::debug_, "Exception while handling request %s %s: %s", [&] {
                        const auto method = detail::sourceHTTPMethodString(c->request.method);
                        const auto uri = c->request.requestURI.retainedString();
                        const auto diagnostic = exceptionDebug(error);
                        return std::tuple(method, uri, diagnostic);
                    });
#endif
                    if (!parsed || c->responseStarted || detail::sourceJustifiesConnectionClose(error.status()))
                        keepAlive = false;
                } catch (const std::bad_alloc&) { throw; }
                catch (const std::exception& error) {
                    // Vibe catches Exception here. Canonical Error
                    // carriers must escape rather than become HTTP 500.
                    const auto status = parsed ? 500u : 400u;
                    const auto debugMessage = exceptionDebug(error);
                    if (!c->responseStarted && c->connected())
                        writeDefaultError(*c, status, std::nullopt, debugMessage);
                    else httpSourceLog(SourceLogLevel::diagnostic, "Error while writing the response: %s",
                        [&] { return std::tuple(exceptionMessage(error)); });
#ifndef NDEBUG
                    httpSourceLog(SourceLogLevel::debug_, "Exception while handling request %s %s: %s", [&] {
                        const auto method = detail::sourceHTTPMethodString(c->request.method);
                        const auto uri = c->request.requestURI.retainedString();
                        const auto diagnostic = exceptionDebug(error);
                        return std::tuple(method, uri, diagnostic);
                    });
#endif
                    if (!parsed || c->responseStarted) keepAlive = false;
                }
                // Source drains a persistent request before finalizing its stream;
                // finalization failures escape to the outer connection handler.
                if (keepAlive && c->connected()) c->drainBody();
                if (c->streaming) c->finishStream();
                httpSourceLog(SourceLogLevel::trace, "return %s", [&] { return std::tuple(keepAlive); });
                if (!keepAlive) {
                    httpSourceLog(SourceLogLevel::trace, "No keep-alive - disconnecting client.");
                    break;
                }
                httpSourceLog(SourceLogLevel::trace, "Waiting for next request...");
                if (!waitForData(c)) {
                    if (!c->connected()) httpSourceLog(SourceLogLevel::trace, "Client disconnected.");
                    else httpSourceLog(SourceLogLevel::debug_, "Keep-alive connection timed out!");
                    break;
                }
            }
            httpSourceLog(SourceLogLevel::trace, "Done handling connection.");
        }, [&] { c->close(); });
    }
    static void accept(Tcp::socket socket, std::shared_ptr<detail::SourceHTTPServerContext> server) {
        auto c = std::make_shared<Connection>(std::move(socket));
        auto& eventLoop = static_cast<asio::io_context&>(c->socket.get_executor().context());
        c->task = detail::sourceTaskSchedulerFor(eventLoop).createTask();
        c->task->onTermination([c] { c->close(); });
        try {
            c->task->start([c, server = std::move(server)] {
                compat::ngRunWithScopeExit([&] {
                Error endpointError;
                const auto peer = c->socket.remote_endpoint(endpointError);
                if (endpointError) httpSourceLog(SourceLogLevel::warn,
                    "Failed to get remote address for TCP connection");
                try { processConnection(c, server); }
                catch (const std::bad_alloc&) { throw; }
                catch (const std::exception& error) {
                    httpSourceLog(SourceLogLevel::error, "HTTP connection handler has thrown at the peer %s: %s", [&] {
                        const auto peerText = endpointError ? literal("<UNSPEC>") : detail::sourceHTTPPeerAddress(peer);
                        return std::tuple(peerText, exceptionMessage(error));
                    });
#ifndef NDEBUG
                    httpSourceLog(SourceLogLevel::debug_, "Full error: %s",
                        [&] { return std::tuple(exceptionDebug(error)); });
#endif
                    try { c->close(); }
                    catch (const std::bad_alloc&) { throw; }
                    catch (const std::exception& closeError) {
                        httpSourceLog(SourceLogLevel::error, "Failed to close connection: %s",
                            [&] { return std::tuple(exceptionMessage(closeError)); });
                    }
                }
                }, [&] { c->task.reset(); });
            });
        } catch (...) { c->task.reset(); throw; }
    }
    struct PeriodicShutdown {
        bool running = false;
        bool pending = false;
        bool stopped = false;
    };
    void shutdownTick(std::shared_ptr<asio::steady_timer> timer, bool first = true,
        std::shared_ptr<PeriodicShutdown> state = {}) {
        if (first) {
            state = std::make_shared<PeriodicShutdown>();
            timer->expires_after(std::chrono::milliseconds(100));
        }
        timer->async_wait(runtime->scheduler().trackNativeCompletion(
            [this, timer, state, lifetime = owner.shared_from_this()](Error error) {
            if (error || state->stopped) return;
            detail::SourceYieldLock callbackLock(runtime->scheduler(), literal("vibe/core/core.d"), 1663);
            auto next = timer->expiry();
            const auto firedAt = std::chrono::steady_clock::now();
            do next += std::chrono::milliseconds(100);
            while (next <= firedAt);
            timer->expires_at(next);
            shutdownTick(timer, false, state);
            if (state->running) { state->pending = true; return; }
            state->running = true;
            const auto task = runtime->scheduler().createTask();
            task->start([this, timer, state, lifetime] {
                (void)lifetime;
                compat::ngRunWithScopeExit([&] {
                    do {
                        state->pending = false;
                        if (shouldExit.load()) {
                            state->stopped = true;
                            timer->cancel();
                            performShutdownOnEventLoop();
                        }
                    } while (state->pending && !state->stopped);
                }, [&] { state->running = false; });
            });
        }));
    }
};

HttpTransport::HttpTransport(String host, std::uint16_t port) : impl_(std::make_unique<Impl>(*this, host, port)) {}
HttpTransport::~HttpTransport() = default;
// Object defaults, Digital Mars 2000-2011, BSL-1.0;
// complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void HttpTransport::prepareSourceClassInfo() { prepareHttpClassInfo(); }
compat::RetainedString HttpTransport::toString() {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t HttpTransport::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool HttpTransport::opEquals(std::shared_ptr<HttpTransport> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int HttpTransport::opCmp(std::shared_ptr<HttpTransport>) {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    std::string message = "need opCmp for class ";
    message += nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)).view();
    throw compat::ThrowableException(compat::RetainedString(std::move(message)),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngHttpTransportObjectEquals(std::shared_ptr<HttpTransport> left, std::shared_ptr<HttpTransport> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right), &prepareHttpClassInfo);
}
bool HttpTransport::authEnabled() { return impl_->auth; }
void HttpTransport::authEnabled(bool value) { impl_->auth = value; }
void HttpTransport::setMessageHandler(MessageHandler handler) {
    const auto keepAlive = shared_from_this();
    impl_->messageHandler = std::move(handler);
}
void HttpTransport::handleMessage(JsonValue message) {
    const auto keepAlive = shared_from_this();
    if (impl_->messageHandler) impl_->messageHandler(message);
}
void HttpTransport::sendMessage(JsonValue message) {
    const auto keepAlive = shared_from_this();
    impl_->deliver(message, RequestTask::currentOrNull());
}
void HttpTransport::run() {
    const auto keepAlive = shared_from_this();
    const auto cleanup = [&] {
        impl_->running.store(false); impl_->loopExited.store(true); impl_->performStopListening();
    };
    compat::ngRunWithScopeExit([&] {
        detail::httpLog("[MCP/HTTP] run(): binding on %s:%s", impl_->host, impl_->port);
        impl_->runtime = detail::SourceHTTPEventRuntime::current();
        impl_->listener = impl_->runtime->listen(impl_->host, impl_->port,
            [receiver = this, lifetime = keepAlive](std::shared_ptr<Connection> connection) {
                (void)lifetime;
                receiver->impl_->dispatch(std::move(connection));
            }, &Impl::accept,
            [](detail::SourceHTTPListenEvent event, std::string_view host, std::uint16_t port) {
                if (event == detail::SourceHTTPListenEvent::listening) {
                    if (host.find(':') != std::string_view::npos) {
                        std::string urlHost;
                        urlHost.reserve(host.size() + 2);
                        urlHost += '['; urlHost += host; urlHost += ']';
                        httpSourceLog(SourceLogLevel::info, "Listening for requests on http://%s:%s/",
                            [&] { return std::tuple(String(std::move(urlHost)), port); });
                    } else httpSourceLog(SourceLogLevel::info, "Listening for requests on http://%s:%s/",
                        [&] { return std::tuple(host, port); });
                } else if (event == detail::SourceHTTPListenEvent::stopped)
                    httpSourceLog(SourceLogLevel::info, "Stopped to listen for HTTP requests on %s:%s",
                        [&] { return std::tuple(host, port); });
                else httpSourceLog(SourceLogLevel::warn, "Failed to listen on %s:%s",
                    [&] { return std::tuple(host, port); });
            });
        impl_->running.store(true);
        if (impl_->shouldExit.load()) {
            // Preserve the source call before runEventLoop, including its actual
            // exitEventLoop assertion boundary when no loop has been entered.
            impl_->performShutdownOnEventLoop();
            return;
        }
        const auto shutdownTimer = std::make_shared<asio::steady_timer>(impl_->loop());
        impl_->shutdownTick(shutdownTimer);
        compat::ngRunWithScopeExit([&] {
            detail::httpLog("[MCP/HTTP] run(): listening on %s:%s", impl_->host, impl_->port);
            impl_->runtime->runEventLoop();
            detail::httpLog("[MCP/HTTP] run(): event loop exited");
        }, [&] { shutdownTimer->cancel(); });
    }, cleanup);
}
void HttpTransport::close() {
    const auto keepAlive = shared_from_this();
    detail::httpLog("[MCP/HTTP] close(): request transport shutdown");
    impl_->shouldExit.store(true); impl_->running.store(false);
}
bool HttpTransport::hasExited() const noexcept { return impl_->loopExited.load(); }
std::shared_ptr<HttpTransport> createHttpTransport(String host, std::uint16_t port) {
    return std::allocate_shared<HttpTransport>(
        nicxlive::core::d_runtime::SourceAllocator<HttpTransport>{}, host, port);
}
ExtMCPServer::ExtMCPServer(std::shared_ptr<Transport> transport, String name, String version)
    : detail::ExtMCPServerTransport(transport), MCPServer(transport, name, version) {}
void ExtMCPServer::prepareSourceClassInfo() { prepareHttpClassInfo(); }
compat::RetainedString ExtMCPServer::toString() {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t ExtMCPServer::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool ExtMCPServer::opEquals(std::shared_ptr<ExtMCPServer> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int ExtMCPServer::opCmp(std::shared_ptr<ExtMCPServer>) {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    std::string message = "need opCmp for class ";
    message += nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)).view();
    throw compat::ThrowableException(compat::RetainedString(std::move(message)),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngExtMCPServerObjectEquals(std::shared_ptr<ExtMCPServer> left, std::shared_ptr<ExtMCPServer> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right), &prepareHttpClassInfo);
}
void ExtMCPServer::stop() {
    const auto keepAlive = shared_from_this();
    const auto receiver = _transport;
    if (receiver) receiver->close();
}
bool ExtMCPServer::transportExited() const {
    const auto keepAlive = shared_from_this();
    auto http = std::dynamic_pointer_cast<HttpTransport>(_transport);
    return !http || http->hasExited();
}
} // namespace nijigenerate::api::mcp::http_transport
