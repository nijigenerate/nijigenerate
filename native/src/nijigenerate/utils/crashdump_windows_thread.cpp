#include "crashdump.hpp"

namespace nijigenerate::utils::crashdump {

// The original Windows thread installer is intentionally empty.
void installNativeCrashDumpThreadHandler() {}

} // namespace nijigenerate::utils::crashdump
