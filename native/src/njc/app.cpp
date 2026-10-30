#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "commands.hpp"
#include "config.hpp"
#include "http_transport.hpp"
#include "detail/stdio_writer.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using Args = nijigenerate::compat::Slice<njc::config::Text>;
int reportFailure(const nijigenerate::compat::ThrowableException& error) {
    const auto message = error.msg;
    njc::detail::ngWriteStdioLine(stderr, {"njc: ", message.view()});
    return 1;
}
int run(Args args) {
    auto options = njc::config::parseOptions(args);
    if (options.help) {
        njc::detail::ngWriteStdioLine(stdout, {njc::config::usage().view()});
        return 0;
    }
    return njc::commands::runCommand(options);
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    // Imported std.socket initializes before entering the source main's try block.
    njc::http_transport::ngInitializeHttpTransport();
    // The runtime constructs source main's argument slice before its try region.
    Args args;
    for (int i = 0; i < argc; ++i) {
        const int length = static_cast<int>(std::char_traits<wchar_t>::length(argv[i]));
        const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], length, nullptr, 0, nullptr, nullptr);
        if (length && bytes == 0) throw std::invalid_argument("Invalid UTF-16 command argument");
        std::string value(static_cast<std::size_t>(bytes), '\0');
        if (bytes && WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], length,
            value.data(), bytes, nullptr, nullptr) != bytes) throw std::runtime_error("Cannot convert command argument");
        args.append(nijigenerate::compat::RetainedString(std::move(value)));
    }
    try {
        return run(args);
    } catch (const nijigenerate::compat::ThrowableException& error) {
        return reportFailure(error);
    }
}
#else
int main(int argc, char** argv) {
    njc::http_transport::ngInitializeHttpTransport();
    Args args;
    for (int i = 0; i < argc; ++i) args.append(nijigenerate::compat::RetainedString(argv[i]));
    try {
        return run(args);
    } catch (const nijigenerate::compat::ThrowableException& error) {
        return reportFailure(error);
    }
}
#endif
