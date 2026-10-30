#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <exception>
#include <limits>
#include <new>
#include <type_traits>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/api/mcp/auth_wait.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <compat/source_allocator.hpp>

namespace nicxlive::core::d_runtime { class SourceMutex; class SourceCondition; }

namespace nijigenerate::api::mcp::task {

// Copies retain callable identity and its closure environment. Reassigning the
// caller's descriptor must not replace a function already stored in the queue.
template<class T>
class MainThreadAction {
    compat::SharedDelegate<T()> callable_;
public:
    MainThreadAction() noexcept = default;
    MainThreadAction(std::nullptr_t) noexcept {}
    explicit MainThreadAction(std::function<T()> callable)
        : callable_(std::move(callable)) {}
    template<class Callable>
        requires (!std::is_same_v<std::remove_cvref_t<Callable>, MainThreadAction> &&
            !std::is_same_v<std::remove_cvref_t<Callable>, std::function<T()>> &&
            !std::is_same_v<std::remove_cvref_t<Callable>, compat::SharedDelegate<T()>> &&
            std::is_invocable_r_v<T, std::decay_t<Callable>&>)
    explicit MainThreadAction(Callable&& callable)
        : callable_(std::forward<Callable>(callable)) {}
    template<class Callable>
        requires(std::is_same_v<std::remove_cvref_t<Callable>, compat::SharedDelegate<T()>>)
    explicit MainThreadAction(Callable&& callable) : callable_(std::forward<Callable>(callable)) {}
    MainThreadAction(const MainThreadAction&) noexcept = default;
    MainThreadAction& operator=(const MainThreadAction&) noexcept = default;
    MainThreadAction(MainThreadAction&& other) noexcept : callable_(other.callable_) {}
    MainThreadAction& operator=(MainThreadAction&& other) noexcept {
        callable_ = other.callable_;
        return *this;
    }
    explicit operator bool() const noexcept { return static_cast<bool>(callable_); }
    void reset() noexcept { callable_ = {}; }
    T operator()() const { return callable_(); }
};
using Action = MainThreadAction<void>;

void ngMcpSetExternalCommandsBlocked(bool blocked);
bool ngMcpExternalCommandsBlocked();
void ngMcpInitTask();
void ngMcpEnqueueAction(Action action);
void ngMcpProcessQueue();

namespace detail {
using SourceMutex = nicxlive::core::d_runtime::SourceMutex;
using SourceCondition = nicxlive::core::d_runtime::SourceCondition;
std::shared_ptr<SourceMutex> ngCreateSourceMutex();
std::shared_ptr<SourceCondition> ngCreateSourceCondition(std::shared_ptr<SourceMutex> mutex);
void ngLockSourceMonitor(const std::shared_ptr<SourceMutex>& mutex);
void ngUnlockSourceMonitor(const std::shared_ptr<SourceMutex>& mutex);
void ngNotifySourceCondition(const std::shared_ptr<SourceCondition>& condition);
void ngWaitSourceCondition(const std::shared_ptr<SourceCondition>& condition);

template<class Body> void withSourceMonitor(std::shared_ptr<SourceMutex> mutex, Body&& body) {
    ngLockSourceMonitor(mutex);
    compat::ngRunWithScopeExit(std::forward<Body>(body), [mutex] { ngUnlockSourceMonitor(mutex); });
}

template<class T> T initialValue() {
    if constexpr(std::is_floating_point_v<T>) return std::numeric_limits<T>::quiet_NaN();
    else if constexpr(std::is_same_v<T,char32_t>) return static_cast<T>(0xffffu);
    else if constexpr(std::is_same_v<T,char> || std::is_same_v<T,char16_t>)
        return static_cast<T>(~0u);
    else return T{};
}
template<class T> struct MainThreadResult { T result=initialValue<T>(); };
template<> struct MainThreadResult<void> {};
template<class T> struct MainThreadWait {
    std::shared_ptr<SourceMutex> mutex = ngCreateSourceMutex();
    std::shared_ptr<SourceCondition> condition = ngCreateSourceCondition(mutex);
    bool done=false;
    std::exception_ptr captured;
    MainThreadResult<T> value;
};
}

// Even a caller on the UI thread enqueues, exactly like the source. Only a
// running Vibe task may cooperatively yield; ordinary threads wait on the OS.
template<class T>
T ngRunInMainThread(MainThreadAction<T> action) {
    auto state=std::allocate_shared<detail::MainThreadWait<T>>(
        nicxlive::core::d_runtime::SourceAllocator<detail::MainThreadWait<T>>{});
    ngMcpEnqueueAction(Action([state,action] {
        detail::MainThreadResult<T> local;
        try {
            if(action) {
                if constexpr(std::is_void_v<T>) action();
                else local.result=action();
            }
            detail::withSourceMonitor(state->mutex, [&] {
                if constexpr(!std::is_void_v<T>) state->value.result=local.result;
                state->done=true;
                detail::ngNotifySourceCondition(state->condition);
            });
        } catch(const compat::ThrowableException&) {
            const auto captured=std::current_exception();
            detail::withSourceMonitor(state->mutex, [&] {
                state->captured=captured;
                state->done=true;
                detail::ngNotifySourceCondition(state->condition);
            });
        }
    }));
    if(core::tasks::TaskFiber::isVibeTask()) {
        for(;;) {
            bool done = false;
            detail::withSourceMonitor(state->mutex, [&] { done = state->done; });
            if(done) break;
            http_transport::ngYield();
        }
    } else {
        detail::withSourceMonitor(state->mutex, [&] {
            while(!state->done) detail::ngWaitSourceCondition(state->condition);
        });
    }
    if(state->captured) std::rethrow_exception(state->captured);
    if constexpr(!std::is_void_v<T>) return state->value.result;
}

} // namespace nijigenerate::api::mcp::task
