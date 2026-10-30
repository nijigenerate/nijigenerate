#pragma once

#include "nijigenerate/compat/retained_string_slice.hpp"

namespace nijigenerate::utils::crashdump::detail {

extern thread_local compat::RetainedStringSlice crashDumpDirOverride;
std::string ngCurrentCrashTimestamp();
std::string ngJoinCrashPath(std::string_view directory, std::string_view leaf);

} // namespace nijigenerate::utils::crashdump::detail
