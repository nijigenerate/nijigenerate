#include "automesh_batch_stack.hpp"
#include <boost/context/stack_traits.hpp>

namespace nijigenerate::platform {

std::size_t ngAutoMeshBatchFiberStackBytes() {
#if defined(_WIN32) || (defined(__APPLE__) && defined(__x86_64__))
    constexpr std::size_t defaultStackPages = 8;
#else
    constexpr std::size_t defaultStackPages = 4;
#endif
    return boost::context::stack_traits::page_size() * defaultStackPages * 4;
}

} // namespace nijigenerate::platform
