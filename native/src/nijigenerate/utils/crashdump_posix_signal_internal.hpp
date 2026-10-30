#pragma once

#include <signal.h>

namespace nijigenerate::utils::crashdump::detail {

void writeNativeCrashDumpFromSignal(int signal);
void notifyNativeCrashUserAfterSignal() noexcept;
extern "C" void nativeCrashSignalHandler(int signal, siginfo_t* info, void* context);
void installNativeCrashHandlerFor(int signal);

} // namespace nijigenerate::utils::crashdump::detail
