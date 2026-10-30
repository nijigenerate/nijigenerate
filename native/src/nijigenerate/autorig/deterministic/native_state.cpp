#include "native.hpp"
#include "native_state.hpp"

namespace nijigenerate::autorig::deterministic::native_detail {
thread_local framework::Table<framework::Text> liveRigSignatures;
}

namespace nijigenerate::autorig::deterministic::native {

void ngRigForgetNativeSession(compat::RetainedString sessionId) {
    if (!sessionId.empty()) native_detail::liveRigSignatures.erase(sessionId);
    else native_detail::liveRigSignatures.reset();
}

} // namespace nijigenerate::autorig::deterministic::native
