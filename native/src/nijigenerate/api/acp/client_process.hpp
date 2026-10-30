#pragma once
#include "transport/mcp_stdio.hpp"
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <memory>
#include <thread>
namespace nijigenerate::api::acp::client::detail {
class Pid {
public:
    struct State;
    std::shared_ptr<State> state;
    std::string toString() const;
    std::int32_t processID() const;
};
struct ProcessPipes {
    providers::mcp::stdio::SharedFile input, output, error;
    Pid pid;
};
ProcessPipes ngLaunchClient(compat::Slice<compat::RetainedStringSlice> command,
    compat::RetainedStringSlice cwd);
compat::RetainedStringSlice ngCurrentDirectory();
void ngJoinClientThread(std::thread& thread);
int ngClientStdoutFd(const providers::mcp::stdio::SharedFile& file);
bool ngReadClientChunk(const providers::mcp::stdio::SharedFile& file,
    compat::RetainedStringSlice& chunk);
providers::mcp::stdio::SharedFile ngOwnClientFile(std::FILE* file, std::string name = {});
}
