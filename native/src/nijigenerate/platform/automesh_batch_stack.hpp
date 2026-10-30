#pragma once

#include <cstddef>

namespace nijigenerate::platform {

// Actual system page size times the source Fiber's platform page count times4.
std::size_t ngAutoMeshBatchFiberStackBytes();

} // namespace nijigenerate::platform
