#pragma once

#include <nijigenerate/compat/retained_string.hpp>

namespace nijigenerate::commands {

struct CommandScopeTransition {
    compat::RetainedString commandId;
    compat::RetainedString description;
    bool operator==(const CommandScopeTransition&) const = default;
};

} // namespace nijigenerate::commands
