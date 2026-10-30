#pragma once

#include <nijigenerate/ext/nodes/exdepthrig_data.hpp>

namespace nijigenerate::commands::depth::bone::detail {
// Required actual JSON apply bodies, not successful fallback providers.
void applyRuleJson(ext::nodes::ExDepthInfluenceRule& rule, compat::RetainedString text);
void applySourceSettingsJson(ext::nodes::ExDepthBoneSourceSettings& setting, compat::RetainedString text);
} // namespace nijigenerate::commands::depth::bone::detail
