#pragma once

#include "common.hpp"
#include <nodes/grid_deformer.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <common/string_key_map.hpp>
#include <concepts>

namespace nijigenerate::commands { class Context; class CommandResult; }
namespace nijigenerate::panels::inspector::griddeform {
template<ModelEditSubMode inspectorMode, class T>
requires std::derived_from<T, nicxlive::core::nodes::GridDeformer>
class NodeInspector;
}

namespace nicxlive::core::d_runtime {
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires std::derived_from<T, nodes::GridDeformer>
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::griddeform::NodeInspector<Mode, T>> {
    static RetainedString className() { return declaration(true, true, true); }
    static RetainedString nestedQualifiedScope() { return declaration(true, true, false); }
    static RetainedString nestedUnqualifiedScope() { return declaration(false, true, false); }
    static RetainedString element(bool qualified) { return declaration(qualified, false, false); }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::panels::inspector::detail::inspectorClassArgument(element(true));
    }
private:
    static RetainedString declaration(bool qualified, bool classLeaf, bool classInfo) {
        const auto argument = ngSourceTypeSpelling<T>(classInfo ? SourceTypeSpelling::QualifiedElement :
            SourceTypeSpelling::UnqualifiedElement);
        return ::nijigenerate::panels::inspector::detail::inspectorNameBytes([&] {
            std::string name = qualified ? "nijigenerate.panels.inspector.griddeform." : "";
            name += "NodeInspector!(ModelEditSubMode.";
            name += Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout ? "Layout, " : "Deform, ";
            name.append(argument.view());
            name += ')';
            if (classLeaf) name += ".NodeInspector";
            return name;
        });
    }
};
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires std::derived_from<T, nodes::GridDeformer>
struct SourceTypeSpellings<::nijigenerate::panels::inspector::griddeform::NodeInspector<Mode, T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::griddeform::NodeInspector<Mode, T>>;
        const auto qualified = Names::element(true);
        const auto unqualified = Names::element(false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};
}

namespace nijigenerate::panels::inspector::griddeform {
using GridDeformer = nicxlive::core::nodes::GridDeformer;
using SessionMap = nicxlive::core::common::PolicyKeyMap<compat::RetainedString,std::uint64_t,
    nicxlive::core::common::StringKeyPolicy>;
void ngDrawGridDepthBoneSources(std::shared_ptr<Node>,std::shared_ptr<Parameter>,Cursor,SessionMap&,
    ext::nodes::ExDepthTargetKind kind=ext::nodes::ExDepthTargetKind::Grid);
void ngRunGridDynamicCommand(std::shared_ptr<Inspector>,Selection<Node>,bool);
std::shared_ptr<commands::CommandResult> ngDispatchGridDynamicCommand(std::shared_ptr<commands::Context>,bool&);
template<class T> struct DynamicDescriptor {
    using Target=T; using Value=bool; static constexpr PropertyId id{"dynamic"};
    template<class Owner> static bool read(Owner&,const std::shared_ptr<T>& node) {
        return compat::ngRequireObject(node)->dynamic;
    }
    template<class Owner> static void write(Owner&,const std::shared_ptr<T>& node,bool value) {
        compat::ngRequireObject(node)->switchDynamic(value);
    }
};
template<ModelEditSubMode inspectorMode, class T>
requires std::derived_from<T, nicxlive::core::nodes::GridDeformer>
class NodeInspector : public BaseInspector<inspectorMode,T>, public MultiEdit<NodeInspector<inspectorMode,T>> {
    using Self=NodeInspector; using Base=BaseInspector<inspectorMode,T>;
    inline static thread_local SessionMap sessions;
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        ::nijigenerate::panels::inspector::detail::prepareInspectorClassRecord<Self>();
    }
public:
    InspectorProperty<Self,DynamicDescriptor<T>> dynamic;
    using Base::Base;
    void capture(Selection<Node> nodes) override {
        auto self=std::static_pointer_cast<Self>(this->shared_from_this());
        Base::capture(std::move(nodes)); dynamic.get(self)->capture();
    }
    void run();
    void run(std::shared_ptr<Parameter> parameter,Cursor cursor) {
        if constexpr(inspectorMode==ModelEditSubMode::Deform) {
            if(this->targets.empty()) return;
            auto self=this->shared_from_this();
            ngDrawGridDepthBoneSources(this->targets[0],std::move(parameter),cursor,sessions);
        }
    }
};
} // namespace nijigenerate::panels::inspector::griddeform
#include "griddeform_run.hpp"
