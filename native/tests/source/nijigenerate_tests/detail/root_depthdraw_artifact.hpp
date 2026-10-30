#pragma once
// Module-private declaration of existing regression.d10600.
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <cstddef>
namespace nijigenerate_tests::regression::detail {
std::size_t runDepthDrawArtifactFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object);
}
