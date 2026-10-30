// Used Thread operations: core/thread/osthread.d, Sean Kelly, BSL-1.0.
#include "autorig_worker.hpp"
#include <nijigenerate/panels/autorig.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/thread_exception.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <atomic>
#include <exception>
#include <new>
#include <source_location>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>
#else
#include <cerrno>
#include <pthread.h>
#include <time.h>
#endif

namespace nijigenerate::panels::autorig::detail {

struct AutoRigWorker::State {
    compat::SharedDelegate<void()> action;
    std::shared_ptr<AutoRigWorker> launchOwner;
    std::exception_ptr unhandled;
    std::atomic<bool> attached{false};
    std::atomic<bool> running{false};
#ifdef _WIN32
    HANDLE handle = nullptr;
    unsigned id = 0;
    static unsigned __stdcall entry(void* raw) noexcept {
#else
    pthread_t handle{};
    static void* entry(void* raw) noexcept {
#endif
        auto* worker = static_cast<AutoRigWorker*>(raw);
        const auto owner = worker->state_->launchOwner;
        owner->state_->launchOwner.reset();
        owner->execute();
#ifdef _WIN32
        return 0;
#else
        return nullptr;
#endif
    }
};

AutoRigWorker::AutoRigWorker(compat::SharedDelegate<void()> action)
    : state_([] {
        try {
            return std::make_unique<State>();
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }()) {
    state_->action = std::move(action);
}

AutoRigWorker::~AutoRigWorker() {
#ifdef _WIN32
    if (state_->handle) CloseHandle(state_->handle);
#else
    if (state_->attached.load(std::memory_order_relaxed)) pthread_detach(state_->handle);
#endif
}

void AutoRigWorker::execute() noexcept {
    // Retain the actual capture even if native thread/module preparation fails.
    const auto action = state_->action;
    state_->action = nullptr;
    try {
        core::thread_context::ManagedThreadScope managed;
        ngInitializeAutoRigPanelModule();
        action();
    } catch (...) {
        state_->unhandled = std::current_exception();
    }
    state_->running.store(false, std::memory_order_release);
}

void AutoRigWorker::start() {
    const auto owner = shared_from_this();
    state_->launchOwner = owner;
#ifdef _WIN32
    state_->handle = reinterpret_cast<HANDLE>(_beginthreadex(
        nullptr, 0, &State::entry, this, CREATE_SUSPENDED, &state_->id));
    if (!state_->handle) {
        state_->launchOwner.reset();
        compat::ngOnThreadError(compat::RetainedString::fromStaticLiteral("Error creating thread"));
    }
    state_->attached.store(true, std::memory_order_release);
    if (ResumeThread(state_->handle) == static_cast<DWORD>(-1)) {
        // The suspended OS entry still refers to this actual object. Preserve
        // that required owner if resumption fails, as the source thread list
        // retains a created thread through this failure boundary.
        compat::ngOnThreadError(compat::RetainedString::fromStaticLiteral("Error resuming thread"));
    }
#else
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) {
        state_->launchOwner.reset();
        compat::ngOnThreadError(compat::RetainedString::fromStaticLiteral("Error initializing thread attributes"));
    }
    state_->running.store(true, std::memory_order_relaxed);
    if (pthread_create(&state_->handle, &attributes, &State::entry, this) != 0) {
        state_->running.store(false, std::memory_order_relaxed);
        state_->launchOwner.reset();
        compat::ngOnThreadError(compat::RetainedString::fromStaticLiteral("Error creating thread"));
    }
    state_->attached.store(true, std::memory_order_release);
    if (pthread_attr_destroy(&attributes) != 0)
        compat::ngOnThreadError(compat::RetainedString::fromStaticLiteral("Error destroying thread attributes"));
#endif
}

bool AutoRigWorker::isRunning() const noexcept {
    if (!state_->attached.load(std::memory_order_acquire)) return false;
#ifdef _WIN32
    DWORD code = 0;
    GetExitCodeThread(state_->handle, &code);
    return code == STILL_ACTIVE;
#else
    return state_->running.load(std::memory_order_acquire);
#endif
}

void AutoRigWorker::join() {
#ifdef _WIN32
    if (state_->attached.load(std::memory_order_relaxed) &&
            WaitForSingleObject(state_->handle, INFINITE) != WAIT_OBJECT_0)
        compat::ngThrowThreadException(compat::RetainedString::fromStaticLiteral("Unable to join thread"),
            compat::RetainedString::fromStaticLiteral("core/thread/osthread.d"), 606);
    state_->attached.store(false, std::memory_order_release);
    state_->id = 0;
    CloseHandle(state_->handle);
    state_->handle = nullptr;
#else
    if (state_->attached.load(std::memory_order_relaxed) && pthread_join(state_->handle, nullptr) != 0)
        compat::ngThrowThreadException(compat::RetainedString::fromStaticLiteral("Unable to join thread"),
            compat::RetainedString::fromStaticLiteral("core/thread/osthread.d"), 617);
    state_->attached.store(false, std::memory_order_release);
    state_->handle = {};
#endif
    if (state_->unhandled) std::rethrow_exception(state_->unhandled);
}

void AutoRigWorker::sleepOneMillisecond() {
#ifdef _WIN32
    Sleep(1);
#else
    timespec requested{0, 1000000};
    timespec remaining{};
    while (nanosleep(&requested, &remaining) != 0) {
        if (errno != EINTR)
            compat::ngAssertUnreachable("Unable to sleep for the specified duration",
                "core/thread/osthread.d", 1047);
        requested = remaining;
    }
#endif
}

} // namespace nijigenerate::panels::autorig::detail
