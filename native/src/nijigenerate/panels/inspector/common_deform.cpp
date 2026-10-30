#include <nijigenerate/panels/inspector/common.hpp>
#include <nijigenerate/actions/binding_add_remove_action.hpp>
#include <nijigenerate/actions/binding_value_action.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/viewport/common/mesheditor/package.hpp>
#include <bit>
#include <tuple>
namespace nijigenerate::panels::inspector {
namespace {
using Binding=nicxlive::core::param::ValueParameterBinding;
using Bindings=compat::Slice<std::shared_ptr<Binding>>;
void adjustPath(const std::shared_ptr<Node>& node,const std::shared_ptr<viewport::common::mesheditor::IncMeshEditor>& editor) {
    if(auto selected=editor->getEditorFor(node)) selected->adjustPathTransform();
}
}
void incInspectorDeformSetValue(const std::shared_ptr<Node>& node,const std::shared_ptr<Parameter>& parameter,
    compat::RetainedString name,Cursor cursor,float value) {
    using namespace actions::binding;
    const auto target = node;
    const auto param = parameter;
    std::shared_ptr<actions::GroupAction> group;
    auto binding=std::dynamic_pointer_cast<Binding>(
        compat::ngRequireObject(param)->getBinding(target,name.toNullableString()));
    if(!binding) {
        binding=std::dynamic_pointer_cast<Binding>(
            compat::ngRequireObject(param)->createBinding(target,name.toNullableString()));
        compat::ngRequireObject(param)->addBinding(binding);
        group=std::allocate_shared<actions::GroupAction>(
            nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});
        group->addAction(ngCreateBindingAddRemoveAction<ParameterBindingAddAction>(param,binding));
    }
    auto action=ngCreateBindingValueActionDeferred<ParameterBindingValueChangeAction<float,Binding>>([&] {
        const auto label = compat::RetainedStringSlice(compat::RetainedString(
            compat::ngRequireObject(binding)->getNameDescriptor()));
        return std::tuple{label, binding,
            std::bit_cast<std::int32_t>(cursor.x), std::bit_cast<std::int32_t>(cursor.y)};
    });
    compat::ngRequireObject(binding)->setValue({cursor.x,cursor.y},value); action->updateNewState();
    if(group) {group->addAction(action);core::actionstack::incActionPush(group);}
    else core::actionstack::incActionPush(action);
    if(auto editor=viewport::model::deform::ngViewportModelDeformGetEditor()) adjustPath(target,editor);
}
void incInspectorDeformSetValue(Selection<Node> nodes,const std::shared_ptr<Parameter>& parameter,
    compat::RetainedString name,Cursor cursor,float value) {
    using namespace actions::binding;
    const auto param = parameter;
    std::shared_ptr<actions::GroupAction> group;
    Bindings bindings;
    if (!nodes.empty()) bindings.resize(nodes.size());
    for(std::size_t i=0;i<nodes.size();++i) {
        const auto node=nodes[i];
        auto binding=std::dynamic_pointer_cast<Binding>(
            compat::ngRequireObject(param)->getBinding(node,name.toNullableString()));
        if(!binding) {
            binding=std::dynamic_pointer_cast<Binding>(
                compat::ngRequireObject(param)->createBinding(node,name.toNullableString()));
            compat::ngRequireObject(param)->addBinding(binding);
            // The original replaces the group for each newly-created binding.
            group=std::allocate_shared<actions::GroupAction>(
                nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});
            group->addAction(ngCreateBindingAddRemoveAction<ParameterBindingAddAction>(param,binding));
        }
        bindings[i]=binding;
    }
    auto action=ngCreateBindingValueActionDeferred<ParameterBindingValueChangeAction<float,Bindings>>([&] {
        if (bindings.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, bindings.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/inspector/common.d"), 329);
        const auto label = compat::RetainedStringSlice(compat::RetainedString(
            compat::ngRequireObject(bindings[0])->getNameDescriptor()));
        return std::tuple{label, bindings,
            std::bit_cast<std::int32_t>(cursor.x), std::bit_cast<std::int32_t>(cursor.y)};
    });
    for(auto binding:bindings) compat::ngRequireObject(binding)->setValue({cursor.x,cursor.y},value);
    action->updateNewState();
    if(group) {group->addAction(action);core::actionstack::incActionPush(group);}
    else core::actionstack::incActionPush(action);
    if(auto editor=viewport::model::deform::ngViewportModelDeformGetEditor())
        for(auto node:nodes) adjustPath(node,editor);
}
}
