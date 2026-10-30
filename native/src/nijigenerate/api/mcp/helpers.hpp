#pragma once
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/commands/context.hpp>
namespace nijigenerate::api::mcp::helpers {
std::shared_ptr<commands::Context> buildContextFromPayload(core::settings::SettingsJsonValue payload);
}
#include "helpers_result.hpp"
#include "helpers_payload.hpp"
