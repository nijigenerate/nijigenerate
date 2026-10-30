#pragma once
#include "protocol.hpp"
#include "types.hpp"
#include "transport/stdio.hpp"
#include <nijigenerate/compat/shared_delegate.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace nijigenerate::api::acp::agent {
using String = compat::RetainedStringSlice;
using JsonValue = core::settings::SettingsJsonValue;
using PromptBlocks = core::settings::SettingsJsonArray;

// Keep the public spelling while consuming the one real retained delegate provider.
template<class Result, class... Args>
using RetainedAgentDelegate = compat::SharedDelegate<Result(Args...)>;
using CancellationHandler = RetainedAgentDelegate<bool>;
using ApplyEditHandler = RetainedAgentDelegate<bool, types::WorkspaceEdit>;
using PromptHandler = RetainedAgentDelegate<String, String, PromptBlocks, CancellationHandler>;
struct ACPAgentState;

class ACPAgent : public std::enable_shared_from_this<ACPAgent> {
private:
    std::shared_ptr<ACPAgentState> state_;
    std::shared_ptr<transport::stdio::Transport> transport_;
protected:
    ACPAgent();
    void initialize(std::shared_ptr<transport::stdio::Transport> transport,
        String name = compat::RetainedString::fromStaticLiteral("nijigenerate-agent"),
        String version = compat::RetainedString::fromStaticLiteral("0.1.0"));
    void initialize(String name = compat::RetainedString::fromStaticLiteral("nijigenerate-agent"),
        String version = compat::RetainedString::fromStaticLiteral("0.1.0"));
public:
    // Callback slots belong to retained state, including when transport retains its delegate.
    ApplyEditHandler& onApplyEdit;
    PromptHandler& onPrompt;
    static std::shared_ptr<ACPAgent> ngCreate(std::shared_ptr<transport::stdio::Transport> transport,
        String name = compat::RetainedString::fromStaticLiteral("nijigenerate-agent"),
        String version = compat::RetainedString::fromStaticLiteral("0.1.0"));
    static std::shared_ptr<ACPAgent> ngCreate(String name = compat::RetainedString::fromStaticLiteral("nijigenerate-agent"),
        String version = compat::RetainedString::fromStaticLiteral("0.1.0"));
    template<class Derived, class... Args>
    static std::shared_ptr<Derived> ngCreateDerived(Args&&... args) {
        static_assert(std::is_base_of_v<ACPAgent, Derived>);
        // Native construction establishes fields; initialize models the source
        // constructor and calls the base initialize at its authored super site.
        // A private derived initialize must friend ACPAgent.
        nicxlive::core::d_runtime::SourceAllocator<Derived> allocator;
        auto* storage=allocator.allocate(1);
        try {
            ::new(static_cast<void*>(storage)) Derived;
        } catch(...) {
            allocator.deallocate(storage,1);
            throw;
        }
        auto receiver=std::shared_ptr<Derived>(storage,[allocator](Derived* object) mutable {
            std::destroy_at(object);
            allocator.deallocate(object,1);
        },allocator);
        receiver->initialize(std::forward<Args>(args)...);
        return receiver;
    }
    virtual ~ACPAgent() = default;
    ACPAgent(const ACPAgent&) = delete;
    ACPAgent& operator=(const ACPAgent&) = delete;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<ACPAgent> other);
    virtual int opCmp(std::shared_ptr<ACPAgent> other);
    virtual void start();
    virtual void sendStatus(types::StatusNotification notification);
    virtual void sendSessionUpdate(String sessionId, JsonValue update);
    // Native terminal lifecycle: closes transport and releases the retained delegate graph.
    void ngClose();
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::api::acp::agent::ACPAgent> {
    inline static constexpr char className[]="nijigenerate.api.acp.agent.ACPAgent";
    inline static constexpr char templateArgumentName[]="(nijigenerate.api.acp.agent.ACPAgent)";
};

template<> struct SourceTypeSpellings<::nijigenerate::api::acp::agent::ACPAgent> {
    inline static constexpr bool isNativeObject=true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.api.acp.agent.ACPAgent)"),
            RetainedString::fromStaticLiteral("(ACPAgent)"),
            RetainedString::fromStaticLiteral("nijigenerate.api.acp.agent.ACPAgent"),
            RetainedString::fromStaticLiteral("ACPAgent")};
    }
};
} // namespace nicxlive::core::d_runtime
