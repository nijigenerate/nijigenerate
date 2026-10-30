#include "dragdrop.hpp"
#include <nodes/node.hpp>
#include <imgui_internal.h>
#include <unordered_map>
#include <cstring>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::widgets {
namespace {
struct PayloadOwner { DragdropNodeHandle node; };
thread_local std::unordered_map<ImGuiContext*,std::unique_ptr<PayloadOwner>> owners;
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
    if(hook->Type==ImGuiContextHookType_Shutdown) { owners.erase(context);return; }
    if(!context->DragDropActive) static_cast<PayloadOwner*>(hook->UserData)->node.reset();
}
PayloadOwner& ownerForContext() {
    auto* context=ImGui::GetCurrentContext();
    auto& owner=allocatePayloadMemory([&]() -> auto& { return owners[context]; });
    if(!owner) {
        owner=allocatePayloadMemory([] { return std::make_unique<PayloadOwner>(); });
        ImGuiContextHook hook{};
        hook.Callback=payloadHook;hook.UserData=owner.get();hook.Type=ImGuiContextHookType_EndFramePost;
        ImGui::AddContextHook(context,&hook);
        hook.Type=ImGuiContextHookType_Shutdown;ImGui::AddContextHook(context,&hook);
    }
    return *owner;
}
}
void ngSetNodeDragDropPayload(DragdropNodeHandle node,ImGuiCond condition) {
    auto& owner=ownerForContext();owner.node=std::move(node);
    auto* pointer=owner.node.get();
    ImGui::SetDragDropPayload("_PUPPETNTREE",&pointer,sizeof(pointer),condition);
}
DragdropNodeHandle ngNodeFromDragDropPayload(const ImGuiPayload* payload) {
    nicxlive::core::nodes::Node* pointer;
    std::memcpy(&pointer,payload->Data,sizeof(pointer));
    return pointer->shared_from_this();
}
}
