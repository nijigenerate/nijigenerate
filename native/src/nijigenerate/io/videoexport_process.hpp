#pragma once
// Platform implementations of the process/pipe subset required by videoexport.d.
#include "videoexport.hpp"
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <cstdio>
#include <string>
#include <utility>
#include <new>
#include <source_location>
#include <stdexcept>
namespace nijigenerate::io::videoexport::detail {
// Parent-side builtin storage only; OS operations, decoders and user calls stay outside.
template<class Operation> auto allocateVideoStorage(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
struct ProcessPipes {
    std::FILE* input = nullptr;
    std::FILE* output = nullptr;
    void* process = nullptr;
    std::int64_t processId = -1;
    bool terminated = false;
    std::int32_t exitCode = 0;
    std::string inputName;
    ProcessPipes() = default;
    ProcessPipes(const ProcessPipes&) = delete;
    ProcessPipes& operator=(const ProcessPipes&) = delete;
    ~ProcessPipes();
    void closeInput();
    void writeFrame(std::span<const std::uint8_t> bytes);
    bool inputError() const;
    bool poll();
    std::int32_t wait();
};
std::shared_ptr<ProcessPipes> launch(compat::Slice<VideoString> arguments, bool captureOutput,
    bool suppressConsole);
std::pair<std::int32_t, std::string> execute(compat::Slice<VideoString> arguments);
}
