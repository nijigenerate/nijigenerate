#pragma once

#include <nijigenerate/panels/package.hpp>
#include <nijigenerate/panels/nodes_icon.hpp>
#include <nijigenerate/panels/nodes_menu.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>
#include <common/resource_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace nicxlive::core::nodes { class Node; }

namespace nijigenerate::panels::nodes {

enum class SelectState : std::int32_t {
    Init = 0,
    Started = 1,
    Ended = 2,
};

struct SelectStateData {
    SelectState state{};
    std::shared_ptr<nicxlive::core::nodes::Node> lastClick;
    std::shared_ptr<nicxlive::core::nodes::Node> shiftSelect;
    bool hasRenderedLastClick{};
    bool hasRenderedShiftSelect{};
};

class NodesPanel : public ::nijigenerate::panels::Panel,
    public std::enable_shared_from_this<NodesPanel> {
    using NodeHandle = std::shared_ptr<nicxlive::core::nodes::Node>;
    struct FilterUuidHash {
        std::size_t operator()(std::uint32_t value) const noexcept { return static_cast<std::size_t>(value); }
    };
    using FilterResults = nicxlive::core::common::ResourceKeyMap<std::uint32_t, bool,
        FilterUuidHash, std::equal_to<std::uint32_t>>;

public:
    NodesPanel() : Panel(compat::RetainedString::fromStaticLiteral("Nodes"),
            core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Nodes")), true) {
        supportsReadOnlyInteraction = true;
        flags |= ImGuiWindowFlags_NoScrollbar;
        activeModes = project::EditMode::ModelEdit;
    }
    ~NodesPanel() override = default;
    // D class values are handles; copying object storage is not a handle copy.
    NodesPanel(const NodesPanel&) = delete;
    NodesPanel& operator=(const NodesPanel&) = delete;

private:
    compat::RetainedString filter;
    FilterResults filterResult;
    SelectStateData nextSelectState;
    SelectStateData curSelectState;
    compat::Slice<NodeHandle> rangeSelectNodes;
    bool selectStateUpdate = false;
    bool revserseOrder = false;
    bool pendingFocus = false;
    std::uint32_t lastSelUuid = 0;

protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(NodesPanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.nodes.NodesPanel"));
            return true;
        }();
        (void)registered;
    }
    virtual void trackingRenderedNode(NodeHandle& node) {
        if (nextSelectState.lastClick == node) nextSelectState.hasRenderedLastClick = true;
        if (nextSelectState.shiftSelect == node) nextSelectState.hasRenderedShiftSelect = true;
    }
    virtual void startTrackingRenderedNodes() {
        nextSelectState.hasRenderedLastClick = false;
        nextSelectState.hasRenderedShiftSelect = false;
    }
    virtual void endTrackingRenderedNodes() {
        if (!nextSelectState.hasRenderedLastClick && nextSelectState.lastClick) {
            nextSelectState.lastClick.reset();
            nextSelectState.shiftSelect.reset();
            selectStateUpdate = true;
        }
        if (!nextSelectState.hasRenderedShiftSelect && nextSelectState.shiftSelect) {
            nextSelectState.shiftSelect.reset();
            selectStateUpdate = true;
        }
    }
    virtual void treeSetEnabled(NodeHandle node, bool enabled);
    virtual void toggleSelect(NodeHandle& node) {
        const auto keepAlive = weak_from_this().lock();
        if (project::incNodeInSelection(node)) project::incRemoveSelectNode(node);
        else project::incAddSelectNode(node);
        rangeSelectNodes = {};
    }
    virtual void selectRange(NodeHandle& node) {
        const auto keepAlive = weak_from_this().lock();
        if (!curSelectState.lastClick) {
            nextSelectState.lastClick = node;
            project::incSelectNode(node);
            return;
        }
        // D foreach keeps the original slice descriptor across signal callbacks.
        const auto previousRange = rangeSelectNodes;
        for (auto selected : previousRange) project::incRemoveSelectNode(selected);
        rangeSelectNodes = {};
        nextSelectState.shiftSelect = node;
    }
    virtual void handleRangeSelect(NodeHandle& node) {
        const auto keepAlive = weak_from_this().lock();
        if (curSelectState.state == SelectState::Ended || !curSelectState.lastClick ||
                !curSelectState.shiftSelect) return;
        if (nicxlive::core::ngResourceObjectEquals(node, curSelectState.lastClick) ||
                nicxlive::core::ngResourceObjectEquals(node, curSelectState.shiftSelect)) {
            switch (curSelectState.state) {
            case SelectState::Init:
                curSelectState.state = SelectState::Started;
                break;
            case SelectState::Started:
                curSelectState.state = SelectState::Ended;
                nextSelectState.shiftSelect.reset();
                break;
            default:
                break;
            }
        }
        if (curSelectState.state != SelectState::Init && !project::incNodeInSelection(node)) {
            project::incAddSelectNode(node);
        }
        if (curSelectState.state != SelectState::Init &&
                !nicxlive::core::ngResourceObjectEquals(node, curSelectState.lastClick)) {
            rangeSelectNodes.append(node);
        }
    }
    template<bool isRoot = false> void treeAddNode(NodeHandle& node);
    virtual bool filterNodes(NodeHandle node);
    void onUpdate() override;
};

} // namespace nijigenerate::panels::nodes

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::panels::nodes::SelectState> {
    using Enum = ::nijigenerate::panels::nodes::SelectState;
    inline static constexpr std::string_view name = "SelectState";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Init, "Init"},
        FormatEnumMember<Enum>{Enum::Started, "Started"},
        FormatEnumMember<Enum>{Enum::Ended, "Ended"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::nodes::SelectState> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.nodes.SelectState)";
    inline static constexpr char unqualifiedArgument[] = "(SelectState)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.nodes.SelectState";
    inline static constexpr char unqualifiedElement[] = "SelectState";
};
template<> struct SourceTypeSpellings<::nijigenerate::panels::nodes::SelectStateData> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.nodes.SelectStateData)";
    inline static constexpr char unqualifiedArgument[] = "(SelectStateData)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.nodes.SelectStateData";
    inline static constexpr char unqualifiedElement[] = "SelectStateData";
};
template<> struct SourceTypeSpellings<::nijigenerate::panels::nodes::NodesPanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.nodes.NodesPanel)";
    inline static constexpr char unqualifiedArgument[] = "(NodesPanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.nodes.NodesPanel";
    inline static constexpr char unqualifiedElement[] = "NodesPanel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::nodes::NodesPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.nodes.NodesPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.nodes.NodesPanel)";
};
}
