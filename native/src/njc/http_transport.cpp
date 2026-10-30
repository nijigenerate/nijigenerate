#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "http_transport.hpp"
#include "detail/utf8_replacement.hpp"
#include "detail/phobos_string_strip.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <new>
#include <source_location>

namespace njc::http_transport {
namespace {
using Value = nijigenerate::core::settings::SettingsJsonValue;
using nijigenerate::compat::RetainedString;

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

void prepareHttpTransportClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(HttpTransport),
            RetainedString::fromStaticLiteral("njc.http_transport.HttpTransport"));
        return true;
    }();
    (void)registered;
}
template<std::size_t N> void enforce(bool condition, const char (&message)[N], std::size_t sourceLine) {
    if (!condition) {
        const auto copiedMessage = allocateMemory([&] { return RetainedString(std::string_view(message, N - 1)); });
        throw HttpTransportException(copiedMessage, sourceLine);
    }
}
#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr auto invalidSocket = INVALID_SOCKET;
struct SocketRuntime {
    SocketRuntime() {
        WSADATA data{};
        // Preserve the actual request word used by current Phobos std.socket.
        const auto result = WSAStartup(0x2020, &data);
        if (result) throw SocketOSException(
            allocateMemory([] { return std::string("Unable to initialize socket library"); }), result);
    }
    ~SocketRuntime() { WSACleanup(); }
};
void closeSocket(SocketHandle handle) { closesocket(handle); }
int lastSocketError() { return WSAGetLastError(); }
#else
using SocketHandle = int;
constexpr auto invalidSocket = -1;
struct SocketRuntime {};
void closeSocket(SocketHandle handle) { ::close(handle); }
int lastSocketError() { return errno; }
#endif

// Adapted from installed independent LDC 1.41 Phobos std.socket/std.windows.syserror.
// Copyright (C) 2004-2011 Christopher E. Miller; The D Language Foundation 2006-2013.
// Boost Software License 1.0: detail/phobos.LICENSE.
std::string formatSocketError(int code) {
#ifdef _WIN32
    wchar_t* message = nullptr;
    const auto length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(code), 0,
        reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    const auto release = [](wchar_t* value) { if (value) LocalFree(value); };
    const std::unique_ptr<wchar_t, decltype(release)> owned(message, release);
    if (message) {
        const auto white = [](wchar_t c) {
            return (c >= 9 && c < 14) || c == 32 || c == 133 || c == 160 || c == 5760 ||
                (c >= 8192 && c < 8203) || (c >= 8232 && c < 8234) || c == 8239 || c == 8287 || c == 12288;
        };
        std::size_t first = 0, end = length;
        while (first < end && white(message[first])) ++first;
        while (end > first && white(message[end - 1])) --end;
        if (first == end) return {};
        const auto count = static_cast<int>(end - first);
        const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, message + first, count,
            nullptr, 0, nullptr, nullptr);
        if (bytes) {
            auto text = allocateMemory([&] { return std::string(static_cast<std::size_t>(bytes), '\0'); });
            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, message + first, count,
                text.data(), bytes, nullptr, nullptr) == bytes) return text;
        }
    }
    return allocateMemory([&] { return "Error " + std::to_string(static_cast<std::uint32_t>(code)); });
#else
    std::array<char, 80> buffer{};
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    const char* message = strerror_r(code, buffer.data(), buffer.size());
#else
    const int result = strerror_r(code, buffer.data(), buffer.size());
    if (result) return allocateMemory([&] { return "Socket error " + std::to_string(code); });
    const char* message = buffer.data();
#endif
    auto text = allocateMemory([&] { return std::string(message); });
    if (!text.empty() && text.back() == '\n') text.pop_back();
    if (!text.empty() && text.back() == '\r') text.pop_back();
    return text;
#endif
}
RetainedString socketErrorMessage(std::string message, int code) {
    if (message.empty()) {
        auto detail = formatSocketError(code);
        return allocateMemory([&] { return RetainedString(std::move(detail)); });
    }
    const auto detail = formatSocketError(code);
    return allocateMemory([&] { return RetainedString(message + ": " + detail); });
}
struct Socket {
    SocketHandle handle = invalidSocket;
    Socket() {
        ngInitializeHttpTransport();
        handle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (handle == invalidSocket) {
            const auto error = lastSocketError();
            throw SocketOSException(allocateMemory([] { return std::string("Unable to create socket"); }), error);
        }
#ifdef SO_NOSIGPIPE
        const int enabled = 1;
        if (setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) != 0) {
            const auto error = lastSocketError();
            closeSocket(handle);
            handle = invalidSocket;
            throw SocketOSException(allocateMemory([] { return std::string("Unable to set socket option"); }), error);
        }
#endif
    }
    ~Socket() { if (handle != invalidSocket) closeSocket(handle); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
};
void connect(Socket& socket, const config::Endpoint& endpoint) {
    const auto host = allocateMemory([&] { return std::string(endpoint.host.view()); });
    const auto* name = endpoint.host.isNull() ? nullptr : host.c_str();
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    address.sin_addr.s_addr = inet_addr(name);
    if (address.sin_addr.s_addr == INADDR_NONE) {
        // InternetAddress parses inet_addr first, then resolves through InternetHost;
        // getaddrinfo has different numeric-host and error behavior.
        hostent* resolved = nullptr;
#if defined(__linux__)
        hostent entry{};
        auto buffer = allocateMemory([] { return std::vector<char>(256); });
        int hostError = 0;
        for (;;) {
            const auto result = gethostbyname_r(name, &entry, buffer.data(), buffer.size(), &resolved, &hostError);
            if (result != ERANGE) break;
            allocateMemory([&] { buffer.resize(buffer.size() * 2); });
        }
#elif !defined(_WIN32)
        static std::mutex hostMutex;
        const std::lock_guard lock(hostMutex);
        resolved = gethostbyname(name);
#else
        resolved = gethostbyname(name);
#endif
        if (!resolved) {
            auto message = allocateMemory([&] { return "Unable to resolve host '" + host + "'"; });
            const auto error = lastSocketError();
            throw AddressException(std::move(message), error);
        }
        if (resolved->h_addrtype != AF_INET || resolved->h_length != 4) {
            const auto error = lastSocketError();
            throw HostException(allocateMemory([] { return std::string("Address family mismatch"); }), error);
        }
        if (!resolved->h_addr_list[0]) throw nijigenerate::compat::ArrayIndexError(0, 0);
        std::memcpy(&address.sin_addr.s_addr, resolved->h_addr_list[0], sizeof(address.sin_addr.s_addr));
    }
    if (::connect(socket.handle, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        const auto error = lastSocketError();
        throw SocketOSException(allocateMemory([] { return std::string("Unable to connect socket"); }), error);
    }
}
void sendAll(Socket& socket, std::string_view request) {
    while (!request.empty()) {
#ifdef _WIN32
        const auto count = static_cast<int>(std::min(request.size(),
            static_cast<std::size_t>(std::numeric_limits<int>::max())));
#else
        const auto count = request.size();
#endif
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const auto sent = ::send(socket.handle, request.data(), count, flags);
        enforce(sent > 0, "failed to send request", 48);
        request.remove_prefix(static_cast<std::size_t>(sent));
    }
}
std::string readAll(Socket& socket) {
    std::string data;
    std::array<char, 8192> buffer{};
    for (;;) {
        const auto count = ::recv(socket.handle, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (count <= 0) break;
        allocateMemory([&] { data.append(buffer.data(), static_cast<std::size_t>(count)); });
    }
    enforce(!data.empty(), "empty response from MCP server", 61);
    return data;
}
std::string_view strip(std::string_view value) {
    const auto [first, last] = njc::detail::ngStripStringBounds(value);
    return value.substr(first, last - first);
}
std::string_view statusLine(std::string_view header) {
    std::size_t end = header.size();
    for (auto separator : {std::string_view("\v"), std::string_view("\f"), std::string_view("\n"),
        std::string_view("\r"), std::string_view("\xc2\x85"), std::string_view("\xe2\x80\xa8"),
        std::string_view("\xe2\x80\xa9")}) end = std::min(end, header.find(separator));
    if (header.empty()) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::ArrayIndexError>(0, 0,
        RetainedString::fromStaticLiteral("source/njc/http_transport.d"), 72);
    return header.substr(0, end);
}
bool chunked(std::string_view header) {
    // Only the ASCII needle is observable. Non-ASCII lowercase scalars are separators;
    // Kelvin sign lowercases to ASCII k; dotted I expands to i plus a combining dot.
    std::string projected;
    for (std::size_t position = 0; position < header.size();) {
        auto c = njc::detail::ngNextReplacementScalar(header, position);
        if (c >= U'A' && c <= U'Z') c += U'a' - U'A';
        if (c == 0x212a) c = U'k';
        allocateMemory([&] {
            if (c == 0x130) projected += 'i';
            projected += c < 128 ? static_cast<char>(c) : '#';
        });
    }
    return projected.find("transfer-encoding: chunked") != std::string::npos;
}
std::size_t parseHexSize(std::string_view text) {
    std::size_t value = 0;
    for (unsigned char c : text) {
        if (c == ';') break;
        unsigned digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = 10 + c - 'a';
        else if (c >= 'A' && c <= 'F') digit = 10 + c - 'A';
        else throw HttpTransportException(RetainedString::fromStaticLiteral("invalid chunk size"), 110);
        value = value * 16 + digit;
    }
    return value;
}
std::string decodeChunked(std::string_view body) {
    std::string output;
    std::size_t position = 0;
    for (;;) {
        const auto lineEnd = body.find("\r\n", position);
        enforce(lineEnd != std::string_view::npos, "invalid chunked response", 90);
        const auto size = parseHexSize(strip(body.substr(position, lineEnd - position)));
        position = lineEnd + 2;
        if (size == 0) break;
        enforce(position + size + 2 <= body.size(), "truncated chunked response", 95);
        // A wrapped D length passes the preceding enforce but fails the slice bounds.
        if (position > body.size() || size > body.size() - position)
            throw nijigenerate::compat::ngStaticError<nijigenerate::compat::ArraySliceError>(
                position, position + size, body.size(),
                RetainedString::fromStaticLiteral("source/njc/http_transport.d"), 96);
        const auto chunk = body.substr(position, size);
        allocateMemory([&] { output.append(chunk); });
        position += size + 2;
    }
    return output;
}
Value parseJson(std::string_view text) {
    const auto input = allocateMemory([&] { return RetainedString(text); });
    nijigenerate::autorig::json::detail::FirstPassReader reader{input};
    // Default parseJSON intentionally accepts trailing input; strictParsing is not set.
    return reader.parseRoot();
}
Value parseJsonResponse(std::string_view raw) {
    const auto separator = raw.find("\r\n\r\n");
    enforce(separator != std::string_view::npos, "invalid HTTP response", 68);
    const auto header = raw.substr(0, separator);
    auto body = raw.substr(separator + 4);
    const auto status = statusLine(header);
    if (status.find(" 200 ") == std::string_view::npos && status.find(" 204 ") == std::string_view::npos) {
        const auto text = allocateMemory([&] { return RetainedString("MCP server returned " + std::string(status)); });
        auto message = allocateMemory([&] { return RetainedString(text.view()); });
        throw HttpTransportException(std::move(message), 73);
    }
    std::string decoded;
    if (chunked(header)) { decoded = decodeChunked(body); body = decoded; }
    if (strip(body).empty()) return parseJson(R"({"jsonrpc":"2.0","result":null})");
    return parseJson(body);
}
} // namespace

void ngInitializeHttpTransport() {
    static SocketRuntime runtime;
    (void)runtime;
}

SocketOSException::SocketOSException(std::string message, int code)
    : SocketException(socketErrorMessage(std::move(message), code)),
      errorCode(code) {}

HttpTransport::HttpTransport(config::Endpoint endpoint) : endpoint(std::move(endpoint)) {}
RetainedString HttpTransport::toString() {
    prepareHttpTransportClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t HttpTransport::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(this);
    return address ^ (address >> 4);
}
bool HttpTransport::opEquals(std::shared_ptr<HttpTransport> other) {
    return other.get() == this;
}
int HttpTransport::opCmp(std::shared_ptr<HttpTransport>) {
    prepareHttpTransportClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    auto message = allocateMemory([&] {
        return RetainedString(std::string("need opCmp for class ") + std::string(className.view()));
    });
    throw nijigenerate::compat::ThrowableException(std::move(message),
        RetainedString::fromStaticLiteral("object.d"), 195);
}
std::string HttpTransport::buildRequest(config::Text body) const {
    return allocateMemory([&] {
        std::string request = "POST " + std::string(endpoint.path.view()) + " HTTP/1.1\r\nHost: " +
            std::string(endpoint.host.view()) + ":" + std::to_string(endpoint.port) +
            "\r\nContent-Type: application/json\r\nAccept: application/json\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\nConnection: close\r\n";
        if (endpoint.bearer.size()) request += "Authorization: Bearer " + std::string(endpoint.bearer.view()) + "\r\n";
        request += "\r\n";
        request += body.view();
        return request;
    });
}
Value HttpTransport::postJson(config::Text body) {
    auto socket = std::allocate_shared<Socket>(nicxlive::core::d_runtime::SourceAllocator<Socket>{});
    connect(*socket, endpoint);
    sendAll(*socket, buildRequest(body));
    return parseJsonResponse(readAll(*socket));
}
} // namespace njc::http_transport
