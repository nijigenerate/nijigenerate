#pragma once
// HTTP/1 request/exchange integration derives from vibe.http.internal.http1.server.
// Copyright 2012-2024 Sönke Ludwig; authors Sönke Ludwig, Jan Krüger, Ilya Shipunov.
// Eventcore socket read/write state: Copyright (c) 2016-2018 Sönke Ludwig.
// MIT license; full text is retained in port/licenses/mcp-http-transport.txt.

#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include "http_request_url.hpp"
#include "http_request_header.hpp"
#include "http_chunk_writer.hpp"
#include "http_auth_time.hpp"
#include "http_task_runtime.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/context/detail/exception.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <chrono>
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <limits>
#include <new>
#include <stdexcept>
#include <span>
#include <string>
#include <system_error>
#include <utility>

namespace nijigenerate::api::mcp::http_transport::detail {
namespace asio = boost::asio;
namespace http = boost::beast::http;
using Tcp = asio::ip::tcp;
using Error = boost::system::error_code;
struct IdleTimeout {};

inline bool sourceSocketHalfClose(Error error) noexcept {
#ifdef _WIN32
    return error == asio::error::shut_down || error.value() == WSAEDISCON;
#else
    return error == asio::error::shut_down;
#endif
}

inline bool sourceSocketDisconnected(Error error) noexcept {
    if (sourceSocketHalfClose(error) || error == asio::error::connection_reset ||
            error == asio::error::network_reset || error == asio::error::timed_out) return true;
#ifdef _WIN32
    return error == asio::error::connection_aborted;
#else
    return error == asio::error::broken_pipe || error == asio::error::not_connected;
#endif
}

// The source captures UTC time before parsing, then caches the formatted date
// per thread until strictly after the next second. Calendar arithmetic avoids
// platform-specific gmtime variants and locale-dependent date names.
inline const std::string& responseDate(SourceSysTime time) {
    using namespace std::chrono;
    static thread_local auto nextUpdate = SourceSysTime::fromStdTime(std::numeric_limits<std::int64_t>::min());
    static thread_local std::string cached;
    if (time <= nextUpdate) return cached;
    static constexpr const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static constexpr const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    // Split before changing epochs: subtracting the Unix epoch from an extreme
    // signed stdTime would overflow even though its calendar day is representable.
    constexpr std::int64_t ticksPerSecond = 10'000'000;
    constexpr std::int64_t ticksPerDay = 86400 * ticksPerSecond;
    auto dayIndex = time.stdTime() / ticksPerDay;
    auto dayTicks = time.stdTime() % ticksPerDay;
    if (dayTicks < 0) { --dayIndex; dayTicks += ticksPerDay; }
    const auto date = sys_days{year{1}/January/1} + std::chrono::days(dayIndex);
    const year_month_day calendar(date);
    const hh_mm_ss clock{seconds(dayTicks / ticksPerSecond)};
    const auto decimal2 = [](unsigned number) {
        return std::string{char('0' + number / 10), char('0' + number % 10)};
    };
    auto rendered = std::string(days[weekday(date).c_encoding()]) + ", " + decimal2(unsigned(calendar.day()))
        + " " + months[unsigned(calendar.month()) - 1] + " " + std::to_string(int(calendar.year()))
        + " " + decimal2(unsigned(clock.hours().count())) + ":" + decimal2(unsigned(clock.minutes().count()))
        + ":" + decimal2(unsigned(clock.seconds().count())) + " GMT";
    // CacheTime.update adds one second before clearing fractional seconds.
    const auto next = time + seconds(1);
    auto fraction = next.stdTime() % ticksPerSecond;
    if (fraction < 0) fraction += ticksPerSecond;
    nextUpdate = SourceSysTime::fromStdTime(std::bit_cast<std::int64_t>(
        static_cast<std::uint64_t>(next.stdTime()) - static_cast<std::uint64_t>(fraction)));
    cached = std::move(rendered);
    return cached;
}


class SourceHTTPOutputStream {
public:
    virtual ~SourceHTTPOutputStream() = default;
    virtual void write(std::string_view bytes) = 0;
    virtual void flush() = 0;
    void writeEvent(std::string bytes) { write(bytes); flush(); }
};
// The selected nullSink is one real NullOutputStream per OS thread. In
// particular, all HEAD responses share its identity rather than their socket.
class SourceNullOutputStream final : public SourceHTTPOutputStream {
public:
    void write(std::string_view) override {}
    void flush() override {}
};
inline std::shared_ptr<SourceHTTPOutputStream> sourceNullOutputStream() {
    static thread_local auto sink = std::make_shared<SourceNullOutputStream>();
    return sink;
}

// Response fields retain InetHeaderMap insertion order and counted bytes.
class SourceResponseHeaders {
    SourceHTTPHeaders fields_;
    static std::string_view name(http::field field) {
        const auto text = http::to_string(field);
        return {text.data(), text.size()};
    }
public:
    bool contains(http::field field) const { return fields_.find(name(field)) != nullptr; }
    void set(http::field field, std::string_view value) {
        fields_.setFirst(compat::RetainedString(std::string(name(field))),
            compat::RetainedString(std::string(value)));
    }
    void erase(http::field field) {
        SourceHTTPHeaders remaining;
        bool removed = false;
        fields_.forEach([&](UrlString key, UrlString value) {
            if (!removed && request_header_source::equalName(key.view(), name(field))) removed = true;
            else remaining.addField(key, value);
        });
        fields_ = std::move(remaining);
    }
    void clear() { fields_ = {}; }
    template<class Visitor> void forEach(Visitor&& visitor) const {
        fields_.forEach(std::forward<Visitor>(visitor));
    }
};

// Real socket write state shared by heartbeat and broadcast tasks.
class Connection : public std::enable_shared_from_this<Connection> {
    const std::thread::id socketThread_ = std::this_thread::get_id();
    // Only the accepted-connection states are used; actual I/O completions,
    // not the native descriptor's open bit, drive the source state transitions.
    enum class SocketState { connected, passiveClose, activeClose, closed };
    SocketState socketState_ = SocketState::connected;
#ifdef _WIN32
    // The TCPConnection owner, pending driver operations and waitForDataEx
    // scopes hold distinct references to the same actual socket registration.
    std::size_t windowsSocketReferences_ = 1;
    bool windowsSocketReleased_ = false;
    void retainWindowsSocket() noexcept { ++windowsSocketReferences_; }
    bool releaseWindowsSocket() {
        if (--windowsSocketReferences_) return true;
        if (auto read = std::exchange(activeRead_, {})) detachRead(read);
        if (auto write = std::exchange(activeWrite_, {})) {
            write->cancellation.clear();
            write->complete = {};
        }
        Error ignored;
        socket.cancel(ignored);
        socket.close(ignored);
        return false;
    }
#endif
#ifndef _WIN32
    std::size_t posixPendingReadBytes_ = 0;
#endif
    struct Write {
        std::string bytes;
        std::function<void(Error)> complete;
        asio::cancellation_slot cancellation;
#ifndef _WIN32
        std::size_t offset = 0;
        std::shared_ptr<asio::cancellation_signal> readinessCancellation;
#endif
    };
    std::shared_ptr<Write> activeWrite_;
    struct Read {
        std::array<char, 4096> bytes;
        std::function<void(Error, std::size_t)> complete;
        asio::cancellation_slot cancellation;
        std::shared_ptr<asio::cancellation_signal> nativeCancellation;
    };
    std::shared_ptr<Read> activeRead_;
    struct HeaderInput {
        Connection& connection;
        std::span<const std::uint8_t> peek() {
            const auto bytes = connection.bufferedInput();
            return {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
        }
        void readExact(std::span<std::uint8_t> bytes) {
            connection.readPayload(reinterpret_cast<char*>(bytes.data()), bytes.size());
        }
        bool empty() {
            if (connection.input.size()) return false;
            return !connection.fillInput();
        }
        void skip(std::size_t count) {
            std::array<std::uint8_t, 256> buffer;
            while (count) {
                const auto amount = std::min(count, buffer.size());
                readExact(std::span(buffer).first(amount));
                count -= amount;
            }
        }
    };
    struct ChunkOutput {
        Connection* connection;
        void writeExact(std::span<const std::uint8_t> bytes) {
            connection->writeBytes(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        }
        // The selected TCPConnection.flush has no buffered-output operation.
        void flush() noexcept {}
    };
    std::shared_ptr<SourceChunkedOutputStream<ChunkOutput>> chunkOutput_;
    class RetainedOutputStream final : public SourceHTTPOutputStream {
        std::shared_ptr<Connection> connection_;
        std::shared_ptr<SourceChunkedOutputStream<ChunkOutput>> chunk_;
    public:
        RetainedOutputStream(std::shared_ptr<Connection> connection,
            std::shared_ptr<SourceChunkedOutputStream<ChunkOutput>> chunk)
            : connection_(std::move(connection)), chunk_(std::move(chunk)) {}
        void write(std::string_view bytes) override {
            if (chunk_) chunk_->write(bytes);
            else connection_->writeBytes(std::string(bytes));
        }
        void flush() override { if (chunk_) chunk_->flush(); }
    };
    bool bodyRead_ = false;
    bool headResponse_ = false;
    unsigned responseVersion_ = 11;
    bool sourceChunked_ = false;
    std::uint64_t chunkRemaining_ = 0;
    std::uint64_t decodedBodySize_ = 0;
    std::uint64_t fixedBodySize_ = 0;
    std::string_view bufferedInput() const {
        const auto bytes = input.data();
        return {static_cast<const char*>(bytes.data()), bytes.size()};
    }
    bool fillInput() {
        if (!isOpen()) return false;
        withReadSocketReference([&] { RequestTask::current().await([&](auto done) {
            const auto cancellation = asio::get_associated_cancellation_slot(done);
            readSome(asio::bind_cancellation_slot(cancellation,
                [done = std::move(done)](Error, std::size_t) { done(Error{}); }));
        }); });
        return isOpen() && input.size() != 0;
    }
    void readInput() {
        if (!fillInput()) throw compat::ThrowableException(
            compat::RetainedString::fromStaticLiteral("Reached end of stream while reading data."),
            compat::RetainedString::fromStaticLiteral("vibe/core/net.d"), 911);
    }
    UrlString readInputLine() {
        HeaderInput reader{*this};
        return request_header_source::readLine(reader, "\r\n", std::numeric_limits<std::size_t>::max());
    }
    void readChunkFooter(std::size_t line) {
        std::array<char, 2> bytes;
        readPayload(bytes.data(), bytes.size());
        const bool valid = bytes[0] == '\r' && bytes[1] == '\n';
        if (!valid) throw HTTPStatusException(400,
            compat::RetainedString::fromStaticLiteral("Bad Request"), line,
            compat::RetainedString::fromStaticLiteral("vibe/http/common.d"));
    }
    void readChunkHeader() {
        const auto line = readInputLine();
        chunkRemaining_ = requestChunkLength(line.view());
        if (!chunkRemaining_) readChunkFooter(459);
    }
    void readPayload(char* destination, std::size_t count) {
        while (count) {
            if (input.size() == 0) readInput();
            const auto bytes = bufferedInput();
            const auto size = std::min(count, bytes.size());
            std::copy_n(bytes.data(), size, destination);
            input.consume(size);
            destination += size;
            count -= size;
        }
    }
    void readChunkedBody(bool retain) {
        // Sequential pipe allocates one 64 KiB buffer before the first read.
        std::unique_ptr<char[]> discard;
        if (!retain && chunkRemaining_) discard.reset(new char[64 * 1024]);
        while (chunkRemaining_) {
            // readAll reserves the underlying leastSize before LimitedInputStream
            // rejects a read exceeding its remaining decoded-byte allowance.
            if (retain) body.reserve(body.size()
                + static_cast<std::size_t>(std::min<std::uint64_t>(chunkRemaining_,
                    std::numeric_limits<std::size_t>::max() - body.size())));
            while (chunkRemaining_) {
                const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(chunkRemaining_,
                    retain ? std::numeric_limits<std::size_t>::max() : 64 * 1024));
                // LimitedInputStream checks the complete IOMode.all request
                // before consuming even its first socket-buffer fragment.
                if (count > 2097152 - decodedBodySize_)
                    throw HTTPStatusException(413,
                        compat::RetainedString::fromStaticLiteral("Request Entity Too Large"), 1838,
                        compat::RetainedString::fromStaticLiteral("vibe/http/server.d"));
                if (retain) {
                    const auto offset = body.size();
                    body.resize(offset + count);
                    readPayload(body.data() + offset, count);
                } else readPayload(discard.get(), count);
                chunkRemaining_ -= count;
                decodedBodySize_ += count;
            }
            // ChunkedInputStream.read advances the next header before returning
            // the completed chunk, including the final zero chunk's own footer.
            readChunkFooter(433);
            readChunkHeader();
        }
    }
#ifndef _WIN32
    void continuePosixWrite(const std::shared_ptr<Write>& write, bool first) {
        Error error;
        const auto remaining = write->bytes.size() - write->offset;
        const auto count = std::min(remaining, static_cast<std::size_t>(std::numeric_limits<int>::max()));
        const auto transferred = socket.write_some(asio::buffer(write->bytes.data() + write->offset, count), error);
        if (error == asio::error::would_block || error == asio::error::in_progress) error.clear();
        else if (error || transferred == remaining) {
            observeWrite(error);
            // The deferred Eventcore error callback reports its read slot's
            // count; the initial immediate-send error instead reports zero.
            if (!first && sourceSocketDisconnected(error) && posixPendingReadBytes_ == write->bytes.size())
                error.clear();
            if (activeWrite_ == write) activeWrite_.reset();
            write->cancellation.clear();
            auto done = std::exchange(write->complete, {});
            if (done) done(error);
            return;
        }
        write->offset += transferred;
        if (first) {
            // POSIX replaces its one pending notification only after the
            // immediate send leaves bytes. An older waiter is not completed.
            if (auto previous = std::exchange(activeWrite_, write)) {
                auto cancellation = std::exchange(previous->cancellation, asio::cancellation_slot{});
                cancellation.clear();
                previous->complete = {};
                if (previous->readinessCancellation)
                    previous->readinessCancellation->emit(asio::cancellation_type::all);
            }
            write->readinessCancellation = std::make_shared<asio::cancellation_signal>();
            if (write->cancellation.is_connected()) {
                write->cancellation.assign([signal = write->readinessCancellation](asio::cancellation_type type) {
                    signal->emit(type);
                });
            }
        }
        socket.async_wait(Tcp::socket::wait_write,
            asio::bind_cancellation_slot(write->readinessCancellation->slot(),
            [self = shared_from_this(), write](Error error) {
                if (self->activeWrite_ != write) return;
                if (!error) { self->continuePosixWrite(write, false); return; }
                self->observeWrite(error);
                self->activeWrite_.reset();
                write->cancellation.clear();
                auto done = std::exchange(write->complete, {});
                if (done) done(error);
            }));
    }
#endif
#ifdef _WIN32
    void beginWindowsWrite(const std::shared_ptr<Write>& write) {
        activeWrite_ = write;
        retainWindowsSocket();
        try {
            auto finish = [self = shared_from_this(), write,
                    retainedCompletion = write->complete](Error error, std::size_t transferred) {
                    (void)retainedCompletion; // Keep the real cancellation signal alive after scope cancellation.
                    if (!write->complete) {
                        // The notification was detached, but its actual native
                        // completion still ends this operation's buffer owner.
                        if (self->activeWrite_ == write) self->activeWrite_.reset();
                        self->releaseWindowsSocket();
                        return;
                    }
                    self->observeWrite(error);
                    if (sourceSocketDisconnected(error) && transferred == write->bytes.size()) error.clear();
                    if (self->activeWrite_ == write) self->activeWrite_.reset();
                    auto complete = std::exchange(write->complete, {});
                    write->cancellation.clear();
                    if (self->releaseWindowsSocket() && complete) complete(error);
                };
            asio::async_write(socket, asio::buffer(write->bytes),
                asio::bind_cancellation_slot(write->cancellation, std::move(finish)));
        } catch (...) {
            if (activeWrite_ == write) activeWrite_.reset();
            write->cancellation.clear();
            write->complete = {};
            releaseWindowsSocket();
            throw;
        }
    }
#endif
    void detachRead(const std::shared_ptr<Read>& read) {
        auto cancellation = std::exchange(read->cancellation, asio::cancellation_slot{});
        cancellation.clear();
        read->complete = {};
        if (read->nativeCancellation) read->nativeCancellation->emit(asio::cancellation_type::all);
    }
public:
    bool isOpen() const noexcept {
#ifdef _WIN32
        if (windowsSocketReleased_) return false;
#endif
        return socket.is_open();
    }
    template<class Body> void withReadSocketReference(Body&& body) {
#ifdef _WIN32
        retainWindowsSocket();
        compat::ngRunWithScopeExit(std::forward<Body>(body), [&] { releaseWindowsSocket(); });
#else
        std::forward<Body>(body)();
#endif
    }
    template<class Completion> void readSome(Completion completion) {
        if (!isOpen()) { completion(asio::error::operation_aborted, 0); return; }
#if defined(_WIN32) && !defined(NDEBUG)
        if (activeRead_ && activeRead_->complete) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("eventcore/drivers/winapi/sockets.d"), 266);
#endif
#ifndef _WIN32
        Error immediateError;
        const auto immediateBytes = socket.read_some(input.prepare(4096), immediateError);
        if (immediateError != asio::error::would_block && immediateError != asio::error::try_again) {
            observeRead(immediateError, immediateBytes);
            input.commit(immediateBytes);
            completion(immediateError, immediateBytes);
            return;
        }
#endif
        auto read = std::make_shared<Read>();
        read->cancellation = asio::get_associated_cancellation_slot(completion);
        read->nativeCancellation = std::make_shared<asio::cancellation_signal>();
        read->complete = std::move(completion);
        (void)input.prepare(read->bytes.size());
#ifdef _WIN32
        retainWindowsSocket();
#endif
        try {
            if (auto previous = std::exchange(activeRead_, read)) detachRead(previous);
#ifndef _WIN32
            posixPendingReadBytes_ = 0;
#endif
            if (read->cancellation.is_connected()) {
                read->cancellation.assign([weak = std::weak_ptr<Read>(read), signal = read->nativeCancellation]
                    (asio::cancellation_type type) {
                        if (const auto operation = weak.lock()) {
                            operation->cancellation = asio::cancellation_slot{};
                            operation->complete = {};
                        }
                        signal->emit(type);
                    });
            }
            socket.async_read_some(asio::buffer(read->bytes),
                asio::bind_cancellation_slot(read->nativeCancellation->slot(),
                [self = shared_from_this(), read](Error error, std::size_t size) {
#ifdef _WIN32
                    // Drop the real I/O reference even for a detached notification.
                    const bool deliver = self->releaseWindowsSocket();
                    if (!deliver) return;
#endif
                    if (self->activeRead_ != read) return;
                    if (!read->complete) {
                        self->activeRead_.reset();
                        return;
                    }
                    self->activeRead_.reset();
                    auto cancellation = std::exchange(read->cancellation, asio::cancellation_slot{});
                    cancellation.clear();
                    self->observeDeferredRead(error, size);
                    if (self->isOpen()) {
                        asio::buffer_copy(self->input.prepare(size), asio::buffer(read->bytes, size));
                        self->input.commit(size);
                    }
                    auto complete = std::exchange(read->complete, {});
                    complete(error, size);
                }));
        } catch (...) {
            if (activeRead_ == read) activeRead_.reset();
            detachRead(read);
#ifdef _WIN32
            releaseWindowsSocket();
#endif
            throw;
        }
    }
    void observeDeferredRead(Error error, std::size_t bytes) noexcept {
#ifndef _WIN32
        posixPendingReadBytes_ = bytes;
#endif
        observeRead(error, bytes);
    }
    bool connected() const noexcept {
        return isOpen() && (socketState_ == SocketState::connected ||
            socketState_ == SocketState::passiveClose);
    }
    void observeRead(Error error, std::size_t bytes) noexcept {
        if (error == asio::error::eof || (!error && !bytes) || sourceSocketHalfClose(error)) {
            if (socketState_ == SocketState::activeClose) socketState_ = SocketState::closed;
            else if (socketState_ != SocketState::closed) socketState_ = SocketState::passiveClose;
        } else if (sourceSocketDisconnected(error)) socketState_ = SocketState::closed;
    }
    void observeWrite(Error error) noexcept {
        if (sourceSocketHalfClose(error)) {
            if (socketState_ == SocketState::passiveClose) socketState_ = SocketState::closed;
            else if (socketState_ != SocketState::closed) socketState_ = SocketState::activeClose;
        } else if (sourceSocketDisconnected(error)) socketState_ = SocketState::closed;
    }
    Tcp::socket socket;
    boost::beast::flat_buffer input;
    SourceRequestHeader request;
    std::string body;
    RequestUrl url;
    std::shared_ptr<RequestTask> task;
    unsigned status = 200;
    SourceResponseHeaders headers;
    bool streaming = false;
    bool responseStarted = false;
    explicit Connection(Tcp::socket value) : socket(std::move(value)) {
#ifndef _WIN32
        socket.non_blocking(true);
#endif
    }
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;
    bool persistent() const {
        const auto found = request.headers.find("Connection");
        const bool keepAlive = found && request_header_source::equalName(found->view(), "keep-alive");
        // HTTPRequest.persistent compares the complete field value, rather
        // than recognizing a token within a comma-separated Connection list.
        if (request.httpVersion == SourceHTTPVersion::HTTP_1_0) return keepAlive;
        return request.httpVersion == SourceHTTPVersion::HTTP_1_1 && (!found || keepAlive);
    }
    template<class SelectHost> void read(SourceSysTime requestTime, SelectHost&& selectHost) {
        responseStarted = false;
        headResponse_ = false;
        responseVersion_ = 11;
        chunkOutput_.reset();
        // The real outer connection loop owns the initial/keep-alive data
        // waits. This parser must not cancel a whole socket or wait before
        // selecting the source virtual host for an already-ready request.
        request = {};
        body.clear();
        bodyRead_ = false;
        sourceChunked_ = false;
        chunkRemaining_ = decodedBodySize_ = 0;
        fixedBodySize_ = 0;
        HeaderInput reader{*this};
        httpSourceLog(SourceLogLevel::trace, "reading request..");
        parseRequestHeader(request, reader, 8192, 4096);
        httpSourceLog(SourceLogLevel::trace, "Got request header.");
        validateRequestHost(request.headers.get("Host").view(), sourceHTTPVersionNumber(request.httpVersion));
        std::forward<SelectHost>(selectHost)();
        if (const auto length = request.headers.find("Content-Length")) {
            fixedBodySize_ = requestContentLength(length->view());
            if (fixedBodySize_ > 2097152)
                throw HTTPStatusException(400,
                    compat::RetainedString::fromStaticLiteral("Request size too big"), 223);
        } else if (const auto encoding = request.headers.find("Transfer-Encoding")) {
            if (!request_header_source::equalName(encoding->view(), "chunked"))
                throw HTTPStatusException(400,
                    compat::RetainedString::fromStaticLiteral("Bad Request"), 226);
            sourceChunked_ = true;
            readChunkHeader();
        }
        // ChunkedInputStream construction has already read the first header.
        if (request_header_source::equalName(request.headers.get("Expect").view(), "100-continue")) {
            httpSourceLog(SourceLogLevel::trace, "sending 100 continue");
            writeBytes("HTTP/1.1 100 Continue\r\n\r\n");
        }
        url = parseRequestUrl(request.requestURI);
        // Wire-compatible defaults for the project's selected vibe-core 2.13.1.
        headResponse_ = request.method == SourceHTTPMethod::HEAD;
        headers.set(http::field::server, "vibe.d/2.13.1");
        headers.set(http::field::date, responseDate(requestTime));
        if (persistent()) headers.set(http::field::keep_alive, "timeout=10");
        responseVersion_ = sourceHTTPVersionNumber(request.httpVersion);
    }
    void logRequestEntry() {
        httpSourceLog(SourceLogLevel::trace, "persist: %s", [&] { return std::tuple(persistent()); });
        httpSourceLog(SourceLogLevel::trace, "handle request (body %d)", [&] {
            return std::tuple(sourceChunked_ ? chunkRemaining_ : fixedBodySize_);
        });
    }
    void readBody() {
        if (bodyRead_) return;
        if (sourceChunked_) {
            readChunkedBody(true);
            bodyRead_ = true;
            return;
        }
        // LimitedInputStream.leastSize is exactly the declared length. readAll
        // allocates for that complete read before consuming its first byte.
        body.resize(static_cast<std::size_t>(fixedBodySize_));
        readPayload(body.data(), body.size());
        bodyRead_ = true;
    }
    void drainBody() {
        if (bodyRead_) return;
        const bool hadBody = sourceChunked_ ? chunkRemaining_ != 0 : fixedBodySize_ != 0;
        if (sourceChunked_) {
            readChunkedBody(false);
            bodyRead_ = true;
        } else {
            if (fixedBodySize_) {
                std::unique_ptr<char[]> discard(new char[64 * 1024]);
                auto remaining = fixedBodySize_;
                while (remaining) {
                    const auto size = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, 64 * 1024));
                    readPayload(discard.get(), size);
                    remaining -= size;
                }
            }
            bodyRead_ = true;
        }
        if (hadBody) httpSourceLog(SourceLogLevel::trace, "dropped body");
    }
    void writeBytes(std::string bytes) {
        // TCPConnection.write returns before touching the socket for zero bytes.
        if (bytes.empty()) return;
        // Eventcore validates socket handles in the calling thread's driver.
        // A stream registered by another thread is not a handle in that driver;
        // do not turn it into an unsynchronized cross-context native operation.
        if (std::this_thread::get_id() != socketThread_) throw compat::ThrowableException(
            compat::RetainedString::fromStaticLiteral("Error writing data to socket."),
            compat::RetainedString::fromStaticLiteral("vibe/core/net.d"), 937);
        std::shared_ptr<Write> write;
        try {
            RequestTask::current().await([&](auto done) {
                if (!isOpen()) { done(asio::error::operation_aborted); return; }
#if defined(_WIN32) && !defined(NDEBUG)
                if (activeWrite_) throw compat::ngStaticError<compat::AssertError>(
                    compat::RetainedString::fromStaticLiteral("Assertion failure"),
                    compat::RetainedString::fromStaticLiteral("eventcore/drivers/winapi/sockets.d"), 392);
#endif
                const auto cancellation = asio::get_associated_cancellation_slot(done);
                write = std::make_shared<Write>(Write{std::move(bytes), std::move(done), cancellation});
#ifdef _WIN32
                beginWindowsWrite(write);
#else
                continuePosixWrite(write, true);
#endif
            }, RequestTask::AwaitKind::socketWrite);
        } catch (...) {
            if (write) {
#ifndef _WIN32
                auto cancellation = std::exchange(write->cancellation, asio::cancellation_slot{});
                cancellation.clear();
#endif
                write->complete = {};
                if (activeWrite_ == write) activeWrite_.reset();
#ifndef _WIN32
                if (write->readinessCancellation)
                    write->readinessCancellation->emit(asio::cancellation_type::all);
#endif
            }
            throw;
        }
    }
    void writeHeader() {
#ifndef NDEBUG
        if (responseStarted) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Try to write header after body has already begun."),
            compat::RetainedString::fromStaticLiteral("vibe/http/internal/http1/server.d"), 606);
        if (responseVersion_ == 10 && status < 200) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Informational status codes aren't supported by HTTP/1.0."),
            compat::RetainedString::fromStaticLiteral("vibe/http/internal/http1/server.d"), 607);
#endif
        if (status >= 200) responseStarted = true;
        ChunkOutput sink{this};
        chunk_writer_source::StreamOutputRange<ChunkOutput, 1024> range(sink);
        compat::ngRunWithScopeExit([&] {
            httpSourceLog(SourceLogLevel::trace, "---------------------");
            httpSourceLog(SourceLogLevel::trace, "HTTP server response:");
            httpSourceLog(SourceLogLevel::trace, "---------------------");
            range.put(responseVersion_ == 10 ? "HTTP/1.0 " : "HTTP/1.1 ");
            range.put(std::to_string(status));
            range.put(" ");
            range.put(sourceHTTPStatusText(static_cast<std::int32_t>(status)).view());
            range.put("\r\n");
            httpSourceLog(SourceLogLevel::trace, "%s %d %s", [&] {
                return std::tuple(responseVersion_ == 10 ? "HTTP/1.0" : "HTTP/1.1", status,
                    sourceHTTPStatusText(static_cast<std::int32_t>(status)));
            });
            headers.forEach([&](UrlString key, UrlString value) {
                range.put(key.view());
                range.put(": ");
                range.put(value.view());
                range.put("\r\n");
                httpSourceLog(SourceLogLevel::trace, "%s: %s", [&] { return std::tuple(key, value); });
            });
            httpSourceLog(SourceLogLevel::trace, "---------------------");
            range.put("\r\n");
        }, [&] { range.finishScope(); });
    }
    void writeBody(std::string body) {
        if (!headers.contains(http::field::content_type))
            headers.set(http::field::content_type, "text/plain; charset=UTF-8");
        headers.set(http::field::content_length, std::to_string(body.size()));
        writeHeader();
        if (!headResponse_) writeBytes(std::move(body));
    }
    void startStream() {
        if (headResponse_ || responseVersion_ > 10) headers.set(http::field::transfer_encoding, "chunked");
        else headers.erase(http::field::connection);
        writeHeader();
        if (!headResponse_ && responseVersion_ > 10)
            chunkOutput_ = std::make_shared<SourceChunkedOutputStream<ChunkOutput>>(ChunkOutput{this});
        streaming = true;
    }
    std::shared_ptr<SourceHTTPOutputStream> bodyWriter() {
        if (!streaming) startStream();
        if (headResponse_) return sourceNullOutputStream();
        return std::make_shared<RetainedOutputStream>(shared_from_this(), chunkOutput_);
    }
    void finishStream() {
        if (streaming && responseVersion_ > 10 && !headResponse_) {
            const auto writer = chunkOutput_;
            writer->finalize();
        }
    }
    void close() {
        // End the actual connection's reverse ownership only at close. The
        // local owner and real completion callbacks retain the executing task
        // through cancellation/notification; no live callback is made weak.
        const auto closingTask = std::exchange(task, {});
        socketState_ = SocketState::closed;
#ifdef _WIN32
        if (!windowsSocketReleased_) {
            windowsSocketReleased_ = true;
            Error ignored;
            socket.shutdown(Tcp::socket::shutdown_both, ignored);
            releaseWindowsSocket();
        }
#else
        // clearFD drops the source notification slots rather than waking their
        // suspended tasks with a manufactured cancellation result.
        if (auto read = std::exchange(activeRead_, {})) detachRead(read);
        if (auto write = std::exchange(activeWrite_, {})) {
            auto cancellation = std::exchange(write->cancellation, asio::cancellation_slot{});
            cancellation.clear();
            write->complete = {};
            if (write->readinessCancellation) write->readinessCancellation->emit(asio::cancellation_type::all);
        }
        Error ignored;
        socket.cancel(ignored); socket.shutdown(Tcp::socket::shutdown_both, ignored); socket.close(ignored);
#endif
        // The active handler retains its own bytes until socket cancellation completes.
        if (closingTask) closingTask->cancelTimer();
    }
};
} // namespace nijigenerate::api::mcp::http_transport::detail
