#pragma once

#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/source_mutex.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace nijigenerate::providers::mcp::stdio {
using JsonValue = core::settings::SettingsJsonValue;
// Descriptor copies share the source delegate's mutable captured context.
class RetainedMessageHandler {
    using Function = std::function<void(JsonValue)>;
    std::shared_ptr<Function> callable_;
public:
    RetainedMessageHandler() noexcept = default;
    RetainedMessageHandler(std::nullptr_t) noexcept {}
    template<class Callable> requires (
        !std::same_as<std::remove_cvref_t<Callable>, RetainedMessageHandler> &&
        std::constructible_from<Function, Callable>)
    RetainedMessageHandler(Callable&& callable) {
        Function function(std::forward<Callable>(callable));
        if (function) callable_ = std::allocate_shared<Function>(
            nicxlive::core::d_runtime::SourceAllocator<Function>{}, std::move(function));
    }
    RetainedMessageHandler(const RetainedMessageHandler&) noexcept = default;
    RetainedMessageHandler& operator=(const RetainedMessageHandler&) noexcept = default;
    RetainedMessageHandler(RetainedMessageHandler&& other) noexcept : callable_(other.callable_) {}
    RetainedMessageHandler& operator=(RetainedMessageHandler&& other) noexcept {
        callable_ = other.callable_;
        return *this;
    }
    explicit operator bool() const noexcept { return static_cast<bool>(callable_); }
    void operator()(JsonValue message) const {
        const auto callable = callable_;
        if (!callable) compat::ngFailNullAccess(false);
        (*callable)(std::move(message));
    }
};

class Transport {
public:
    using MessageHandler = RetainedMessageHandler;
    virtual ~Transport() noexcept(false) = default;
    virtual void setMessageHandler(MessageHandler handler) = 0;
    virtual void handleMessage(JsonValue message) = 0;
    virtual void sendMessage(JsonValue message) = 0;
    virtual void run() = 0;
    virtual void close() = 0;
};

// A missing D delegate is a programming error, not a caught Exception response.
struct NullMessageHandlerError {};

class StdioTransport : public Transport, public std::enable_shared_from_this<StdioTransport> {
public:
    explicit StdioTransport(SharedFile input = standardInput(), SharedFile output = standardOutput());
    ~StdioTransport() noexcept(false) override = default;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Transport> other);
    virtual int opCmp(std::shared_ptr<Transport> other);
    void setMessageHandler(MessageHandler handler) override;
    void handleMessage(JsonValue message) override;
    void sendMessage(JsonValue message) override;
    void run() override;
    void close() override;
protected:
    virtual void prepareSourceClassInfo();
private:
    MessageHandler messageHandler_;
    SharedFile input_;
    SharedFile output_;
    bool closed_ = false;
    ::nicxlive::core::d_runtime::SourceMutex monitor_;
};

std::shared_ptr<StdioTransport> createStdioTransport();
} // namespace nijigenerate::providers::mcp::stdio

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::providers::mcp::stdio::Transport> {
    inline static constexpr char className[]="mcp.transport.stdio.Transport";
    inline static constexpr char templateArgumentName[]="(mcp.transport.stdio.Transport)";
};

template<> struct SourceTypeSpellings<::nijigenerate::providers::mcp::stdio::Transport> {
    inline static constexpr bool isNativeObject=true;
    inline static constexpr char qualifiedArgument[]="(mcp.transport.stdio.Transport)";
    inline static constexpr char unqualifiedArgument[]="(Transport)";
    inline static constexpr char qualifiedElement[]="mcp.transport.stdio.Transport";
    inline static constexpr char unqualifiedElement[]="Transport";
};

template<> struct SourceObjectTypeNames<::nijigenerate::providers::mcp::stdio::StdioTransport> {
    inline static constexpr char className[] = "mcp.transport.stdio.StdioTransport";
    inline static constexpr char templateArgumentName[] = "(mcp.transport.stdio.StdioTransport)";
};
template<> struct SourceTypeSpellings<::nijigenerate::providers::mcp::stdio::StdioTransport> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(mcp.transport.stdio.StdioTransport)";
    inline static constexpr char unqualifiedArgument[] = "(StdioTransport)";
    inline static constexpr char qualifiedElement[] = "mcp.transport.stdio.StdioTransport";
    inline static constexpr char unqualifiedElement[] = "StdioTransport";
};
} // namespace nicxlive::core::d_runtime
