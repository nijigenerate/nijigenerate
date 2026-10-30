#pragma once
// Selected std.socket IPv4 TCP interface, Phobos, Boost Software License 1.0.
// Copyright (C) 2004-2011 Christopher E. Miller. See port/licenses/phobos-socket.txt.
// Original: C:/opt/ldc-1.41/import/std/socket.d. No implicit module startup.
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include "throwable.hpp"
#include "retained_string.hpp"
#include "slice.hpp"
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <cstdint>
#include <memory>
#include <utility>

namespace nijigenerate::compat::phobos::socket {
enum class AddressFamily : std::uint16_t { INET = AF_INET };
#ifdef _WIN32
using SocketHandle = SOCKET;
using AddressLength = int;
inline constexpr SocketHandle invalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
using AddressLength = socklen_t;
inline constexpr SocketHandle invalidSocket = -1;
#endif

// These are module hooks. The Source program entry owns their one-time pairing.
void ngInitializeSourceSocketModule();
void ngFinalizeSourceSocketModule() noexcept;
int ngLastSocketError() noexcept;
RetainedString ngFormatSocketError(int error);

class SocketException : public ThrowableException {
public:
    SocketException(RetainedString message, std::size_t line);
    std::string_view ngThrowableTypeName() const noexcept override { return "std.socket.SocketException"; }
};
class SocketOSException : public SocketException {
public:
    int errorCode;
    SocketOSException(RetainedString message, std::size_t line, int error = ngLastSocketError());
    std::string_view ngThrowableTypeName() const noexcept override { return "std.socket.SocketOSException"; }
};
class AddressException : public SocketOSException {
public:
    AddressException(RetainedString message, std::size_t line, int error = ngLastSocketError());
    std::string_view ngThrowableTypeName() const noexcept override { return "std.socket.AddressException"; }
};
class HostException : public SocketOSException {
public:
    HostException(RetainedString message, std::size_t line, int error = ngLastSocketError());
    std::string_view ngThrowableTypeName() const noexcept override { return "std.socket.HostException"; }
};

#define NG_SOCKET_OBJECT_INTERFACE(Type) \
    virtual RetainedString toString() const; \
    virtual std::size_t toHash() const noexcept; \
    virtual bool opEquals(std::shared_ptr<Type> other) const; \
    virtual int opCmp(std::shared_ptr<Type> other) const

class InternetHost {
public:
    RetainedString name;
    Slice<RetainedString> aliases;
    Slice<std::uint32_t> addrList;
    InternetHost();
    virtual ~InternetHost() = default;
    bool getHostByName(RetainedString host);
    NG_SOCKET_OBJECT_INTERFACE(InternetHost);
};

class Address {
public:
    virtual ~Address() = default;
    virtual sockaddr* name() noexcept = 0;
    virtual const sockaddr* name() const noexcept = 0;
    virtual AddressLength nameLen() const noexcept = 0;
    void setNameLen(AddressLength length);
    AddressFamily addressFamily() const noexcept { return static_cast<AddressFamily>(name()->sa_family); }
    virtual RetainedString toAddrString() const = 0;
    virtual RetainedString toPortString() const = 0;
    NG_SOCKET_OBJECT_INTERFACE(Address);
};

// Only the used IPv4 family and methods are exposed; this is not an IPv6 facade.
class InternetAddress : public Address {
    sockaddr_in sin_{};
    template<class> friend struct ::nicxlive::core::d_runtime::SourceAllocator;
    InternetAddress();
    void construct(RetainedString host, std::uint16_t port);
public:
    static std::shared_ptr<InternetAddress> createSource(RetainedString host, std::uint16_t port);
    // Use this entry for authored new-expressions with evaluated arguments.
    // Both producers execute after allocation of the actual InternetAddress.
    template<class HostProducer, class PortProducer>
    static std::shared_ptr<InternetAddress> createSourceDeferred(HostProducer&& host, PortProducer&& port) {
        auto receiver = std::allocate_shared<InternetAddress>(
            ::nicxlive::core::d_runtime::SourceAllocator<InternetAddress>{});
        RetainedString actualHost = std::forward<HostProducer>(host)();
        std::uint16_t actualPort = std::forward<PortProducer>(port)();
        receiver->construct(std::move(actualHost), actualPort);
        return receiver;
    }
    static std::shared_ptr<InternetAddress> createSourceEmpty();
    sockaddr* name() noexcept override { return reinterpret_cast<sockaddr*>(&sin_); }
    const sockaddr* name() const noexcept override { return reinterpret_cast<const sockaddr*>(&sin_); }
    AddressLength nameLen() const noexcept override { return sizeof(sin_); }
    std::uint16_t port() const noexcept;
    RetainedString toAddrString() const override;
    RetainedString toPortString() const override;
    bool opEquals(std::shared_ptr<Address> other) const override;
};

class Socket {
protected:
    SocketHandle sock_ = invalidSocket;
    AddressFamily family_ = static_cast<AddressFamily>(0);
#ifdef _WIN32
    bool blocking_ = true;
#endif
    Socket();
    void constructTcpIPv4();
public:
    virtual ~Socket() noexcept;
    void bind(const std::shared_ptr<Address>& address);
    void connect(const std::shared_ptr<Address>& address);
    void listen(int backlog);
    void close() noexcept;
    std::shared_ptr<Address> localAddress();
    NG_SOCKET_OBJECT_INTERFACE(Socket);
};
class TcpSocket : public Socket {
    template<class> friend struct ::nicxlive::core::d_runtime::SourceAllocator;
    TcpSocket();
public:
    static std::shared_ptr<TcpSocket> createSource();
};
#undef NG_SOCKET_OBJECT_INTERFACE
}

namespace nicxlive::core::d_runtime {
#define NG_SOCKET_TYPE_NAMES(Type) \
template<> struct SourceObjectTypeNames<::nijigenerate::compat::phobos::socket::Type> { \
    inline static constexpr char className[] = "std.socket." #Type; \
    inline static constexpr char templateArgumentName[] = "(std.socket." #Type ")"; \
}; \
template<> struct SourceTypeSpellings<::nijigenerate::compat::phobos::socket::Type> { \
    inline static constexpr bool isNativeObject = true; \
    inline static constexpr char qualifiedArgument[] = "(std.socket." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "std.socket." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
};
NG_SOCKET_TYPE_NAMES(Address)
NG_SOCKET_TYPE_NAMES(InternetAddress)
NG_SOCKET_TYPE_NAMES(Socket)
NG_SOCKET_TYPE_NAMES(TcpSocket)
NG_SOCKET_TYPE_NAMES(SocketException)
NG_SOCKET_TYPE_NAMES(SocketOSException)
NG_SOCKET_TYPE_NAMES(AddressException)
NG_SOCKET_TYPE_NAMES(HostException)
NG_SOCKET_TYPE_NAMES(InternetHost)
#undef NG_SOCKET_TYPE_NAMES
}
