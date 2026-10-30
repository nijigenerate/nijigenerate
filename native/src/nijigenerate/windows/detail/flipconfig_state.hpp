#pragma once

#include <nijigenerate/windows/flipconfig.hpp>

namespace nijigenerate::windows::flipconfig::detail {

// Logical module-private TLS: actual load/save/mutation bodies must use these
// same descriptors. FlipPair stays the canonical incomplete real class.
extern thread_local compat::Slice<std::shared_ptr<FlipPair>> flipPairs;
extern thread_local std::shared_ptr<nicxlive::core::Puppet> activePuppet;

} // namespace nijigenerate::windows::flipconfig::detail
