#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::panels::agent {

struct ACPResult {
    compat::RetainedStringSlice kind;
    bool isError{};
    compat::RetainedStringSlice message;
};

struct PermissionRequest;
class AgentPanel;
void ngAcpStopAll();

// Explicit native module lifecycle seams; declarations do not register a panel.
void ngInitializeAgentPanelModule();
void ngShutdownAgentPanelModule();

} // namespace nijigenerate::panels::agent

#include <nijigenerate/panels/agent_permission.hpp>
#include <nijigenerate/panels/agent_panel.hpp>
