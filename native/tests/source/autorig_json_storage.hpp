#pragma once
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>

namespace autorig_json_storage_test {
// Port of the existing tests/autorig_json_storage.d entry point, including argv[0].
void ngMain(nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> arguments);
}
