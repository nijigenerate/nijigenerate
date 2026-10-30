#include "detail/bone_flush.hpp"
#include "detail/bone_input.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include <cstddef>

namespace nijigenerate::commands::depth::bone {

void ngFlushDepthBoneDirty() {
    try {
        detail::flushDepthBoneDirty();
    } catch (compat::ThrowableException& error) {
        detail::abortDepthBoneGpuRefresh(error.msg);
        throw;
    }
}

bool ngFlushDepthBoneDirtyForFrame() {
    try {
        ngFlushDepthBoneDirty();
        return true;
    } catch (compat::ThrowableException& error) {
        detail::writeDepthBoneGpuFatalLog(error.msg);
        auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Depth Bone update failed: %s"));
        std::array<core::translation::TypedFormatArgument, 1> arguments{
            core::translation::ngFormatArgument(error.msg)};
        core::tasks::ngSetStatus(core::translation::ngFormatTyped(pattern.view(), arguments));
        return false;
    }
}

void ngFlushDepthBoneDirtyImmediate() {
    std::size_t guard = 0;
    while (ngHasPendingDepthBoneRefresh()) {
        ngFlushDepthBoneDirty();
        ++guard;
        if (!(guard < 10000))
            detail::failInput("Depth bone refresh queue did not drain", 3775);
    }
}

} // namespace nijigenerate::commands::depth::bone
