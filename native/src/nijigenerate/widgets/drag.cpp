// Copyright 2020-2023 Inochi2D Project; 2024 nijigenerate Project.
// Author: Luna Nielsen. BSD-2-Clause; see LICENSE.
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/widgets/drag_internal.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <cstdlib>
namespace nijigenerate::widgets {
namespace {
template<class Text> const char* endOf(const Text& id) { return id.data() ? id.data()+id.size() : id.data(); }
template<class Text> bool dragFloat(Text id, float* value, float adjustSpeed, float minValue, float maxValue,
    Text format, ImGuiSliderFlags flags) {
    auto* storage=ImGui::GetStateStorage();
    const auto uiID=ImGui::GetID(id.data(),endOf(id));
    const float initial=*compat::ngRequireObject(value);
    auto* state=static_cast<detail::DragState*>(compat::ngRequireObject(storage)->GetVoidPtr(uiID));
    if (!state) {
        state=static_cast<detail::DragState*>(std::malloc(sizeof(detail::DragState)));
        if (!state) compat::ngFailNullAccess(true);
        state->initialState=initial;
        state->isActive=true;
        state->wasJustCreated=true;
        storage->SetVoidPtr(uiID,state);
    }
    if (ImGui::DragFloat("###DRAG",value,adjustSpeed,minValue,maxValue,format.data(),flags)) {
        if (!state->isActive) {
            state->initialState=initial;
            state->isActive=true;
            state->wasJustCreated=false;
        }
        return true;
    }
    if (state && state->isActive) {
        state->isActive=false;
        return !state->wasJustCreated;
    }
    return false;
}
template<class Text> bool hasDragState(Text id) {
    auto* storage=ImGui::GetStateStorage();
    const auto uiID=ImGui::GetID(id.data(),endOf(id));
    return compat::ngRequireObject(storage)->GetVoidPtr(uiID)!=nullptr;
}
template<class Text> float dragFloatInitialValue(Text id) {
    auto* storage=ImGui::GetStateStorage();
    const auto uiID=ImGui::GetID(id.data(),endOf(id));
    auto* state=static_cast<detail::DragState*>(compat::ngRequireObject(storage)->GetVoidPtr(uiID));
    return state ? state->initialState : std::numeric_limits<float>::quiet_NaN();
}
} // namespace
bool ngDragFloat(compat::RetainedStringSlice id, float* value, float speed, float min, float max,
    compat::RetainedStringSlice format, ImGuiSliderFlags flags) {
    return dragFloat(id, value, speed, min, max, format, flags);
}
bool ngDragFloat(std::string_view id, float* value, float speed, float min, float max,
    std::string_view format, ImGuiSliderFlags flags) {
    return dragFloat(id, value, speed, min, max, format, flags);
}
bool ngGetHasDragState(compat::RetainedStringSlice id) { return hasDragState(id); }
bool ngGetHasDragState(std::string_view id) { return hasDragState(id); }
float ngGetDragFloatInitialValue(compat::RetainedStringSlice id) { return dragFloatInitialValue(id); }
float ngGetDragFloatInitialValue(std::string_view id) { return dragFloatInitialValue(id); }
}
