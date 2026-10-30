#include "nijigenerate/backend/gl_state_internal.hpp"

namespace nijigenerate::backend::gl::detail {

GLBackendState& glBackendState() {
    thread_local GLBackendState state;
    return state;
}

} // namespace nijigenerate::backend::gl::detail
