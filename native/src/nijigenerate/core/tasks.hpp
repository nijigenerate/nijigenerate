#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <functional>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::core::tasks {

using TaskString = std::optional<std::string>;
using TaskWorker = compat::SharedDelegate<void()>;

namespace detail {
inline compat::RetainedString retainNativeTaskString(TaskString value) {
    if (!value) return {};
    try {
        return compat::RetainedString(std::move(*value));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline TaskString copyNativeTaskString(compat::RetainedString value) {
    if (value.isNull()) return {};
    try {
        return TaskString{std::string(value.view())};
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

class TaskFiber {
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    struct SourceReceiver {};
    explicit TaskFiber(SourceReceiver) {}
    void initializeSource(TaskWorker worker);
public:
    enum class State { hold, exec, term };
    enum class Kind { editor, vibe, mcp = vibe };
    explicit TaskFiber(std::function<void()> worker);
    TaskFiber(std::function<void()> worker, std::size_t stackBytes);
    TaskFiber(std::function<void()> worker, Kind kind);
    TaskFiber(std::function<void()> worker, std::size_t stackBytes, Kind kind);
    ~TaskFiber();
    TaskFiber(const TaskFiber&) = delete;
    TaskFiber& operator=(const TaskFiber&) = delete;
    template<class Producer> static std::shared_ptr<TaskFiber> createSourceDeferred(Producer&& produce) {
        auto owner = std::allocate_shared<TaskFiber>(
            nicxlive::core::d_runtime::SourceAllocator<TaskFiber>{}, SourceReceiver{});
        owner->initializeSource(TaskWorker(std::invoke(std::forward<Producer>(produce))));
        return owner;
    }
    State state() const;
    void call();
    static bool hasCurrent() noexcept;
    static void yield();
    static bool isMcpTask();
    // Query the actual current Vibe task, rather than any core/editor Fiber.
    static bool isVibeTask();
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

struct Task {
    compat::RetainedString name;
    std::shared_ptr<TaskFiber> worker;
};

// An instance is driven cooperatively on one thread, like the D UI scheduler.
class TaskScheduler {
public:
    void add(TaskString name, std::function<void()> worker);
    void addSource(compat::RetainedString name, TaskWorker worker);
    void status(TaskString value) {
        statusSource(detail::retainNativeTaskString(std::move(value)));
    }
    TaskString status() const {
        return detail::copyNativeTaskString(status_);
    }
    void statusSource(compat::RetainedString value) { status_ = std::move(value); }
    compat::RetainedString statusSource() const { return status_; }
    void progress(float value) { progress_ = value; }
    float progress() const { return progress_; }
    std::size_t length() const { return tasks_.size(); }
    compat::Slice<Task> entries() const { return tasks_; }
    void update();
private:
    compat::Slice<Task> tasks_;
    compat::RetainedString status_ = compat::RetainedString::fromStaticLiteral("No pending tasks...");
    float progress_ = -1;
};

void ngTaskAdd(TaskString name, std::function<void()> worker);
void ngTaskAddSource(compat::RetainedString name, TaskWorker worker);
void ngTaskStatus(TaskString status);
TaskString ngTaskGetStatus();
void ngTaskStatusSource(compat::RetainedString status);
compat::RetainedString ngTaskGetStatusSource();
float ngTaskGetProgress();
void ngTaskProgress(float progress);
std::size_t ngTaskLength();
void ngTaskYield();
void ngTaskUpdate();

inline constexpr double statusTimeSet = 15.0;
// Notification definitions require the native M307/ImGui implementation.
void ngHoverStatus(TaskString status);
void ngSetStatus(TaskString status);
void ngHoverStatus(compat::RetainedStringSlice status);
void ngSetStatus(compat::RetainedStringSlice status);
compat::RetainedStringSlice ngGetStatus();
void ngStatusUpdate();

} // namespace nijigenerate::core::tasks

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::tasks::Task> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.core.tasks.Task)"),
            RetainedString::fromStaticLiteral("(Task)"),
            RetainedString::fromStaticLiteral("nijigenerate.core.tasks.Task"),
            RetainedString::fromStaticLiteral("Task")};
    }
};
} // namespace nicxlive::core::d_runtime
