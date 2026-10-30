// Used std.socket bodies, Phobos, Boost Software License 1.0.
// Copyright (C) 2004-2011 Christopher E. Miller. See port/licenses/phobos-socket.txt.
#include "phobos_socket.hpp"
#include "phobos_temp_cstring.hpp"
#include "phobos_file_exception.hpp"
#include "assert_error.hpp"
#include "native_access_fault.hpp"
#include "out_of_memory_error.hpp"
#include <array>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

namespace nijigenerate::compat::phobos::socket {
namespace {
template<class Operation> decltype(auto) storage(Operation&& operation) {
    try { return std::forward<Operation>(operation)(); }
    catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}
template<std::size_t N> RetainedString literal(const char (&text)[N]) {
    return RetainedString::fromStaticLiteral(text);
}
RetainedString copy(std::string_view text) {
    return storage([&] { return RetainedString(text); });
}
RetainedString decimal(std::uint64_t value) {
    std::array<char, 20> buffer;
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return copy({buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())});
}
template<class T> void prepare() {
    static const bool ready = [] {
        storage([] {
            using namespace ::nicxlive::core::d_runtime;
            ngRegisterObjectClassName(typeid(T), RetainedString::fromStaticLiteral(SourceObjectTypeNames<T>::className));
            (void)ngObjectTemplateArgumentName<T>();
            ngPrepareSourceTypeSpellings<T>();
        });
        return true;
    }();
    (void)ready;
}
RetainedString errorMessage(RetainedString message, int error) {
    const auto detail = ngFormatSocketError(error);
    if (message.empty()) return detail;
    return storage([&] {
        return RetainedString(std::string(message.view()) + ": " + std::string(detail.view()));
    });
}
#ifdef _WIN32
decltype(&::getnameinfo) getnameinfoPointer = nullptr;
decltype(&::getaddrinfo) getaddrinfoPointer = nullptr;
decltype(&::freeaddrinfo) freeaddrinfoPointer = nullptr;
#else
[[maybe_unused]] decltype(&::getnameinfo) getnameinfoPointer = nullptr;
[[maybe_unused]] decltype(&::getaddrinfo) getaddrinfoPointer = nullptr;
[[maybe_unused]] decltype(&::freeaddrinfo) freeaddrinfoPointer = nullptr;
#endif
}

int ngLastSocketError() noexcept {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}
RetainedString ngFormatSocketError(int error) {
#ifdef _WIN32
    // The canonical provider implements generateSysErrorMsg, without the
    // WindowsException constructor's additional numeric suffix.
    return file_exception_detail::systemMessage(static_cast<std::uint32_t>(error));
#else
    char buffer[80];
    const char* message;
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    message = ::strerror_r(error, buffer, sizeof(buffer));
#else
    if (::strerror_r(error, buffer, sizeof(buffer)) != 0) {
        return storage([&] { return RetainedString("Socket error " + std::to_string(error)); });
    }
    message = buffer;
#endif
    auto length = std::strlen(message);
    if (length && message[length - 1] == '\n') --length;
    if (length && message[length - 1] == '\r') --length;
    return copy({message, length});
#endif
}
SocketException::SocketException(RetainedString message, std::size_t line)
    : ThrowableException(std::move(message), literal("std/socket.d"), line) { prepare<SocketException>(); }
SocketOSException::SocketOSException(RetainedString message, std::size_t line, int error)
    : SocketException(errorMessage(std::move(message), error), line), errorCode(error) {
    prepare<SocketException>();
    prepare<SocketOSException>();
}
AddressException::AddressException(RetainedString message, std::size_t line, int error)
    : SocketOSException(std::move(message), line, error) { prepare<AddressException>(); }
HostException::HostException(RetainedString message, std::size_t line, int error)
    : SocketOSException(std::move(message), line, error) { prepare<HostException>(); }
void ngInitializeSourceSocketModule() {
#ifdef _WIN32
    WSADATA data;
    const int result = WSAStartup(0x2020, &data);
    if (result) throw SocketOSException(literal("Unable to initialize socket library"), 302, result);
    const auto library = GetModuleHandleA("ws2_32.dll");
    if (library) {
        getnameinfoPointer = reinterpret_cast<decltype(getnameinfoPointer)>(GetProcAddress(library, "getnameinfo"));
        getaddrinfoPointer = reinterpret_cast<decltype(getaddrinfoPointer)>(GetProcAddress(library, "getaddrinfo"));
        freeaddrinfoPointer = reinterpret_cast<decltype(freeaddrinfoPointer)>(GetProcAddress(library, "freeaddrinfo"));
    }
#else
    getnameinfoPointer = &::getnameinfo;
    getaddrinfoPointer = &::getaddrinfo;
    freeaddrinfoPointer = &::freeaddrinfo;
#endif
}
void ngFinalizeSourceSocketModule() noexcept {
#ifdef _WIN32
    (void)WSACleanup();
#endif
}

InternetHost::InternetHost() { prepare<InternetHost>(); }
bool InternetHost::getHostByName(RetainedString host) {
    const auto populate = [&](hostent* value) {
        if (!value) return false;
        if (value->h_addrtype != AF_INET || value->h_length != 4)
            throw HostException(literal("Address family mismatch"), 640);
        name = copy(value->h_name);
        std::size_t count = 0;
        while (value->h_aliases[count]) ++count;
        aliases = {};
        if (count) {
            aliases.resize(count);
            for (std::size_t index = 0; index != count; ++index) aliases[index] = copy(value->h_aliases[index]);
        }
        count = 0;
        while (value->h_addr_list[count]) ++count;
        addrList = {};
        if (count) {
            addrList.resize(count);
            for (std::size_t index = 0; index != count; ++index) {
                std::uint32_t address;
                std::memcpy(&address, value->h_addr_list[index], sizeof(address));
                addrList[index] = ntohl(address);
            }
        }
        return true;
    };
#if defined(__linux__)
    hostent value;
    hostent* result;
    std::array<char, 256> initial;
    std::vector<char> expanded;
    char* buffer = initial.data();
    std::size_t size = initial.size();
    {
        TempCStringBuffer<char> text(host.view());
        for (;;) {
            result = &value;
            int hostError;
            if (::gethostbyname_r(text.c_str(), &value, buffer, size, &result, &hostError) != ERANGE) break;
            size *= 2;
            storage([&] { expanded.resize(size); });
            buffer = expanded.data();
        }
    }
    return populate(result);
#elif defined(_WIN32)
    hostent* result;
    { TempCStringBuffer<char> text(host.view()); result = ::gethostbyname(text.c_str()); }
    return populate(result);
#else
    // The selected non-reentrant POSIX branch locks the InternetHost ClassInfo.
    static std::mutex hostClassMutex;
    const std::lock_guard lock(hostClassMutex);
    hostent* result;
    { TempCStringBuffer<char> text(host.view()); result = ::gethostbyname(text.c_str()); }
    return populate(result);
#endif
}

InternetAddress::InternetAddress() { prepare<Address>(); prepare<InternetAddress>(); }
std::shared_ptr<InternetAddress> InternetAddress::createSourceEmpty() {
    return std::allocate_shared<InternetAddress>(::nicxlive::core::d_runtime::SourceAllocator<InternetAddress>{});
}
std::shared_ptr<InternetAddress> InternetAddress::createSource(RetainedString host, std::uint16_t port) {
    auto receiver = createSourceEmpty();
    receiver->construct(std::move(host), port);
    return receiver;
}
void InternetAddress::construct(RetainedString host, std::uint16_t port) {
    std::uint32_t address;
    {
        TempCStringBuffer<char> text(host.view());
        address = ntohl(::inet_addr(text.c_str()));
    }
    if (address == INADDR_NONE) {
        auto resolver = std::allocate_shared<InternetHost>(::nicxlive::core::d_runtime::SourceAllocator<InternetHost>{});
        if (!resolver->getHostByName(host)) {
            const auto message = storage([&] {
                return RetainedString("Unable to resolve host '" + std::string(host.view()) + "'");
            });
            throw AddressException(message, 1546);
        }
        address = resolver->addrList[0];
    }
    sin_.sin_family = AF_INET;
    sin_.sin_addr.s_addr = htonl(address);
    sin_.sin_port = htons(port);
}
std::uint16_t InternetAddress::port() const noexcept { return ntohs(sin_.sin_port); }
RetainedString InternetAddress::toAddrString() const { return copy(::inet_ntoa(sin_.sin_addr)); }
RetainedString InternetAddress::toPortString() const { return decimal(port()); }
bool InternetAddress::opEquals(std::shared_ptr<Address> other) const {
    const auto address = std::dynamic_pointer_cast<InternetAddress>(other);
    return address && sin_.sin_addr.s_addr == address->sin_.sin_addr.s_addr && sin_.sin_port == address->sin_.sin_port;
}
void Address::setNameLen(AddressLength length) {
    if (length != nameLen()) {
        const auto className = ::nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
        const auto message = storage([&] {
            return RetainedString(std::string(className.view()) + " expects address of length " +
                std::to_string(nameLen()) + ", not " + std::to_string(length));
        });
        throw AddressException(message, 1267, 0);
    }
}
RetainedString Address::toString() const {
    try {
        const auto host = toAddrString();
        const auto port = toPortString();
        return storage([&] {
            return RetainedString(host.view().find(':') == std::string_view::npos
                ? std::string(host.view()) + ':' + std::string(port.view())
                : '[' + std::string(host.view()) + "]:" + std::string(port.view()));
        });
    } catch (const SocketException&) { return literal("Unknown"); }
}

Socket::Socket() { prepare<Socket>(); }
TcpSocket::TcpSocket() { prepare<TcpSocket>(); }
void Socket::constructTcpIPv4() {
    family_ = AddressFamily::INET;
    auto handle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (handle == invalidSocket) throw SocketOSException(literal("Unable to create socket"), 2720);
    sock_ = handle;
#ifdef SO_NOSIGPIPE
    const int enabled = 1;
    if (::setsockopt(sock_, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) == -1)
        throw SocketOSException(literal("Unable to set socket option"), 3267);
#endif
}
std::shared_ptr<TcpSocket> TcpSocket::createSource() {
    auto receiver = std::allocate_shared<TcpSocket>(::nicxlive::core::d_runtime::SourceAllocator<TcpSocket>{});
    receiver->constructTcpIPv4();
    return receiver;
}
Socket::~Socket() noexcept { close(); }
void Socket::close() noexcept {
#ifdef _WIN32
    (void)::closesocket(sock_);
#else
    (void)::close(sock_);
#endif
    sock_ = invalidSocket;
}
void Socket::bind(const std::shared_ptr<Address>& address) {
    const auto handle = sock_;
    if (!address) ngFailNullAccess(false);
    const auto name = address->name();
    const auto length = address->nameLen();
    if (::bind(handle, name, length) == -1)
        throw SocketOSException(literal("Unable to bind socket"), 2863);
}
void Socket::connect(const std::shared_ptr<Address>& address) {
    const auto handle = sock_;
    if (!address) ngFailNullAccess(false);
    const auto name = address->name();
    const auto length = address->nameLen();
    if (::connect(handle, name, length) == -1) {
        const auto error = ngLastSocketError();
        // Only the blocking constructor/interface is exposed by this closure.
        throw SocketOSException(literal("Unable to connect socket"), 2895, error);
    }
}
void Socket::listen(int backlog) {
    if (::listen(sock_, backlog) == -1) throw SocketOSException(literal("Unable to listen on socket"), 2907);
}
std::shared_ptr<Address> Socket::localAddress() {
    auto address = InternetAddress::createSourceEmpty();
    AddressLength length = address->nameLen();
    if (::getsockname(sock_, address->name(), &length) == -1)
        throw SocketOSException(literal("Unable to obtain local socket address"), 3018);
    address->setNameLen(length);
#ifndef NDEBUG
    if (address->addressFamily() != family_)
        throw ngStaticError<AssertError>(literal("Assertion failure"), literal("std/socket.d"), 3020);
#endif
    return address;
}

#define NG_SOCKET_OBJECT_DEFAULTS(Type) \
std::size_t Type::toHash() const noexcept { \
    const auto value = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this)); \
    return value ^ (value >> 4); \
} \
bool Type::opEquals(std::shared_ptr<Type> other) const { \
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get()); \
} \
int Type::opCmp(std::shared_ptr<Type>) const { \
    auto name = ::nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)); \
    auto message = storage([&] { return RetainedString("need opCmp for class " + std::string(name.view())); }); \
    throw ThrowableException(message, literal("object.d"), 195); \
}
NG_SOCKET_OBJECT_DEFAULTS(Address)
NG_SOCKET_OBJECT_DEFAULTS(Socket)
NG_SOCKET_OBJECT_DEFAULTS(InternetHost)
#undef NG_SOCKET_OBJECT_DEFAULTS
RetainedString Socket::toString() const { return ::nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)); }
RetainedString InternetHost::toString() const { return ::nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)); }
}
