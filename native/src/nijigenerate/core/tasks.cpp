#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <boost/context/fiber.hpp>
#include <boost/context/fixedsize_stack.hpp>
#include <boost/context/detail/exception.hpp>
#include <exception>
#include <new>
#include <source_location>
#include <utility>

namespace nijigenerate::core::tasks {

namespace {
template<class T, class... Args> std::shared_ptr<T> allocateTaskObject(Args&&... args) {
    try {
        return std::make_shared<T>(std::forward<Args>(args)...);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

struct TaskFiber::Impl {
    State state = State::hold;
    Kind kind = Kind::editor;
    boost::context::fiber caller;
    boost::context::fiber context;
    std::exception_ptr failure;
    static thread_local Impl* active;

    auto body(std::function<void()> worker) {
        return [this, worker = std::move(worker)](boost::context::fiber&& parent) {
            caller = std::move(parent);
            try {
                worker();
            } catch (const boost::context::detail::forced_unwind&) {
                // Boost must unwind a suspended stack when its owner is destroyed.
                throw;
            } catch (...) {
                failure = std::current_exception();
            }
            state = State::term;
            return std::move(caller);
        };
    }
    explicit Impl(std::function<void()> worker, Kind value = Kind::editor)
        : kind(value), context(body(std::move(worker))) {}
    Impl(std::function<void()> worker, std::size_t stackBytes, Kind value = Kind::editor)
        : kind(value), context(std::allocator_arg, boost::context::fixedsize_stack(stackBytes), body(std::move(worker))) {}
};

thread_local TaskFiber::Impl* TaskFiber::Impl::active = nullptr;

TaskFiber::TaskFiber(std::function<void()> worker) : impl_(allocateTaskObject<Impl>(std::move(worker))) {}
TaskFiber::TaskFiber(std::function<void()> worker, std::size_t stackBytes)
    : impl_(allocateTaskObject<Impl>(std::move(worker), stackBytes)) {}
TaskFiber::TaskFiber(std::function<void()> worker, Kind kind)
    : impl_(allocateTaskObject<Impl>(std::move(worker), kind)) {}
TaskFiber::TaskFiber(std::function<void()> worker, std::size_t stackBytes, Kind kind)
    : impl_(allocateTaskObject<Impl>(std::move(worker), stackBytes, kind)) {}
void TaskFiber::initializeSource(TaskWorker worker) {
    // Only the native callable's storage is allocated here; never invoke it.
    std::function<void()> retained;
    try { retained = [worker] { worker(); }; }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    impl_ = allocateTaskObject<Impl>(std::move(retained));
}
TaskFiber::~TaskFiber() = default;
TaskFiber::State TaskFiber::state() const { return impl_->state; }
bool TaskFiber::isVibeTask() { return Impl::active && Impl::active->kind == Kind::vibe; }
bool TaskFiber::isMcpTask() { return isVibeTask(); }

void TaskFiber::call() {
    const auto current=impl_;
#ifndef NDEBUG
    if (current->state != State::hold)
        throw compat::ngStaticError<compat::AssertError>("Assertion failure", "core/thread/fiber/base.d", 435);
#endif
    if (current->state == State::term) return;
    auto* previous = Impl::active;
    Impl::active = current.get();
    current->state = State::exec;
    try {
        current->context = std::move(current->context).resume();
    } catch (...) {
        Impl::active = previous;
        throw;
    }
    Impl::active = previous;
    if (current->failure) std::rethrow_exception(std::exchange(current->failure, {}));
}

bool TaskFiber::hasCurrent() noexcept { return Impl::active != nullptr; }

void TaskFiber::yield() {
    auto* running = Impl::active;
#ifndef NDEBUG
    if (!running)
        throw compat::ngStaticError<compat::AssertError>(
            "Fiber.yield() called with no active fiber", "core/thread/fiber/base.d", 586);
    if (running->state != State::exec)
        throw compat::ngStaticError<compat::AssertError>("Assertion failure", "core/thread/fiber/base.d", 587);
#endif
    if (!running) compat::ngFailNullAccess(true);
    running->state = State::hold;
    running->caller = std::move(running->caller).resume();
    running->state = State::exec;
}

void TaskScheduler::add(TaskString name, std::function<void()> worker) {
    addSource(detail::retainNativeTaskString(std::move(name)), std::move(worker));
}

void TaskScheduler::addSource(compat::RetainedString name, TaskWorker worker) {
    // The native Fiber transports one Source delegate descriptor. Copies of this
    // bridge retain its callable environment rather than cloning captured state.
    tasks_.append(Task{std::move(name), allocateTaskObject<TaskFiber>([worker] { worker(); })});
}

void TaskScheduler::update() {
    if (tasks_.empty()) return;
    // A worker may append and replace the backing slice while it executes.
    auto worker = compat::ngRequireObject(tasks_[0].worker);
    if (worker->state() != TaskFiber::State::term) {
        worker->call();
    } else {
        tasks_ = tasks_.slice(1, tasks_.size());
        progress_ = -1;
    }
    if (tasks_.empty()) statusSource(compat::RetainedString::fromStaticLiteral("No pending tasks..."));
}

namespace {
TaskScheduler scheduler;
}
void ngTaskAdd(TaskString name, std::function<void()> worker) { scheduler.add(std::move(name), std::move(worker)); }
void ngTaskAddSource(compat::RetainedString name, TaskWorker worker) {
    scheduler.addSource(std::move(name), std::move(worker));
}
void ngTaskStatus(TaskString status) { scheduler.status(std::move(status)); }
TaskString ngTaskGetStatus() { return scheduler.status(); }
void ngTaskStatusSource(compat::RetainedString status) { scheduler.statusSource(std::move(status)); }
compat::RetainedString ngTaskGetStatusSource() { return scheduler.statusSource(); }
float ngTaskGetProgress() { return scheduler.progress(); }
void ngTaskProgress(float progress) { scheduler.progress(progress); }
std::size_t ngTaskLength() { return scheduler.length(); }
void ngTaskYield() { TaskFiber::yield(); }
void ngTaskUpdate() { scheduler.update(); }

} // namespace nijigenerate::core::tasks
