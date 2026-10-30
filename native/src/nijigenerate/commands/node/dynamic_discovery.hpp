#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>

namespace nijigenerate::commands::node::dynamic::detail {

inline compat::Slice<compat::RetainedString> discoverNodeTypes() {
    using compat::RetainedString;
    return {
        RetainedString::fromStaticLiteral("Node"),
        RetainedString::fromStaticLiteral("Mask"),
        RetainedString::fromStaticLiteral("Composite"),
        RetainedString::fromStaticLiteral("SimplePhysics"),
        RetainedString::fromStaticLiteral("MeshGroup"),
        RetainedString::fromStaticLiteral("DynamicComposite"),
        RetainedString::fromStaticLiteral("PathDeformer"),
        RetainedString::fromStaticLiteral("GridDeformer"),
        RetainedString::fromStaticLiteral("DepthRigRoot"),
        RetainedString::fromStaticLiteral("DepthBone"),
        RetainedString::fromStaticLiteral("Camera"),
    };
}

} // namespace nijigenerate::commands::node::dynamic::detail
