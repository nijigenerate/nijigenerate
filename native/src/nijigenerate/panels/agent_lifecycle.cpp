#include "agent_panel.hpp"
#include "package.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::panels::agent {

void ngInitializeAgentPanelModule() {
    ngInitializePanelModule<AgentPanel>();
}

void ngAcpStopAll() {
    const auto panel = incFindPanelByName(compat::RetainedString::fromStaticLiteral("Agent"));
    if (panel) {
        const auto agent = std::dynamic_pointer_cast<AgentPanel>(panel);
        compat::ngRequireObject(agent)->stopAgent();
    }
}

void ngShutdownAgentPanelModule() {
    ngAcpStopAll();
}

} // namespace nijigenerate::panels::agent
