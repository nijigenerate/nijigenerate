// Port of the independent ACP_ECHO_TOOL source; process execution is opt-in.
// Phobos std.string/std.conv/std.stdio adaptations: port/licenses/phobos-videoexport.txt.
// Current std.string strip/chomp: Copyright The D Language Foundation 2007-.
// Boost Software License 1.0: port/licenses/phobos-stdio.txt.
#if defined(ACP_ECHO_TOOL)
#include "echo_agent.hpp"
#include "detail/echo_response.hpp"
#include "nijigenerate/compat/json_stringify.hpp"
#include "nijigenerate/compat/phobos_conv_exception.hpp"
#include "nijigenerate/compat/phobos_stdio.hpp"
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include "nijigenerate/core/i18n_case.hpp"
#include "nijigenerate/autorig/detail/json_first_pass_reader.hpp"
#include <limits>
#include <string>
#include <utility>
#include <span>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::api::acp::echo_agent {
namespace {
template<class Operation> auto allocateEchoStorage(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
std::string chomp(std::string line) {
    const auto n = line.size();
    if (!n) return line;
    const auto last = static_cast<unsigned char>(line.back());
    if (last == '\n') line.resize(n > 1 && line[n - 2] == '\r' ? n - 2 : n - 1);
    else if (last == '\r' || last == '\v' || last == '\f') line.resize(n - 1);
    else if ((last == 0xa8 || last == 0xa9) && n > 2 &&
        static_cast<unsigned char>(line[n - 2]) == 0x80 && static_cast<unsigned char>(line[n - 3]) == 0xe2)
        line.resize(n - 3);
    else if (last == 0x85 && n > 1 && static_cast<unsigned char>(line[n - 2]) == 0xc2) line.resize(n - 2);
    return line;
}
bool white(std::uint32_t c) {
    return (c >= 9 && c <= 13) || c == 32 || c == 0x85 || c == 0xa0 || c == 0x1680 ||
        (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f || c == 0x3000;
}
std::string_view strip(std::string_view text) {
    core::i18n::detail::LocaleCaseCursor cursor{text};
    std::size_t first = text.size();
    while (!cursor.empty()) {
        const auto start = cursor.offset;
        if (!white(cursor.take())) {
            first = start;
            break;
        }
    }
    text.remove_prefix(first);
    // stripRight deliberately decodes only the terminal byte patterns. It
    // does not apply stripLeft's replacement/UTF validity rules to the suffix.
    while (!text.empty()) {
        const auto last = static_cast<unsigned char>(text.back());
        if (last <= 0x7f) {
            if (!white(last)) break;
            text.remove_suffix(1);
            continue;
        }
        if (text.size() < 2 || (last & 0xc0) != 0x80) break;
        const std::uint32_t low = last & 0x3f;
        const auto second = static_cast<unsigned char>(text[text.size() - 2]);
        if ((second & 0xe0) == 0xc0) {
            if (!white(low + (std::uint32_t(second & 0x1f) << 6))) break;
            text.remove_suffix(2);
            continue;
        }
        if (text.size() < 3 || (second & 0xc0) != 0x80) break;
        const auto third = static_cast<unsigned char>(text[text.size() - 3]);
        if ((third & 0xf0) != 0xe0 || !white(low + (std::uint32_t(second & 0x3f) << 6)
                + (std::uint32_t(third & 0x0f) << 12))) break;
        text.remove_suffix(3);
    }
    return text;
}
std::size_t parseLength(std::string_view text) {
    if (text.empty()) compat::phobos::ngThrowUnsignedConversion(text);
    std::size_t value = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto digit = static_cast<unsigned char>(text[i]) - static_cast<unsigned>('0');
        if (digit > 9) compat::phobos::ngThrowUnsignedConversion(text.substr(i));
        if (value > std::numeric_limits<std::size_t>::max() / 10 ||
            (value == std::numeric_limits<std::size_t>::max() / 10 && digit > 5))
            throw compat::PhobosConvOverflowException();
        value = value * 10 + digit;
    }
    return value;
}
std::string readFramed() {
    auto& input = providers::mcp::stdio::standardInput();
    auto line = input.readln();
    if (line.empty()) return line;
    line = chomp(std::move(line));
    if (!line.starts_with("Content-Length")) return line;
    // std.array.split materializes every field before the conversion of [1].
    // Each descriptor still views the live line, including empty final fields.
    compat::Slice<std::string_view> parts;
    const std::string_view header(line);
    std::size_t start = 0;
    for (;;) {
        const auto colon = header.find(':', start);
        parts.append(header.substr(start, colon == std::string_view::npos ? colon : colon - start));
        if (colon == std::string_view::npos) break;
        start = colon + 1;
    }
    const std::size_t length = parts.size() > 1 ? parseLength(strip(parts[1])) : 0;
    (void)input.readln();
    // D new char[] initializes each element to char.init (0xff). The original
    // intentionally ignores rawRead's shorter returned slice at end-of-file.
    auto buffer = allocateEchoStorage([&] { return std::string(length, static_cast<char>(0xff)); });
    (void)input.rawRead(std::span<char>(buffer.data(), buffer.size()));
    return buffer;
}
void writeFramed(std::string_view payload) {
    auto& output = providers::mcp::stdio::standardOutput();
    output.writeln("Content-Length: ", payload.size());
    output.writeln({});
    output.writeln(payload);
    output.flush();
}
}
int ngEchoAgentMain() {
    while (!providers::mcp::stdio::standardInput().eof()) {
        auto message = readFramed();
        if (message.empty()) continue;
        core::settings::SettingsJsonValue json;
        try {
            const auto retained = allocateEchoStorage([&] {
                return compat::RetainedString(std::move(message));
            });
            autorig::json::detail::FirstPassReader reader{
                compat::RetainedStringSlice(retained)};
            json = reader.parseRoot();
        } catch (const compat::ThrowableException&) { continue; }
        const auto response = detail::handle(std::move(json));
        if (!response.is_null()) writeFramed(compat::ngJsonStringify(response).view());
    }
    return 0;
}
}
int main() {
    struct ErrorThreadCleanup {
        ~ErrorThreadCleanup() noexcept {
            ::nicxlive::core::d_runtime::Throwable::ngReleaseStaticErrorThreadStorage();
        }
    };
    // Register canonical Error TLS teardown without collector/mutator attachment.
    thread_local ErrorThreadCleanup errorCleanup;
    (void)errorCleanup;
    return nijigenerate::api::acp::echo_agent::ngEchoAgentMain();
}
#endif
