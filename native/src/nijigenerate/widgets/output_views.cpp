#include "output.hpp"
#include "output_internal.hpp"
#include <nijigenerate/core/selector/query.hpp>
#include <nijigenerate/panels/inspector/base.hpp>
#include <nijigenerate/panels/parameters.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::widgets::output {
using namespace project;
using namespace core::selector;
namespace detail {
void initInspectors() {
    if(!nodeInspector) nodeInspector=panels::inspector::ngNodeInspector({});
}
void onNodeView(std::shared_ptr<Node> node,std::shared_ptr<NodeInspector> inspector) {
    initInspectors();
    if(!inspector) { inspector=nodeInspector;compat::ngRequireObject(nodeInspector)->capture({node}); }
    const auto mode=ngModelEditSubMode();
    if(node) {
        if(incEditMode()==EditMode::ModelEdit) {
            auto parameter=incArmedParameter();
            compat::Vec2u cursor{};
            if(parameter) { const auto point=parameter->findClosestKeypoint();cursor={point.x,point.y}; }
            compat::ngRequireObject(inspector)->subMode(mode);inspector->inspect(parameter,cursor);
        } else panels::inspector::incCommonNonEditHeader(node);
    }
}
void onParameterView(std::uint64_t index,std::shared_ptr<nicxlive::core::param::Parameter> parameter) {
    // The authored int cast precedes the parameter view call.
    const auto narrowed=static_cast<std::int32_t>(index);
    auto puppet=incActivePuppet();
    // The original evaluates this property even though the forwarded array is unused.
    if(!puppet)compat::ngFailNullAccess(false);
    panels::parameters::incParameterView<false,false,true>(static_cast<std::size_t>(narrowed),parameter,
        &parameterGrabStr,false,puppet->parameters);
}
}
}
