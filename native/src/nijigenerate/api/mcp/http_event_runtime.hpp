// Derived from vibe.http.server and vibe.core.core.
// Copyright 2012-2017, 2012-2020 Sönke Ludwig. MIT license.
// Eventcore socket/backlog mapping: Copyright (c) 2016-2018 Sönke Ludwig.
// NetworkAddress formatting: Copyright 2012-2016 Sönke Ludwig.
// Full license is retained in http_event_runtime.cpp.
#pragma once

#include <boost/asio.hpp>
#include "http_task_runtime.hpp"
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace nijigenerate::api::mcp::http_transport::detail {
class Connection;
class SourceHTTPEventRuntime;
class SourceHTTPServerContext;
compat::RetainedString sourceHTTPPeerAddress(const boost::asio::ip::tcp::endpoint& endpoint);

// The connection adapter must be independent of any one registration. It parses
// through the existing Connection/RequestTask engine, selecting a fresh retained
// defaultRequestHandler() for each request BEFORE reading/parsing can suspend.
using SourceHTTPRequestHandler = compat::SharedDelegate<void(std::shared_ptr<Connection>)>;
using SourceHTTPAccept = compat::SharedDelegate<void(boost::asio::ip::tcp::socket,
    std::shared_ptr<SourceHTTPServerContext>)>;
enum class SourceHTTPListenEvent { listening, stopped, failed };
using SourceHTTPListenLog = compat::SharedDelegate<void(SourceHTTPListenEvent, std::string_view, std::uint16_t)>;

class SourceHTTPListener {
    compat::Slice<std::size_t> ids_;
    explicit SourceHTTPListener(compat::Slice<std::size_t> ids) : ids_(std::move(ids)) {}
    friend class SourceHTTPEventRuntime;
public:
    SourceHTTPListener() = default;
    // Deliberately retains IDs after stopping. Like the source value, copies
    // refer to the same registrations; destruction does not stop a listener.
    void stopListening() const;
};

class SourceHTTPServerContext : public std::enable_shared_from_this<SourceHTTPServerContext> {
    friend class SourceHTTPEventRuntime;
    friend class SourceHTTPListener;
    struct VirtualHost { std::size_t id; SourceHTTPRequestHandler handler; std::uint16_t settingsPort; };
    std::shared_ptr<SourceHTTPEventRuntime> runtime_;
    boost::asio::ip::tcp::acceptor socket_;
    compat::RetainedString bindAddress_;
    std::uint16_t bindPort_;
    compat::Slice<VirtualHost> hosts_;
    SourceHTTPAccept accept_;
    SourceHTTPListenLog log_;
    struct PendingAccept { std::shared_ptr<SourceHTTPServerContext> receiver; };
    std::shared_ptr<PendingAccept> pendingAccept_;
    SourceHTTPServerContext(std::shared_ptr<SourceHTTPEventRuntime>, compat::RetainedString,
        std::uint16_t, SourceHTTPAccept, SourceHTTPListenLog);
    void acceptNext(bool initial = false);
    void closeListener();
    bool removeVirtualHost(std::size_t);
public:
    compat::RetainedString bindAddress() const noexcept { return bindAddress_; }
    std::uint16_t bindPort() const noexcept { return bindPort_; }
    // Retain the first fallback before header parsing; reconsider the current
    // empty-host registrations only after the actual Host validation point.
    SourceHTTPRequestHandler defaultRequestHandler() const;
    SourceHTTPRequestHandler selectRequestHandler(std::string_view rawHost,
        SourceHTTPRequestHandler fallback) const;
};

class SourceHTTPEventRuntime : public std::enable_shared_from_this<SourceHTTPEventRuntime> {
    friend class SourceHTTPServerContext;
    friend class SourceHTTPListener;
    boost::asio::io_context loop_;
    SourceTaskScheduler scheduler_{loop_};
    std::unique_ptr<boost::asio::signal_set> signals_;
    struct ExitWatch {
        bool active = true;
        boost::asio::cancellation_signal cancellation;
        std::shared_ptr<SourceNativeWait> registration;
        void cancel() {
            active = false;
            if (registration) registration->complete();
            cancellation.emit(boost::asio::cancellation_type::all);
        }
    };
    compat::Slice<std::shared_ptr<SourceHTTPServerContext>> listeners_;
    std::size_t nextHostID_ = 1;
    bool running_ = false;
    bool exit_ = false;
    std::optional<bool> registeredMain_;
    SourceHTTPEventRuntime() = default;
    void waitForSignal();
    void waitForExitSignal(const std::shared_ptr<ExitWatch>& watch);
public:
    static std::shared_ptr<SourceHTTPEventRuntime> current();
    static std::shared_ptr<SourceHTTPEventRuntime> existing() noexcept;
    boost::asio::io_context& context() noexcept { return loop_; }
    SourceTaskScheduler& scheduler() noexcept { return scheduler_; }
    SourceHTTPListener listen(compat::RetainedString bindAddress, std::uint16_t port,
        SourceHTTPRequestHandler, SourceHTTPAccept, SourceHTTPListenLog);
    int runEventLoop();
    void exitEventLoop();
    bool eventLoopRunning() const noexcept { return running_; }
    bool exitRequested() const noexcept { return exit_ || sourceTaskGlobalExitRequested(); }
    // Called by the TLS owner at actual thread teardown, breaking registry
    // ownership cycles without executing queued user continuations.
    void releaseThreadListeners() noexcept;
};
}
