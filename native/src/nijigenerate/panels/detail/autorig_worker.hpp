#pragma once

#include <nijigenerate/compat/shared_delegate.hpp>
#include <memory>

namespace nijigenerate::panels::autorig::detail {

// The actual application worker: construction does not start its OS thread.
// Used operations follow core/thread/osthread.d (Sean Kelly, BSL-1.0).
class AutoRigWorker final : public std::enable_shared_from_this<AutoRigWorker> {
    struct State;
    std::unique_ptr<State> state_;
    void execute() noexcept;
public:
    explicit AutoRigWorker(compat::SharedDelegate<void()> action);
    ~AutoRigWorker();
    AutoRigWorker(const AutoRigWorker&) = delete;
    AutoRigWorker& operator=(const AutoRigWorker&) = delete;
    void start();
    bool isRunning() const noexcept;
    void join();
    static void sleepOneMillisecond();
};

} // namespace nijigenerate::panels::autorig::detail
