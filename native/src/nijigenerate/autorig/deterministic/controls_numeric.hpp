#pragma once
#include <cstddef>

namespace nijigenerate::autorig::deterministic::controls_detail {
// Keep the original cast operation and its truncation, without an added
// pre-conversion exception or an invented out-of-domain integer sentinel.
inline std::size_t sourceControlIndex(double value) {
    return static_cast<std::size_t>(value);
}
}
