#include "thread_context.hpp"
// Used core/thread/threadbase.d225-248 and osthread.d374/thread attachment.
// Copyright Sean Kelly 2005-2009 / D Language Foundation; BSL-1.0.
// Existing full terms: SDK core/licenses/druntime-thread-exceptions.txt.
#include <compat/throwable.hpp>
#include <common/object_key_policy.hpp>
#include <nijigenerate/compat/thread_exception.hpp>
#include <cerrno>
#include <chrono>
#include <limits>
#include <mutex>
#include <thread>
#include <stdexcept>
#include <string>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <process.h>
#else
#include <time.h>
#include <pthread.h>
#endif

namespace nijigenerate::core::thread_context {
namespace {
void prepareThreadClassInfo() {
    static const bool registered = [] {
        using nicxlive::core::d_runtime::ngRegisterObjectClassName;
        ngRegisterObjectClassName(typeid(ThreadBase),
            compat::RetainedString::fromStaticLiteral("core.thread.threadbase.ThreadBase"));
        ngRegisterObjectClassName(typeid(Thread),
            compat::RetainedString::fromStaticLiteral("core.thread.osthread.Thread"));
        return true;
    }();
    (void)registered;
}
std::mutex registrationLock;
std::thread::id mainThread;
struct CurrentThreadContext;
// Like ThreadBase.sm_this: an unattached lookup never initializes an owner.
thread_local CurrentThreadContext* attachedContext = nullptr;
struct CurrentThreadContext {
    unsigned managedDepth = 0;
    bool mainAttachment = false;
    std::shared_ptr<Thread> thread;
    ~CurrentThreadContext() {
        if (attachedContext == this) attachedContext = nullptr;
    }
};
thread_local CurrentThreadContext currentThread;
struct StaticErrorThreadExit {
    ~StaticErrorThreadExit() noexcept {
        // Source scopes may end and restart on one OS thread. Release its
        // actual Error storage only at OS thread exit, after source users.
        nicxlive::core::d_runtime::Throwable::ngReleaseStaticErrorThreadStorage();
    }
};
void prepareStaticErrorThreadExit() {
    // Register native TLS teardown during normal thread entry, never from
    // a first staticError failure path, which must not allocate bookkeeping.
    thread_local StaticErrorThreadExit cleanup;
    (void)cleanup;
}
std::unique_lock<std::mutex> lockRegistration() {
    return std::unique_lock(registrationLock);
}
bool currentIsMain() {
    const auto lock = lockRegistration();
    if(mainThread==std::thread::id{}) throw std::logic_error("UI main thread has not been registered");
    return mainThread==std::this_thread::get_id();
}
}
// Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString ThreadBase::toString() {
    prepareThreadClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t ThreadBase::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool ThreadBase::opEquals(std::shared_ptr<ThreadBase> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int ThreadBase::opCmp(std::shared_ptr<ThreadBase>) {
    prepareThreadClassInfo();
    std::string message = "need opCmp for class ";
    message += nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)).view();
    throw nicxlive::core::d_runtime::ThrowableException(compat::RetainedString(std::move(message)),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngThreadObjectEquals(std::shared_ptr<ThreadBase> left, std::shared_ptr<ThreadBase> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right), &prepareThreadClassInfo);
}
compat::RetainedString ThreadBase::name() {
    const std::lock_guard lock(monitor_);
    return name_;
}
void ThreadBase::name(compat::RetainedString value) {
    const std::lock_guard lock(monitor_);
    name_ = value;
}
std::shared_ptr<ThreadBase> ThreadBase::getThis() noexcept { return Thread::getThis(); }
std::shared_ptr<Thread> Thread::getThis() noexcept {
    const auto context = attachedContext;
    return context ? context->thread : nullptr;
}

void Thread::initialize(compat::SharedDelegate<void()> call) {
#ifndef NDEBUG
    if (!call) throw compat::ngStaticError<compat::AssertError>(
        "Assertion failure", "core/thread/threadbase.d", 112);
#endif
    call_ = call;
}
Thread::~Thread() {
#ifdef _WIN32
    CloseHandle(workerHandle_);
#else
    if (hasWorker_) (void)pthread_detach(workerHandle_);
#endif
}
void Thread::runSource() {
    [[maybe_unused]] const auto owner = launchOwner_;
    launchOwner_.reset();
    ManagedThreadScope attachment(shared_from_this());
    try {
        call_();
    } catch (compat::Throwable& error) {
        // Retain the actual thrown object and its native rethrow transport.
        // Exception and Error remain different canonical dynamic families.
        unhandledException_ = std::current_exception();
        unhandledPointer_ = &error;
    } catch (...) {
        // A foreign native exception has no invented Source Throwable facade.
        unhandledException_ = std::current_exception();
        unhandledPointer_ = nullptr;
    }
    running_.store(false, std::memory_order_relaxed);
}
#ifdef _WIN32
unsigned __stdcall Thread::workerEntry(void* argument) {
    static_cast<Thread*>(argument)->runSource();
    return 0;
}
#else
void* Thread::workerEntry(void* argument) {
    static_cast<Thread*>(argument)->runSource();
    return nullptr;
}
#endif
std::shared_ptr<Thread> Thread::start() {
#ifndef NDEBUG
    if (running_.load(std::memory_order_relaxed))
        throw compat::ngStaticError<compat::AssertError>(
            "Assertion failure", "core/thread/osthread.d", 459);
#endif
    const auto owner = shared_from_this();
    launchOwner_ = owner;
    running_.store(true, std::memory_order_relaxed);
    const auto creationFailure = [&](const char* message) -> void {
        running_.store(false, std::memory_order_relaxed);
        launchOwner_.reset();
        compat::ngOnThreadError(compat::RetainedString::fromStaticView(message));
    };
#ifdef _WIN32
    unsigned address;
    workerHandle_ = reinterpret_cast<void*>(
        _beginthreadex(nullptr, 0, &Thread::workerEntry, this, CREATE_SUSPENDED, &address));
    if (!workerHandle_) creationFailure("Error creating thread");
    hasWorker_ = true;
    if (ResumeThread(workerHandle_) == static_cast<DWORD>(-1))
        creationFailure("Error resuming thread");
#else
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes)) creationFailure("Error initializing thread attributes");
    if (pthread_create(&workerHandle_, &attributes, &Thread::workerEntry, this) != 0)
        creationFailure("Error creating thread");
    hasWorker_ = true;
    if (pthread_attr_destroy(&attributes) != 0)
        compat::ngOnThreadError(compat::RetainedString::fromStaticLiteral("Error destroying thread attributes"));
#endif
    return owner;
}
std::shared_ptr<compat::Throwable> Thread::join(bool rethrow) {
    if (hasWorker_) {
#ifdef _WIN32
        const bool failed = WaitForSingleObject(workerHandle_, INFINITE) != WAIT_OBJECT_0;
#else
        const bool failed = pthread_join(workerHandle_, nullptr) != 0;
#endif
        if (failed) {
            compat::ngThrowThreadException(compat::RetainedString::fromStaticLiteral("Unable to join thread"),
                compat::RetainedString::fromStaticLiteral("core/thread/osthread.d"),
#ifdef _WIN32
                606
#else
                617
#endif
            );
        }
        hasWorker_ = false;
#ifdef _WIN32
        CloseHandle(workerHandle_);
        workerHandle_ = {};
#endif
    }
    if (unhandledException_ && (rethrow || !unhandledPointer_)) std::rethrow_exception(unhandledException_);
    if (unhandledPointer_ && !unhandled_) {
        // Build only the returned native handle, not a copy of the Throwable.
        auto transport = std::allocate_shared<std::exception_ptr>(
            nicxlive::core::d_runtime::SourceAllocator<std::exception_ptr>{}, unhandledException_);
        unhandled_ = std::shared_ptr<compat::Throwable>(std::move(transport), unhandledPointer_);
    }
    return unhandled_;
}
void Thread::sleep(compat::SourceMonoTime::Duration duration) {
#ifndef NDEBUG
    if (duration.count() < 0) throw compat::ngStaticError<compat::AssertError>(
        "Assertion failure", "core/thread/osthread.d", 1007);
#endif
#ifdef _WIN32
    using Duration = compat::SourceMonoTime::Duration;
    constexpr auto maximum = std::chrono::duration_cast<Duration>(
        std::chrono::milliseconds{std::numeric_limits<std::uint32_t>::max() - 1});
    if (duration > Duration::zero() && duration < std::chrono::milliseconds{1})
        duration = std::chrono::milliseconds{1};
    while (duration > maximum) {
        Sleep(std::numeric_limits<std::uint32_t>::max() - 1);
        duration -= maximum;
    }
    Sleep(static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()));
#else
    timespec input, remaining;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    input.tv_sec = static_cast<decltype(input.tv_sec)>(seconds);
    input.tv_nsec = static_cast<decltype(input.tv_nsec)>(
        (duration.count() % 10'000'000) * 100);
    if (seconds > std::numeric_limits<decltype(input.tv_sec)>::max())
        input.tv_sec = std::numeric_limits<decltype(input.tv_sec)>::max();
    for (;;) {
        if (!::nanosleep(&input, &remaining)) return;
#ifndef NDEBUG
        if (errno != EINTR) throw compat::ngStaticError<compat::AssertError>(
            "Unable to sleep for the specified duration", "core/thread/osthread.d", 1047);
#endif
        input = remaining;
    }
#endif
}

void ngRegisterMainThread() {
    {
        const auto lock = lockRegistration();
        const auto current=std::this_thread::get_id();
        if(mainThread!=std::thread::id{} && mainThread!=current)
            throw std::logic_error("UI main thread cannot be registered from another thread");
        mainThread=current;
    }
    prepareStaticErrorThreadExit();
    if (!currentThread.thread) currentThread.thread = std::allocate_shared<Thread>(
        nicxlive::core::d_runtime::SourceAllocator<Thread>{});
    currentThread.mainAttachment = true;
    attachedContext = &currentThread;
}
bool ngHasManagedThread() { return currentIsMain() || currentThread.managedDepth!=0; }
bool ngIsMainThread() { return currentIsMain(); }
bool ngIsMainThreadOrUnattached() {
    const auto thread = Thread::getThis();
    if (!thread) return true;
    return attachedContext && attachedContext->mainAttachment;
}
ManagedThreadScope::ManagedThreadScope() {
    prepareStaticErrorThreadExit();
    if (!currentThread.thread) currentThread.thread = std::allocate_shared<Thread>(
        nicxlive::core::d_runtime::SourceAllocator<Thread>{});
    ++currentThread.managedDepth;
    attachedContext = &currentThread;
}
ManagedThreadScope::ManagedThreadScope(std::shared_ptr<Thread> thread)
    : explicitAttachment_(true), previousThread_(currentThread.thread) {
    prepareStaticErrorThreadExit();
    currentThread.thread = std::move(thread);
    ++currentThread.managedDepth;
    attachedContext = &currentThread;
}
ManagedThreadScope::~ManagedThreadScope() {
    if (explicitAttachment_) {
        --currentThread.managedDepth;
        currentThread.thread = std::move(previousThread_);
        if (!currentThread.managedDepth && !currentThread.mainAttachment) attachedContext = nullptr;
        return;
    }
    if (!--currentThread.managedDepth && !currentThread.mainAttachment) {
        attachedContext = nullptr;
        currentThread.thread.reset();
    }
}
}
