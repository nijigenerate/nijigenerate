#include "grid.hpp"
#include "nijigenerate/autorig/detail/json_first_pass_reader.hpp"
#include "nijigenerate/compat/json_stringify.hpp"
#include <array>

namespace nijigenerate::viewport::vertex::automesh::grid {
namespace {
using core::settings::SettingsJsonArray;
using core::settings::SettingsJsonObject;
using core::settings::SettingsJsonValue;
using Text = core::settings::SettingsString;
template<std::size_t N> Text literal(const char (&value)[N]) {
    return Text(compat::RetainedString::fromStaticLiteral(value));
}
struct FloatField { Text id; Text label; float* value; };
auto floatFields(GridAutoMeshProcessor& processor) {
    return std::array<FloatField, 4>{{
        {literal("mask_threshold"), literal("Mask threshold"), &processor.maskThreshold},
        {literal("x_segments"), literal("X Segments"), &processor.xSegments},
        {literal("y_segments"), literal("Y Segments"), &processor.ySegments},
        {literal("margin"), literal("Margin"), &processor.margin}}};
}
struct ArrayField { Text id; Text label; compat::Slice<float>* value; };
auto arrayFields(GridAutoMeshProcessor& processor) {
    return std::array<ArrayField, 2>{{
        {literal("scale_x"), literal("X Scale"), &processor.scaleX}, {literal("scale_y"), literal("Y Scale"), &processor.scaleY}}};
}
}

compat::RetainedString GridAutoMeshProcessor::schemaLegacy() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    SettingsJsonObject object;
    object[literal("type")] = literal("GridAutoMeshProcessor");
    SettingsJsonArray simple, advanced;
    for (const auto field : floatFields(*this)) {
        SettingsJsonObject item;
        item[literal("id")] = field.id;
        item[literal("label")] = field.label;
        item[literal("type")] = literal("float");
        simple.append(SettingsJsonValue(item));
    }
    for (const auto field : arrayFields(*this)) {
        SettingsJsonObject item;
        item[literal("id")] = field.id;
        item[literal("label")] = field.label;
        item[literal("type")] = literal("float[]");
        advanced.append(SettingsJsonValue(item));
    }
    object[literal("Simple")] = simple;
    object[literal("Advanced")] = advanced;
    object[literal("presets")] = SettingsJsonArray{};
    return compat::ngJsonStringify(SettingsJsonValue(object));
}

compat::RetainedString GridAutoMeshProcessor::valuesLegacy(compat::RetainedString levelName) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    SettingsJsonObject object;
    if (levelName.view() == "Advanced") {
        for (const auto field : arrayFields(*this)) {
            SettingsJsonArray values;
            const auto source = *field.value;
            for (const float value : source) values.append(SettingsJsonValue(static_cast<double>(value)));
            object[field.id] = values;
        }
    } else {
        for (const auto field : floatFields(*this)) object[field.id] = static_cast<double>(*field.value);
    }
    return compat::ngJsonStringify(SettingsJsonValue(object));
}

bool GridAutoMeshProcessor::applyPresetLegacy(compat::RetainedString) { return false; }

bool GridAutoMeshProcessor::writeValuesLegacy(compat::RetainedString levelName,
    compat::RetainedString updatesJson) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    auto updates = autorig::json::detail::FirstPassReader(updatesJson).parseRoot();
    bool any = false;
    if (levelName.view() == "Advanced") {
        for (const auto field : arrayFields(*this)) {
            const auto* found = updates.as_object().if_contains(field.id.view());
            if (!found) continue;
            const auto array = *found;
            if (!array.is_array()) continue;
            field.value->resize(0);
            const auto elements = array.as_array();
            for (const auto element : elements) {
                if (element.is_double() || element.is_int64()) {
                    // The authored Legacy code admits integers but then calls
                    // the floating-only getter. Its integer failure is retained.
                    field.value->append(static_cast<float>(element.as_double()));
                }
            }
            any = true;
        }
    } else {
        for (const auto field : floatFields(*this)) {
            const auto* item = updates.as_object().if_contains(field.id.view());
            if (item && (item->is_double() || item->is_int64())) {
                *field.value = static_cast<float>(item->as_double());
                any = true;
            }
        }
    }
    return any;
}

} // namespace nijigenerate::viewport::vertex::automesh::grid
