#pragma once
#include "output.hpp"
#include "output_internal.hpp"
#include <nijigenerate/core/selector/treestore.hpp>
#include <nijigenerate/backend/runtime_snapshot.hpp>
#include <nijigenerate/panels/parameters.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/input_events.hpp>
#include <limits>

namespace nijigenerate::widgets::output {
using Resource = core::selector::ResourcePtr;
using Node = std::shared_ptr<nicxlive::core::nodes::Node>;
using Parameter = std::shared_ptr<nicxlive::core::param::Parameter>;
class ListOutput : public Output {
protected:
    std::shared_ptr<core::selector::TreeStore> self;
    CommandIssuerPtr panel;
    int IconSize = 20;
    Resource popupOpened, focused;
    std::shared_ptr<detail::NodeInspector> activeInspector;
    detail::UuidMap<bool> contentsDrawn;
    panels::parameters::ParamDragDropData* dragDropData=nullptr;
    virtual core::selector::SelectorArray<Resource>& nodes() { return compat::ngRequireObject(self)->nodes; }
    virtual decltype(core::selector::TreeStore::children)& children() {
        return compat::ngRequireObject(self)->children;
    }
    virtual core::selector::SelectorArray<Resource>& roots() { return compat::ngRequireObject(self)->roots; }
    virtual decltype(core::selector::TreeStore::nodeIncluded)& nodeIncluded() {
        return compat::ngRequireObject(self)->nodeIncluded;
    }
    virtual void setNodeReorderDragTarget(Resource,bool horizontal=true,float width=-1);
    virtual void setNodeDragSource(Node);
    virtual void setNodeReparentDragTarget(Node);
    virtual void setParamDragSource(Parameter);
    virtual void setParamDragTarget(std::shared_ptr<ext::ExParameterGroup>);
    virtual void showContents(Resource);
    virtual ImGuiTreeNodeFlags setFlag(Resource);
    virtual void drawTreeItem(Resource);
private:
    std::shared_ptr<panels::parameters::ParamDragDropData> dragDropOwner_;
public:
    ListOutput(std::shared_ptr<core::selector::TreeStore> store,CommandIssuerPtr issuer);
    void onUpdate() override;
    virtual void setResources(core::selector::SelectorArray<Resource> resources);
    void reset() override;
};
class IconTreeOutput : public ListOutput {
public:
    struct SubItemLayout {
        ImRect bounds{ImVec2(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()),
            ImVec2(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN())};
        bool nextInHorizontal=false,scrolledOut=false,folded=false,buttonPressed=false;
    };
protected:
    compat::RetainedString grabParam;
    detail::UuidMap<SubItemLayout> layout_;
    bool prevMouseDown=false,mouseDown=false,showRootThumb=false;
    std::shared_ptr<nicxlive::core::Texture> rootThumbTex;
    std::shared_ptr<backend::snapshot::Snapshot> snapshot;
    ImGuiTreeNodeFlags setFlag(Resource) override;
    template<class R> bool isNodeClicked(R resource,ImGuiMouseButton button=ImGuiMouseButton_Left) {
        [[maybe_unused]] const auto receiver = weak_from_this().lock();
        const auto uuid=[&] {
            auto value=compat::ngRequireObject(resource);
            if constexpr(requires { value->uuid(); }) return value->uuid();
            else return value->getUUID();
        };
        if(ImGui::IsItemHovered() && ImGui::IsMouseDown(button)) {
            layout().try_emplace(uuid());layout().at(uuid()).buttonPressed=true;return false;
        }
        if(layout().contains(uuid()) && layout().at(uuid()).buttonPressed && ImGui::IsItemHovered() &&
            core::input::ngInputIsMouseReleased(button)) {
            layout().try_emplace(uuid());layout().at(uuid()).buttonPressed=false;return true;
        }
        if(layout().contains(uuid()) && layout().at(uuid()).buttonPressed && !ImGui::IsItemHovered() &&
            core::input::ngInputIsMouseReleased(button)) {
            layout().try_emplace(uuid());layout().at(uuid()).buttonPressed=false;return false;
        }
        return false;
    }
    virtual void drawNode(Resource,Node,ImVec2&,ImVec2&,bool&);
    virtual bool drawParameter(Resource,ImVec2&,ImVec2&,bool&);
    virtual void drawBinding(Resource,ImVec2&,ImVec2&,bool&);
    void drawTreeItem(Resource) override;
public:
    IconTreeOutput(std::shared_ptr<core::selector::TreeStore> store,CommandIssuerPtr issuer);
    void onUpdate() override;
    void setResources(core::selector::SelectorArray<Resource> resources) override;
    virtual detail::UuidMap<SubItemLayout>& layout() { return layout_; }
    void reset() override;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::output::ListOutput> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.output.ListOutput)";
    inline static constexpr char unqualifiedArgument[] = "(ListOutput)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.output.ListOutput";
    inline static constexpr char unqualifiedElement[] = "ListOutput";
};
template<> struct SourceObjectTypeNames<::nijigenerate::widgets::output::ListOutput> {
    inline static constexpr char className[] = "nijigenerate.widgets.output.ListOutput";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.widgets.output.ListOutput)";
};
template<> struct SourceTypeSpellings<::nijigenerate::widgets::output::IconTreeOutput> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.output.IconTreeOutput)";
    inline static constexpr char unqualifiedArgument[] = "(IconTreeOutput)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.output.IconTreeOutput";
    inline static constexpr char unqualifiedElement[] = "IconTreeOutput";
};
template<> struct SourceObjectTypeNames<::nijigenerate::widgets::output::IconTreeOutput> {
    inline static constexpr char className[] = "nijigenerate.widgets.output.IconTreeOutput";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.widgets.output.IconTreeOutput)";
};
template<> struct SourceTypeSpellings<::nijigenerate::widgets::output::IconTreeOutput::SubItemLayout> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.widgets.output.IconTreeOutput.SubItemLayout)";
    inline static constexpr char unqualifiedArgument[] = "(SubItemLayout)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.output.IconTreeOutput.SubItemLayout";
    inline static constexpr char unqualifiedElement[] = "SubItemLayout";
};
}
