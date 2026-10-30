// Derived from the independent panels/autorig.d module-wide cleanup callers.
#include "autorig.hpp"
#include "detail/autorig_sessions.hpp"

namespace nijigenerate::panels::autorig {
void ngAutoRigStopAll() {
    auto panel = std::dynamic_pointer_cast<AutoRigPanel>(
        incFindPanelByName(compat::RetainedString::fromStaticLiteral("AutoRig")));
    if (panel) panel->stop();
    if (detail::sharedSessions) detail::sharedSessions->disposeAll();
}

void ngAutoRigPollWorker() {
    auto panel = std::dynamic_pointer_cast<AutoRigPanel>(
        incFindPanelByName(compat::RetainedString::fromStaticLiteral("AutoRig")));
    if (panel) panel->finishWorker();
}
}
