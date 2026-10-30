// Used std.datetime Clock/LocalTime/ISO implementation, Jonathan M Davis.
// BSL-1.0: port/licenses/phobos-monotime.txt.
#include "phobos_local_timestamp.hpp"
#include "core_time_exception.hpp"
#include "assert_error.hpp"
#include <bit>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <limits>
#include <mutex>
#include <string>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__APPLE__)
#include <sys/time.h>
#else
#include <time.h>
#endif

namespace nijigenerate::compat::phobos {
namespace {
constexpr std::int64_t ticksPerSecond = 10'000'000;
constexpr std::int64_t ticksPerDay = 86400 * ticksPerSecond;
constexpr std::int64_t unixEpoch = 621'355'968'000'000'000;
constexpr std::int64_t unixEpochDays = unixEpoch / ticksPerDay;
std::once_flag localZoneInitialized;

std::int64_t add(std::int64_t a, std::int64_t b) {
    return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b));
}
std::int64_t multiply(std::int64_t a, std::int64_t b) {
    return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b));
}
struct Calendar {
    int year;
    unsigned month, day, hour, minute, second;
    std::uint32_t fraction;
};
Calendar calendar(std::int64_t ticks) {
    auto days = ticks / ticksPerDay;
    auto remainder = ticks % ticksPerDay;
    if (remainder < 0) { remainder += ticksPerDay; --days; }
    const std::chrono::year_month_day date{
        std::chrono::sys_days{std::chrono::days{days - unixEpochDays}}};
    const auto hour = static_cast<unsigned>(remainder / (3600 * ticksPerSecond));
    remainder %= 3600 * ticksPerSecond;
    const auto minute = static_cast<unsigned>(remainder / (60 * ticksPerSecond));
    remainder %= 60 * ticksPerSecond;
    return {int(date.year()), unsigned(date.month()), unsigned(date.day()), hour, minute,
        static_cast<unsigned>(remainder / ticksPerSecond), static_cast<std::uint32_t>(remainder % ticksPerSecond)};
}
std::int64_t currentStdTime() {
#if defined(_WIN32)
    FILETIME value;
    GetSystemTimeAsFileTime(&value);
    const auto ticks = (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
    return std::bit_cast<std::int64_t>(ticks + 504'911'232'000'000'000ULL);
#elif defined(__APPLE__)
    timeval value;
    (void)::gettimeofday(&value, nullptr);
    return add(add(multiply(value.tv_sec, ticksPerSecond), multiply(value.tv_usec, 10)), unixEpoch);
#else
#if defined(__OpenBSD__)
    timespec value{};
#else
    timespec value;
#endif
    const auto error = ::clock_gettime(CLOCK_REALTIME, &value);
#if defined(__linux__)
    constexpr std::size_t sourceLine = 272;
#elif defined(__FreeBSD__)
    constexpr std::size_t sourceLine = 296;
#elif defined(__NetBSD__)
    constexpr std::size_t sourceLine = 317;
#elif defined(__OpenBSD__)
    constexpr std::size_t sourceLine = 337;
#elif defined(__DragonFly__)
    constexpr std::size_t sourceLine = 360;
#elif defined(__sun)
    constexpr std::size_t sourceLine = 385;
#elif defined(__GNU__)
    constexpr std::size_t sourceLine = 412;
#else
#error "Clock.currStdTime: Unsupported OS"
#endif
#if defined(__OpenBSD__)
    constexpr bool checkError = true;
#else
    constexpr bool checkError = (std::numeric_limits<decltype(value.tv_sec)>::max)() <
        (std::numeric_limits<std::int64_t>::max)();
#endif
    if constexpr (checkError) {
        if (error) throw TimeException(RetainedString::fromStaticLiteral("Call to clock_gettime() failed"),
            RetainedString::fromStaticLiteral("std/datetime/systime.d"), sourceLine);
    }
    (void)error;
    return add(add(multiply(value.tv_sec, ticksPerSecond), value.tv_nsec / 100), unixEpoch);
#endif
}
#if defined(_WIN32)
std::int64_t wholeSeconds(const Calendar& value) {
    const auto date = std::chrono::year{value.year} / value.month / value.day;
    const auto days = std::chrono::sys_days{date}.time_since_epoch().count();
    return days * 86400 + value.hour * 3600 + value.minute * 60 + value.second;
}
bool daylightAt(const TIME_ZONE_INFORMATION& zone, std::int64_t ticks) {
    if (zone.DaylightDate.wMonth == 0) return false;
    auto date = calendar(ticks);
    if (date.year < 1601 || date.year > 30827) {
        if (date.month == 2 && date.day == 29) date.day = 28;
        date.year = date.year < 1601 ? 1601 : 30827;
    }
    // Preserve the original workaround at the edges of the calendar year.
    if (date.month == 1 && date.day == 1) date.day = 2;
    else if (date.month == 12 && date.day == 31) date.day = 30;
    SYSTEMTIME utc;
    utc.wYear = static_cast<WORD>(date.year);
    utc.wMonth = static_cast<WORD>(date.month);
    utc.wDay = static_cast<WORD>(date.day);
    utc.wHour = static_cast<WORD>(date.hour);
    utc.wMinute = static_cast<WORD>(date.minute);
    utc.wSecond = static_cast<WORD>(date.second);
    utc.wMilliseconds = 0;
    SYSTEMTIME local;
    const auto result = SystemTimeToTzSpecificLocalTime(&zone, &utc, &local);
#ifndef NDEBUG
    if (!result) throw ngStaticError<AssertError>(
        RetainedString::fromStaticLiteral("Failed to create SystemTimeToTzSpecificLocalTime"),
        RetainedString::fromStaticLiteral("std/datetime/timezone.d"), 3126);
#endif
    (void)result;
    const Calendar other{local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond, 0};
    const auto minutes = (wholeSeconds(date) - wholeSeconds(other)) / 60 - zone.Bias;
    if (minutes == zone.DaylightBias) return true;
#ifndef NDEBUG
    if (minutes != zone.StandardBias) throw ngStaticError<AssertError>(
        RetainedString::fromStaticLiteral("Unexpected difference"),
        RetainedString::fromStaticLiteral("std/datetime/timezone.d"), 3140);
#endif
    return false;
}
#endif
std::int64_t adjustedTime(std::int64_t ticks) {
#if defined(_WIN32)
    TIME_ZONE_INFORMATION zone;
    (void)GetTimeZoneInformation(&zone);
    TIME_ZONE_INFORMATION current;
    (void)GetTimeZoneInformation(&current);
    const auto offset = current.DaylightDate.wMonth != 0 && daylightAt(zone, ticks)
        ? zone.DaylightBias : zone.StandardBias;
    const auto bias = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(zone.Bias) + static_cast<std::uint32_t>(offset));
    return add(ticks, -multiply(bias, 60 * ticksPerSecond));
#else
    auto seconds = add(ticks, -unixEpoch) / ticksPerSecond;
    if constexpr (sizeof(time_t) < sizeof(seconds)) {
        if (seconds > (std::numeric_limits<time_t>::max)()) seconds = (std::numeric_limits<time_t>::max)();
        if (seconds < (std::numeric_limits<time_t>::min)()) seconds = (std::numeric_limits<time_t>::min)();
    }
    const auto unixTime = static_cast<time_t>(seconds);
    std::tm local;
    (void)::localtime_r(&unixTime, &local);
    return add(ticks, multiply(local.tm_gmtoff, ticksPerSecond));
#endif
}
void appendNumber(std::string& result, unsigned value, std::size_t width) {
    char bytes[20];
    const auto end = std::to_chars(bytes, bytes + sizeof(bytes), value).ptr;
    const auto length = static_cast<std::size_t>(end - bytes);
    if (length < width) result.append(width - length, '0');
    result.append(bytes, end);
}
}

RetainedString ngLocalIsoTimestamp() {
    // LocalTime's default argument initializes the C timezone before reading Clock.
    std::call_once(localZoneInitialized, [] {
#if defined(_WIN32)
        ::_tzset();
#else
        ::tzset();
#endif
    });
    const auto date = calendar(adjustedTime(currentStdTime()));
    std::string result;
    result.reserve(30);
    if (date.year < 0) result.push_back('-');
    else if (date.year >= 10000) result.push_back('+');
    appendNumber(result, static_cast<unsigned>(date.year < 0 ? -date.year : date.year), 4);
    appendNumber(result, date.month, 2);
    appendNumber(result, date.day, 2);
    result.push_back('T');
    appendNumber(result, date.hour, 2);
    appendNumber(result, date.minute, 2);
    appendNumber(result, date.second, 2);
    if (date.fraction) {
        result.push_back('.');
        appendNumber(result, date.fraction, 7);
        while (result.back() == '0') result.pop_back();
    }
    return RetainedString(std::move(result));
}
} // namespace nijigenerate::compat::phobos
