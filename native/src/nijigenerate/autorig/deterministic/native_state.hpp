#pragma once

#include <nijigenerate/autorig/framework.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {

// Module-internal TLS storage shared by the real native.d consumers only.
extern thread_local framework::Table<framework::Text> liveRigSignatures;

} // namespace nijigenerate::autorig::deterministic::native_detail
