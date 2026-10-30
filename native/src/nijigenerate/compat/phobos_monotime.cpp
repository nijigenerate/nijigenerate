#include "phobos_monotime.hpp"
#include "assert_error.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include <bit>
#include <charconv>
#include <cstdlib>
#include <limits>
#include <string_view>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#include <unistd.h>
#else
#include <time.h>
#include <unistd.h>
#endif

namespace nijigenerate::compat {
namespace {
std::int64_t add(std::int64_t lhs, std::int64_t rhs) {
    return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(lhs) + static_cast<std::uint64_t>(rhs));
}
std::int64_t multiply(std::int64_t lhs, std::int64_t rhs) {
    return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(lhs) * static_cast<std::uint64_t>(rhs));
}
std::int64_t convertFrequency(std::int64_t ticks, std::int64_t source, std::int64_t destination) {
    return add(multiply(ticks / source, destination), multiply(ticks % source, destination) / source);
}
[[noreturn]] void abortTime(std::string_view message, unsigned line) {
#if defined(_WIN32)
    const auto handle = ::GetStdHandle(STD_ERROR_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
        ngAssertUnreachable("Cannot get stderr handle for message", "core/internal/abort.d", 33);
    const auto write = [handle](std::string_view part) {
        ::WriteFile(handle, part.data(), static_cast<DWORD>(part.size()), nullptr, nullptr);
    };
#else
    const auto write = [](std::string_view part) { ::write(2, part.data(), part.size()); };
#endif
    char number[20];
    const auto end = std::to_chars(number, number + sizeof(number), line).ptr;
    write("Aborting from "); write("core/time.d"); write("(");
    write(std::string_view(number, static_cast<std::size_t>(end - number)));
    write(") "); write(message);
    std::abort();
}
std::int64_t normalFrequency() {
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    return ::QueryPerformanceFrequency(&frequency) ? frequency.QuadPart : 0;
#elif defined(__APPLE__)
    mach_timebase_info_data_t info;
    if (::mach_timebase_info(&info) != 0) abortTime("Failed in mach_timebase_info().", 3876);
    const auto scaled = multiply(1'000'000'000, info.denom);
    if (scaled % info.numer != 0)
        abortTime("Non integral ticksPerSecond from mach_timebase_info.", 3880);
    return scaled / info.numer;
#else
    timespec resolution;
    if (::clock_getres(CLOCK_MONOTONIC, &resolution) != 0) return 0;
    return resolution.tv_sec != 0 || resolution.tv_nsec <= 0 || resolution.tv_nsec >= 1000 ?
        1'000'000'000 : 1'000'000'000 / resolution.tv_nsec;
#endif
}
// The source initializes the selected clock frequency before application work.
const std::int64_t frequency = normalFrequency();
}

std::int64_t SourceMonoTime::ticksPerSecond() { return frequency; }
RetainedString SourceMonoTime::toString() const {
    using namespace core::translation;
    const std::array<TypedFormatArgument, 2> arguments{
        ngFormatArgument(ticks()), ngFormatArgument(ticksPerSecond())};
    return core::translation::detail::retainFormatMessage(
        ngFormatTyped("MonoTime(%s ticks, %s ticks per second)", arguments));
}
SourceMonoTime::Duration operator-(SourceMonoTime lhs, SourceMonoTime rhs) {
    const auto difference = std::bit_cast<std::int64_t>(
        static_cast<std::uint64_t>(lhs.ticks_) - static_cast<std::uint64_t>(rhs.ticks_));
    return SourceMonoTime::Duration(convertFrequency(difference, frequency, 10'000'000));
}
SourceMonotonicClock::time_point SourceMonotonicClock::now() {
    if (frequency == 0)
        abortTime("MonoTimeImpl!(ClockType.normal) failed to get the frequency of the system's monotonic clock.",
            2170);
#if defined(_WIN32)
    LARGE_INTEGER ticks;
    ::QueryPerformanceCounter(&ticks);
    return time_point::fromTicks(ticks.QuadPart);
#elif defined(__APPLE__)
    return time_point::fromTicks(std::bit_cast<std::int64_t>(::mach_absolute_time()));
#else
    timespec value;
    const auto error = ::clock_gettime(CLOCK_MONOTONIC, &value);
    if constexpr (std::numeric_limits<decltype(value.tv_sec)>::max() < std::numeric_limits<std::int64_t>::max()) {
        if (error) abortTime("Call to clock_gettime failed.", 2193);
    }
    return time_point::fromTicks(convertFrequency(
        add(multiply(value.tv_sec, 1'000'000'000), value.tv_nsec), 1'000'000'000, frequency));
#endif
}
} // namespace nijigenerate::compat
