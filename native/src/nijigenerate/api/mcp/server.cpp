#include "server_state.hpp"

namespace nijigenerate::api::mcp::server {
namespace detail {
constinit bool gServerStarted = false;
constinit std::atomic<bool> gServerThreadExited{false};
constinit std::uint16_t gServerPort = 8088;
constinit bool gServerEnabled = false;
constinit bool gLoadHookRegistered = false;

bool _mcpValidToolName(std::string_view text) {
    if (text.empty()) return false;
    for (unsigned char value : text) {
        // Exact char-domain specialization of pinned Phobos std.uni.isAlphaNum.
        const bool alphaNum = (value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') ||
            (value >= 'a' && value <= 'z') || value == 0xaa || value == 0xb2 || value == 0xb3 ||
            value == 0xb5 || value == 0xb9 || value == 0xba || (value >= 0xbc && value <= 0xbe) ||
            (value >= 0xc0 && value <= 0xd6) || (value >= 0xd8 && value <= 0xf6) || value >= 0xf8;
        if (!(alphaNum || value == '_' || value == '-')) return false;
    }
    return true;
}
}

namespace {
// Preserve the original translation-unit names as aliases to the real storage.
using detail::gServerStarted;
using detail::gServerThreadExited;
using detail::gServerPort;
using detail::gServerEnabled;
using detail::gLoadHookRegistered;
using detail::_mcpValidToolName;
}
}
