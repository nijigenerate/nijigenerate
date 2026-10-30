#pragma once

#include <nijigenerate/autorig/framework.hpp>
#include <memory>

namespace nijigenerate::panels::autorig::detail {
// The D module variable is thread-local, rather than a process singleton.
// Keep the declaration private to this module's implementation files.
extern thread_local std::shared_ptr<::nijigenerate::autorig::framework::AutoRigSessionManager> sharedSessions;
}
