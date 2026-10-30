#pragma once
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>

namespace autorig_head_neck_test {
// args includes the executable name, as in the original D module entry point.
void ngMain(nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> args);
}
