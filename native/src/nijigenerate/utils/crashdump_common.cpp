#include "crashdump.hpp"
#include "crashdump_internal.hpp"

namespace nijigenerate::utils::crashdump {
namespace detail {
thread_local compat::RetainedStringSlice crashDumpDirOverride;
}

void ngSetCrashDumpDirOverrideForTests(compat::RetainedStringSlice dir) {
    detail::crashDumpDirOverride = dir;
}

#if !defined(_WIN32) && !defined(__unix__) && !defined(__APPLE__)
void installNativeCrashDumpHandler() {}
void installNativeCrashDumpThreadHandler() {}
#endif

} // namespace nijigenerate::utils::crashdump
