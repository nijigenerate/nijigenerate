#include "autosave.hpp"
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <chrono>
#include <new>
#include <source_location>
#include <string>
#include <string_view>

namespace nijigenerate::io::autosave {
namespace {

struct AutosaveTimerState {
    using Clock = std::chrono::steady_clock;
    Clock::time_point startedAt{};
    Clock::duration elapsed{};
    bool running = false;
};

thread_local AutosaveTimerState autosaveTimer;
constexpr std::string_view InProjectLockfile = "nijigenerate-project.lock";

AutosaveString formatEpochSeconds(std::int64_t seconds) {
    try {
        return AutosaveString(compat::RetainedString(std::to_string(seconds)));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

} // namespace

namespace detail {
std::string_view lockfileName() noexcept { return InProjectLockfile; }
std::int64_t elapsedAutosaveMinutes() {
    auto elapsed = autosaveTimer.elapsed;
    if (autosaveTimer.running) elapsed += AutosaveTimerState::Clock::now() - autosaveTimer.startedAt;
    return std::chrono::duration_cast<std::chrono::minutes>(elapsed).count();
}
void resetAutosaveTimer() {
    if (autosaveTimer.running) autosaveTimer.startedAt = AutosaveTimerState::Clock::now();
    autosaveTimer.elapsed = {};
}
}

void startAutosaveTimer() {
#ifndef NDEBUG
    if (autosaveTimer.running) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("start was called when the StopWatch was already running."),
        compat::RetainedString::fromStaticLiteral("std/datetime/stopwatch.d"), 181);
#endif
    autosaveTimer.running = true;
    autosaveTimer.startedAt = AutosaveTimerState::Clock::now();
}

AutosaveString bakStampString() {
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return formatEpochSeconds(seconds);
}


} // namespace nijigenerate::io::autosave
