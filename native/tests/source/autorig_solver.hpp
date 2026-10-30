#pragma once
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>

namespace autorig_solver {
// Existing tests/autorig_solver.d entry; this does not add or run any cases.
void ngMain(nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> args);
}
