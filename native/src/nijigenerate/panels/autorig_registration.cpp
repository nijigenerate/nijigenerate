// Derived from the independent panels/autorig.d module storage and registration.
#include "autorig.hpp"
#include "detail/autorig_sessions.hpp"

namespace nijigenerate::panels::autorig {
namespace detail {
thread_local std::shared_ptr<::nijigenerate::autorig::framework::AutoRigSessionManager> sharedSessions;
}

void ngRegisterAutoRigProcessor(std::shared_ptr<::nijigenerate::autorig::framework::AutoRigProcessor> processor) {
    ngAutoRigSessionManager()->registerProcessor(processor);
}
}
