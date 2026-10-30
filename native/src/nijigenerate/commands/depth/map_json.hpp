#pragma once
#include "map_internal.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/autorig/deterministic/contracts_file.hpp>
#include <string>
#include <utility>
namespace nijigenerate::commands::depth::map::detail {
// std.exception.enforce duplicates its lazy message only on the failed path.
inline compat::ThrowableException depthMapEnforcementFailure(
    compat::RetainedString message, compat::RetainedString file, std::size_t line) {
    const auto copied = ::nijigenerate::autorig::deterministic::contracts::detail::copyContractMessage(message.view());
    return compat::ThrowableException(copied, std::move(file), line);
}
using Json = core::settings::SettingsJsonValue;
using Object = core::settings::SettingsJsonObject;
using Array = core::settings::SettingsJsonArray;
using Vec2 = nicxlive::core::math::Vec2;
void enforce(bool condition, std::string_view text);
float jsonFloat(Json value, compat::RetainedString name, float fallback = 0.0f);
std::size_t jsonSize(Json value, compat::RetainedString name, std::size_t fallback = 0);
Vec2 jsonVec2(Json value, compat::RetainedString name, Vec2 fallback = {0, 0});
Json vec2ToJson(Vec2 value);
ext::nodes::ExDepthOp depthOpFromJson(Json value);
compat::Slice<ext::nodes::ExDepthOp> depthOpsFromJson(Json value);
Json depthOpsToJson(compat::Slice<ext::nodes::ExDepthOp> ops);
Json depthsToJson(compat::Slice<float> depths);
}
