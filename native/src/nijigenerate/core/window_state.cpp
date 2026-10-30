#include "nijigenerate/core/window_state_internal.hpp"

namespace nijigenerate::core::detail {

WindowState& windowState() {
    thread_local WindowState state;
    return state;
}

} // namespace nijigenerate::core::detail
