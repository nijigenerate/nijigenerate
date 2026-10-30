#include "flipconfig_window.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/utils.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/io/imageexport_case.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <common/object_key_policy.hpp>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>
#include <string>
#include <new>
#include <stdexcept>

namespace nijigenerate::windows::flipconfig {
namespace {
void prepareFlipWindowClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(FlipPairWindow),
            compat::RetainedString::fromStaticLiteral("nijigenerate.windows.flipconfig.FlipPairWindow"));
        return true;
    }();
    (void)registered;
}
}
// Inherited object.d defaults, Digital Mars 2000-2011, BSL-1.0;
// retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString FlipPairWindow::toString() {
    const auto receiver = weak_from_this().lock();
    prepareFlipWindowClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t FlipPairWindow::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
int FlipPairWindow::opCmp(std::shared_ptr<FlipPairWindow>) {
    const auto receiver = weak_from_this().lock();
    prepareFlipWindowClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = [&] {
        try {
            std::string text = "need opCmp for class ";
            text += className.view();
            return compat::RetainedString(std::move(text));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>();
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>();
        }
    }();
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngFlipPairWindowObjectEquals(std::shared_ptr<FlipPairWindow> left, std::shared_ptr<FlipPairWindow> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right),
        &prepareFlipWindowClassInfo);
}
namespace detail { compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> ngFlipNodes(
    std::shared_ptr<nicxlive::core::Puppet>, std::shared_ptr<nicxlive::core::nodes::Node>); }
using namespace widgets;
namespace {
using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
template<class F> auto allocateFlipWindowText(F&& operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) { throw compat::ngStaticError<compat::OutOfMemoryError>(); }
    catch (const std::length_error&) { throw compat::ngStaticError<compat::OutOfMemoryError>(); }
}
std::uint32_t nodeUuid(const NodeRef& node) {
    const auto receiver = node;
    return compat::ngRequireObject(receiver)->getUUID();
}
bool sameUuid(const NodeRef& left, const NodeRef& right) {
    const auto receiver = left;
    const auto leftUuid = compat::ngRequireObject(receiver)->getUUID();
    const auto rightUuid = nodeUuid(right);
    return leftUuid == rightUuid;
}
std::string iconName(const std::shared_ptr<nicxlive::core::nodes::Node>& node) {
    const auto icon = [&] {
        const auto receiver = node;
        const auto type = compat::ngRequireObject(receiver)->typeId();
        const auto text = utils::incTypeIdToIcon(type);
        return allocateFlipWindowText([&] { return std::string(text); });
    }();
    const auto name = [&] {
        const auto receiver = node;
        return compat::ngRequireObject(receiver)->getName();
    }();
    return allocateFlipWindowText([&] { return icon + name; });
}
std::string iconName(const std::shared_ptr<FlipPair>& pair, std::size_t side) {
    const auto icon = [&] {
        const auto owner = pair;
        const auto receiver = compat::ngArrayAt(compat::ngRequireObject(owner)->parts, side);
        const auto type = compat::ngRequireObject(receiver)->typeId();
        const auto text = utils::incTypeIdToIcon(type);
        return allocateFlipWindowText([&] { return std::string(text); });
    }();
    const auto name = [&] {
        const auto owner = pair;
        const auto receiver = compat::ngArrayAt(compat::ngRequireObject(owner)->parts, side);
        return compat::ngRequireObject(receiver)->getName();
    }();
    return allocateFlipWindowText([&] { return icon + name; });
}
std::shared_ptr<nicxlive::core::nodes::Node> payloadNode(const ImGuiPayload* payload) {
    nicxlive::core::nodes::Node* node;
    std::memcpy(&node,compat::ngRequireObject(payload->Data),sizeof(node));
    return node ? node->shared_from_this() : NodeRef{};
}
}
void FlipPairWindow::initialize() {
    const auto receiver = weak_from_this().lock();
    auto puppet = project::incActivePuppet();
    nodes = detail::ngFlipNodes(puppet, compat::ngRequireObject(puppet)->root);
    pairs = ngGetFlipPairs().duplicate();
    std::size_t destination = 0;
    for (std::size_t i = 0; i < pairs.size(); ++i)
        if (compat::ngRequireObject(pairs[i])->parts[0] || pairs[i]->parts[1])
            pairs[destination++] = pairs[i];
    pairs.resize(destination);
    const auto indexedPairs = pairs;
    for (std::size_t i = 0; i < indexedPairs.size(); ++i) {
        const auto pair = indexedPairs[i];
        for (auto part : compat::ngRequireObject(pair)->parts) if (part) map[nodeUuid(part)] = i;
    }
    Window::initialize(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Flip Pairing")));
    flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
}
void FlipPairWindow::apply() {
    const auto receiver = weak_from_this().lock();
    ngReplaceFlipPairs(pairs);
}
void FlipPairWindow::autoPair(compat::RetainedStringSlice part1, compat::RetainedStringSlice part2) {
    const auto receiver = weak_from_this().lock();
    auto puppet = project::incActivePuppet();
    ngReplaceFlipPairs(pairs);
    ngAutoAddFlipPairs(puppet, part1, part2);
    pairs = ngGetFlipPairs().duplicate(); map.clear();
    const auto indexedPairs = pairs;
    for (std::size_t i = 0; i < indexedPairs.size(); ++i) {
        const auto pair = indexedPairs[i];
        for (auto part : compat::ngRequireObject(pair)->parts) if (part) map[nodeUuid(part)] = i;
    }
}
nicxlive::core::math::Vec4 FlipPairWindow::previewImage(std::shared_ptr<Part> part, ImVec2 center,
        float previewSize, ImVec2 uv0, ImVec2 uv1, ImVec4 tint) {
    const auto owner = weak_from_this().lock();
    const auto texture = [&](std::size_t sourceLine) {
        const auto receiver = compat::ngRequireObject(part);
        const auto textures = receiver->textures;
        if (textures.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, textures.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/flipconfig.d"), sourceLine);
        const auto current = textures[0];
        (void)compat::ngRequireObject(current);
        return current;
    };
    if (texture(319)->width() < previewSize && texture(319)->height() < previewSize) {
        const auto width = texture(320)->width();
        const auto height = texture(320)->height();
        previewSize = static_cast<float>(std::max(width, height));
    }
    const float widthScale = previewSize / static_cast<float>(texture(322)->width());
    const float heightScale = previewSize / static_cast<float>(texture(323)->height());
    // Phobos min selects its first operand on ties and unordered comparisons.
    const float scale = heightScale < widthScale ? heightScale : widthScale;
    const auto boundsWidth = texture(326)->width() * scale;
    const auto boundsHeight = texture(326)->height() * scale;
    nicxlive::core::math::Vec4 bounds{0, 0, boundsWidth, boundsHeight};
    if (widthScale > heightScale) bounds.x = (previewSize - bounds.z) / 2;
    else if (widthScale < heightScale) bounds.y = (previewSize - bounds.w) / 2;
    auto tl = ImGui::GetCursorPos();
    ImGui::ItemSize(ImVec2(PreviewSize, PreviewSize));
    ImGui::SetCursorPos({tl.x + center.x - previewSize / 2 + bounds.x,
        tl.y + center.y - previewSize / 2 + bounds.y});
    ImGui::Image(static_cast<ImTextureID>(texture(340)->getTextureId()), {bounds.z, bounds.w}, uv0, uv1, tint);
    return bounds;
}
void FlipPairWindow::treeView() {
    const auto receiver = weak_from_this().lock();
    const auto retained = nodes;
    for (std::size_t i = 0; i < retained.size(); ++i) {
        const auto& node = retained[i];
        if (!nodeFilter.empty()) {
            const auto name = [&] {
                const auto receiver = node;
                return ext::nodes::ngRetainNodeName(compat::ngRequireObject(receiver)->getName());
            }();
            const auto loweredName = io::imageexport::detail::lowerExtension(name);
            const auto loweredFilter = io::imageexport::detail::lowerExtension(nodeFilter);
            if (loweredName.view().find(loweredFilter.view()) == std::string_view::npos) continue;
        }
        if (map.contains(nodeUuid(node))) continue;
        ImGui::PushID(static_cast<int>(i));
        const auto labelText = iconName(node);
        const compat::SourceCString label{std::string_view(labelText)};
        compat::detail::ngSourceCStringPostconditions(labelText, label.data());
        ImGui::Selectable(label.data(), false, ImGuiSelectableFlags_SpanAvailWidth);
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            auto* raw = node.get();
            ImGui::SetDragDropPayload("__PAIRING", &raw, sizeof(raw), ImGuiCond_Always);
            ngText(iconName(node)); ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip(); ngText(iconName(node));
            if (auto part = std::dynamic_pointer_cast<Part>(node))
                previewImage(part, {PreviewSize / 2, PreviewSize / 2}, PreviewSize);
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
}
void FlipPairWindow::assignPair(std::shared_ptr<FlipPair>& pair, std::shared_ptr<Node> node,
        std::size_t index, int side) {
    const auto owner = weak_from_this().lock();
    // The original foreach(pair2; pairs) captures a value slot, not the array slot.
    std::shared_ptr<FlipPair> target;
    const auto candidates = pairs;
    for (auto other : candidates) if (sameUuid(compat::ngRequireObject(other)->parts[0], node) ||
            (other->parts[1] && sameUuid(other->parts[1], node))) { target = other; break; }
    if (target) {
        if (sameUuid(target->parts[0], node)) target->parts[0].reset();
        else target->parts[1].reset();
    }
    compat::ngArrayAt(compat::ngRequireObject(pair)->parts, static_cast<std::size_t>(side)) = node;
    map[nodeUuid(node)] = index;
    const auto receiver = pair;
    compat::ngRequireObject(receiver)->update();
}
void FlipPairWindow::pairView() {
    const auto receiver = weak_from_this().lock();
    [[maybe_unused]] ImGuiStyle* style = &ImGui::GetStyle();
    int deleted = -1;
    auto retained = pairs;
    for (std::size_t i = 0; i < retained.size(); ++i) {
        auto& pair = retained[i];
        if (!compat::ngRequireObject(pair)->parts[0] && !pair->parts[1]) continue;
        if (!pair->parts[0]) { pair->parts[0] = pair->parts[1]; pair->parts[1].reset(); }
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.5f, .5f, .5f, 0));
        const bool selected = hasActive && &activeOwner[activeIndex] == &pair;
        const auto rowId = [&] {
            const auto owner = pair;
            const auto receiver = compat::ngArrayAt(compat::ngRequireObject(owner)->parts, 0);
            const auto name = compat::ngRequireObject(receiver)->cName();
            if (!name) return allocateFlipWindowText([] { return std::string("##null"); });
            // Phobos %s formats a pointer as an uppercase hexadecimal address.
            const std::array<core::translation::TypedFormatArgument, 1> arguments{
                core::translation::ngFormatArgument(reinterpret_cast<std::uintptr_t>(name))};
            return core::translation::ngFormatTyped("##%X", arguments);
        }();
        const compat::SourceCString rowLabel{std::string_view(rowId)};
        compat::detail::ngSourceCStringPostconditions(rowId, rowLabel.data());
        ImGui::Selectable(rowLabel.data(), selected,
            ImGuiSelectableFlags_SpanAllColumns, {0, 16});
        if (ImGui::IsItemClicked()) { activeOwner = retained; activeIndex = i; hasActive = true; }
        ImGui::SetItemAllowOverlap(); ImGui::PopStyleColor(); ImGui::SameLine();
        ngText(iconName(pair, 0));
        for (int side = 0; side < 2; ++side) {
            if (side == 1) {
                ImGui::TableNextColumn();
                if (compat::ngRequireObject(pair)->parts[1]) {
                    ngText(iconName(pair, 1));
                } else {
                    const auto text = core::translation::ngTranslateRetained(
                        compat::RetainedString::fromStaticLiteral("<< Not assigned >>"));
                    ngText(compat::RetainedStringSlice(text));
                }
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto payload = ImGui::AcceptDragDropPayload("__PAIRING")) {
                    assignPair(pair, payloadNode(payload), i, side);
                    ImGui::EndDragDropTarget(); ImGui::PopID(); return;
                }
                ImGui::EndDragDropTarget();
            }
        }
        ImGui::SameLine(0, 0); ngDummy({-14, 12}); ImGui::SameLine(0, 0);
        ImGui::PushID(static_cast<int>(i + pairs.size()));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::SetWindowFontScale(.55f);
        if (ngButtonColored("\xee\x97\x8d", {16, 16})) deleted = static_cast<int>(i);
        ImGui::SetWindowFontScale(1); ImGui::PopStyleVar(2); ImGui::PopID(); ImGui::PopID();
    }
    if (deleted >= 0) {
        auto pair = pairs[static_cast<std::size_t>(deleted)];
        for (auto part : compat::ngRequireObject(pair)->parts) if (part) map.erase(nodeUuid(part));
        for (auto i = static_cast<std::size_t>(deleted); i + 1 < pairs.size(); ++i) pairs[i] = pairs[i + 1];
        pairs.resize(pairs.size() - 1);
    }
    ImGui::TableNextRow(); ImGui::TableNextColumn();
    ImGui::Text(core::translation::ngTranslateCString("< Add new row >").data());
}
void FlipPairWindow::onBeginUpdate() {
    const auto receiver = weak_from_this().lock();
    flags |= ImGuiWindowFlags_NoSavedSettings;
    const auto x = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const auto width = compat::ngRequireObject(ImGui::GetMainViewport())->Size.x;
    const auto y = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const auto height = compat::ngRequireObject(ImGui::GetMainViewport())->Size.y;
    const ImVec2 position{x + width / 2, y + height / 2};
    ImGui::SetNextWindowPos(position, ImGuiCond_Appearing, {.5f, .5f});
    ImGui::SetNextWindowSize({800, 600}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({800, 600}, {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()});
    Window::onBeginUpdate();
}
void FlipPairWindow::onUpdate() {
    const auto receiver = weak_from_this().lock();
    const auto space = ngAvailableSpace();
    const float gap = 8, width = space.x / 2;
    const float a = space.x / 3, b = space.y / 3, preview = b < a ? b : a;
    const float height = std::floor(space.y - 28 - (preview + gap + 6));
    ImGui::BeginGroup();
    auto tl = ImGui::GetCursorPos(); ImGui::SetCursorPos({tl.x + (width - preview / 2), tl.y});
    if (ImGui::BeginChild("###Preview", {preview, preview}, ImGuiChildFlags_Borders,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        const float quarter = preview / 4;
        if (hasActive) {
            const auto activePair = [&] {
                return compat::ngRequireObject(compat::ngArrayAt(activeOwner, activeIndex));
            };
            tl = ImGui::GetCursorPos();
            if (auto part = std::dynamic_pointer_cast<Part>(activePair()->parts[0]))
                previewImage(part, {quarter, quarter * 2}, preview, {0, 0}, {1, 1}, {1, 1, 1, .6f});
            ImGui::SetCursorPos(tl);
            if (auto part = std::dynamic_pointer_cast<Part>(activePair()->parts[1]))
                previewImage(part, {quarter * 3, quarter * 2}, preview, {1, 0}, {0, 1}, {1, 1, 1, .6f});
        }
    }
    ImGui::EndChild(); ImGui::Dummy({0, gap});
    if (ImGui::BeginChild("###Nodes", {width, height})) {
        ngInputText("##", width, nodeFilter);
        ImGui::BeginListBox("###NodeList", {width, height - 26}); treeView(); ImGui::EndListBox();
    }
    ImGui::EndChild(); ImGui::SameLine(0, gap);
    if (ImGui::BeginChild("###Pairs", {width, height})) {
        ngInputText("##part1", (width - 50) / 2, part1Pattern); ImGui::SameLine(0, 0);
        if (ngButtonColored(core::translation::ngTranslateCString("Pair").data(), {48, 0}))
            autoPair(part1Pattern, part2Pattern);
        ImGui::SameLine(0, 0); ngInputText("##part2", (width - 50) / 2, part2Pattern);
        if (ImGui::BeginChild("###PairList", {width, height - 26})) {
            ImGui::BeginTable("###PairsTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders, {width - gap, height - 26});
            ImGui::TableHeader("###PairsTableHeader");
            ImGui::TableSetupColumn(core::translation::ngTranslateCString("Part 1").data(),
                ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(core::translation::ngTranslateCString("Part 2").data(),
                ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow(); pairView(); ImGui::EndTable();
        }
        ImGui::EndChild();
        if (ImGui::BeginDragDropTarget()) {
            if (auto payload = ImGui::AcceptDragDropPayload("__PAIRING")) {
                auto node = payloadNode(payload); bool found = false;
                const auto candidates = pairs;
                for (auto pair : candidates) if (sameUuid(compat::ngRequireObject(pair)->parts[0], node) ||
                        (pair->parts[1] && sameUuid(pair->parts[1], node))) { found = true; break; }
                if (!found) {
                    pairs.append(FlipPair::create(
                        std::array<std::shared_ptr<Node>, 2>{node, nullptr}, compat::RetainedString{}));
                    map[nodeUuid(node)] = pairs.size() - 1;
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::EndChild(); ImGui::EndGroup(); ImGui::BeginGroup();
    ngDummy({-192, 0}); ImGui::SameLine(0, 0);
    if (ngButtonColored(core::translation::ngTranslateCString("Cancel").data(), {96, 24})) {
        close(); ImGui::EndGroup(); return;
    }
    ImGui::SameLine(0, 0);
    if (ngButtonColored(core::translation::ngTranslateCString("Save").data(), {96, 24})) {
        apply(); close(); ImGui::EndGroup(); return;
    }
    ImGui::EndGroup();
}
void FlipPairWindow::onClose() {
    const auto receiver = weak_from_this().lock();
    // The source hook only requests D garbage collection. C++ members retain
    // their normal RAII lifetime; closing a window does not run a collector.
}
}
