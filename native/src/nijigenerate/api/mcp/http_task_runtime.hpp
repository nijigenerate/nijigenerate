#pragma once
// Used vibe.core.task/core scheduling and async wait implementation.
// Copyright 2012-2020 Sönke Ludwig. MIT; port/licenses/mcp-http-transport.txt.
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <boost/asio.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/context/detail/exception.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nijigenerate::api::mcp::http_transport::detail {
namespace asio = boost::asio;
using Error = boost::system::error_code;
class RequestTask;

// The actual vibe.core.task.Task value retains its Fiber and incarnation.
// Its counter belongs to that real pooled Fiber, never a diagnostic ID generator.
class SourceTask {
    friend class RequestTask;
    std::shared_ptr<RequestTask> fiber_;
    std::size_t counter_ = 0;
    std::array<char, 4> debugCharacters() const noexcept;
    SourceTask(std::shared_ptr<RequestTask> fiber, std::size_t counter)
        : fiber_(fiber), counter_(counter) {}
public:
    SourceTask() = default;
    SourceTask(const SourceTask&) = default;
    SourceTask& operator=(const SourceTask&) = default;
    SourceTask(SourceTask&& other) noexcept : SourceTask(static_cast<const SourceTask&>(other)) {}
    SourceTask& operator=(SourceTask&& other) noexcept {
        return *this = static_cast<const SourceTask&>(other);
    }
    static SourceTask getThis() noexcept;
    explicit operator bool() const noexcept { return static_cast<bool>(fiber_); }
    std::size_t taskCounter() const noexcept { return counter_; }
    std::shared_ptr<core::thread_context::Thread> thread() const noexcept;
    bool running() const;
    void interrupt() const;
    compat::RetainedString getDebugID() const;
    template<class Output> void getDebugID(Output& output) const {
        if (!fiber_) { output.put(std::string_view("----")); return; }
        const auto text = debugCharacters();
        output.put(std::string_view(text.data(), text.size()));
        if (!running()) output.put(std::string_view("-fin"));
    }
    bool operator==(const SourceTask& other) const noexcept {
        return fiber_.get() == other.fiber_.get() && counter_ == other.counter_;
    }
};
enum class SourceTaskPriority { normal, prioritized, immediate };

enum class SourceLogLevel { trace, debugV, debug_, diagnostic, info, warn, error, critical, fatal, none };
enum class SourceLogFormat { plain, thread, threadTime };
void ngSetSourceLogLevel(SourceLogLevel level);
SourceLogLevel ngGetSourceLogLevel();
void ngSetSourceLogFormat(SourceLogFormat diagnostic, SourceLogFormat info = SourceLogFormat::plain);

// A synchronous borrowed writer; neither the logger nor the message may retain it.
class SourceLogSink {
    const void* context_;
    void (*put_)(const void*, std::string_view);
public:
    SourceLogSink(const void* context, void (*put)(const void*, std::string_view))
        : context_(context), put_(put) {}
    void operator()(std::string_view text) const { put_(context_, text); }
    void put(std::string_view text) const { (*this)(text); }
};

// Both callbacks finish before this call returns. Preparation is gated by the
// same default logger and precedes beginLine, matching the source log entry.
void ngSourceLogPrepared(SourceLogLevel level, void* context,
    void (*prepare)(void*), void (*emit)(void*, SourceLogSink));

template<class Prepare>
void ngSourceLog(SourceLogLevel level, Prepare&& prepare) {
    using Message = std::remove_cvref_t<std::invoke_result_t<Prepare&>>;
    struct State {
        Prepare& prepare;
        std::optional<Message> message;
    } state{prepare, std::nullopt};
    ngSourceLogPrepared(level, &state,
        [](void* context) {
            auto& state = *static_cast<State*>(context);
            state.message.emplace(std::invoke(state.prepare));
        },
        [](void* context, SourceLogSink sink) {
            auto& state = *static_cast<State*>(context);
            std::invoke(*state.message, sink);
        });
}
enum class SourceTaskExit { exited, outOfWaiters, idle, timeout };
enum class SourceScheduleStatus { idle, allProcessed, busy };

// The selected driver's waiter count comes from actual registered operations.
// Callback ownership, completion and cancellation define each registration;
// suspended tasks and open descriptors alone are not native waiters.
struct SourceNativeWaitRegistry { std::atomic_size_t pending{0}; };
class SourceNativeWait {
    std::shared_ptr<SourceNativeWaitRegistry> registry_;
    std::atomic_bool active_{true};
public:
    explicit SourceNativeWait(std::shared_ptr<SourceNativeWaitRegistry> registry)
        : registry_(std::move(registry)) { ++registry_->pending; }
    ~SourceNativeWait() { complete(); }
    SourceNativeWait(const SourceNativeWait&) = delete;
    SourceNativeWait& operator=(const SourceNativeWait&) = delete;
    void complete() noexcept {
        if (active_.exchange(false)) --registry_->pending;
    }
};

template<class Condition, std::size_t N>
inline void sourceTaskAssert(Condition&& condition, const char (&message)[N], std::size_t line) {
#ifndef NDEBUG
    if (!std::invoke(condition)) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral(message),
        compat::RetainedString::fromStaticLiteral("vibe/core/task.d"), line);
#else
    (void)condition; (void)message; (void)line;
#endif
}

class SourceTaskScheduler {
    friend class RequestTask;
    using Task = std::shared_ptr<RequestTask>;
    asio::io_context& loop_;
    Task readyFirst_;
    RequestTask* readyLast_ = nullptr;
    std::size_t readyCount_ = 0;
    std::vector<Task> active_;
    std::array<Task, 100> available_;
    std::size_t availableCount_ = 0;
    Task dummy_;
    bool running_ = false;
    bool exit_ = false;
    bool shuttingDown_ = false;
    bool yieldingGlobal_ = false;
    std::shared_ptr<SourceNativeWaitRegistry> nativeWaits_ = std::make_shared<SourceNativeWaitRegistry>();
    void reschedule(const Task& task);
    void remove(RequestTask* task);
    void insertAfter(Task task, RequestTask* previous, std::size_t assertionLine = 1274);
    Task popReady();
    void resume(const Task& task);
    SourceTaskExit processEvents(bool block);
public:
    explicit SourceTaskScheduler(asio::io_context& loop) : loop_(loop) {}
    ~SourceTaskScheduler();
    SourceTaskScheduler(const SourceTaskScheduler&) = delete;
    SourceTaskScheduler& operator=(const SourceTaskScheduler&) = delete;
    asio::io_context& context() noexcept { return loop_; }
    bool running() const noexcept { return running_; }
    void running(bool value) noexcept { running_ = value; }
    bool shuttingDown() const noexcept { return shuttingDown_; }
    bool exitRequested() const noexcept { return exit_; }
    void requestExit() noexcept { exit_ = true; }
    void clearExit() noexcept { exit_ = false; }
    RequestTask& currentFiber();
    std::shared_ptr<RequestTask> retainCurrentFiber();
    void attach(const Task& task);
    Task createTask();
    void complete(RequestTask* task, bool recycle = false) noexcept;
    void switchTo(const Task& task, SourceTaskPriority priority);
    void start(const Task& task);
    void yield(bool interruptible = true);
    void yieldGlobal();
    void hibernate();
    SourceScheduleStatus schedule();
    SourceTaskExit process();
    SourceTaskExit waitAndProcess();
    void shutdown();
    std::shared_ptr<SourceNativeWait> beginNativeWait() {
        return std::make_shared<SourceNativeWait>(nativeWaits_);
    }
    template<class Handler> auto trackNativeCompletion(Handler&& handler) {
        return [registration = beginNativeWait(), handler = std::forward<Handler>(handler)]
            (auto&&... arguments) mutable {
                registration->complete();
                std::invoke(handler, std::forward<decltype(arguments)>(arguments)...);
            };
    }
};

// Bound to the actual per-thread event context, never a second queue/engine.
SourceTaskScheduler& sourceTaskSchedulerForThread();
SourceTaskScheduler& sourceTaskSchedulerFor(asio::io_context& loop);
bool sourceTaskGlobalExitRequested() noexcept;
bool sourceTaskDriverExitRequested() noexcept;

class SourceTaskInterrupt : public compat::ThrowableException {
public:
    SourceTaskInterrupt() : ThrowableException(
        compat::RetainedString::fromStaticLiteral("Task interrupted."),
        compat::RetainedString::fromStaticLiteral("vibe/core/task.d"), 291) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "vibe.core.task.InterruptException"; }
};

// The single native counterpart of the used Vibe TaskFiber, using the existing
// generic Boost.Context Fiber. No self-root, collector or synthetic Fiber ID.
class RequestTask final : public std::enable_shared_from_this<RequestTask> {
    friend class SourceTaskScheduler;
    friend class SourceYieldLock;
    friend class SourceTask;
    // Native stack-exit control, deliberately outside the source Exception
    // hierarchy so application catch(Exception) cannot swallow thread teardown.
    struct ThreadUnwind {};
    SourceTaskScheduler& scheduler_;
    std::shared_ptr<asio::steady_timer> timer_;
    std::unique_ptr<core::tasks::TaskFiber> fiber_;
    std::function<void()> body_;
    std::function<void()> terminate_;
    std::shared_ptr<core::thread_context::Thread> thread_;
    bool dummy_ = false;
    static constexpr std::uint64_t runningFlag = 1;
    static constexpr std::uint64_t initializedFlag = 2;
    static constexpr std::uint64_t interruptFlag = 4;
    static constexpr std::uint64_t flagsMask = 7;
    static constexpr unsigned counterShift = 3;
    std::atomic<std::uint64_t> counterAndFlags_{0};
    bool parked_ = false;
    bool queued_ = false;
    std::shared_ptr<RequestTask> readyNext_;
    RequestTask* readyPrevious_ = nullptr;
    bool disposing_ = false;
    bool pooled_ = false;
    std::uint32_t staticPriority_ = 0x10000;
    std::uint32_t dynamicPriority_ = 0x10000;
    int yieldLocks_ = 0;
    compat::RetainedString yieldFile_;
    std::size_t yieldLine_ = 0;
    inline static thread_local RequestTask* current_ = nullptr;
    void run();
    void releaseFiber();
    void checkInterrupt();
    void checkYieldLock();
    bool initialized() const noexcept { return (counterAndFlags_.load() & initializedFlag) != 0; }
    bool takeInterrupt() noexcept { return (counterAndFlags_.fetch_and(~interruptFlag) & interruptFlag) != 0; }
    void clearFlags() noexcept { counterAndFlags_.fetch_and(~flagsMask); }
    std::size_t taskCounter() const noexcept {
        return static_cast<std::size_t>(counterAndFlags_.load() >> counterShift);
    }
    void interrupt(std::size_t counter);
public:
    enum class AwaitKind { native, socketWrite };
    explicit RequestTask(asio::io_context& loop);
    explicit RequestTask(SourceTaskScheduler& scheduler, bool dummy = false);
    virtual ~RequestTask();
    static RequestTask& current();
    static RequestTask* currentOrNull() noexcept {
        return core::tasks::TaskFiber::isVibeTask() && current_ && !current_->dummy_
            && current_->initialized() ? current_ : nullptr;
    }
    asio::io_context& context() noexcept { return scheduler_.context(); }
    void start(std::function<void()> body);
    void wake();
    void interrupt();
    void sleep(std::chrono::milliseconds duration);
    bool finished() const noexcept { return !initialized(); }
    void onTermination(std::function<void()> operation) { terminate_ = std::move(operation); }
    void cancelTimer();
    void disposeAfterLoopExit();

    template<class Start> void await(Start&& start, AwaitKind kind = AwaitKind::native) {
        struct WaitState {
            Error completion;
            bool inside = true;
            bool fired = false;
            bool waiting = false;
            bool started = false;
            asio::cancellation_signal cancellation;
        };
        const auto state = std::make_shared<WaitState>();
        std::weak_ptr<SourceNativeWait> nativeWait;
        parked_ = true;
        compat::ngRunWithScopeExit([&] {
            auto registration = scheduler_.beginNativeWait();
            nativeWait = registration;
            start(asio::bind_cancellation_slot(state->cancellation.slot(),
                [self = shared_from_this(), state, registration = std::move(registration)](Error error) {
                    registration->complete();
                    if (!state->inside) return;
                    state->completion = error;
                    state->fired = true;
                    if (state->waiting && !self->dummy_) self->wake();
                }));
            state->started = true;
            if (!state->fired) {
                state->waiting = true;
                do {
                    bool interrupted = takeInterrupt();
                    if (dummy_) {
                        checkYieldLock();
                        scheduler_.waitAndProcess();
                    } else scheduler_.hibernate();
                    if (disposing_) throw ThreadUnwind{};
                    interrupted |= takeInterrupt();
                    if (interrupted) throw SourceTaskInterrupt();
                } while (!state->fired);
            }
            if (state->completion) failWait(state->completion, kind);
        }, [&] {
            state->inside = false;
            parked_ = false;
            if (const auto registration = nativeWait.lock()) registration->complete();
            if (state->started && !state->fired) state->cancellation.emit(asio::cancellation_type::all);
        });
    }
    // The actual final Source TaskFiber inherits Object's default protocol.
    virtual compat::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<RequestTask> other);
    virtual int opCmp(std::shared_ptr<RequestTask> other);
private:
    static void failWait(Error error, AwaitKind kind);
};

class SourceYieldLock {
    std::shared_ptr<RequestTask> fiber_;
public:
    SourceYieldLock(SourceTaskScheduler& scheduler, compat::RetainedString file, std::size_t line);
    SourceYieldLock(const SourceYieldLock&) = delete;
    SourceYieldLock& operator=(const SourceYieldLock&) = delete;
    SourceYieldLock(SourceYieldLock&&) noexcept = default;
    ~SourceYieldLock() noexcept(false);
};
}
