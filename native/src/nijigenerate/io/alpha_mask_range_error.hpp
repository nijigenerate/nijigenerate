#pragma once
#include <cstddef>
namespace nijigenerate::io::depthimage::detail {
// Checked D array indexing reports an Error rather than an ordinary Exception.
struct AlphaMaskRangeError {
    std::size_t index;
    std::size_t length;
};
}
