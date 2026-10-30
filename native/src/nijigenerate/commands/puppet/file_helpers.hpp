#pragma once

#include "nijigenerate/compat/retained_string_slice.hpp"
#include <memory>

namespace nicxlive::core { class Puppet; }
namespace nijigenerate::commands { class CommandResult; }

namespace nijigenerate::commands::puppet::file::detail {

double ngRealtimeNow();
std::shared_ptr<CommandResult> ngMissingPathResult(compat::RetainedStringSlice what =
    compat::RetainedString::fromStaticLiteral("Path"));
compat::RetainedStringSlice ngUniqueAnimationName(std::shared_ptr<nicxlive::core::Puppet> puppet,
                                                 compat::RetainedStringSlice baseName);

} // namespace nijigenerate::commands::puppet::file::detail
