#pragma once

#include <atomic>
#include <cstdint>

namespace nijigenerate::autorig::framework::detail {

// D shared module storage: one process-wide identity, accessed atomically.
inline std::atomic<std::uint64_t> nativeBlobBytes{};
inline std::atomic<std::uint64_t> nativeBlobCount{};

} // namespace nijigenerate::autorig::framework::detail
