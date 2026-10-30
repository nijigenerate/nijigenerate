#pragma once
#include <nijigenerate/core/settings_value.hpp>
namespace nijigenerate::api::mcp::resource_listing {
core::settings::SettingsJsonValue buildCurrentResourceList();
void rewriteResourcesListResponse(core::settings::SettingsJsonValue& response,
    core::settings::SettingsJsonValue& request);
}
