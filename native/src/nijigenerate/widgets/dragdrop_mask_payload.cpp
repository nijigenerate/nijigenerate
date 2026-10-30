#include "dragdrop_mask_payload.hpp"
#include <imgui_internal.h>
#include <cstring>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::widgets {
namespace {
using MaskBinding=nicxlive::core::nodes::MaskBinding;
struct ContextPayload {
    std::optional<MaskBinding> binding;
};
thread_local std::unordered_map<ImGuiContext*,std::unique_ptr<ContextPayload>> payloads;

template<class Operation> decltype(auto) allocatePayloadMemory(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

void payloadHook(ImGuiContext* context,ImGuiContextHook* hook) {
    if(hook->Type==ImGuiContextHookType_Shutdown) {
        const auto found=payloads.find(context);
        if(found!=payloads.end()) {
            [[maybe_unused]] const auto previous=std::move(found->second);
            payloads.erase(found);
        }
        return;
    }
    if(!context->DragDropActive) {
        auto& binding=static_cast<ContextPayload*>(hook->UserData)->binding;
        [[maybe_unused]] const auto previous=binding;
        binding.reset();
    }
}

ContextPayload& payloadForContext() {
    auto* context=ImGui::GetCurrentContext();
    auto& payload=allocatePayloadMemory([&]() -> auto& { return payloads[context]; });
    if(!payload) {
        payload=allocatePayloadMemory([] { return std::make_unique<ContextPayload>(); });
        ImGuiContextHook hook{};
        hook.Callback=payloadHook;
        hook.UserData=payload.get();
        hook.Type=ImGuiContextHookType_EndFramePost;
        ImGui::AddContextHook(context,&hook);
        hook.Type=ImGuiContextHookType_Shutdown;
        ImGui::AddContextHook(context,&hook);
    }
    return *payload;
}
}

void ngSetMaskDragDropPayload(const MaskBinding& binding,ImGuiCond condition) {
    // Retain the actual value, never memcpy a nontrivial MaskBinding object.
    auto& owner=payloadForContext();
    // Finish replacing all fields and publishing the wire before destruction of
    // the previous Drawable can reenter this same context through user cleanup.
    [[maybe_unused]] const auto previous=owner.binding;
    owner.binding=binding;
    const MaskBinding* pointer=&*owner.binding;
    ImGui::SetDragDropPayload("_MASKITEM",&pointer,sizeof(pointer),condition);
}

const MaskBinding* ngMaskFromDragDropPayload(const ImGuiPayload* payload) {
    if(!payload || !payload->Data || payload->DataSize!=static_cast<int>(sizeof(const MaskBinding*))) return nullptr;
    const MaskBinding* pointer=nullptr;
    std::memcpy(&pointer,payload->Data,sizeof(pointer));
    return pointer;
}
}
