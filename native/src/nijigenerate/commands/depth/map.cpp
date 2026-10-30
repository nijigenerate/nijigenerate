#include "map_json.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <cmath>

namespace nijigenerate::commands::depth::map {

thread_local DepthCommandMap commands;

bool PsdDepthGpuComposeWork::isActive() const {
    return active;
}

#if defined(NG_COMMAND_BROWSER_DIFFERENTIAL)
PsdDepthComposedView ngPsdDepthComposedViewForRegression(io::depthmap_psd::PsdDepthImportResult& imported) {
    PsdDepthComposedView composed;
    composed.imported = &imported;
    return composed;
}
#endif

namespace detail {
thread_local std::shared_ptr<actions::asyncprogress::AsyncGroupActionProgress> activePsdDepthImportProgress;

float finiteFloat(float value, compat::RetainedStringSlice name) {
    if (!std::isfinite(value)) {
        const auto argument = core::translation::ngFormatArgument(name);
        throw depthMapEnforcementFailure(compat::RetainedString(core::translation::ngFormatTyped(
            "%s must be finite", std::span(&argument, 1))),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 134);
    }
    return value;
}
} // namespace detail

} // namespace nijigenerate::commands::depth::map
