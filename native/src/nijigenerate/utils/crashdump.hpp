#pragma once

#include "nijigenerate/compat/retained_string_slice.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include <memory>

namespace nijigenerate::utils::crashdump {

void ngSetCrashDumpDirOverrideForTests(compat::RetainedStringSlice dir);
compat::RetainedStringSlice linuxStateHome();
compat::RetainedStringSlice getCrashDumpDir();
compat::RetainedStringSlice genCrashDumpPath(compat::RetainedStringSlice filename);
void mkdirCrashDumpDir();
void notifyCrashUser(compat::RetainedStringSlice dumpPath);
void installNativeCrashDumpHandler();
void installNativeCrashDumpThreadHandler();

template<class... State>
compat::RetainedString genCrashDump(const compat::Throwable& throwable, State&&... state);

template<class... State>
compat::RetainedString genCrashDump(std::shared_ptr<const compat::Throwable> throwable, State&&... state);

template<class... State>
compat::RetainedStringSlice writeCrashDump(compat::RetainedStringSlice filename,
    const compat::Throwable& throwable, State&&... state);

template<class... State>
void crashdump(const compat::Throwable& throwable, State&&... state);

template<class... State>
compat::RetainedStringSlice writeCrashDump(compat::RetainedStringSlice filename,
    std::shared_ptr<const compat::Throwable> throwable, State&&... state);

template<class... State>
void crashdump(std::shared_ptr<const compat::Throwable> throwable, State&&... state);

} // namespace nijigenerate::utils::crashdump

#include "crashdump_text.hpp"

// File-output and fallback definitions retain their own OS implementation gates.
// Common report text is available independently of those platform bodies.
#if defined(_WIN32)
#include "crashdump_windows_state.hpp"
#include "crashdump_report.hpp"
#elif defined(__unix__) || defined(__APPLE__)
#include "crashdump_posix_state.hpp"
#include "crashdump_report.hpp"
#endif
