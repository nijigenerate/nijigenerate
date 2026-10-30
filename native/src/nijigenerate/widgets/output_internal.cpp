#include "output_internal.hpp"
#include "output_tree.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::widgets::output {
namespace {
// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Notice: nicxlive/core/licenses/druntime-associative-array.txt.
void registerOutputClassNames() {
    static const bool registered=[] {
        const auto registerType = []<class T> {
            using namespace nicxlive::core::d_runtime;
            using Names = SourceObjectTypeNames<T>;
            const auto name = compat::RetainedString::fromStaticLiteral(Names::className);
            ngRegisterObjectTypeNames<T>(name,
                compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
            ngRegisterObjectClassName(typeid(T), name);
        };
        registerType.template operator()<ListOutput>();
        registerType.template operator()<IconTreeOutput>();
        registerType.template operator()<ViewOutput>();
        return true;
    }();
    (void)registered;
}
}
compat::RetainedString Output::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    registerOutputClassNames();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Output::toHash() {
    const auto address=reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool Output::opEquals(std::shared_ptr<Output> other) {
    return other && dynamic_cast<const void*>(this)==dynamic_cast<const void*>(other.get());
}
int Output::opCmp(std::shared_ptr<Output>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    registerOutputClassNames();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    auto message = detail::allocateOutputMemory([&] {
        return compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    });
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("object.d"),195);
}
namespace detail {
thread_local compat::RetainedString parameterGrabStr;
thread_local compat::RetainedStringMap<compat::RetainedString> transformName;
thread_local std::shared_ptr<NodeInspector> nodeInspector;

compat::RetainedString getTransformText(compat::RetainedString name) {
    const auto value = transformName.find(name.view());
    return value ? *value : name;
}

void setTransparency(float alpha, float text) {
    constexpr ImGuiCol colors[] = {ImGuiCol_WindowBg, ImGuiCol_Text, ImGuiCol_FrameBg,
        ImGuiCol_Button, ImGuiCol_Border, ImGuiCol_PopupBg};
    for (auto id : colors) {
        ImVec4 style = ImGui::GetStyleColorVec4(id);
        style.w = id == ImGuiCol_Text ? text : alpha;
        ImGui::PushStyleColor(id, style);
    }
}

bool isWindowHovered() {
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) return true;
    const ImVec2 current = ImGui::GetMousePos();
    const ImVec2 position = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    return position.x <= current.x && current.x < position.x + size.x &&
        position.y <= current.y && current.y < position.y + size.y;
}

ImRect getOuterRect(ImGuiWindow* window) {
    return ImGui::GetPopupAllowedExtentRect(window);
}
} // namespace detail

void ngBootstrapOutputThread() {
    thread_local bool initialized=false;
    if(initialized)return;
    compat::RetainedStringMap<compat::RetainedString> values;
    using Text = compat::RetainedString;
    values.setRetained(Text::fromStaticLiteral("transform.t.x"), Text::fromStaticLiteral("\uE89F-X"));
    values.setRetained(Text::fromStaticLiteral("transform.t.y"), Text::fromStaticLiteral("\uE89F-Y"));
    values.setRetained(Text::fromStaticLiteral("transform.t.z"), Text::fromStaticLiteral("\uE89F-Z"));
    values.setRetained(Text::fromStaticLiteral("transform.s.x"), Text::fromStaticLiteral("\uE8FF-X"));
    values.setRetained(Text::fromStaticLiteral("transform.s.y"), Text::fromStaticLiteral("\uE8FF-Y"));
    values.setRetained(Text::fromStaticLiteral("transform.s.z"), Text::fromStaticLiteral("\uE8FF-Z"));
    values.setRetained(Text::fromStaticLiteral("transform.r.x"), Text::fromStaticLiteral("\uE863-X"));
    values.setRetained(Text::fromStaticLiteral("transform.r.y"), Text::fromStaticLiteral("\uE863-Y"));
    values.setRetained(Text::fromStaticLiteral("transform.r.z"), Text::fromStaticLiteral("\uE863-Z"));
    values.setRetained(Text::fromStaticLiteral("deform"), Text::fromStaticLiteral("\uE3EA"));
    detail::transformName = std::move(values);
    initialized=true;
}
} // namespace nijigenerate::widgets::output
