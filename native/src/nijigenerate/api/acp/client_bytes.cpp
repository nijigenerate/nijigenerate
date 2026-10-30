#include "client_bytes.hpp"
#include "detail/client_storage.hpp"
#include <string>

namespace nijigenerate::api::acp::client::detail {
namespace {
void appendControl(std::string& output, std::uint8_t value) {
    switch (value) {
    case '\b': output += "\\b"; break;
    case '\t': output += "\\t"; break;
    case '\n': output += "\\n"; break;
    case '\f': output += "\\f"; break;
    case '\r': output += "\\r"; break;
    default:
        constexpr char hex[] = "0123456789abcdef";
        output += "\\u00";
        output += hex[value >> 4];
        output += hex[value & 15];
    }
}

compat::RetainedStringSlice finish(std::string text) {
    if (text.empty()) return {};
    return compat::RetainedString(std::move(text));
}
} // namespace

std::ptrdiff_t findByteIndex(compat::ConstSlice<std::uint8_t> bytes, std::uint8_t value) {
    for (std::size_t i = 0; i < bytes.size(); ++i)
        if (bytes[i] == value) return static_cast<std::ptrdiff_t>(i);
    return -1;
}

compat::Slice<std::uint8_t> truncateAtNull(compat::Slice<std::uint8_t> bytes) {
    for (std::size_t i = 0; i < bytes.size(); ++i)
        if (bytes[i] == 0) return bytes.slice(0, i);
    return bytes;
}

compat::RetainedStringSlice sanitizeJsonPayload(compat::RetainedStringSlice payload) try {
    std::string output;
    bool inString = false;
    bool escaped = false;
    const auto bytes = payload.view();
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto value = static_cast<std::uint8_t>(bytes[i]);
        if (!inString) {
            if (value == '"') inString = true;
            output += static_cast<char>(value);
            continue;
        }
        if (escaped) {
            escaped = false;
            output += static_cast<char>(value);
            continue;
        }
        if (value == '\\') {
            if (i + 1 < bytes.size()) {
                const auto next = static_cast<std::uint8_t>(bytes[i + 1]);
                const bool validEscape = next == '"' || next == '\\' || next == '/' ||
                    next == 'b' || next == 'f' || next == 'n' || next == 'r' || next == 't' || next == 'u';
                if (validEscape) {
                    escaped = true;
                    output += '\\';
                } else {
                    output += "\\\\";
                    if (next < 0x20) {
                        constexpr char hex[] = "0123456789abcdef";
                        output += "\\u00";
                        output += hex[next >> 4];
                        output += hex[next & 15];
                    } else {
                        output += static_cast<char>(next);
                    }
                    ++i;
                }
            } else {
                output += "\\\\";
            }
            continue;
        }
        if (value == '"') {
            inString = false;
            output += '"';
            continue;
        }
        if (value < 0x20) appendControl(output, value);
        else output += static_cast<char>(value);
    }
    return finish(std::move(output));
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

compat::RetainedStringSlice escapeJsonString(compat::RetainedStringSlice text) try {
    std::string output;
    for (const char byte : text.view()) {
        const auto value = static_cast<std::uint8_t>(byte);
        if (value == '\\') output += "\\\\";
        else if (value == '"') output += "\\\"";
        else if (value < 0x20) appendControl(output, value);
        else output += byte;
    }
    return finish(std::move(output));
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}

} // namespace nijigenerate::api::acp::client::detail
