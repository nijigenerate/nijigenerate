#include "auth.hpp"
#include "auth_wait.hpp"
#include "http_auth_time.hpp"
#include "task.hpp"
#include <nijigenerate/compat/phobos_monotime.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/widgets/notification.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <array>
#include <chrono>
#include <string_view>
#include <string>
#include <utility>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::api::mcp::auth {
namespace {
using Duration = compat::SourceMonoTime::Duration;
using Clock = http_transport::detail::SourceAuthClock;
thread_local Duration approvalTimeout{std::chrono::seconds(120)};
template<class Format, class... Args> void authLog(Format&& format, Args&&... args) {
#if defined(NIJIGENERATE_MCP_LOG)
    using namespace core::translation;
    const std::array<TypedFormatArgument, sizeof...(Args)> arguments{
        ngFormatArgument(std::forward<Args>(args))...};
    providers::mcp::stdio::standardOutput().withTextWriter(
        [&](providers::mcp::stdio::FileTextWriter& writer) {
            ngFormatTypedTo([&](std::string_view bytes) { writer.put(bytes); },
                std::string_view(format), arguments);
            writer.put(static_cast<unsigned char>('\n'));
        });
#else
    (void)format;
    ((void)args, ...);
#endif
}
struct ApprovalState {
    compat::RetainedStringSlice decision;
    std::uint64_t popupId = 0;
};
compat::RetainedString retainMessage(std::string message) {
    try { return compat::RetainedString(std::move(message)); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
compat::RetainedStringSlice ngSimpleAuth(ApprovalRequest request) {
    const auto state = std::allocate_shared<ApprovalState>(
        nicxlive::core::d_runtime::SourceAllocator<ApprovalState>{});
    task::ngRunInMainThread(task::Action([state, request] {
        authLog("[MCP/auth] popup");
        state->popupId = widgets::NotificationPopup::instance()->popup(
            widgets::NotificationPopup::Callback([state, request](ImGuiIO*) {
                using namespace core::translation;
                const auto text = ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                    "Got authentication request for MCP server from %s (scope=%s), Do you approve it?"));
                const std::array arguments{ngFormatArgument(request.clientId), ngFormatArgument(request.scopeId)};
                const auto message = retainMessage(ngFormatTyped(text.view(), arguments));
                const compat::SourceCString cMessage{compat::RetainedStringSlice(message)};
                compat::detail::ngSourceCStringPostconditions(message.view(), cMessage.data());
                ImGui::Text(cMessage.data());
                ImGui::SameLine();
                const auto deny = ngTranslateCString(std::string_view("\xEE\x97\x8D Deny"));
                if (widgets::ngButtonColored(deny.data())) {
                    state->decision = compat::RetainedString::fromStaticLiteral("deny");
                    widgets::NotificationPopup::instance()->close(state->popupId);
                }
                ImGui::SameLine();
                const auto approve = ngTranslateCString(std::string_view("\xEE\x99\xA8 Approve"));
                if (widgets::ngButtonColored(approve.data())) {
                    state->decision = compat::RetainedString::fromStaticLiteral("approve");
                    widgets::NotificationPopup::instance()->close(state->popupId);
                }
            }), 120);
    }));
    authLog("[MCP/auth] wait for close");
    const auto deadline = Clock::now() + approvalTimeout;
    while (Clock::now() < deadline) {
        if (!state->decision.empty()) break;
        http_transport::ngSleep(std::chrono::milliseconds(200));
    }
    task::ngRunInMainThread(task::Action([state] {
        widgets::NotificationPopup::instance()->close(state->popupId);
    }));
    authLog("[MCP/auth] auth done");
    return state->decision;
}
}
