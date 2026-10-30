#pragma once
#include "retained_string.hpp"

namespace nijigenerate::compat::phobos {
// Used Clock.currTime(LocalTime).toISOString profile, backed by the real OS clock.
RetainedString ngLocalIsoTimestamp();
} // namespace nijigenerate::compat::phobos
