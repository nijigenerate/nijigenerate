#pragma once
#include <nijigenerate/api/mcp/task/task.hpp>

namespace nijigenerate::panels::autorig::detail {
void runActionOnMainThread(api::mcp::task::Action action);
}
