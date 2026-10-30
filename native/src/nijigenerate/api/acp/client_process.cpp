#include "client_process.hpp"
#include "detail/client_storage.hpp"

// System calls and the real opaque Pid state live in platform/acp_client_process_windows.cpp.
namespace nijigenerate::api::acp::client::detail {
std::string Pid::toString() const {
    return allocateClientStorage([&] { return std::string(state ? "std.process.Pid" : "null"); });
}
providers::mcp::stdio::SharedFile ngOwnClientFile(std::FILE* file, std::string name) {
    try {
        // The real SDK constructor owns fclose on allocation failure. Its State
        // constructor is noexcept and only moves this actual native filename.
        return providers::mcp::stdio::SharedFile(file, std::move(name));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
