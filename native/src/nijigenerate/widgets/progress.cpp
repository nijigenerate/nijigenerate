#include <nijigenerate/widgets/progress.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <imgui_internal.h>
#include <cmath>
#include <numbers>
namespace nijigenerate::widgets {
namespace {
template<class Text> const char* endOf(const Text& s) { return s.data() ? s.data()+s.size() : s.data(); }
// Retained Source inputs and explicit native views share these actual bodies.
template<class Text> bool bufferBar(Text label, float value, ImVec2 size, ImU32 background, ImU32 foreground) {
    auto* window=ImGui::GetCurrentWindow();
    if (compat::ngRequireObject(window)->SkipItems) return false;
    const auto available=ImGui::GetContentRegionAvail();
    if (size.x<=0) size.x=available.x-size.x;
    if (size.y==0) size.y=64;
    value=value<0 ? 0 : (value>1 ? 1 : value);
    auto* context=ImGui::GetCurrentContext();
    const auto style=compat::ngRequireObject(context)->Style;
    const auto id=window->GetID(label.data(),endOf(label));
    const auto position=window->DC.CursorPos;
    size.x-=style.FramePadding.x*2;
    const ImRect strip({position.x,position.y+((size.y/2)-2)},
                       {position.x+size.x,position.y+((size.y/2)+2)});
    const ImRect bounds(position,{position.x+size.x,position.y+size.y});
    ImGui::ItemSize(bounds,style.FramePadding.y);
    if (!ImGui::ItemAdd(bounds,id)) return false;
    const float circleStart=size.x;
    auto* backgroundDraw=ImGui::GetWindowDrawList();
    const ImVec2 backgroundEnd{position.x+circleStart,strip.Max.y};
    compat::ngRequireObject(backgroundDraw)->AddRectFilled(
        strip.Min,backgroundEnd,background,8,ImDrawFlags_RoundCornersAll);
    auto* foregroundDraw=ImGui::GetWindowDrawList();
    const ImVec2 foregroundEnd{position.x+circleStart*value,strip.Max.y};
    compat::ngRequireObject(foregroundDraw)->AddRectFilled(
        strip.Min,foregroundEnd,foreground,8,ImDrawFlags_RoundCornersAll);
    ImGui::NewLine();
    return false;
}
template<class Text> bool spinner(Text label, float radius, int thickness, ImU32 color) {
    auto* window=ImGui::GetCurrentWindow();
    if (compat::ngRequireObject(window)->SkipItems) return false;
    const auto available=ImGui::GetContentRegionAvail();
    if (radius<=0) radius=(available.y<available.x ? available.y : available.x)/2-radius;
    auto* context=ImGui::GetCurrentContext();
    const auto style=compat::ngRequireObject(context)->Style;
    const auto id=window->GetID(label.data(),endOf(label));
    const auto position=window->DC.CursorPos;
    const ImVec2 size(radius*2,(radius+style.FramePadding.y)*2);
    const ImRect bounds(position,{position.x+size.x,position.y+size.y});
    ImGui::ItemSize(bounds,style.FramePadding.y);
    if (!ImGui::ItemAdd(bounds,id)) return false;
    compat::ngRequireObject(ImGui::GetWindowDrawList())->PathClear();
    constexpr int segments=45;
    // LDC promotes the source literal before rounding it to float (observed at time 1.25).
    const float start=static_cast<float>(std::abs(std::sin(context->Time*1.4)*(segments-5)));
    const float aMin=static_cast<float>(std::numbers::pi*2*(start/static_cast<float>(segments)));
    const float aMax=static_cast<float>(std::numbers::pi*2*(static_cast<float>(segments-3)/segments));
    const ImVec2 center(position.x+radius,position.y+radius+style.FramePadding.y);
    for (int i=0;i<segments;++i) {
        const float a=aMin+(static_cast<float>(i)/segments)*(aMax-aMin);
        auto* draw=ImGui::GetWindowDrawList();
        const ImVec2 point{
            static_cast<float>(center.x+std::cos(a+context->Time*8)*(radius-thickness)),
            static_cast<float>(center.y+std::sin(a+context->Time*8)*(radius-thickness))};
        compat::ngRequireObject(draw)->PathLineTo(point);
    }
    auto* strokeDraw=ImGui::GetWindowDrawList();
    const auto strokeThickness=static_cast<float>(thickness);
    compat::ngRequireObject(strokeDraw)->PathStroke(color,ImDrawFlags_None,strokeThickness);
    ImGui::NewLine();
    return false;
}
} // namespace
bool ngBufferBar(compat::RetainedStringSlice label, float value, ImVec2 size, ImU32 background, ImU32 foreground) {
    return bufferBar(label, value, size, background, foreground);
}
bool ngBufferBar(std::string_view label, float value, ImVec2 size, ImU32 background, ImU32 foreground) {
    return bufferBar(label, value, size, background, foreground);
}
bool ngSpinner(compat::RetainedStringSlice label, float radius, int thickness, ImU32 color) {
    return spinner(label, radius, thickness, color);
}
bool ngSpinner(std::string_view label, float radius, int thickness, ImU32 color) {
    return spinner(label, radius, thickness, color);
}
}
