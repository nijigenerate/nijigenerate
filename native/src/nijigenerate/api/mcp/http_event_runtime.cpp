// Derived from vibe.http.server and vibe.core.core.
// Copyright 2012-2017, 2012-2020 Sönke Ludwig.
// Eventcore socket/backlog mapping: Copyright (c) 2016-2018 Sönke Ludwig.
// NetworkAddress formatting: Copyright 2012-2016 Sönke Ludwig.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#include "http_event_runtime.hpp"
#include "http_request_url.hpp"
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/compat/phobos_unicode_exception.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <csignal>
#include <cstring>
#include <exception>
#include <new>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace nijigenerate::api::mcp::http_transport::detail {
namespace asio = boost::asio;
using Tcp = asio::ip::tcp;
namespace {
std::atomic_bool processTermination{false};
using SignalHandler = void (*)(int);
std::atomic<SignalHandler> interruptDriverHandler{nullptr};
std::atomic<SignalHandler> terminateDriverHandler{nullptr};
std::mutex signalRegistration;
constinit thread_local volatile std::sig_atomic_t hasThreadEventRuntime = 0;
constinit thread_local volatile std::sig_atomic_t directSignalExit = 0;
static_assert(std::atomic_bool::is_always_lock_free);
static_assert(std::atomic<SignalHandler>::is_always_lock_free);
void onSourceSignal(int signal) noexcept {
    // Mark the actual OS signal once, before Asio broadcasts the real signal
    // event. Per-context callbacks must not reassert a flag cleared by main.
    processTermination.store(true, std::memory_order_relaxed);
    if (hasThreadEventRuntime) directSignalExit = 1;
    const auto handler = (signal == SIGINT ? interruptDriverHandler : terminateDriverHandler)
        .load(std::memory_order_relaxed);
    if (handler) handler(signal);
    std::signal(signal, &onSourceSignal);
}
void armSourceSignalHandlers() {
    const std::lock_guard lock(signalRegistration);
    for (const auto signal : {SIGINT, SIGTERM}) {
        const auto previous = std::signal(signal, &onSourceSignal);
        if (previous != SIG_DFL && previous != SIG_IGN && previous != SIG_ERR && previous != &onSourceSignal)
            (signal == SIGINT ? interruptDriverHandler : terminateDriverHandler).store(previous);
    }
}
struct ThreadRuntimeOwner;
thread_local ThreadRuntimeOwner* existingRuntimeOwner = nullptr;
struct ThreadRuntimeOwner {
    // Register canonical OS-thread identity/TLS cleanup before this owner's
    // destructor, so real request stacks unwind before Error storage is freed.
    core::thread_context::ManagedThreadScope threadEntry;
    std::shared_ptr<SourceHTTPEventRuntime> value;
    ThreadRuntimeOwner() { existingRuntimeOwner = this; }
    ~ThreadRuntimeOwner() {
        if (value) value->releaseThreadListeners();
        hasThreadEventRuntime = 0;
        existingRuntimeOwner = nullptr;
    }
};
ThreadRuntimeOwner& threadRuntimeOwner() {
    static thread_local ThreadRuntimeOwner owner;
    return owner;
}
int backlogSize() noexcept {
    int backlog = 128;
#if defined(__linux__)
    if (auto* file = std::fopen("/proc/sys/net/core/somaxconn", "re")) {
        int value;
        if (std::fscanf(file, "%d", &value) == 1) backlog = value;
        std::fclose(file);
    }
#endif
    return backlog;
}
template<std::size_t N> auto literal(const char (&text)[N]) {
    return compat::RetainedString::fromStaticLiteral(text);
}

bool isMaybeIPAddress(std::string_view name) {
    // vibe.core.net selects numeric parsing before attempting any DNS lookup.
    if (name.find(':') != std::string_view::npos) return true;
    if (name.size() < 7) return false;
    if (name.find('.') == std::string_view::npos) return false;
    // std.algorithm.all auto-decodes this char range and stops at the first
    // failed predicate. Preserve its UTFException prefix on malformed UTF8.
    compat::phobos::detail::UnicodeCursor<std::string_view, char> cursor(name);
    while (!cursor.empty()) {
        const auto value = cursor.front();
        if (!((value >= '0' && value <= '9') || value == '.')) return false;
        cursor.popFront();
    }
    return true;
}
#ifdef _WIN32
std::wstring windowsHostName(std::string_view input) {
    using Cursor = compat::phobos::detail::UnicodeCursor<std::string_view, char>;
    Cursor count(input);
    std::size_t units = 0;
    while (!count.empty()) { units += count.front() < 0x10000 ? 1 : 2; count.popFront(); }
    std::wstring result(units, wchar_t{});
    Cursor cursor(input);
    std::size_t offset = 0;
    while (!cursor.empty()) {
        auto scalar = cursor.front(); cursor.popFront();
        if (scalar < 0x10000) result[offset++] = static_cast<wchar_t>(scalar);
        else {
            scalar -= 0x10000;
            result[offset++] = static_cast<wchar_t>(0xd800 + (scalar >> 10));
            result[offset++] = static_cast<wchar_t>(0xdc00 + (scalar & 0x3ff));
        }
    }
    return result;
}
std::optional<Tcp::endpoint> windowsAddress(const std::wstring& host, bool numeric, std::uint16_t port) {
    if (!numeric) {
        for (const auto family : {AF_INET, AF_INET6}) {
            SOCKADDR_STORAGE storage{};
            int length = sizeof(storage);
            if (WSAStringToAddressW(const_cast<wchar_t*>(host.c_str()), family, nullptr,
                    reinterpret_cast<sockaddr*>(&storage), &length) != 0) continue;
            Tcp::endpoint endpoint;
            std::memcpy(endpoint.data(), &storage, static_cast<std::size_t>(length));
            endpoint.resize(static_cast<std::size_t>(length));
            endpoint.port(port);
            return endpoint;
        }
    }
    ADDRINFOW hints{};
    hints.ai_flags = AI_NUMERICHOST;
    ADDRINFOW* result = nullptr;
    if (GetAddrInfoW(host.c_str(), nullptr, numeric ? &hints : nullptr, &result)) return {};
    const std::unique_ptr<ADDRINFOW, decltype(&FreeAddrInfoW)> addresses(result, &FreeAddrInfoW);
    for (auto* current = result; current; current = current->ai_next) {
        if (current->ai_family != AF_INET && current->ai_family != AF_INET6) continue;
        Tcp::endpoint endpoint;
        std::memcpy(endpoint.data(), current->ai_addr, current->ai_addrlen);
        endpoint.resize(current->ai_addrlen);
        endpoint.port(port);
        return endpoint;
    }
    return {};
}
#endif
template<class T> compat::Slice<T> without(const compat::Slice<T>& values, std::size_t index) {
    const auto left = values.slice(0, index);
    const auto right = values.slice(index + 1, values.size());
    const auto count = left.size() + right.size();
    if (!count) return {};
    compat::Slice<T> result{std::vector<T>(count)};
    std::size_t output = 0;
    for (const auto& value : left) result[output++] = value;
    for (const auto& value : right) result[output++] = value;
    return result;
}
}

compat::RetainedString sourceHTTPPeerAddress(const Tcp::endpoint& endpoint) {
    std::string output;
    const auto number = [&](unsigned value, int radix = 10) {
        char buffer[12];
        const auto end = std::to_chars(buffer, buffer + sizeof(buffer), value, radix).ptr;
        output.append(buffer, end);
    };
    const auto ipv4 = [&](const auto& bytes, std::size_t offset) {
        for (std::size_t i = 0; i < 4; ++i) {
            if (i) output += '.';
            number(bytes[offset + i]);
        }
    };
    if (endpoint.address().is_v4()) {
        ipv4(endpoint.address().to_v4().to_bytes(), 0);
    } else {
        const auto bytes = endpoint.address().to_v6().to_bytes();
        std::array<std::uint16_t, 8> words;
        int bestBase = -1, bestLength = 0, base = -1, length = 0;
        for (int i = 0; i < 8; ++i) {
            words[i] = static_cast<std::uint16_t>((unsigned(bytes[2 * i]) << 8) | bytes[2 * i + 1]);
            if (words[i] == 0) {
                if (base == -1) { base = i; length = 1; }
                else ++length;
            } else if (base != -1) {
                if (bestBase == -1 || length > bestLength) { bestBase = base; bestLength = length; }
                base = -1;
            }
            if (base != -1 && (bestBase == -1 || length > bestLength)) {
                bestBase = base; bestLength = length;
            }
            if (bestLength < 2) bestBase = -1;
        }
        output += '[';
        for (int i = 0; i < 8; ++i) {
            if (bestBase != -1 && i >= bestBase && i < bestBase + bestLength) {
                if (i == bestBase || i == 7) output += ':';
                continue;
            }
            if (i) output += ':';
            if (i == 6 && bestBase == 0 &&
                    (bestLength == 6 || (bestLength == 5 && words[5] == 0xffff))) {
                ipv4(bytes, 12);
                break;
            }
            number(words[i], 16);
        }
        output += ']';
    }
    output += ':';
    number(endpoint.port());
    return compat::RetainedString(std::move(output));
}

std::shared_ptr<SourceHTTPEventRuntime> SourceHTTPEventRuntime::current() {
    auto& owner = threadRuntimeOwner();
    if (!owner.value) owner.value.reset(new SourceHTTPEventRuntime);
    hasThreadEventRuntime = 1;
    return owner.value;
}

std::shared_ptr<SourceHTTPEventRuntime> SourceHTTPEventRuntime::existing() noexcept {
    return existingRuntimeOwner ? existingRuntimeOwner->value : nullptr;
}

SourceHTTPServerContext::SourceHTTPServerContext(std::shared_ptr<SourceHTTPEventRuntime> runtime,
    compat::RetainedString address, std::uint16_t port, SourceHTTPAccept accept, SourceHTTPListenLog log)
    : runtime_(std::move(runtime)), socket_(runtime_->context()), bindAddress_(std::move(address)),
      bindPort_(port), accept_(std::move(accept)), log_(std::move(log)) {}

SourceHTTPListener SourceHTTPEventRuntime::listen(compat::RetainedString address, std::uint16_t port,
    SourceHTTPRequestHandler handler, SourceHTTPAccept accept, SourceHTTPListenLog log) {
    auto found = std::find_if(listeners_.begin(), listeners_.end(), [&](const auto& item) {
        return item->bindAddress_.view() == address.view() && item->bindPort_ == port;
    });
    std::shared_ptr<SourceHTTPServerContext> server;
    if (found != listeners_.end()) server = *found;
    else {
        auto candidate = std::shared_ptr<SourceHTTPServerContext>(new SourceHTTPServerContext(
            shared_from_this(), std::move(address), port, std::move(accept), std::move(log)));
        try { compat::ngRunWithScopeExit([&] {
            if (candidate->bindAddress_.empty()) throw compat::ThrowableException(
                literal("Host name must not be empty."), literal("vibe/core/net.d"), 63);
            const auto addressText = std::string(candidate->bindAddress_.view());
            Tcp::resolver resolver(loop_);
            std::optional<Tcp::endpoint> resolved;
#ifdef _WIN32
            if (isMaybeIPAddress(addressText)) {
                resolved = windowsAddress(windowsHostName(addressText), true, port);
            } else {
                // Selected WinAPI DNS calls GetAddrInfoW synchronously and
                // invokes its real callback before lookupHost returns.
                RequestTask::current().await([&](auto done) {
                    std::wstring name;
                    try { name = windowsHostName(addressText); }
                    catch (const compat::UTFException&) {
                        // The selected driver returns an invalid lookup handle
                        // here without invoking the supplied completion.
                        return;
                    }
                    resolved = windowsAddress(name, false, port);
                    done(Error{});
                });
            }
#else
            auto flags = Tcp::resolver::numeric_service;
            Tcp::resolver::results_type addresses;
            if (isMaybeIPAddress(addressText)) {
                flags |= Tcp::resolver::numeric_host;
                addresses = resolver.resolve(addressText, std::to_string(port), flags);
            } else {
                auto result = std::make_shared<Tcp::resolver::results_type>();
#if defined(__linux__)
                // Selected Eventcore passes null hints for AF_UNSPEC. glibc's
                // null-hints defaults include these flags; Asio passes hints.
                flags |= Tcp::resolver::address_configured | Tcp::resolver::v4_mapped;
#endif
                RequestTask::current().await([&](auto done) {
                    const auto cancellation = asio::get_associated_cancellation_slot(done);
                    resolver.async_resolve(addressText, std::to_string(port), flags,
                        asio::bind_cancellation_slot(cancellation,
                        [result, done = std::move(done)](Error error, Tcp::resolver::results_type found) {
                            if (!error) *result = std::move(found);
                            done(Error{});
                        }));
                });
                addresses = std::move(*result);
            }
            if (!addresses.empty()) resolved = addresses.begin()->endpoint();
#endif
            if (!resolved) throw compat::ThrowableException(
                compat::RetainedString("Failed to lookup host '" + addressText + "'."),
                literal("vibe/core/net.d"), 105);
            const auto endpoint = *resolved;
            candidate->socket_.open(endpoint.protocol());
#if !defined(_WIN32)
            candidate->socket_.non_blocking(true);
#endif
            candidate->socket_.set_option(asio::socket_base::reuse_address(true));
            candidate->socket_.bind(endpoint);
            candidate->socket_.listen(backlogSize());
            candidate->acceptNext(true);
            if (!port) candidate->bindPort_ = candidate->socket_.local_endpoint().port();
            if (candidate->log_) candidate->log_(SourceHTTPListenEvent::listening,
                candidate->bindAddress_.view(), candidate->bindPort_);
            server = std::move(candidate);
        }, [&] {
            if (!candidate) return;
            Error ignored;
            candidate->socket_.close(ignored);
            if (const auto operation = std::exchange(candidate->pendingAccept_, {})) operation->receiver.reset();
        }); } catch (const std::bad_alloc&) { throw; }
        catch (const std::exception&) {
            // listenHTTPPlain catches per-interface ordinary failures, then
            // performs the final enforce below. Canonical Error escapes.
            if (candidate->log_) candidate->log_(SourceHTTPListenEvent::failed,
                candidate->bindAddress_.view(), candidate->bindPort_);
        }
        if (server) {
            try { listeners_.append(server); }
            catch (...) {
                Error ignored;
                server->socket_.close(ignored);
                if (const auto operation = std::exchange(server->pendingAccept_, {})) operation->receiver.reset();
                throw;
            }
        }
    }
    if (!server) throw compat::ThrowableException(literal(
        "Failed to listen for incoming HTTP connections on any of the supplied interfaces."),
        literal("vibe/http/server.d"), 1967);
    const auto id = nextHostID_++;
    server->hosts_.append({id, std::move(handler), port});
    return SourceHTTPListener(compat::Slice<std::size_t>{id});
}

void SourceHTTPServerContext::acceptNext(bool initial) {
#ifndef _WIN32
    if (initial) {
        // Selected POSIX waitForConnections calls onAccept once immediately,
        // before listenTCP returns and before this context has virtual hosts.
        Error error;
        auto socket = socket_.accept(error);
        if (!error) accept_(std::move(socket), shared_from_this());
    }
#else
    (void)initial;
#endif
    const auto operation = std::make_shared<PendingAccept>();
    operation->receiver = shared_from_this();
    pendingAccept_ = operation;
    try {
        socket_.async_accept(runtime_->scheduler().trackNativeCompletion(
            [operation](boost::system::error_code error, Tcp::socket socket) {
                const auto self = std::exchange(operation->receiver, {});
                if (!self) return; // The real listening operation was closed/cancelled.
                if (self->pendingAccept_ == operation) self->pendingAccept_.reset();
                if (!error) self->accept_(std::move(socket), self);
                if (self->socket_.is_open()) self->acceptNext();
            }));
    } catch (...) {
        operation->receiver.reset();
        if (pendingAccept_ == operation) pendingAccept_.reset();
        throw;
    }
}

void SourceHTTPServerContext::closeListener() {
    socket_.close();
    // Cancellation completion can be queued beyond the last loop turn. Keep
    // its actual operation state alive, but detach the ended receiver explicitly.
    if (const auto operation = std::exchange(pendingAccept_, {})) operation->receiver.reset();
}

SourceHTTPRequestHandler SourceHTTPServerContext::defaultRequestHandler() const {
    if (hosts_.empty()) return {};
    return hosts_[0].handler;
}

SourceHTTPRequestHandler SourceHTTPServerContext::selectRequestHandler(std::string_view rawHost,
    SourceHTTPRequestHandler fallback) const {
    // The existing Host validator ran immediately before this hook. All used
    // settings have empty hostName, so only an empty parsed host can match.
    std::uint16_t port = 0;
    if (!rawHost.empty()) {
        if (!rawHost.starts_with("[]")) return fallback;
        rawHost.remove_prefix(2);
        if (rawHost.starts_with(':')) port = urlPort(rawHost.substr(1));
    }
    const auto visited = hosts_;
    for (const auto& host : visited) {
        if (!port || port == host.settingsPort) return host.handler;
    }
    return fallback;
}

bool SourceHTTPServerContext::removeVirtualHost(std::size_t id) {
    for (std::size_t i = 0; i < hosts_.size(); ++i) {
        if (hosts_[i].id != id) continue;
        const auto retained = hosts_[i];
        hosts_ = without(hosts_, i);
        return true;
    }
    return false;
}

void SourceHTTPListener::stopListening() const {
    if (ids_.empty()) return;
    const auto runtime = SourceHTTPEventRuntime::existing();
    if (!runtime) return;
    for (const auto id : ids_) {
        const auto visited = runtime->listeners_;
        for (std::size_t i = 0; i < visited.size(); ++i) {
            const auto server = visited[i];
            if (!server->removeVirtualHost(id)) continue;
            if (server->hosts_.empty()) {
                server->closeListener();
                if (server->log_) server->log_(SourceHTTPListenEvent::stopped,
                    server->bindAddress_.view(), server->bindPort_);
                runtime->listeners_ = without(runtime->listeners_, i);
            }
            break;
        }
    }
}

int SourceHTTPEventRuntime::runEventLoop() {
    registeredMain_.reset();
    try { registeredMain_ = core::thread_context::ngIsMainThread(); }
    catch (const std::logic_error&) {
        // An embedding host has not registered its UI/main thread. Preserve
        // that unknown identity; never infer main from the first HTTP caller.
    }
    if (!signals_) {
        signals_ = std::make_unique<asio::signal_set>(loop_, SIGINT, SIGTERM);
#ifndef _WIN32
        signals_->add(SIGPIPE);
#endif
        waitForSignal();
    }
    armSourceSignalHandlers();
    running_ = true;
    scheduler_.running(true);
    struct Cleanup {
        SourceHTTPEventRuntime& runtime;
        std::optional<bool> registeredMain;
        std::shared_ptr<ExitWatch> exitWatch;
        ~Cleanup() {
            if (exitWatch) exitWatch->cancel();
            runtime.running_ = false; runtime.exit_ = false;
            runtime.scheduler_.running(false); runtime.scheduler_.clearExit();
            directSignalExit = 0;
            if (registeredMain && *registeredMain) processTermination.store(false);
        }
    } cleanup{*this, registeredMain_, {}};
#ifndef NDEBUG
    if (exit_) throw compat::ngStaticError<compat::AssertError>(literal("Exit flag set before event loop has started."),
        literal("vibe/core/core.d"), 204);
#endif
    exit_ = false;
    // core.d performs the initial ready-task/driver pass before constructing
    // the main-thread exit watcher. That pass can itself request loop exit.
    (void)scheduler_.process();
    if (exit_ || sourceTaskGlobalExitRequested()) return 0;
    if (registeredMain_ && *registeredMain_) {
        cleanup.exitWatch = std::make_shared<ExitWatch>();
        waitForExitSignal(cleanup.exitWatch);
    }
    // Each real driver completion returns control here. Unlike io_context.stop,
    // this exit flag can be cleared by a nested source loop's scope cleanup.
    for (;;) {
        const auto result = scheduler_.waitAndProcess();
        if (exit_ || result == SourceTaskExit::exited || result == SourceTaskExit::outOfWaiters) break;
    }
    return 0;
}

void SourceHTTPEventRuntime::waitForSignal() {
    signals_->async_wait([this](boost::system::error_code error, int signal) {
        if (error) return;
#ifndef _WIN32
        if (signal == SIGPIPE) { waitForSignal(); return; }
#endif
        // Source watchExitFlag is installed only by the actual main thread.
        // Other threads observe their own driver exit or transport close.
        if (registeredMain_ && *registeredMain_ && sourceTaskGlobalExitRequested()) {
            exit_ = true;
            scheduler_.requestExit();
        }
        waitForSignal();
    });
    // Only the source main-thread watchExitFlag keeps an event loop alive.
    // Other signal registrations wake real I/O but are not application waiters.
}

void SourceHTTPEventRuntime::waitForExitSignal(const std::shared_ptr<ExitWatch>& watch) {
    watch->registration = scheduler_.beginNativeWait();
    signals_->async_wait(asio::bind_cancellation_slot(watch->cancellation.slot(),
        [this, watch, registration = watch->registration](Error error, int) {
            registration->complete();
            if (error || !watch->active) return;
            if (sourceTaskGlobalExitRequested()) {
                exit_ = true;
                scheduler_.requestExit();
                return;
            }
            waitForExitSignal(watch);
        }));
}

void SourceHTTPEventRuntime::exitEventLoop() {
#ifndef NDEBUG
    if (!running_) throw compat::ngStaticError<compat::AssertError>(literal("Exiting event loop when none is running."),
        literal("vibe/core/core.d"), 253);
#endif
    exit_ = true;
    scheduler_.requestExit();
}

void SourceHTTPEventRuntime::releaseThreadListeners() noexcept {
    if (signals_) {
        boost::system::error_code ignored;
        signals_->cancel(ignored);
        signals_.reset();
        armSourceSignalHandlers();
    }
    for (const auto& server : listeners_) {
        boost::system::error_code ignored;
        server->socket_.close(ignored);
        if (const auto operation = std::exchange(server->pendingAccept_, {})) operation->receiver.reset();
        server->hosts_ = {};
        server->runtime_.reset();
    }
    listeners_.clear();
    scheduler_.shutdown();
}

SourceTaskScheduler& sourceTaskSchedulerForThread() {
    return SourceHTTPEventRuntime::current()->scheduler();
}
bool sourceTaskGlobalExitRequested() noexcept { return processTermination.load(); }
bool sourceTaskDriverExitRequested() noexcept { return directSignalExit; }
SourceTaskScheduler& sourceTaskSchedulerFor(asio::io_context& loop) {
    auto& scheduler = sourceTaskSchedulerForThread();
    if (&scheduler.context() != &loop) throw std::logic_error("Task event context belongs to another thread");
    return scheduler;
}
}
