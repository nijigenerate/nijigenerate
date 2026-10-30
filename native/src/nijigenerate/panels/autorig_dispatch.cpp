#include "detail/autorig_dispatch.hpp"
#include <nijigenerate/core/thread_context.hpp>

namespace nijigenerate::panels::autorig::detail {
void runActionOnMainThread(api::mcp::task::Action action) {
    if (core::thread_context::ngIsMainThreadOrUnattached()) {
        action();
    } else {
        api::mcp::task::ngRunInMainThread<void>(action);
    }
}
}
