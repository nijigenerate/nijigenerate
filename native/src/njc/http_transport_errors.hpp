#pragma once

#include <nijigenerate/compat/throwable.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace njc::http_transport {

// Source enforce/new Exception messages retain their complete byte payload.
// In particular, an HTTP status line can contain embedded NUL bytes.
class HttpTransportException : public nijigenerate::compat::ThrowableException {
public:
    explicit HttpTransportException(nijigenerate::compat::RetainedString message)
        : ThrowableException(std::move(message)) {}
    HttpTransportException(nijigenerate::compat::RetainedString message, std::size_t sourceLine)
        : ThrowableException(std::move(message),
            nijigenerate::compat::RetainedString::fromStaticLiteral("source/njc/http_transport.d"), sourceLine) {}
};

class SocketException : public nijigenerate::compat::ThrowableException {
public:
    using ThrowableException::ThrowableException;
    std::string_view ngThrowableTypeName() const noexcept override { return "std.socket.SocketException"; }
};

// Actual std.socket OS exceptions retain the failing platform error code.
class SocketOSException : public SocketException {
public:
    SocketOSException(std::string message, int code);
    int errorCode;
    std::string_view ngThrowableTypeName() const noexcept override { return "std.socket.SocketOSException"; }
};
class AddressException : public SocketOSException {
public:
    using SocketOSException::SocketOSException;
    std::string_view ngThrowableTypeName() const noexcept override { return "std.socket.AddressException"; }
};
class HostException : public SocketOSException {
public:
    using SocketOSException::SocketOSException;
    std::string_view ngThrowableTypeName() const noexcept override { return "std.socket.HostException"; }
};

} // namespace njc::http_transport
