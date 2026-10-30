#pragma once
#include "client_process.hpp"
#include "client_bytes.hpp"
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <exception>
#include <new>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
namespace nicxlive::core::d_runtime {
class SourceMutex;
class SourceCondition;
}
namespace nijigenerate::api::acp::client {
using String = compat::RetainedStringSlice;
using JsonValue = core::settings::SettingsJsonValue;
using StdioTransport = providers::mcp::stdio::StdioTransport;
class ACPClient : public std::enable_shared_from_this<ACPClient> {
    // Allocate the real class and its native owner before process-launch effects.
    // Explicit friendship preserves the actual protected constructor access.
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    template<class T> using SourceAllocator = nicxlive::core::d_runtime::SourceAllocator<T>;
public:
    struct PollResult { bool hasValue = false; JsonValue result; String error; };
    static std::shared_ptr<ACPClient> ngCreate(std::shared_ptr<StdioTransport> transport);
    static std::shared_ptr<ACPClient> ngCreate(compat::Slice<String> command, String cwd = {});
    template<class T, class... A> static std::shared_ptr<T> ngCreateDerived(A&&... args) {
        auto result = std::allocate_shared<T>(SourceAllocator<T>{},std::forward<A>(args)...);
        result->startReader();
        if (result->ownsProcess_) result->startStderrReader();
        return result;
    }
    virtual ~ACPClient();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<ACPClient> other);
    virtual int opCmp(std::shared_ptr<ACPClient> other);
    virtual String getInitError();
    virtual JsonValue getInitResult();
    virtual detail::Pid getPid();
    virtual int getStdoutFd();
    virtual PollResult decodeJson(String payload);
    virtual std::ptrdiff_t findByteIndex(compat::ConstSlice<std::uint8_t> bytes, std::uint8_t value);
    virtual compat::Slice<std::uint8_t> truncateAtNull(compat::Slice<std::uint8_t> bytes);
    virtual bool tryDecodeEscapedJsonString(String payload, JsonValue& result);
    virtual void logParseFailure(String payload, String error);
    virtual void logParseFailureBytes(compat::ConstSlice<std::uint8_t> payload, String error);
    virtual String sanitizeJsonPayload(String payload);
    virtual String escapeJsonString(String text);
    virtual void setDebugStdout(bool enabled);
    virtual void setDebugEcho(bool enabled);
    virtual void setLogger(compat::SharedDelegate<void(String)> callback);
    virtual void setCancelCheck(compat::SharedDelegate<bool()> callback);
    virtual void log(String message);
    virtual void setSendMode(String mode);
    virtual void sendRawLine(String raw);
    virtual String sendPrompt(String text);
    virtual void cancelPrompt();
    virtual void sendPermissionResponse(String id, String option, bool cancelled = false);
    virtual void sendPermissionResponse(String id, bool granted);
    virtual void startReader();
    virtual void startStderrReader();
    virtual compat::Slice<PollResult> takeInbound();
    virtual JsonValue initialize();
    virtual void newSession();
    virtual void initializeAsync();
    virtual bool pollInitialize();
    virtual JsonValue buildInitParams();
    virtual JsonValue currentMcpServers();
    virtual void ping();
    virtual void close();
    virtual PollResult tryPollOnce();
protected:
    explicit ACPClient(std::shared_ptr<StdioTransport> transport);
    explicit ACPClient(compat::Slice<String> command, String cwd = {});
private:
    std::shared_ptr<StdioTransport> transport_;
    detail::ProcessPipes pipes_;
    std::size_t nextId_ = 0;
    bool ownsProcess_ = false;
    std::atomic_bool debugStdout_{false}, debugEcho_{true}, readerRunning_{false}, stderrRunning_{false};
    compat::SharedDelegate<void(String)> logger_;
    compat::SharedDelegate<bool()> cancelCheck_;
    std::recursive_mutex callbackMutex_;
    bool initPending_ = false;
    JsonValue initResult_;
    String initError_, sessionId_;
    compat::Slice<PollResult> inboundQueue_;
    std::shared_ptr<nicxlive::core::d_runtime::SourceMutex> inboundMutex_;
    std::shared_ptr<nicxlive::core::d_runtime::SourceCondition> inboundCondition_;
    struct NativeThread;
    std::shared_ptr<NativeThread> reader_, stderrThread_;
    compat::Slice<std::uint8_t> readBuffer_;
    String sendMode_ = compat::RetainedString::fromStaticLiteral("line");
    void send(JsonValue value);
    String stateSnapshot();
    int stdoutFd();
    PollResult popInbound();
    void readerLoop();
    void appendChunkBytes(compat::ConstSlice<std::uint8_t> bytes);
    void parseReadBuffer();
    void enqueueResult(PollResult result);
    JsonValue waitResponse();
    String drainStderr();
    void stderrLoop();
    bool readStdoutChunk(String& chunk);
    bool readStderrChunk(String& chunk);
};
} // namespace nijigenerate::api::acp::client

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::client::ACPClient> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.api.acp.client.ACPClient)";
    inline static constexpr char unqualifiedArgument[] = "(ACPClient)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.api.acp.client.ACPClient";
    inline static constexpr char unqualifiedElement[] = "ACPClient";
};
template<> struct SourceObjectTypeNames<::nijigenerate::api::acp::client::ACPClient> {
    inline static constexpr char className[] = "nijigenerate.api.acp.client.ACPClient";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.api.acp.client.ACPClient)";
};
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::client::ACPClient::PollResult> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.api.acp.client.ACPClient.PollResult)";
    inline static constexpr char unqualifiedArgument[] = "(PollResult)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.api.acp.client.ACPClient.PollResult";
    inline static constexpr char unqualifiedElement[] = "PollResult";
};
}
