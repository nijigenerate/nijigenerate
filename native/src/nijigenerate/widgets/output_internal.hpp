#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/retained_string_map.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/resource_key_map.hpp>
#include <nijigenerate/panels/inspector/common.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdint>
#include <functional>
#include <source_location>
#include <new>
#include <stdexcept>
#include <utility>

namespace nijigenerate::widgets::output {

// The source's per-thread static constructor runs once before normal panel startup.
// Inspector creation remains a separate lazy operation.
void ngBootstrapOutputThread();

namespace detail {
template<class Operation> decltype(auto) allocateOutputMemory(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline compat::SourceCString outputCString(std::string_view input) {
    compat::SourceCString result(input);
    compat::detail::ngSourceCStringPostconditions(input, result.data());
    return result;
}
// D TypeInfo_uint.getHash uses the scalar value; the SDK owns the actual
// AA mixing, probing, shared storage and bucket enumeration.
struct UuidKeyHash {
    std::size_t operator()(std::uint32_t value) const noexcept { return value; }
};
template<class Value> class UuidMap : public nicxlive::core::common::ResourceKeyMap<std::uint32_t,Value,
    UuidKeyHash,std::equal_to<std::uint32_t>> {
    using Storage=nicxlive::core::common::ResourceKeyMap<std::uint32_t,Value,
        UuidKeyHash,std::equal_to<std::uint32_t>>;
public:
    // Existing panel callers access actual struct fields through these lvalues.
    // All entries and AA behavior remain in the single canonical SDK storage.
    Value& operator[](std::uint32_t key) { return static_cast<Value&>(Storage::operator[](key)); }
    Value& at(std::uint32_t key) {
        auto found=this->find(key);
        if(found==this->end()) throw compat::ngStaticError<compat::RangeError>(std::source_location::current());
        return found->second;
    }
    Value at(std::uint32_t key) const { return Storage::at(key); }
    void try_emplace(std::uint32_t key) { (void)Storage::operator[](key); }
};
extern thread_local compat::RetainedString parameterGrabStr;
extern thread_local compat::RetainedStringMap<compat::RetainedString> transformName;
using Node = nicxlive::core::nodes::Node;
using NodeInspector = panels::inspector::InspectorHolder<Node>;
extern thread_local std::shared_ptr<NodeInspector> nodeInspector;
// The actual factory/view providers remain required; these are declarations.
void initInspectors();
void onNodeView(std::shared_ptr<Node> node, std::shared_ptr<NodeInspector> inspector = {});
void onParameterView(std::uint64_t index, std::shared_ptr<nicxlive::core::param::Parameter> parameter);
inline constexpr int NodeViewWidth = 250;
enum class PopupSide : std::int32_t { HorzLarger, HorzNarrower, VertLarger, VertNarrower };

compat::RetainedString getTransformText(compat::RetainedString name);
void setTransparency(float alpha, float text);
bool isWindowHovered();
ImRect getOuterRect(ImGuiWindow* window);

template<PopupSide side>
void adjustWindowPos(ImVec2 minPos, ImVec2 maxPos, ImGuiWindow* window) {
    if (!window || !window->RootWindow) return;
    const ImRect outer = getOuterRect(window);
    const float spaceLeft = minPos.x - outer.Min.x;
    const float spaceRight = outer.Max.x - outer.Min.x - maxPos.x;
    const float spaceUp = minPos.y - outer.Min.y;
    const float spaceDown = outer.Max.y - outer.Min.y - minPos.y;

    switch (side) {
    case PopupSide::HorzLarger:
        if (spaceLeft > spaceRight)
            ImGui::SetWindowPos(window, ImVec2(minPos.x - window->Size.x, minPos.y));
        else ImGui::SetWindowPos(window, ImVec2(maxPos.x, minPos.y));
        break;
    case PopupSide::HorzNarrower:
        if (spaceLeft > spaceRight)
            ImGui::SetWindowPos(window, ImVec2(maxPos.x, minPos.y));
        else ImGui::SetWindowPos(window, ImVec2(minPos.x - window->Size.x, minPos.y));
        break;
    case PopupSide::VertLarger:
        if (spaceUp > spaceDown)
            ImGui::SetWindowPos(window, ImVec2(minPos.x, minPos.y - window->Size.y));
        else ImGui::SetWindowPos(window, ImVec2(minPos.x, maxPos.y));
        break;
    case PopupSide::VertNarrower:
        if (spaceUp > spaceDown)
            ImGui::SetWindowPos(window, ImVec2(minPos.x, maxPos.y));
        else ImGui::SetWindowPos(window, ImVec2(minPos.x, minPos.y - window->Size.y));
        break;
    default:
        break;
    }
    if (side == PopupSide::HorzLarger || side == PopupSide::HorzNarrower) {
        if (window->Size.y >= spaceDown)
            ImGui::SetWindowPos(window, ImVec2(window->Pos.x, window->Pos.y - (window->Size.y - spaceDown)));
        if (window->Pos.y < 0)
            ImGui::SetWindowPos(window, ImVec2(window->Pos.x, 0));
    } else {
        if (window->Size.x >= spaceRight)
            ImGui::SetWindowPos(window, ImVec2(window->Pos.x - (window->Size.x - spaceRight), window->Pos.y));
        if (window->Pos.x < 0)
            ImGui::SetWindowPos(window, ImVec2(0, window->Pos.y));
    }
}

} // namespace detail
} // namespace nijigenerate::widgets::output
