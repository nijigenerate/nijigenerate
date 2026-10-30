#include <nijigenerate/widgets/dragdrop.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <imgui_internal.h>
namespace nijigenerate::widgets {
void ngBeginDragDropFake() {
    auto* storage=ImGui::GetStateStorage();
    auto* context=ImGui::GetCurrentContext();
    const auto activeId = ImGui::GetID("DRAG_DROP_ACTIVE");
    const bool active = compat::ngRequireObject(context)->DragDropActive;
    compat::ngRequireObject(storage)->SetBool(activeId, active);
    const auto frameId = ImGui::GetID("DRAG_DROP_FRAME_COUNT");
    const int frame = context->DragDropPayload.DataFrameCount;
    storage->SetInt(frameId, frame);
    context->DragDropActive=true;
    context->DragDropPayload.DataFrameCount=context->FrameCount;
}
void ngEndDragDropFake() {
    auto* storage=ImGui::GetStateStorage();
    auto* context=ImGui::GetCurrentContext();
    const auto activeId = ImGui::GetID("DRAG_DROP_ACTIVE");
    const bool active=compat::ngRequireObject(storage)->GetBool(activeId,false);
    const auto frameId = ImGui::GetID("DRAG_DROP_FRAME_COUNT");
    const int frame=storage->GetInt(frameId,-1);
    if (!context) compat::ngFailNullAccess(true);
    context->DragDropActive=active;
    context->DragDropPayload.DataFrameCount=frame;
}
bool ngHasDragDrop(const char* name) {
    ngBeginDragDropFake();
    const auto* peek=ImGui::AcceptDragDropPayload(name,
        ImGuiDragDropFlags_AcceptPeekOnly|ImGuiDragDropFlags_SourceAllowNullID);
    ngEndDragDropFake();
    return peek && peek->Data;
}
}
