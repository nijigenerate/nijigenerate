#pragma once
#include <string_view>
namespace nijigenerate::backend::runtime_gl {
// The independent production source deliberately disables buffer fences to avoid
// per-draw glFenceSync overhead. These functions preserve that explicit policy.
inline void waitForBuffer(unsigned, std::string_view) {}
inline void markBufferInUse(unsigned) {}
}
