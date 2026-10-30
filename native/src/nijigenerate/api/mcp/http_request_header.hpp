#pragma once
#include "http_source_log.hpp"
// Selected Vibe HTTP/stream/inet algorithms, Copyright 2012-2024 Sönke Ludwig
// and contributors (MIT). See port/licenses/mcp-http-transport.txt.
#include "http_request_url.hpp"
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <common/detail/phobos_lowercase_data.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::api::mcp::http_transport::detail {
enum class SourceHTTPMethod {
    GET, HEAD, PUT, POST, PATCH, DELETE_, OPTIONS, TRACE, CONNECT,
    PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, LOCK, UNLOCK,
    VERSIONCONTROL, REPORT, CHECKOUT, CHECKIN, UNCHECKOUT, MKWORKSPACE,
    UPDATE, LABEL, MERGE, BASELINECONTROL, MKACTIVITY, ORDERPATCH, ACL
};
enum class SourceHTTPVersion { HTTP_1_0, HTTP_1_1 };
inline constexpr std::array<std::string_view, 29> sourceHTTPMethods{
    "GET", "HEAD", "PUT", "POST", "PATCH", "DELETE", "OPTIONS", "TRACE", "CONNECT",
    "PROPFIND", "PROPPATCH", "MKCOL", "COPY", "MOVE", "LOCK", "UNLOCK",
    "VERSION-CONTROL", "REPORT", "CHECKOUT", "CHECKIN", "UNCHECKOUT", "MKWORKSPACE",
    "UPDATE", "LABEL", "MERGE", "BASELINE-CONTROL", "MKACTIVITY", "ORDERPATCH", "ACL"};
inline std::string_view sourceHTTPMethodString(SourceHTTPMethod method) noexcept {
    return sourceHTTPMethods[static_cast<std::size_t>(method)];
}
inline unsigned sourceHTTPVersionNumber(SourceHTTPVersion version) noexcept {
    return version == SourceHTTPVersion::HTTP_1_0 ? 10 : 11;
}

namespace request_header_source {
template<std::size_t N> [[noreturn]] void failure(const char (&message)[N],
    compat::RetainedString file, std::size_t line) {
    throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral(message), std::move(file), line);
}
inline compat::RetainedString streamSource() {
    return compat::RetainedString::fromStaticLiteral("vibe/stream/operations.d");
}
inline compat::RetainedString messageSource() {
    return compat::RetainedString::fromStaticLiteral("vibe/inet/message.d");
}
inline UrlString trim(UrlString text) {
    std::size_t first = 0, last = text.size();
    while (last && (text.view()[last - 1] == ' ' || text.view()[last - 1] == '\t')) --last;
    while (first < last && (text.view()[first] == ' ' || text.view()[first] == '\t')) ++first;
    return text.slice(first, last);
}
inline std::uint32_t decode(std::string_view text, std::size_t& offset) {
    const auto scalar = compat::phobos::detail::UnicodeCursor<std::string_view, char>(text.substr(offset)).front();
    offset += scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
    return scalar;
}
inline std::uint32_t simpleLower(std::uint32_t scalar) {
    if (scalar < 0xaa) return scalar >= 'A' && scalar <= 'Z' ? scalar + 32 : scalar;
    namespace data = ::nicxlive::core::common::unicode::detail::phobos_lowercase_data;
    const auto packed = [](std::uint64_t offset, std::size_t index, unsigned bits) {
        const auto word = data::trieWords[offset + index / (64u / bits)];
        return static_cast<std::uint32_t>((word >> (bits * (index % (64u / bits)))) & ((1ULL << bits) - 1));
    };
    auto index = packed(data::trieOffsets[0], scalar >> 13, 8);
    index = packed(data::trieOffsets[1], (index << 7) + ((scalar >> 6) & 0x7f), 16);
    index = packed(data::trieOffsets[2], (index << 6) + (scalar & 0x3f), 16);
    // Selected Phobos lower table: the only multi-scalar lowercase is U+0130.
    // Its first scalar is also its simple mapping; no full-string expansion.
    return index == 0xffff ? scalar : data::lowerValues[index] & 0xffffff;
}
inline bool equalName(std::string_view a, std::string_view b) {
    std::size_t i = 0, j;
    const auto commonSize = (std::min)(a.size(), b.size());
    while (i < commonSize && a[i] == b[i]) ++i;
    // Preserve icmp2's exact one-byte retreat, including malformed UTF effects.
    if (i && (static_cast<unsigned char>(a[i - 1]) & 0x80)) --i;
    j = i;
    while (i < a.size() && j < b.size()) {
        auto ac = static_cast<unsigned char>(a[i]);
        auto bc = static_cast<unsigned char>(b[j]);
        if (!((ac | bc) & 0x80)) {
            ++i; ++j;
            if (ac >= 'A' && ac <= 'Z') ac += 'a' - 'A';
            if (bc >= 'A' && bc <= 'Z') bc += 'a' - 'A';
            if (ac != bc) return false;
        } else {
            const auto left = decode(a, i);
            const auto right = decode(b, j);
            if (left != right && simpleLower(left) != simpleLower(right)) return false;
        }
    }
    return i == a.size() && j == b.size();
}
} // namespace request_header_source

struct SourceHTTPHeaderField { UrlString name, value; };
class SourceHTTPHeaders {
    // InetHeaderMap has twelve value-copied inline fields and a shared tail.
    std::array<SourceHTTPHeaderField, 12> inline_{};
    std::size_t count_ = 0;
    compat::Slice<SourceHTTPHeaderField> tail_;
    SourceHTTPHeaderField& at(std::size_t index) {
        return index < count_ ? inline_[index] : tail_[index - count_];
    }
    const SourceHTTPHeaderField& at(std::size_t index) const {
        return index < count_ ? inline_[index] : tail_[index - count_];
    }
public:
    std::size_t size() const noexcept { return count_ + tail_.size(); }
    void addField(UrlString name, UrlString value) {
        if (count_ < inline_.size()) inline_[count_++] = {name, value};
        else tail_.append(SourceHTTPHeaderField{name, value});
    }
    UrlString* find(std::string_view name) {
        for (std::size_t i = 0; i < size(); ++i)
            if (at(i).name.size() == name.size()
                && request_header_source::equalName(at(i).name.view(), name)) return &at(i).value;
        return nullptr;
    }
    const UrlString* find(std::string_view name) const {
        for (std::size_t i = 0; i < size(); ++i)
            if (at(i).name.size() == name.size()
                && request_header_source::equalName(at(i).name.view(), name)) return &at(i).value;
        return nullptr;
    }
    UrlString get(std::string_view name) const {
        if (const auto value = find(name)) return *value;
        return {};
    }
    UrlString setFirst(UrlString name, UrlString value) {
        if (auto previous = find(name.view())) *previous = value;
        else addField(name, value);
        return value;
    }
    template<class Visitor> void forEach(Visitor&& visitor) const {
        for (std::size_t i = 0; i < size(); ++i) {
            const auto field = at(i);
            visitor(field.name, field.value);
        }
    }
};
struct SourceRequestHeader {
    SourceHTTPMethod method = SourceHTTPMethod::GET;
    UrlString requestURI = compat::RetainedString::fromStaticLiteral("/");
    SourceHTTPVersion httpVersion = SourceHTTPVersion::HTTP_1_1;
    SourceHTTPHeaders headers;
};

// Actual input boundary: peek borrows buffered, UNCONSUMED bytes without waiting.
// readExact fills/consumes the whole span or propagates the real reader exception;
// it may suspend the owning request fiber. No partial-success count is accepted.
template<class Reader> concept SourceHeaderReader = requires(Reader& reader, std::span<std::uint8_t> bytes) {
    { reader.peek() } -> std::convertible_to<std::span<const std::uint8_t>>;
    { reader.readExact(bytes) } -> std::same_as<void>;
};

namespace request_header_source {
template<SourceHeaderReader Reader> class LimitedInput {
    Reader& source_;
    std::uint64_t remaining_;
public:
    LimitedInput(Reader& source, std::uint64_t limit) : source_(source), remaining_(limit) {}
    bool empty() const noexcept { return remaining_ == 0; }
    std::span<const std::uint8_t> peek() { return source_.peek(); }
    void readExact(std::span<std::uint8_t> bytes) {
        if (bytes.size() > remaining_)
            throw HTTPStatusException(413,
                compat::RetainedString::fromStaticLiteral("Request Entity Too Large"), 1838,
                compat::RetainedString::fromStaticLiteral("vibe/http/server.d"));
        source_.readExact(bytes);
        remaining_ -= bytes.size();
    }
    void skip(std::size_t count) {
        std::array<std::uint8_t, 256> buffer;
        while (count) {
            const auto amount = (std::min)(count, buffer.size());
            readExact(std::span(buffer).first(amount));
            count -= amount;
        }
    }
};

template<class Input> UrlString readLine(Input& stream, std::string_view marker, std::size_t maximum) {
    std::string output;
    output.reserve((std::min)(maximum, std::size_t{64}));
#ifndef NDEBUG
    if (!maximum) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Assertion failure"), streamSource(), 145);
#endif
    std::uint64_t left = maximum;
    std::size_t matched = 0;
    for (;;) {
        if (stream.empty()) failure("Reached EOF while searching for end marker.", streamSource(), 389);
        if (!left) failure("Reached maximum number of bytes while searching for end marker.", streamSource(), 390);
        auto peeked = stream.peek();
        peeked = peeked.first(static_cast<std::size_t>(std::min<std::uint64_t>(peeked.size(), left)));
        if (peeked.empty() || matched == 1) {
            std::array<std::uint8_t, 2> buffer;
            const auto amount = marker.size() - matched;
            stream.readExact(std::span(buffer).first(amount));
            for (std::size_t i = 0; i < amount; ++i) {
                if (buffer[i] == static_cast<unsigned char>(marker[matched])) ++matched;
                else if (buffer[i] == static_cast<unsigned char>(marker[0])) {
                    output.append(marker.data(), matched);
                    matched = 1;
                } else {
                    output.append(marker.data(), matched);
                    matched = 0;
                    output.push_back(static_cast<char>(buffer[i]));
                }
                if (matched == marker.size()) return UrlString(std::move(output));
            }
        } else {
            const auto found = std::find(peeked.begin(), peeked.end(), static_cast<std::uint8_t>(marker[0]));
            const auto count = static_cast<std::size_t>(found - peeked.begin());
            output.append(reinterpret_cast<const char*>(peeked.data()), count);
            if (found == peeked.end()) {
                left -= peeked.size();
                stream.skip(peeked.size());
            } else if (marker.size() == 1) {
                stream.skip(count + 1);
                return UrlString(std::move(output));
            } else if (count + 1 < peeked.size() && peeked[count + 1] == static_cast<unsigned char>(marker[1])) {
                stream.skip(count + 2);
                return UrlString(std::move(output));
            } else {
                ++matched;
                stream.skip(count + 1);
            }
        }
    }
}

inline SourceHTTPMethod method(std::string_view text) {
    for (std::size_t i = 0; i < sourceHTTPMethods.size(); ++i)
        if (text == sourceHTTPMethods[i]) return static_cast<SourceHTTPMethod>(i);
    std::string message = "Invalid HTTP method: ";
    message.append(text);
    throw compat::ThrowableException(compat::RetainedString(std::move(message)),
        compat::RetainedString::fromStaticLiteral("vibe/http/common.d"), 104);
}
[[noreturn]] inline void invalidInteger(std::string_view suffix) {
    if (suffix.empty()) throw compat::phobos::ConvException(
        compat::RetainedString::fromStaticLiteral("Unexpected end of input when converting from type string to type int"),
        compat::RetainedString::fromStaticLiteral("std/conv.d"), 2547);
    const auto length = compat::phobos::ngUnexpectedScalarLength(suffix);
    const auto shown = suffix.front() == '\n' ? std::string("\\n") : std::string(suffix.substr(0, length));
    throw compat::phobos::ConvException(
        compat::RetainedString("Unexpected '" + shown + "' when converting from type string to type int"),
        compat::RetainedString::fromStaticLiteral("std/conv.d"), 2547);
}
inline SourceHTTPVersion version(UrlString& text) {
    const auto bad = [](std::size_t line) {
        throw HTTPStatusException(400, compat::RetainedString::fromStaticLiteral("Bad Request"), line,
            compat::RetainedString::fromStaticLiteral("vibe/http/common.d"));
    };
    if (!text.view().starts_with("HTTP/1.")) bad(373);
    text = text.slice(7, text.size());
    const auto bytes = text.view();
    std::size_t offset = 0;
    bool negative = false;
    if (!bytes.empty() && (bytes[0] == '-' || bytes[0] == '+')) {
        negative = bytes[0] == '-';
        ++offset;
    }
    if (offset == bytes.size() || bytes[offset] < '0' || bytes[offset] > '9')
        invalidInteger(bytes.substr(offset));
    std::uint32_t magnitude = 0;
    const auto maximum = std::uint32_t{2147483647} + unsigned(negative);
    while (offset < bytes.size() && bytes[offset] >= '0' && bytes[offset] <= '9') {
        const auto digit = unsigned(bytes[offset] - '0');
        if (magnitude > maximum / 10 || (magnitude == maximum / 10 && digit > maximum % 10))
            throw compat::phobos::ConvOverflowException(
                compat::RetainedString::fromStaticLiteral("Overflow in integral conversion"),
                compat::RetainedString::fromStaticLiteral("std/conv.d"), 2527);
        magnitude = magnitude * 10 + digit;
        ++offset;
    }
    text = text.slice(offset, text.size());
    const auto minor = std::bit_cast<std::int32_t>(negative ? 0u - magnitude : magnitude);
    if (minor != 0 && minor != 1) bad(377);
    return minor == 0 ? SourceHTTPVersion::HTTP_1_0 : SourceHTTPVersion::HTTP_1_1;
}
} // namespace request_header_source

template<SourceHeaderReader Reader> std::uint32_t parseRequestHeader(SourceRequestHeader& request, Reader& reader,
    std::uint64_t maxHeaderBytes = 8192, std::size_t maxHeaderLineBytes = 4096) {
    using namespace request_header_source;
    LimitedInput<Reader> stream(reader, maxHeaderBytes);
    httpSourceLog(SourceLogLevel::trace, "HTTP server reading status line");
    auto line = readLine(stream, "\r\n", maxHeaderLineBytes);
    httpSourceLog(SourceLogLevel::trace, "--------------------");
    httpSourceLog(SourceLogLevel::trace, "HTTP server request:");
    httpSourceLog(SourceLogLevel::trace, "--------------------");
    httpSourceLog(SourceLogLevel::trace, "%s", [&] { return std::tuple(line); });
    auto position = line.view().find(' ');
    if (position == std::string_view::npos)
        throw HTTPStatusException(400, compat::RetainedString::fromStaticLiteral("invalid request method"), 382);
    request.method = method(line.view().substr(0, position));
    line = line.slice(position + 1, line.size());
    position = line.view().find(' ');
    if (position == std::string_view::npos)
        throw HTTPStatusException(400, compat::RetainedString::fromStaticLiteral("invalid request path"), 388);
    request.requestURI = line.slice(0, position);
    line = line.slice(position + 1, line.size());
    request.httpVersion = version(line);

    UrlString name, value;
    const auto commit = [&] { if (!name.empty()) request.headers.addField(name, value); };
    for (;;) {
        auto header = readLine(stream, "\n", maxHeaderLineBytes);
        if (!header.empty() && header.view().back() == '\r') header = header.slice(0, header.size() - 1);
        if (header.empty()) break;
        if (header.view()[0] != ' ' && header.view()[0] != '\t') {
            commit();
            const auto colon = header.view().find(':');
            if (colon == std::string_view::npos) failure("Header is missing ':'.", messageSource(), 69);
            if (!colon) failure("Header name is empty.", messageSource(), 70);
            name = trim(header.slice(0, colon));
            value = trim(header.slice(colon + 1, header.size()));
        } else {
            const auto continuation = trim(header);
            std::string joined(value.view());
            joined += ' ';
            joined.append(continuation.view());
            value = UrlString(std::move(joined));
        }
    }
    commit();
    request.headers.forEach([](UrlString key, UrlString value) {
        httpSourceLog(SourceLogLevel::trace, "%s: %s", [&] { return std::tuple(key, value); });
    });
    httpSourceLog(SourceLogLevel::trace, "--------------------");
    return 0; // Original settings disable HTTP/2 negotiation.
}
} // namespace nijigenerate::api::mcp::http_transport::detail
