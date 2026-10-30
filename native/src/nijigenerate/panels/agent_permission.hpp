#pragma once

#include <nijigenerate/api/acp/client.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::panels::agent {

// Use the actual ACP client's canonical shared JSON value, including its
// retained strings and shared object/array descriptors.
using AgentJson = api::acp::client::JsonValue;
using AgentString = compat::RetainedStringSlice;

struct PermissionRequest {
    AgentString id;
    AgentString reason;
    AgentJson params;
    bool responded = false;
    bool granted = false;
};

} // namespace nijigenerate::panels::agent
