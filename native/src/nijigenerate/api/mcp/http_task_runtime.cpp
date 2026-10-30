// Used vibe.core.task scheduling. Copyright 2012-2020 Sönke Ludwig.
// MIT; full license in port/licenses/mcp-http-transport.txt.
#include "http_task_runtime.hpp"
#include "auth_wait.hpp"
#include "http_auth_time.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <common/object_key_policy.hpp>
#include <cstdlib>
#include <bit>
#include <charconv>
#include <string>
#include <limits>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace nijigenerate::api::mcp::http_transport::detail {
namespace {
void prepareTaskFiberClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(RequestTask),
            compat::RetainedString::fromStaticLiteral("vibe.core.task.TaskFiber"));
        return true;
    }();
    (void)registered;
}
// std.digest.md MD5 used-task specialization (Piotr Szturmaj, Kai Nacke,
// Johannes Pfau), BSL-1.0. Derived from RSA Data Security, Inc. MD5.
// Task.getDebugID feeds exactly two little-endian native size_t values.
std::array<unsigned char, 3> taskIdDigest(std::size_t address, std::size_t counter) noexcept {
    constexpr std::size_t inputBytes = 2 * sizeof(std::size_t);
    static_assert(inputBytes < 56);
    std::array<std::uint32_t, 16> words{};
    const std::array<std::size_t, 2> input{address, counter};
    for (std::size_t item = 0; item < input.size(); ++item)
        for (std::size_t byte = 0; byte < sizeof(std::size_t); ++byte) {
            const auto index = item * sizeof(std::size_t) + byte;
            const auto value = static_cast<unsigned char>(input[item] >> (byte * 8));
            words[index / 4] |= static_cast<std::uint32_t>(value) << ((index % 4) * 8);
        }
    words[inputBytes / 4] |= std::uint32_t{0x80} << ((inputBytes % 4) * 8);
    words[14] = static_cast<std::uint32_t>(inputBytes * 8);
    std::uint32_t a = 0x67452301u, b = 0xefcdab89u, c = 0x98badcfeu, d = 0x10325476u;
    const auto F = [](std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) | (~x & z); };
    const auto G = [](std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & z) | (y & ~z); };
    const auto H = [](std::uint32_t x, std::uint32_t y, std::uint32_t z) { return x ^ y ^ z; };
    const auto I = [](std::uint32_t x, std::uint32_t y, std::uint32_t z) { return y ^ (x | ~z); };
    const auto round = [](std::uint32_t& first, std::uint32_t second, std::uint32_t third,
            std::uint32_t fourth, std::uint32_t word, unsigned shift, std::uint32_t constant, auto operation) {
        first += operation(second, third, fourth) + word + constant;
        first = std::rotl(first, static_cast<int>(shift));
        first += second;
    };
    const auto FF = [&](auto& x, auto y, auto z, auto w, auto word, auto shift, auto k) {
        round(x, y, z, w, word, shift, k, F);
    };
    const auto GG = [&](auto& x, auto y, auto z, auto w, auto word, auto shift, auto k) {
        round(x, y, z, w, word, shift, k, G);
    };
    const auto HH = [&](auto& x, auto y, auto z, auto w, auto word, auto shift, auto k) {
        round(x, y, z, w, word, shift, k, H);
    };
    const auto II = [&](auto& x, auto y, auto z, auto w, auto word, auto shift, auto k) {
        round(x, y, z, w, word, shift, k, I);
    };
    FF(a, b, c, d, words[0], 7, 0xd76aa478u);
    FF(d, a, b, c, words[1], 12, 0xe8c7b756u);
    FF(c, d, a, b, words[2], 17, 0x242070dbu);
    FF(b, c, d, a, words[3], 22, 0xc1bdceeeu);
    FF(a, b, c, d, words[4], 7, 0xf57c0fafu);
    FF(d, a, b, c, words[5], 12, 0x4787c62au);
    FF(c, d, a, b, words[6], 17, 0xa8304613u);
    FF(b, c, d, a, words[7], 22, 0xfd469501u);
    FF(a, b, c, d, words[8], 7, 0x698098d8u);
    FF(d, a, b, c, words[9], 12, 0x8b44f7afu);
    FF(c, d, a, b, words[10], 17, 0xffff5bb1u);
    FF(b, c, d, a, words[11], 22, 0x895cd7beu);
    FF(a, b, c, d, words[12], 7, 0x6b901122u);
    FF(d, a, b, c, words[13], 12, 0xfd987193u);
    FF(c, d, a, b, words[14], 17, 0xa679438eu);
    FF(b, c, d, a, words[15], 22, 0x49b40821u);
    GG(a, b, c, d, words[1], 5, 0xf61e2562u);
    GG(d, a, b, c, words[6], 9, 0xc040b340u);
    GG(c, d, a, b, words[11], 14, 0x265e5a51u);
    GG(b, c, d, a, words[0], 20, 0xe9b6c7aau);
    GG(a, b, c, d, words[5], 5, 0xd62f105du);
    GG(d, a, b, c, words[10], 9, 0x2441453u);
    GG(c, d, a, b, words[15], 14, 0xd8a1e681u);
    GG(b, c, d, a, words[4], 20, 0xe7d3fbc8u);
    GG(a, b, c, d, words[9], 5, 0x21e1cde6u);
    GG(d, a, b, c, words[14], 9, 0xc33707d6u);
    GG(c, d, a, b, words[3], 14, 0xf4d50d87u);
    GG(b, c, d, a, words[8], 20, 0x455a14edu);
    GG(a, b, c, d, words[13], 5, 0xa9e3e905u);
    GG(d, a, b, c, words[2], 9, 0xfcefa3f8u);
    GG(c, d, a, b, words[7], 14, 0x676f02d9u);
    GG(b, c, d, a, words[12], 20, 0x8d2a4c8au);
    HH(a, b, c, d, words[5], 4, 0xfffa3942u);
    HH(d, a, b, c, words[8], 11, 0x8771f681u);
    HH(c, d, a, b, words[11], 16, 0x6d9d6122u);
    HH(b, c, d, a, words[14], 23, 0xfde5380cu);
    HH(a, b, c, d, words[1], 4, 0xa4beea44u);
    HH(d, a, b, c, words[4], 11, 0x4bdecfa9u);
    HH(c, d, a, b, words[7], 16, 0xf6bb4b60u);
    HH(b, c, d, a, words[10], 23, 0xbebfbc70u);
    HH(a, b, c, d, words[13], 4, 0x289b7ec6u);
    HH(d, a, b, c, words[0], 11, 0xeaa127fau);
    HH(c, d, a, b, words[3], 16, 0xd4ef3085u);
    HH(b, c, d, a, words[6], 23, 0x4881d05u);
    HH(a, b, c, d, words[9], 4, 0xd9d4d039u);
    HH(d, a, b, c, words[12], 11, 0xe6db99e5u);
    HH(c, d, a, b, words[15], 16, 0x1fa27cf8u);
    HH(b, c, d, a, words[2], 23, 0xc4ac5665u);
    II(a, b, c, d, words[0], 6, 0xf4292244u);
    II(d, a, b, c, words[7], 10, 0x432aff97u);
    II(c, d, a, b, words[14], 15, 0xab9423a7u);
    II(b, c, d, a, words[5], 21, 0xfc93a039u);
    II(a, b, c, d, words[12], 6, 0x655b59c3u);
    II(d, a, b, c, words[3], 10, 0x8f0ccc92u);
    II(c, d, a, b, words[10], 15, 0xffeff47du);
    II(b, c, d, a, words[1], 21, 0x85845dd1u);
    II(a, b, c, d, words[8], 6, 0x6fa87e4fu);
    II(d, a, b, c, words[15], 10, 0xfe2ce6e0u);
    II(c, d, a, b, words[6], 15, 0xa3014314u);
    II(b, c, d, a, words[13], 21, 0x4e0811a1u);
    II(a, b, c, d, words[4], 6, 0xf7537e82u);
    II(d, a, b, c, words[11], 10, 0xbd3af235u);
    II(c, d, a, b, words[2], 15, 0x2ad7d2bbu);
    II(b, c, d, a, words[9], 21, 0xeb86d391u);
    const std::array<std::uint32_t, 4> state{
        0x67452301u + a, 0xefcdab89u + b, 0x98badcfeu + c, 0x10325476u + d};
    words.fill(0);
    return {static_cast<unsigned char>(state[0]), static_cast<unsigned char>(state[0] >> 8),
        static_cast<unsigned char>(state[0] >> 16)};
}

struct SourceLogInfo {
    SourceLogLevel level;
    SourceSysTime time;
    std::shared_ptr<core::thread_context::Thread> thread;
    std::uint32_t threadID = 0;
    compat::RetainedString threadName;
};

// The selected default stdout/stderr FileLogger, with the actual File owners.
// This is the real log.d module global, not a logger-local Thread identity.
class SourceFileLoggerRuntime final {
    std::recursive_mutex monitor_;
    bool enabled_ = false;
    bool colors_ = false;
    std::atomic<SourceLogLevel> minimum_{SourceLogLevel::info};
    SourceLogFormat diagnosticFormat_ = SourceLogFormat::thread;
    SourceLogFormat infoFormat_ = SourceLogFormat::plain;
    providers::mcp::stdio::SharedFile info_, diagnostic_, current_;

    [[noreturn]] static void finalSwitch(std::size_t line) {
        throw compat::ngStaticError<compat::SwitchError>(
            compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
            compat::RetainedString::fromStaticLiteral("vibe/core/log.d"), line);
    }
    static void writeDecimal(providers::mcp::stdio::FileTextWriter& writer,
            std::int64_t value, unsigned width = 0) {
        std::array<char, 32> text;
        const auto end = std::to_chars(text.data(), text.data() + text.size(), value).ptr;
        const auto count = static_cast<unsigned>(end - text.data());
        for (auto i = count; i < width; ++i) writer.put(static_cast<unsigned char>('0'));
        writer.put(std::string_view(text.data(), count));
    }
    static void writeTime(providers::mcp::stdio::FileTextWriter& writer, SourceSysTime time) {
        constexpr std::int64_t ticksPerDay = 864'000'000'000;
        auto day = time.stdTime() / ticksPerDay;
        auto within = time.stdTime() % ticksPerDay;
        if (within < 0) { --day; within += ticksPerDay; }
        const std::chrono::year_month_day date{
            std::chrono::sys_days{std::chrono::days{day - 719'162}}};
        writeDecimal(writer, static_cast<int>(date.year())); writer.put(static_cast<unsigned char>('-'));
        writeDecimal(writer, static_cast<unsigned>(date.month()), 2); writer.put(static_cast<unsigned char>('-'));
        writeDecimal(writer, static_cast<unsigned>(date.day()), 2); writer.put(static_cast<unsigned char>(' '));
        writeDecimal(writer, within / 36'000'000'000, 2); writer.put(static_cast<unsigned char>(':'));
        writeDecimal(writer, within / 600'000'000 % 60, 2); writer.put(static_cast<unsigned char>(':'));
        writeDecimal(writer, within / 10'000'000 % 60, 2); writer.put(static_cast<unsigned char>('.'));
        writeDecimal(writer, within % 10'000'000 / 10'000, 3); writer.put(static_cast<unsigned char>(' '));
    }
    void beginLine(const SourceLogInfo& info) {
        constexpr std::array<std::string_view, 9> labels{
            "trc", "dbv", "dbg", "dia", "INF", "WRN", "ERR", "CRITICAL", "FATAL"};
        const auto level = static_cast<unsigned>(info.level);
        if (level >= labels.size()) {
#ifndef NDEBUG
            if (info.level == SourceLogLevel::none) throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral("Assertion failure"),
                compat::RetainedString::fromStaticLiteral("vibe/core/log.d"), 402);
#endif
            finalSwitch(392);
        }
        const bool diagnostic = info.level != SourceLogLevel::info;
        current_ = diagnostic ? diagnostic_ : info_;
        const auto format = diagnostic ? diagnosticFormat_ : infoFormat_;
        current_.withTextWriter([&](providers::mcp::stdio::FileTextWriter& writer) {
#ifndef _WIN32
            if (colors_) {
                constexpr std::array<std::string_view, 9> colors{
                    "\x1b[49;38;5;243m", "\x1b[49;38;5;245m", "\x1b[49;38;5;180m",
                    "\x1b[49;38;5;143m", "\x1b[49;38;5;29m", "\x1b[49;38;5;220m",
                    "\x1b[49;38;5;9m", "\x1b[41;38;5;15m", "\x1b[48;5;9;30m"};
                writer.put(colors[level]);
            }
#endif
            switch (format) {
            case SourceLogFormat::plain: return;
            case SourceLogFormat::thread:
            case SourceLogFormat::threadTime: break;
            default: finalSwitch(426);
            }
            writer.put(static_cast<unsigned char>('['));
            if (format == SourceLogFormat::threadTime) writeTime(writer, info.time);
            if (!info.threadName.empty()) writer.put(info.threadName.view());
            else {
                constexpr std::string_view digits = "0123456789ABCDEF";
                std::array<char, 8> number;
                for (unsigned i = 0; i < number.size(); ++i)
                    number[i] = digits[(info.threadID >> ((7 - i) * 4)) & 15];
                writer.put(std::string_view(number.data(), number.size()));
            }
            writer.put(static_cast<unsigned char>('('));
            SourceTask::getThis().getDebugID(writer);
            writer.put(") "); writer.put(labels[level]); writer.put("] ");
        });
    }
    void endLine() {
#ifndef _WIN32
        if (colors_) current_.write("\x1b[0m");
#endif
        current_.writeln("");
        current_.flush();
    }
public:
    SourceFileLoggerRuntime() {
#ifdef _WIN32
        if (!GetStdHandle(STD_OUTPUT_HANDLE) || !GetStdHandle(STD_ERROR_HANDLE)) return;
#endif
        info_ = providers::mcp::stdio::standardOutput();
        diagnostic_ = providers::mcp::stdio::standardError();
#ifndef _WIN32
        colors_ = ::isatty(static_cast<int>(info_.nativeHandle())) != 0;
#endif
        enabled_ = true;
    }
    bool accepts(SourceLogLevel level) const noexcept { return enabled_ && level >= minimum_.load(); }
    void level(SourceLogLevel value) { const std::lock_guard lock(monitor_); if (enabled_) minimum_ = value; }
    SourceLogLevel level() { const std::lock_guard lock(monitor_); return enabled_ ? minimum_.load() : SourceLogLevel::none; }
    void format(SourceLogFormat diagnostic, SourceLogFormat info) {
        const std::lock_guard lock(monitor_);
        if (enabled_) { diagnosticFormat_ = diagnostic; infoFormat_ = info; }
    }
    template<class Emit> void line(SourceLogLevel level, Emit&& emit) {
        const std::lock_guard lock(monitor_);
        if (!accepts(level)) return;
        SourceLogInfo info{level, {}, {}, 0, {}};
        {
            SourceYieldLock noYield(sourceTaskSchedulerForThread(),
                compat::RetainedString::fromStaticLiteral("vibe/core/log.d"), 1105);
            try {
                info.time = SourceAuthClock::now();
                info.thread = core::thread_context::Thread::getThis();
                const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(info.thread.get()));
                info.threadID = static_cast<std::uint32_t>(address) ^ static_cast<std::uint32_t>(address >> 32);
                info.threadName = info.thread ? info.thread->name() : compat::RetainedString::fromStaticLiteral("");
            } catch (const compat::ThrowableException& error) {
                try {
                    const auto description = error.toString();
                    providers::mcp::stdio::standardOutput().withTextWriter([&](auto& writer) {
                        writer.put("Error during logging: "); writer.put(description.view());
                        writer.put(static_cast<unsigned char>('\n'));
                    });
                } catch (const compat::ThrowableException&) {}
#ifndef NDEBUG
                std::string message("Exception during logging: ");
                message.append(error.msg.view());
                throw compat::ngStaticError<compat::AssertError>(compat::RetainedString(std::move(message)),
                    compat::RetainedString::fromStaticLiteral("vibe/core/log.d"), 1124);
#endif
            }
            beginLine(info);
        }
        const auto put = [&](std::string_view text) {
            if (text.empty()) return;
            SourceYieldLock noYield(sourceTaskSchedulerForThread(),
                compat::RetainedString::fromStaticLiteral("vibe/core/log.d"), 1142);
            auto position = text.find('\n');
            current_.write(text.substr(0, position));
            while (position != std::string_view::npos) {
                text.remove_prefix(position + 1);
                endLine();
                beginLine(info);
                position = text.find('\n');
                current_.write(text.substr(0, position));
            }
        };
        std::invoke(emit, put);
        {
            SourceYieldLock noYield(sourceTaskSchedulerForThread(),
                compat::RetainedString::fromStaticLiteral("vibe/core/log.d"), 1132);
            endLine();
        }
    }
    // Object defaults, Digital Mars 2000-2011, BSL-1.0.
    // Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
    virtual ~SourceFileLoggerRuntime() = default;
    virtual compat::RetainedString toString() {
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    virtual std::size_t toHash() noexcept {
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(SourceFileLoggerRuntime* other) {
        return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other);
    }
    virtual int opCmp(SourceFileLoggerRuntime*) {
        prepareSourceClassInfo();
        std::string message = "need opCmp for class ";
        message += nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)).view();
        throw compat::ThrowableException(compat::RetainedString(std::move(message)),
            compat::RetainedString::fromStaticLiteral("object.d"), 195);
    }
private:
    static void prepareSourceClassInfo() {
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(SourceFileLoggerRuntime),
                compat::RetainedString::fromStaticLiteral("vibe.core.log.FileLogger"));
            return true;
        }();
        (void)registered;
    }
};
SourceFileLoggerRuntime sourceDefaultLogger;

template<class Prepare> void sourceLog(SourceLogLevel level, Prepare&& prepare) {
    try {
        if (!sourceDefaultLogger.accepts(level)) return;
        auto message = std::invoke(prepare);
        sourceDefaultLogger.line(level, message);
    } catch (const compat::ThrowableException& error) {
#ifndef NDEBUG
        throw compat::ngStaticError<compat::AssertError>(error.msg,
            compat::RetainedString::fromStaticLiteral("vibe/core/log.d"), 1085);
#else
        (void)error;
#endif
    }
}
void taskExceptionLog(std::string_view prefix, const compat::Throwable& error) {
    sourceLog(SourceLogLevel::critical, [&] {
        const auto message = error.msg;
        return [prefix, message](auto put) { put(prefix); put(message.view()); };
    });
    sourceLog(SourceLogLevel::diagnostic, [&] {
        return [&](auto put) { error.toString(put); };
    });
}
[[noreturn]] void taskFatal(const compat::Throwable& error) {
    try {
#ifdef _WIN32
        if (!GetConsoleWindow()) {
            USEROBJECTFLAGS flags{};
            GetUserObjectInformationA(GetProcessWindowStation(), UOI_FLAGS, &flags, sizeof(flags), nullptr);
            if (flags.dwFlags & WSF_VISIBLE) {
                std::array<char, 2048> message{};
                std::size_t fill = 0;
                const auto append = [&](std::string_view bytes) {
                    const auto size = std::min(bytes.size(), message.size() - 1 - fill);
                    std::copy_n(bytes.data(), size, message.data() + fill);
                    fill += size;
                };
                append(error.ngThrowableTypeName()); append(": ");
                error.toString(append);
                MessageBoxA(nullptr, message.data(), "FATAL: Uncaught exception in task fiber", MB_ICONERROR);
            }
        }
#endif
        sourceLog(SourceLogLevel::fatal, [&] {
            const auto type = error.ngThrowableTypeName();
            return [&, type](auto put) {
                put("TaskFiber getting terminated due to an uncaught ");
                put(type); put(": "); error.toString(put);
            };
        });
    } catch (...) {}
    std::abort();
}
}
void ngSetSourceLogLevel(SourceLogLevel level) { sourceDefaultLogger.level(level); }
SourceLogLevel ngGetSourceLogLevel() { return sourceDefaultLogger.level(); }
void ngSetSourceLogFormat(SourceLogFormat diagnostic, SourceLogFormat info) {
    sourceDefaultLogger.format(diagnostic, info);
}
void ngSourceLogPrepared(SourceLogLevel level, void* context,
        void (*prepare)(void*), void (*emit)(void*, SourceLogSink)) {
    sourceLog(level, [&] {
        prepare(context);
        return [=](const auto& put) {
            using Writer = std::remove_cvref_t<decltype(put)>;
            emit(context, SourceLogSink{std::addressof(put),
                [](const void* writer, std::string_view text) {
                    std::invoke(*static_cast<const Writer*>(writer), text);
                }});
        };
    });
}
SourceTaskScheduler::~SourceTaskScheduler() { shutdown(); }
RequestTask::RequestTask(asio::io_context& loop) : RequestTask(sourceTaskSchedulerFor(loop)) {}
RequestTask::RequestTask(SourceTaskScheduler& scheduler, bool dummy)
    : scheduler_(scheduler), thread_(core::thread_context::Thread::getThis()), dummy_(dummy) {}
RequestTask::~RequestTask() { releaseFiber(); }
// Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString RequestTask::toString() {
    prepareTaskFiberClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t RequestTask::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool RequestTask::opEquals(std::shared_ptr<RequestTask> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int RequestTask::opCmp(std::shared_ptr<RequestTask>) {
    prepareTaskFiberClassInfo();
    std::string message = "need opCmp for class ";
    message += nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)).view();
    throw compat::ThrowableException(compat::RetainedString(std::move(message)),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
SourceTask SourceTask::getThis() noexcept {
    const auto current = RequestTask::current_;
    if (!core::tasks::TaskFiber::isVibeTask() || !current || current->dummy_) return {};
    return SourceTask(current->shared_from_this(), current->taskCounter());
}
std::shared_ptr<core::thread_context::Thread> SourceTask::thread() const noexcept {
    return fiber_ ? fiber_->thread_ : nullptr;
}
bool SourceTask::running() const {
    sourceTaskAssert([&] { return static_cast<bool>(fiber_); }, "Invalid task handle", 72);
    const auto fiber = compat::ngRequireObject(fiber_);
    const auto status = fiber->counterAndFlags_.load();
    // Every real normal/error/forced-unwind/OS-exit path clears these flags
    // before the underlying Fiber can become TERM or its native storage ends.
    return static_cast<std::size_t>(status >> RequestTask::counterShift) == counter_ &&
        (status & RequestTask::initializedFlag) != 0;
}
void SourceTask::interrupt() const {
    if (fiber_ && running()) fiber_->interrupt(counter_);
}
std::array<char, 4> SourceTask::debugCharacters() const noexcept {
    const auto digest = taskIdDigest(reinterpret_cast<std::size_t>(fiber_.get()), counter_);
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const std::uint32_t bits = (static_cast<std::uint32_t>(digest[0]) << 16) |
        (static_cast<std::uint32_t>(digest[1]) << 8) | digest[2];
    return {alphabet[(bits >> 18) & 63], alphabet[(bits >> 12) & 63],
        alphabet[(bits >> 6) & 63], alphabet[bits & 63]};
}
compat::RetainedString SourceTask::getDebugID() const {
    std::string output;
    struct Appender {
        std::string& target;
        void put(std::string_view bytes) { target.append(bytes); }
    } appender{output};
    getDebugID(appender);
    return compat::RetainedString(std::move(output));
}
RequestTask& RequestTask::current() { return sourceTaskSchedulerForThread().currentFiber(); }
RequestTask& SourceTaskScheduler::currentFiber() {
    if (core::tasks::TaskFiber::isVibeTask() && RequestTask::current_
        && &RequestTask::current_->scheduler_ == this) return *RequestTask::current_;
    if (!dummy_) dummy_ = std::make_shared<RequestTask>(*this, true);
    return *dummy_;
}
std::shared_ptr<RequestTask> SourceTaskScheduler::retainCurrentFiber() { return currentFiber().shared_from_this(); }
void SourceTaskScheduler::attach(const Task& task) { active_.push_back(task); }
SourceTaskScheduler::Task SourceTaskScheduler::createTask() {
    if (shuttingDown_) throw std::logic_error("Task context has exited its OS thread");
    while (availableCount_) {
        auto task = std::exchange(available_[--availableCount_], {});
        task->pooled_ = false;
        if (task->fiber_ && task->fiber_->state() == core::tasks::TaskFiber::State::hold) return task;
    }
    return std::make_shared<RequestTask>(*this);
}
void SourceTaskScheduler::complete(RequestTask* task, bool recycle) noexcept {
    remove(task);
    if (recycle && !shuttingDown_ && !task->disposing_ && availableCount_ < available_.size()) {
        available_[availableCount_++] = task->shared_from_this();
        task->pooled_ = true;
    }
    std::erase_if(active_, [task](const Task& value) { return value.get() == task; });
}
void RequestTask::releaseFiber() {
    if (!fiber_) return;
    auto* previous = current_;
    current_ = this;
    compat::ngRunWithScopeExit([&] { fiber_.reset(); }, [&] { current_ = previous; });
}
void RequestTask::start(std::function<void()> body) {
    if (initialized() || disposing_) throw std::logic_error("Request task is already active or disposed");
    body_ = std::move(body);
    try {
        if (!fiber_)
            fiber_ = std::make_unique<core::tasks::TaskFiber>([this] { run(); }, core::tasks::TaskFiber::Kind::vibe);
        sourceTaskAssert([&] { return (counterAndFlags_.load() & flagsMask) == 0; },
            "bumpTaskCounter() called on fiber with non-zero flags", 656);
        sourceTaskAssert([&] { return static_cast<bool>(body_); },
            "bumpTaskCounter() called without initializing the task function", 657);
        scheduler_.attach(shared_from_this());
    } catch (...) {
        body_ = {};
        if (auto terminate = std::exchange(terminate_, {})) terminate();
        releaseFiber();
        throw;
    }
    counterAndFlags_.fetch_add((std::uint64_t{1} << counterShift) + initializedFlag);
    scheduler_.start(shared_from_this());
}
void RequestTask::run() {
    for (;;) {
        if (disposing_) return;
        // Source handleTasks waits for a genuine function before entering a task.
        // In the assertion-disabled profile an empty function is not success.
        while (!body_) {
            core::tasks::TaskFiber::yield();
            if (disposing_) return;
        }
        staticPriority_ = dynamicPriority_ = 0x10000;
        bool reusable = false;
        try { compat::ngRunWithScopeExit([&] {
            auto body = std::exchange(body_, {});
            try { compat::ngRunWithScopeExit([&] {
                counterAndFlags_.fetch_or(runningFlag);
                if (!scheduler_.running()) scheduler_.yield(false);
                body();
            }, [&] { clearFlags(); }); }
            catch (const boost::context::detail::forced_unwind&) { throw; }
            catch (const ThreadUnwind&) { throw; }
            catch (const compat::ThrowableError&) { throw; }
            catch (const std::bad_alloc&) { throw; }
            catch (const compat::ThrowableException& error) {
                taskExceptionLog("Task terminated with uncaught exception: ", error);
            }
            catch (const std::exception& error) {
                auto& output = providers::mcp::stdio::standardError();
                output.write("Task terminated with native exception: ");
                output.writeln(error.what());
            }
            reusable = true;
        }, [&] {
            clearFlags();
            body_ = {};
            try { if (auto terminate = std::exchange(terminate_, {})) terminate(); }
            catch (...) { scheduler_.complete(this); throw; }
            scheduler_.complete(this, reusable);
        }); } catch (const compat::ThrowableError& error) { taskFatal(error); }
        catch (const std::bad_alloc& error) {
            try { providers::mcp::stdio::standardError().writeln(error.what()); } catch (...) {}
            std::abort();
        }
        if (!pooled_ || disposing_) return;
        core::tasks::TaskFiber::yield();
    }
}
void SourceTaskScheduler::resume(const Task& task) {
    if (!task->initialized() || !task->fiber_) return;
    auto* previous = RequestTask::current_;
    RequestTask::current_ = task.get();
    compat::ngRunWithScopeExit([&] {
        try { task->fiber_->call(); }
        catch (const compat::ThrowableError& error) {
            using namespace nicxlive::core::d_runtime;
            if (ngCastSourceError<AssertError>(&error) || ngCastSourceError<ArrayIndexError>(&error) ||
                ngCastSourceError<ArraySliceError>(&error) || ngCastSourceError<RangeError>(&error) ||
                ngCastSourceError<SwitchError>(&error) || ngCastSourceError<OutOfMemoryError>(&error) ||
                ngCastSourceError<InvalidMemoryOperationError>(&error) || ngCastSourceError<ForkError>(&error))
                ngRethrowSourceError(error);
            throw;
        }
    }, [&] { RequestTask::current_ = previous; });
}
void SourceTaskScheduler::remove(RequestTask* task) {
    if (!task->queued_) return;
    const auto next = task->readyNext_;
    auto* previous = task->readyPrevious_;
    if (previous) previous->readyNext_ = next;
    else readyFirst_ = next;
    if (next) next->readyPrevious_ = previous;
    else readyLast_ = previous;
    task->readyNext_.reset();
    task->readyPrevious_ = nullptr;
    task->queued_ = false;
    --readyCount_;
}
void SourceTaskScheduler::insertAfter(Task task, RequestTask* previous, std::size_t assertionLine) {
    sourceTaskAssert([&] { return !task->queued_; },
        "Task is already scheduled to be resumed!", assertionLine);
    sourceTaskAssert([&] { return !task->readyPrevious_; },
        "Task has m_prev set without being in a queue!?", assertionLine + 1);
    sourceTaskAssert([&] { return !task->readyNext_; },
        "Task has m_next set without being in a queue!?", assertionLine + 2);
    const auto next = previous ? previous->readyNext_ : readyFirst_;
    task->readyPrevious_ = previous;
    task->readyNext_ = next;
    task->queued_ = true;
    if (next) next->readyPrevious_ = task.get();
    else readyLast_ = task.get();
    if (previous) previous->readyNext_ = std::move(task);
    else readyFirst_ = std::move(task);
    ++readyCount_;
}
SourceTaskScheduler::Task SourceTaskScheduler::popReady() {
    sourceTaskAssert([&] { return readyFirst_ && readyFirst_->queued_; },
        "Popping from empty or mismatching queue", 1297);
    auto task = readyFirst_;
    remove(task.get());
    return task;
}
void SourceTaskScheduler::reschedule(const Task& task) {
    if (task->queued_ || !task->initialized() || shuttingDown_) return;
    auto* previous = readyLast_;
    unsigned remaining = 10;
    while (previous) {
        if (!remaining-- || previous->dynamicPriority_ >= task->dynamicPriority_) break;
        auto& priority = previous->dynamicPriority_;
        priority += std::min(previous->staticPriority_, std::numeric_limits<std::uint32_t>::max() - priority);
        previous = previous->readyPrevious_;
    }
    insertAfter(task, previous);
}
void SourceTaskScheduler::switchTo(const Task& task, SourceTaskPriority priority) {
    if (task.get() == RequestTask::currentOrNull() || !task->initialized() || shuttingDown_) return;
    sourceTaskAssert([&] { return core::thread_context::Thread::getThis() == task->thread_; },
        "Cannot switch to a task that lives in a different thread.", 1051);
    if (task->queued_) {
        if (priority == SourceTaskPriority::normal) return;
        remove(task.get());
    }
    auto* current = RequestTask::currentOrNull();
    if (!current && priority == SourceTaskPriority::immediate) {
        sourceTaskAssert([&] { return currentFiber().yieldLocks_ == 0; },
            "Cannot yield within an active yieldLock()!", 1065);
        resume(task); return;
    }
    sourceTaskAssert([&] { return !current || !current->queued_; },
        "Calling task is running, but scheduled to be resumed!?", 1071);
    if (priority == SourceTaskPriority::normal) { reschedule(task); return; }
    if (priority == SourceTaskPriority::prioritized) {
        task->dynamicPriority_ = std::numeric_limits<std::uint32_t>::max();
        reschedule(task); return;
    }
    task->dynamicPriority_ = std::numeric_limits<std::uint32_t>::max();
    insertAfter(current->shared_from_this(), nullptr, 1236);
    insertAfter(task, nullptr, 1236);
    current->checkYieldLock();
    core::tasks::TaskFiber::yield();
    if (current->disposing_) throw RequestTask::ThreadUnwind{};
    sourceTaskAssert([&] { return !current->queued_; }, "Task is still scheduled after resumption.", 1212);
}
void SourceTaskScheduler::start(const Task& task) {
    switchTo(task, currentFiber().yieldLocks_ ? SourceTaskPriority::prioritized : SourceTaskPriority::immediate);
}
void RequestTask::wake() { scheduler_.switchTo(shared_from_this(), SourceTaskPriority::normal); }
void RequestTask::checkInterrupt() {
    if (takeInterrupt()) throw SourceTaskInterrupt();
}
void RequestTask::checkYieldLock() {
    if (yieldLocks_ > 0 && !yieldFile_.empty()) {
        sourceLog(SourceLogLevel::error, [&] {
            const auto file = yieldFile_;
            const auto line = yieldLine_;
            return [file, line](auto put) {
                put("Yield lock violation for lock at "); put(file.view()); put(":");
                std::array<char, std::numeric_limits<std::size_t>::digits10 + 1> digits;
                const auto end = std::to_chars(digits.data(), digits.data() + digits.size(), line).ptr;
                put(std::string_view(digits.data(), static_cast<std::size_t>(end - digits.data())));
            };
        });
    }
    sourceTaskAssert([&] { return yieldLocks_ == 0; }, "May not yield while in an active yieldLock()!", 739);
}
void RequestTask::interrupt() {
    SourceTask(shared_from_this(), taskCounter()).interrupt();
}
void RequestTask::interrupt(std::size_t counter) {
    sourceTaskAssert([&] { return !core::tasks::TaskFiber::isVibeTask() || current_ != this; },
        "A task cannot interrupt itself.", 626);
    for (;;) {
        auto previous = counterAndFlags_.load();
        if (!(previous & initializedFlag) || (previous & interruptFlag) ||
                static_cast<std::size_t>(previous >> counterShift) != counter) return;
        if (counterAndFlags_.compare_exchange_weak(previous, previous | interruptFlag)) break;
    }
    if (core::thread_context::Thread::getThis() == thread_)
        scheduler_.start(shared_from_this());
}
void SourceTaskScheduler::yield(bool interruptible) {
    auto* task = RequestTask::currentOrNull();
    if (!task) return;
    if (interruptible) task->checkInterrupt();
    if (task->queued_) return;
    reschedule(task->shared_from_this());
    task->checkYieldLock();
    core::tasks::TaskFiber::yield();
    if (task->disposing_) throw RequestTask::ThreadUnwind{};
    sourceTaskAssert([&] { return !task->queued_; }, "Task is still scheduled after resumption.", 1212);
    if (interruptible) task->checkInterrupt();
}
void SourceTaskScheduler::hibernate() {
    auto* task = RequestTask::currentOrNull();
    if (!task) {
        sourceTaskAssert([&] { return !running_; },
            "Event processing outside of a fiber should only happen before the event loop is running!?", 1029);
        waitAndProcess();
        return;
    }
    task->checkYieldLock();
    core::tasks::TaskFiber::yield();
    if (task->disposing_) throw RequestTask::ThreadUnwind{};
    sourceTaskAssert([&] { return !task->queued_; }, "Task is still scheduled after resumption.", 1212);
}
void SourceTaskScheduler::yieldGlobal() {
    if (yieldingGlobal_) return;
    yieldingGlobal_ = true;
    compat::ngRunWithScopeExit([&] {
        currentFiber().checkYieldLock();
        const auto result = processEvents(false);
        if ((result == SourceTaskExit::exited || result == SourceTaskExit::outOfWaiters) && !readyFirst_) {
            if (running_) requestExit();
            return;
        }
        schedule();
    }, [&] { yieldingGlobal_ = false; });
}
SourceScheduleStatus SourceTaskScheduler::schedule() {
    if (!readyFirst_) return SourceScheduleStatus::idle;
    sourceTaskAssert([] { return !RequestTask::currentOrNull(); },
        "TaskScheduler.schedule() may not be called from a task!", 1106);
    const auto count = readyCount_;
    for (std::size_t i = 0; i < count && readyFirst_; ++i) {
        auto task = popReady();
        task->dynamicPriority_ = task->staticPriority_;
        resume(task);
    }
    return !readyFirst_ ? SourceScheduleStatus::allProcessed : SourceScheduleStatus::busy;
}
SourceTaskExit SourceTaskScheduler::processEvents(bool block) {
    if (exit_ || sourceTaskDriverExitRequested()) return SourceTaskExit::exited;
    if (block && !nativeWaits_->pending.load()) return SourceTaskExit::outOfWaiters;
    if (loop_.stopped()) loop_.restart();
    auto events = block ? loop_.run_one() : std::size_t{0};
    // Drain the driver's ready batch before resuming queued tasks. In particular,
    // a read completion in this batch must be visible alongside a fired timeout.
    if (!exit_ && !sourceTaskDriverExitRequested()) events += loop_.poll();
    if (exit_ || sourceTaskDriverExitRequested()) return SourceTaskExit::exited;
    if (!nativeWaits_->pending.load()) return SourceTaskExit::outOfWaiters;
    if (events) return SourceTaskExit::idle;
    return loop_.stopped() ? SourceTaskExit::outOfWaiters : SourceTaskExit::timeout;
}
SourceTaskExit SourceTaskScheduler::process() {
    sourceTaskAssert([&] { return currentFiber().yieldLocks_ == 0; },
        "May not process events within an active yieldLock()!", 929);
    bool anyEvents = false;
    for (;;) {
        const bool anyTasksProcessed = schedule() != SourceScheduleStatus::idle;
        const auto result = processEvents(false);
        switch (result) {
        case SourceTaskExit::exited: return result;
        case SourceTaskExit::outOfWaiters:
            if (!readyFirst_) return result;
            break;
        case SourceTaskExit::timeout:
            if (!readyFirst_)
                return anyEvents || anyTasksProcessed ? SourceTaskExit::idle : SourceTaskExit::timeout;
            break;
        case SourceTaskExit::idle:
            anyEvents = true;
            if (!readyFirst_) return SourceTaskExit::idle;
            break;
        }
    }
}
SourceTaskExit SourceTaskScheduler::waitAndProcess() {
    const auto result = process();
    if (result != SourceTaskExit::timeout) return result;
    const auto waited = processEvents(true);
    if (waited == SourceTaskExit::exited || waited == SourceTaskExit::outOfWaiters) return waited;
    const auto after = process();
    return after == SourceTaskExit::timeout ? SourceTaskExit::idle : after;
}
void RequestTask::sleep(std::chrono::milliseconds duration) {
#ifndef NDEBUG
    if (duration.count() < 0) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Argument to sleep must not be negative."),
        compat::RetainedString::fromStaticLiteral("vibe/core/core.d"), 1089);
#endif
    if (duration.count() <= 0) return;
    // core.d sleep constructs a fresh Timer for each invocation. Retain that
    // exact timer through its real native completion, including cancellation;
    // a recycled TaskFiber must not keep and reuse the previous sleep's timer.
    const auto timer = std::make_shared<asio::steady_timer>(context());
    timer_ = timer;
    compat::ngRunWithScopeExit([&] {
        await([&](auto done) {
            const auto cancellation = asio::get_associated_cancellation_slot(done);
            timer->expires_after(duration);
            timer->async_wait(asio::bind_cancellation_slot(cancellation,
                [done = std::move(done), timer](Error error) {
                    // Timer.wait reports false for a stopped timer; sleep discards
                    // that boolean rather than throwing an I/O exception.
                    if (error == asio::error::operation_aborted) error.clear();
                    done(error);
                }));
        });
    }, [&] {
        if (timer_ == timer) timer_.reset();
    });
}
void RequestTask::cancelTimer() {
    if (timer_) { Error ignored; timer_->cancel(ignored); }
}
void RequestTask::disposeAfterLoopExit() {
    if (disposing_) return;
    disposing_ = true;
    cancelTimer();
    if (fiber_ && fiber_->state() == core::tasks::TaskFiber::State::hold) {
        auto* previous = current_;
        current_ = this;
        compat::ngRunWithScopeExit([&] {
            try { fiber_->call(); }
            catch (const ThreadUnwind&) {}
        }, [&] { current_ = previous; });
    }
    releaseFiber();
    body_ = {};
    if (auto terminate = std::exchange(terminate_, {})) terminate();
    clearFlags();
    scheduler_.complete(this);
}
void SourceTaskScheduler::shutdown() {
    if (shuttingDown_) return;
    shuttingDown_ = true;
    while (readyFirst_) (void)popReady();
    auto tasks = std::exchange(active_, {});
    for (const auto& task : tasks) task->disposeAfterLoopExit();
    while (availableCount_) {
        auto task = std::exchange(available_[--availableCount_], {});
        task->disposeAfterLoopExit();
    }
    dummy_.reset();
}
SourceYieldLock::SourceYieldLock(SourceTaskScheduler& scheduler, compat::RetainedString file, std::size_t line)
    : fiber_(scheduler.retainCurrentFiber()) {
    if (fiber_->yieldLocks_ <= 0)
        sourceTaskAssert([&] { return fiber_->yieldLocks_ == 0; }, "Yield lock counter negative!?", 715);
    sourceTaskAssert([&] { return fiber_->yieldLocks_ >= 0; },
        "Acquiring yield lock with negative yield lock counter!?", 722);
    if (!fiber_->yieldLocks_) { fiber_->yieldFile_ = std::move(file); fiber_->yieldLine_ = line; }
    ++fiber_->yieldLocks_;
}
SourceYieldLock::~SourceYieldLock() noexcept(false) {
    if (!fiber_) return;
    sourceTaskAssert([&] { return fiber_->yieldLocks_ > 0; },
        "Releasing yield lock with non-positive yield lock counter!?", 728);
    if (!--fiber_->yieldLocks_) { fiber_->yieldFile_ = {}; fiber_->yieldLine_ = 0; }
}
void RequestTask::failWait(Error error, AwaitKind kind) {
    if (kind == AwaitKind::native) throw boost::system::system_error(error);
    bool disconnected = error == asio::error::shut_down || error == asio::error::connection_reset ||
        error == asio::error::network_reset || error == asio::error::timed_out;
#ifdef _WIN32
    disconnected |= error.value() == WSAEDISCON || error == asio::error::connection_aborted;
#else
    disconnected |= error == asio::error::broken_pipe || error == asio::error::not_connected;
#endif
    throw compat::ThrowableException(disconnected ? compat::RetainedString::fromStaticLiteral(
        "Connection closed while writing data.") : compat::RetainedString::fromStaticLiteral("Error writing data to socket."),
        compat::RetainedString::fromStaticLiteral("vibe/core/net.d"), disconnected ? 943 : 937);
}
}

namespace nijigenerate::api::mcp::http_transport {
void ngSleep(std::chrono::milliseconds duration) { detail::RequestTask::current().sleep(duration); }
void ngYield() {
    auto& scheduler = detail::sourceTaskSchedulerForThread();
    if (detail::RequestTask::currentOrNull()) scheduler.yield();
    else scheduler.yieldGlobal();
}
}
