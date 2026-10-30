#pragma once
// Compatibility spellings of the one canonical shared SDK default generator.
#include "assert_error.hpp"
#include <compat/default_random.hpp>

namespace nijigenerate::compat {
using ::nicxlive::core::d_runtime::RandomSeedError;
using ::nicxlive::core::d_runtime::DefaultRandom;
using ::nicxlive::core::d_runtime::ngDefaultRandom;
using ::nicxlive::core::d_runtime::ngRandomUlongExcludingMax;
using ::nicxlive::core::d_runtime::ngRandomUintExcludingMax;
}