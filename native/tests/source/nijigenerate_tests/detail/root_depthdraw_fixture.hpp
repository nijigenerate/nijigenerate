#pragma once
// Selected existing regression.d DepthDraw helper declarations.
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <cstddef>
namespace nijigenerate_tests::regression::detail {
nijigenerate::core::settings::SettingsJsonValue loadDepthDrawFixture(nijigenerate::compat::RetainedString name);
std::size_t runDepthDrawAlphaMaskFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object);
std::size_t runDepthDrawAlphaDepthGapFillFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object);
std::size_t runDepthDrawGridConvolutionFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object);
std::size_t runDepthDrawDepthPruneFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object);
std::size_t runDepthDrawDepthCleanupFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object);
std::size_t runDepthDrawLayerBoundsSamplingFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object);
std::size_t runDepthDrawDepthSplitFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object);
std::size_t runDepthDrawPsdMaskExportFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object);
std::size_t runDepthDrawGlobalDepthScaleFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object);
}
