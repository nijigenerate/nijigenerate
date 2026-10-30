#include "node_class_name.hpp"
#include <mutex>
#include <compat/object_metadata.hpp>
#include <resource.hpp>
#include <nijigenerate/ext/nodes/exgriddeformer_node.hpp>
#include <nijigenerate/ext/nodes/expathdeformer_node.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/ext/nodes/excamera.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/ext/package.hpp>

namespace nijigenerate::compat {
void ngRegisterResourceClassNames() {
    static std::once_flag initialized;
    std::call_once(initialized, [] {
        nicxlive::core::ngRegisterModelResourceClassNames();
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ext::ExPuppet),
            RetainedString::fromStaticLiteral("nijigenerate.ext.ExPuppet"));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ext::nodes::ExGridDeformer),
            RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.exgriddeformer.ExGridDeformer"));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ext::nodes::ExPathDeformer),
            RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.expathdeformer.ExPathDeformer"));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ext::nodes::ExPart),
            RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.expart.ExPart"));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ext::nodes::ExCamera),
            RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.excamera.ExCamera"));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ext::nodes::ExDepthBone),
            RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.exdepthbone.ExDepthBone"));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ext::nodes::ExDepthRigRoot),
            RetainedString::fromStaticLiteral("nijigenerate.ext.nodes.exdepthbone.ExDepthRigRoot"));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ext::ExParameter),
            RetainedString::fromStaticLiteral("nijigenerate.ext.param.ExParameter"));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ext::ExParameterGroup),
            RetainedString::fromStaticLiteral("nijigenerate.ext.param.ExParameterGroup"));
    });
}
void detail::registerNodeClassName(const std::type_info& type, RetainedString name) {
    ngRegisterResourceClassNames();
    nicxlive::core::d_runtime::ngRegisterObjectClassName(type, std::move(name));
}
RetainedString ngNodeDynamicClassName(const nicxlive::core::nodes::Node& node) {
    ngRegisterResourceClassNames();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(node));
}
}
