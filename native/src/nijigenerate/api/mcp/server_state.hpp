#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

namespace nijigenerate::api::mcp::server::detail {

// One set of the original module's private shared scalar storage.
extern bool gServerStarted;
extern std::atomic<bool> gServerThreadExited;
extern std::uint16_t gServerPort;
extern bool gServerEnabled;
extern bool gLoadHookRegistered;
bool _mcpValidToolName(std::string_view text);

} // namespace nijigenerate::api::mcp::server::detail
