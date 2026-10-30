#pragma once
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/windows/psddepthmap.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <span>
namespace nijigenerate::commands::depth::psd_dialog::detail {
using Json = core::settings::SettingsJsonValue;
using Object = core::settings::SettingsJsonObject;
using Array = core::settings::SettingsJsonArray;
using namespace windows::psddepthmap;
template<std::size_t N> core::settings::SettingsString literalString(const char (&text)[N]) {
    return compat::RetainedString::fromStaticLiteral(text);
}
Json finiteFloatToJson(float value);
Json floatArrayToJson(compat::ConstSlice<float> values);
Json stringArrayToJson(compat::ConstSlice<compat::RetainedString> values);
Json boolArrayToJson(nicxlive::core::common::SharedArray<bool> values);
Json vertexPositionsToJson(PsdDepthDialogPartData& part);
Json colorStatisticsToJson(PsdDepthDialogPartLayerData& layer);
bool appendPngContent(Array& content, compat::ConstSlice<std::uint8_t> rgba, int width, int height,
    std::int64_t& contentIndex, compat::RetainedString& error);
bool appendFloat32ResourceContent(Array& content, compat::ConstSlice<float> values,
    compat::RetainedString uri, std::int64_t& contentIndex, compat::RetainedString& error);
Json imageReference(std::int64_t contentIndex, int width, int height, int left = 0, int top = 0);
}
