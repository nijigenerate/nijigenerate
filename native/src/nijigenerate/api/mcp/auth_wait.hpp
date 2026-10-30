#pragma once
#include <chrono>

namespace nijigenerate::api::mcp::http_transport {
// Implemented by the HTTP request-fiber event loop; it must yield the request,
// not block the event-loop thread while other requests remain ready.
void ngSleep(std::chrono::milliseconds duration);
void ngYield();
}
