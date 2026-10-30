#pragma once
// Used core.time MonoTime/Duration subset. BSL-1.0; see port/licenses/phobos-monotime.txt.
#include <nijigenerate/compat/retained_string.hpp>
#include <chrono>
#include <compare>
#include <cstdint>
#include <ratio>

namespace nijigenerate::compat {

class SourceMonoTime {
    std::int64_t ticks_{};
public:
    using Duration = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;
    constexpr SourceMonoTime() = default;
    static constexpr SourceMonoTime fromTicks(std::int64_t value) {
        SourceMonoTime result;
        result.ticks_ = value;
        return result;
    }
    constexpr std::int64_t ticks() const { return ticks_; }
    static std::int64_t ticksPerSecond();
    RetainedString toString() const;
    auto operator<=>(const SourceMonoTime&) const = default;
    friend Duration operator-(SourceMonoTime lhs, SourceMonoTime rhs);
};

struct SourceMonotonicClock {
    using time_point = SourceMonoTime;
    static time_point now();
};

} // namespace nijigenerate::compat
