#include "stdio.hpp"

namespace nijigenerate::api::acp::transport::stdio {
std::shared_ptr<StdioTransport> createStdioTransport() {
    return providers::mcp::stdio::createStdioTransport();
}
}
