#include "bone.hpp"
#include "detail/bone_debug_log.hpp"
#include "detail/bone_vector_math.hpp"
#include "detail/standard_depth_types.hpp"
#include "detail/standard_depth_fields.hpp"
#include "detail/bone_json_number.hpp"
#include "detail/bone_json.hpp"
#include "detail/bone_source_settings_json_impl.hpp"
#include "detail/bone_standard_lookup.hpp"
#include "detail/bone_parameter_axis.hpp"
#include "detail/bone_standard_tables.hpp"
#include "detail/bone_standard_parameters.hpp"
#include "detail/bone_grid_index_impl.hpp"
#include "detail/bone_grid_prepare_impl.hpp"
#include "detail/bone_grid_stopper_impl.hpp"
#include "detail/bone_parameter_lookup_impl.hpp"
#include "detail/bone_strict_scope_impl.hpp"
#include "detail/bone_deform_enqueue_impl.hpp"
#include "detail/bone_pivot_observer_impl.hpp"
#include "detail/bone_influence_preview_impl.hpp"
#include "detail/bone_affected_parameters.hpp"
#include "detail/bone_async_claim.hpp"

namespace nijigenerate::commands::depth::bone {

thread_local DepthBoneCommandMap commands;

namespace {

// Ordinary D module variables are local to each thread, including their initial values.
thread_local std::uint64_t nextDepthBoneSourceSettingsMergeSession = 1;
thread_local std::uint64_t depthBoneSourceSettingsMergeSession = 0;
thread_local compat::RetainedString depthBoneSourceSettingsMergeProperty;

} // namespace

std::uint64_t ngCreateDepthBoneSourceSettingsMergeSession() {
    const auto result = nextDepthBoneSourceSettingsMergeSession++;
    if (nextDepthBoneSourceSettingsMergeSession == 0) {
        nextDepthBoneSourceSettingsMergeSession = 1;
    }
    return result;
}

void ngBeginDepthBoneSourceSettingsMerge(std::uint64_t session, compat::RetainedString property) {
    depthBoneSourceSettingsMergeSession = session;
    depthBoneSourceSettingsMergeProperty = property;
}

void ngEndDepthBoneSourceSettingsMerge() {
    depthBoneSourceSettingsMergeSession = 0;
    depthBoneSourceSettingsMergeProperty.reset();
}

} // namespace nijigenerate::commands::depth::bone

#include "detail/bone_source_command_runs_impl.hpp"
#include "detail/bone_standard_command_run_impl.hpp"
