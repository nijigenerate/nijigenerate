// Port of mcp 1.0.1 stdio transport. Copyright 2025 Garret Noble.
// MIT license: native/port/licenses/mcp-stdio.txt.
#include "mcp_stdio.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/api/acp/mcp_protocol.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <new>
#include <stdexcept>
#include <source_location>
#include <utility>

namespace nijigenerate::providers::mcp::stdio {
namespace {
template<class Allocate> decltype(auto) allocateTransportBytes(Allocate&& allocate,
        std::source_location location = std::source_location::current()) {
    try { return std::forward<Allocate>(allocate)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(location);
    }
    catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(location);
    }
}
// The current std.uni.isWhite set, matched as canonical UTF-8 byte sequences.
// Invalid UTF-8 decodes to a non-whitespace replacement in std.string.strip.
std::string_view strip(std::string_view input) {
    static constexpr std::array<std::string_view, 25> white{
        "\t", "\n", "\v", "\f", "\r", " ", "\xc2\x85", "\xc2\xa0", "\xe1\x9a\x80",
        "\xe2\x80\x80", "\xe2\x80\x81", "\xe2\x80\x82", "\xe2\x80\x83", "\xe2\x80\x84",
        "\xe2\x80\x85", "\xe2\x80\x86", "\xe2\x80\x87", "\xe2\x80\x88", "\xe2\x80\x89",
        "\xe2\x80\x8a", "\xe2\x80\xa8", "\xe2\x80\xa9", "\xe2\x80\xaf", "\xe2\x81\x9f",
        "\xe3\x80\x80"};
    for (;;) {
        bool removed = false;
        for (const auto bytes : white) {
            if (input.starts_with(bytes)) { input.remove_prefix(bytes.size()); removed = true; break; }
        }
        if (!removed) break;
    }
    for (;;) {
        bool removed = false;
        for (const auto bytes : white) {
            if (input.ends_with(bytes)) { input.remove_suffix(bytes.size()); removed = true; break; }
        }
        if (!removed) return input;
    }
}

JsonValue errorResponse(std::int32_t code, std::string message) {
    const auto text = allocateTransportBytes([&] { return compat::RetainedString(std::move(message)); });
    return protocol::Response::makeError({}, code, text).toJSON();
}
}

StdioTransport::StdioTransport(SharedFile input, SharedFile output)
    : input_(std::move(input)), output_(std::move(output)) {
    // Qualified Object formatting can query ClassInfo before toString runs.
    // Register this defining class without dispatching to a derived hook.
    StdioTransport::prepareSourceClassInfo();
}

// Source Object defaults, object.d, Digital Mars 2000-2011, BSL-1.0.
// Retained notice: nicxlive/licenses/druntime-associative-array.txt.
void StdioTransport::prepareSourceClassInfo() {
    using namespace nicxlive::core::d_runtime;
    allocateTransportBytes([] {
        ngPrepareSourceTypeSpellings<Transport>();
        (void)ngObjectTemplateArgumentName<Transport>();
        ngPrepareSourceTypeSpellings<StdioTransport>();
        (void)ngObjectTemplateArgumentName<StdioTransport>();
        ngRegisterObjectTypeNames(typeid(StdioTransport),
            compat::RetainedString::fromStaticLiteral(SourceObjectTypeNames<StdioTransport>::className),
            compat::RetainedString::fromStaticLiteral(SourceObjectTypeNames<StdioTransport>::templateArgumentName));
    });
}
compat::RetainedString StdioTransport::toString() {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t StdioTransport::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool StdioTransport::opEquals(std::shared_ptr<Transport> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int StdioTransport::opCmp(std::shared_ptr<Transport>) {
    [[maybe_unused]] const auto lifetime = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = allocateTransportBytes([&] {
        std::string text = "need opCmp for class ";
        text += name.view();
        return compat::RetainedString(std::move(text));
    });
    throw compat::ThrowableException(message, compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

void StdioTransport::setMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}
void StdioTransport::handleMessage(JsonValue message) {
    const auto lifetime = weak_from_this().lock();
    const auto handler = messageHandler_;
    handler(std::move(message));
}

void StdioTransport::sendMessage(JsonValue message) {
    const auto lifetime = weak_from_this().lock();
    monitor_.lock();
    compat::ngRunWithScopeExit([&] {
        if (!closed_) {
            output_.writeln(compat::ngJsonStringify(message).view());
            output_.flush();
        }
    }, [&] { monitor_.unlock(); });
}

void StdioTransport::run() {
    const auto lifetime = weak_from_this().lock();
    // Keep eof outside the try and before closed, as in the source loop.
    while (!input_.eof() && !closed_) {
        try {
            auto bytes = input_.readln();
            const compat::RetainedStringSlice storage{allocateTransportBytes([&] {
                return compat::RetainedString(std::move(bytes));
            })};
            const auto line = strip(storage.view());
            if (line.empty()) continue;
            JsonValue message;
            try {
                const auto first = static_cast<std::size_t>(line.data() - storage.view().data());
                autorig::json::detail::FirstPassReader reader{storage.slice(first, first + line.size())};
                message = reader.parseRoot();
            } catch (const core::settings::SettingsJsonError& error) {
                auto message = allocateTransportBytes([&] {
                    return std::string("Invalid JSON: ") + std::string(error.msg.view());
                });
                sendMessage(errorResponse(-32700, std::move(message)));
                continue;
            }
            handleMessage(std::move(message));
        } catch (const compat::ThrowableException& error) {
            auto message = allocateTransportBytes([&] {
                return std::string("Internal error: ") + std::string(error.msg.view());
            });
            sendMessage(errorResponse(-32603, std::move(message)));
        }
    }
}

void StdioTransport::close() {
    const auto lifetime = weak_from_this().lock();
    monitor_.lock();
    compat::ngRunWithScopeExit([&] {
        closed_ = true;
        input_.close();
        output_.close();
    }, [&] { monitor_.unlock(); });
}

std::shared_ptr<StdioTransport> createStdioTransport() {
    standardInput().setLineBuffering(1024);
    standardOutput().setLineBuffering(1024);
    return std::allocate_shared<StdioTransport>(nicxlive::core::d_runtime::SourceAllocator<StdioTransport>{});
}
} // namespace nijigenerate::providers::mcp::stdio
