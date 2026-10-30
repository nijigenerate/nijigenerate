#include "attachedpoint.hpp"
#include "nijigenerate/core/translation.hpp"
#include <compat/object_metadata.hpp>

namespace nijigenerate::viewport::depth::tools::attachedpoint {

namespace {
[[maybe_unused]] const bool attachedPointSourceClassInfo = [] {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DepthAttachedPointTool),
        compat::RetainedString::fromStaticLiteral(
            "nijigenerate.viewport.depth.tools.attachedpoint.DepthAttachedPointTool"));
    return true;
}();
}

DepthToolMode DepthAttachedPointTool::mode() {
    return DepthToolMode::AttachedPoint;
}

const char* DepthAttachedPointTool::icon() {
    return "\xEE\xAC\xB5";
}

compat::RetainedString DepthAttachedPointTool::tooltip() {
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Edit Attached Depth Point"));
}

} // namespace nijigenerate::viewport::depth::tools::attachedpoint
