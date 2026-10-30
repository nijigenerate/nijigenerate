#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/phobos_monotime.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <atomic>
#include <exception>
#include <cstddef>
#include <memory>
#include <mutex>
#if !defined(_WIN32)
#include <pthread.h>
#endif
#include <compat/source_allocator.hpp>

namespace nijigenerate::core::thread_context {
// Used core.thread ThreadBase/Thread surface, attached by the real native entry.
// Reference-class objects are never copied, moved, or synthesized by a logger.
class Thread;
class ThreadBase {
    std::recursive_mutex monitor_;
    compat::RetainedString name_;
protected:
    ThreadBase() = default;
public:
    virtual ~ThreadBase() = default;
    ThreadBase(const ThreadBase&) = delete;
    ThreadBase& operator=(const ThreadBase&) = delete;
    ThreadBase(ThreadBase&&) = delete;
    ThreadBase& operator=(ThreadBase&&) = delete;
    compat::RetainedString name();
    void name(compat::RetainedString value);
    static std::shared_ptr<ThreadBase> getThis() noexcept;
    // Inherited Source Object methods, after the existing virtual slots.
    virtual compat::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<ThreadBase> other);
    virtual int opCmp(std::shared_ptr<ThreadBase> other);
};
class Thread : public ThreadBase, public std::enable_shared_from_this<Thread> {
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    friend void ngRegisterMainThread();
    friend class ManagedThreadScope;
    Thread() = default;
    compat::SharedDelegate<void()> call_;
#ifdef _WIN32
    void* workerHandle_{};
#else
    pthread_t workerHandle_{};
#endif
    bool hasWorker_ = false;
    std::shared_ptr<Thread> launchOwner_;
    std::atomic<bool> running_{false};
    std::exception_ptr unhandledException_;
    compat::Throwable* unhandledPointer_ = nullptr;
    std::shared_ptr<compat::Throwable> unhandled_;
    void initialize(compat::SharedDelegate<void()> call);
    void runSource();
#ifdef _WIN32
    static unsigned __stdcall workerEntry(void* argument);
#else
    static void* workerEntry(void* argument);
#endif
public:
    ~Thread() override;
    template<class Producer> static std::shared_ptr<Thread> createSourceDeferred(Producer&& produce) {
        auto owner = std::allocate_shared<Thread>(nicxlive::core::d_runtime::SourceAllocator<Thread>{});
        owner->initialize(compat::SharedDelegate<void()>(std::invoke(std::forward<Producer>(produce))));
        return owner;
    }
    static std::shared_ptr<Thread> createSource(compat::SharedDelegate<void()> call) {
        return createSourceDeferred([&] { return call; });
    }
    std::shared_ptr<Thread> start();
    std::shared_ptr<compat::Throwable> join(bool rethrow = true);
    static void sleep(compat::SourceMonoTime::Duration duration);
    static std::shared_ptr<Thread> getThis() noexcept;
};

bool ngThreadObjectEquals(std::shared_ptr<ThreadBase> left, std::shared_ptr<ThreadBase> right);

// Called by the real UI startup, never lazily inferred from an arbitrary caller.
void ngRegisterMainThread();
bool ngHasManagedThread();
bool ngIsMainThread();
// Source callers which explicitly treat Thread.getThis()==null as main use this.
bool ngIsMainThreadOrUnattached();
class ManagedThreadScope {
public:
    ManagedThreadScope();
    ~ManagedThreadScope();
    ManagedThreadScope(const ManagedThreadScope&)=delete;
    ManagedThreadScope& operator=(const ManagedThreadScope&)=delete;
private:
    friend class Thread;
    explicit ManagedThreadScope(std::shared_ptr<Thread> thread);
    bool explicitAttachment_ = false;
    std::shared_ptr<Thread> previousThread_;
};
}
