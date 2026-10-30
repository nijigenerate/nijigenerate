#pragma once
// Used instant/Duration subset of independent LDC 1.41 Phobos std.datetime.systime
// (Jonathan M Davis) and core.time. Boost Software License 1.0, full text in
// port/licenses/mcp-http-transport.txt. No timezone API is implemented here.
#include <nijigenerate/compat/throwable.hpp>
#include <bit>
#include <chrono>
#include <compare>
#include <cstdint>
#include <limits>
#include <ratio>
#include <type_traits>

namespace nijigenerate::api::mcp::http_transport::detail {

class SourceAuthTimeException : public compat::ThrowableException {
public:
    SourceAuthTimeException()
        : ThrowableException(compat::RetainedString::fromStaticLiteral(
              "The given FILETIME cannot be represented as a stdTime value."),
              compat::RetainedString::fromStaticLiteral("std/datetime/systime.d"), 10134) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "core.time.TimeException"; }
};

// SysTime's used observable state is the UTC count; comparisons ignore timezone.
class SourceSysTime {
    std::int64_t stdTime_ = 0;
public:
    constexpr SourceSysTime() noexcept = default;
    static constexpr SourceSysTime fromStdTime(std::int64_t ticks) noexcept {
        SourceSysTime value;
        value.stdTime_ = ticks;
        return value;
    }
    constexpr std::int64_t stdTime() const noexcept { return stdTime_; }
    constexpr auto operator<=>(const SourceSysTime&) const noexcept = default;
    constexpr bool operator==(const SourceSysTime&) const noexcept = default;

    // The actual source durations are integral days/seconds (100 ns multiples).
    template<class Rep, class Period>
        requires (std::is_integral_v<Rep> && sizeof(Rep) <= sizeof(std::uint64_t) &&
            std::ratio_divide<Period, std::ratio<1, 10'000'000>>::den == 1)
    friend constexpr SourceSysTime operator+(SourceSysTime time,
            std::chrono::duration<Rep, Period> duration) noexcept {
        using Scale = std::ratio_divide<Period, std::ratio<1, 10'000'000>>;
        const auto ticks = static_cast<std::uint64_t>(duration.count())
            * static_cast<std::uint64_t>(Scale::num);
        return fromStdTime(std::bit_cast<std::int64_t>(
            static_cast<std::uint64_t>(time.stdTime_) + ticks));
    }
};

// No separate clock state: read the real standard realtime backend on every call.
struct SourceAuthClock {
    using time_point = SourceSysTime;
    static time_point now() {
        using Backend = std::chrono::system_clock;
        using Rep = Backend::duration::rep;
        using Scale = std::ratio_divide<Backend::duration::period, std::ratio<1, 10'000'000>>;
        static_assert(std::is_integral_v<Rep> && std::is_signed_v<Rep> && sizeof(Rep) <= 8);
        static_assert(Scale::num == 1 || Scale::den == 1,
            "The selected realtime backend must have an integral 100ns conversion ratio");
        const auto count = static_cast<std::int64_t>(Backend::now().time_since_epoch().count());
        std::int64_t ticks;
        if constexpr (Scale::den == 1) {
            ticks = std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(count)
                * static_cast<std::uint64_t>(Scale::num));
        } else {
            // POSIX sec+nsec conversion discards fractional 100ns toward -infinity.
            constexpr auto divisor = static_cast<std::int64_t>(Scale::den);
            ticks = count / divisor;
            if (count < 0 && count % divisor != 0) --ticks;
        }
        constexpr std::int64_t unixEpochStdTime = 621'355'968'000'000'000;
#if defined(_WIN32)
        // Source FILETIMEToStdTime rejects instants beyond the signed stdTime range.
        if (ticks > std::numeric_limits<std::int64_t>::max() - unixEpochStdTime)
            throw SourceAuthTimeException();
#endif
        return time_point::fromStdTime(std::bit_cast<std::int64_t>(
            static_cast<std::uint64_t>(ticks) + static_cast<std::uint64_t>(unixEpochStdTime)));
    }
};

} // namespace nijigenerate::api::mcp::http_transport::detail
