#include "native_arithmetic_fault.hpp"

#include <cstdlib>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#endif

namespace nijigenerate::compat {

[[noreturn]] void ngFailIntegerDivideByZero() {
#if defined(_WIN32)
    ::RaiseException(EXCEPTION_INT_DIVIDE_BY_ZERO, 0, 0, nullptr);
#else
    std::raise(SIGFPE);
#endif
    std::abort();
}

} // namespace nijigenerate::compat
