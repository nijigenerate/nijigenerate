#pragma once
#include "crashdump.hpp"
#include "crashdump_text.hpp"
#if !defined(_WIN32)
#include <nijigenerate/compat/phobos_stdio.hpp>
#endif
#include <functional>

namespace nijigenerate::utils::crashdump {
namespace detail {
void ngWriteCrashReport(compat::RetainedStringSlice path, std::string_view content, bool append);
void ngAppendFallbackReport(const std::function<compat::RetainedString()>& report);

}

template<class... State>
compat::RetainedStringSlice writeCrashDump(compat::RetainedStringSlice filename,
    const compat::Throwable& throwable, State&&... incoming) {
    return [&]<class... Value>(Value... state) {
        mkdirCrashDumpDir();
        auto path = genCrashDumpPath(filename);
        detail::ngWriteCrashReport(path, genCrashDump(throwable, state...).view(), false);
        return path;
    }.template operator()<detail::CrashSourceFrameType<std::remove_reference_t<State>>...>(
        detail::crashSourceFrameValue(incoming)...);
}

namespace detail {
template<class Throwable, class... State> void writeFallbackCrashLog(const Throwable& throwable, State... state) {
    try {
        const auto report = allocateCrashText([&] {
            return std::function<compat::RetainedString()>([&] { return genCrashDump(throwable, state...); });
        });
        ngAppendFallbackReport(report);
    }
    catch (const compat::ThrowableException&) {}
}
}

namespace detail {
template<class Throwable, class... State> void runCrashDump(const Throwable& throwable, State&... state) {
    detail::writeFallbackCrashLog<Throwable, State...>(throwable, state...);
    compat::RetainedStringSlice dumpPath;
    try {
        dumpPath = writeCrashDump(compat::RetainedString::fromStaticLiteral("nijigenerate-crashdump"), throwable, state...);
    } catch (const compat::ThrowableException& error) {
#if !defined(_WIN32)
        const auto message = allocateCrashText([&] {
            std::string bytes = "Failed to write crash dump";
            bytes.append(error.msg.view());
            return bytes;
        });
        ::nijigenerate::providers::mcp::stdio::standardOutput().writeln(message);
#endif
    }
    notifyCrashUser(dumpPath);
}
}

template<class... State> void crashdump(const compat::Throwable& throwable, State&&... incoming) {
    [&]<class... Value>(Value... state) {
        detail::runCrashDump(throwable, state...);
    }.template operator()<detail::CrashSourceFrameType<std::remove_reference_t<State>>...>(
        detail::crashSourceFrameValue(incoming)...);
}

template<class... State>
compat::RetainedStringSlice writeCrashDump(compat::RetainedStringSlice filename,
    std::shared_ptr<const compat::Throwable> throwable, State&&... incoming) {
    return [&]<class... Value>(Value... state) {
        mkdirCrashDumpDir();
        auto path = genCrashDumpPath(filename);
        detail::ngWriteCrashReport(path, genCrashDump(throwable, state...).view(), false);
        return path;
    }.template operator()<detail::CrashSourceFrameType<std::remove_reference_t<State>>...>(
        detail::crashSourceFrameValue(incoming)...);
}

template<class... State>
void crashdump(std::shared_ptr<const compat::Throwable> throwable, State&&... incoming) {
    [&]<class... Value>(Value... state) {
        detail::runCrashDump(throwable, state...);
    }.template operator()<detail::CrashSourceFrameType<std::remove_reference_t<State>>...>(
        detail::crashSourceFrameValue(incoming)...);
}
}
