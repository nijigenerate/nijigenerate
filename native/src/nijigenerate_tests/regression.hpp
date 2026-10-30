#pragma once
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <cstdint>

namespace nijigenerate_tests::regression {
// Translated original program entry; no scenario is executed by including it.
std::int32_t ngRegressionMain(nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> args);
}
