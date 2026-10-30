#include "review.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <algorithm>
#include <initializer_list>
#include <string>

namespace nijigenerate::autorig::deterministic::review {
namespace {
using namespace core::settings;
using Value = SettingsJsonValue;
using Object = SettingsJsonObject;
using Array = SettingsJsonArray;
using String = SettingsString;
using namespace contracts;
template<std::size_t N> String literal(const char (&text)[N]) {
    return compat::RetainedString::fromStaticLiteral(text);
}

Object& object(Value& value) {
    return value.sourceObjectRef();
}
Object duplicate(Value value) {
    return object(value).duplicate();
}
Value row(std::initializer_list<std::pair<String, Value>> entries) {
    Object result;
    for (const auto& [key, value] : entries) result[key] = value;
    return Value(result);
}
Value get(Value value, std::string_view key) {
    return value.sourceIndexValue(key);
}
Array array(Value value, std::string_view key) { return get(value, key).sourceArrayValue(); }
bool oneOf(std::string_view key, std::initializer_list<std::string_view> keys) {
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}
String string(RetainedStringSlice value) { return String(value); }
} // namespace

bool ngRigMaterialForcedStatic(bool active, RetainedStringSlice role, RetainedStringSlice semanticSource) {
    return !active || role.view() == "background" ||
        (semanticSource.view() == "full_body_backdrop_alpha_perimeter" &&
            (role.view() == "ear" || role.size() == 0));
}
bool ngRigMaterialStaticPreference(Value material) {
    const bool active = get(material, "active").as_bool(); const auto role = ngRigString(material, literal("role"));
    const auto semanticSource = ngRigString(material, literal("semantic_source"));
    return !ngRigMaterialForcedStatic(active, role, semanticSource) &&
        ngRigGet(material, literal("static"), Value(false)).as_bool();
}
Value ngRigEditedMaterialOverride(Value previous, std::uint8_t fields, RetainedStringSlice role,
    bool stationary, RetainedStringSlice feature, RetainedStringSlice side) {
    auto result = previous.is_object() ? duplicate(previous) : Object{};
    if (fields & static_cast<std::uint8_t>(RigMaterialField::role)) {
        if (role.size()) result[literal("role")] = Value(string(role));
        else result.erase("role");
    }
    if (fields & static_cast<std::uint8_t>(RigMaterialField::stationary)) result[literal("static")] = Value(stationary);
    if (fields & static_cast<std::uint8_t>(RigMaterialField::feature)) result[literal("feature")] = Value(string(feature));
    if (fields & static_cast<std::uint8_t>(RigMaterialField::side)) result[literal("side")] = Value(string(side));
    return Value(result);
}
Value ngRigReviewCompact(Value value, std::size_t depth) {
    if (value.is_array()) {
        const auto entries = value.as_array();
        if (depth > 8 || (entries.size() > 32 && !entries[0].is_object()))
            return row({{literal("items"), Value(entries.size())}});
        Array result;
        for (const auto entry : entries) result.append(ngRigReviewCompact(entry, depth + 1));
        return Value(result);
    }
    if (value.is_object()) {
        Object result;
        for (const auto entry : object(value)) {
            const auto key = entry.name.view();
            if (oneOf(key, {"data", "alpha", "cloud", "offsets", "values", "depth",
                "draw_order_cloud", "mapping"})) continue;
            if (entry.item.is_array() && oneOf(key, {"vertices", "uv", "triangles", "points",
                "allowed_vertices", "skin_protected_vertices"}))
                result[entry.name] = row({{literal("items"), Value(entry.item.as_array().size())}});
            else result[entry.name] = depth > 8 ? Value(literal("\xe2\x80\xa6")) :
                ngRigReviewCompact(entry.item, depth + 1);
        }
        return Value(result);
    }
    return value;
}
Value ngRigClassificationReview(Value observation, Value program) {
    Array rows;
    for (auto material : array(observation, "materials")) {
        Object item;
        for (const auto key : {literal("uuid"), literal("name"), literal("path"), literal("active"),
            literal("static"), literal("role"), literal("feature"), literal("side_hint"), literal("side_override"),
            literal("owner"), literal("chart"), literal("semantic_source"), literal("support_part"), literal("receiver")})
            if (auto* entry = object(material).if_contains(key.view())) item[key] = ngRigReviewCompact(*entry);
        rows.append(Value(item));
    }
    auto result = row({{literal("classification"), Value(rows)}});
    if (program.is_object()) {
        for (const auto key : {literal("scaffold"), literal("hierarchy")})
            if (auto* entry = object(program).if_contains(key.view())) object(result)[key] = ngRigReviewCompact(*entry);
        Array carriers;
        if (auto* targets = object(program).if_contains("carriers")) {
            for (auto target : targets->as_array()) {
                Object item;
                for (const auto key : {literal("part"), literal("path"), literal("role"), literal("side"),
                    literal("owner"), literal("chart"), literal("bones"), literal("domain_id")})
                    if (auto* entry = object(target).if_contains(key.view())) item[key] = *entry;
                carriers.append(Value(item));
            }
        }
        object(result)[literal("carriers")] = Value(carriers);
    }
    return result;
}
bool ngRigReviewEnabled(Value review, RetainedStringSlice id) {
    if (review.is_null()) return true;
    if (!review.is_object()) throw compat::ThrowableException(
        contracts::detail::copyContractMessage("Rig review settings must be an object"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/review.d"), 87);
    const auto* disabled = object(review).if_contains("disabled");
    if (!disabled) return true;
    for (const auto entry : disabled->as_array()) if (entry.as_string().view() == id.view()) return false;
    return true;
}
RetainedStringSlice ngRigReviewOperationId(RetainedStringSlice kind, Value operation, RetainedStringSlice name) {
    const auto partValue = ngRigUnsigned(get(operation, kind.view() == "weld" ? "source" : "part"));
    const auto part = contracts::detail::allocateContractStorage([&] { return std::to_string(partValue); });
    if (kind.view() == "weld" || kind.view() == "mask") {
        const auto prefix = contracts::detail::allocateContractStorage([&] {
            return std::string(kind.view() == "weld" ? "weld:" : "mask:") + part + ":";
        });
        // The original concatenates its prefix before reading the second JSON operand.
        const auto otherValue = ngRigUnsigned(get(operation, kind.view() == "weld" ? "target" : "source"));
        const auto other = contracts::detail::allocateContractStorage([&] { return std::to_string(otherValue); });
        return contracts::detail::allocateContractStorage([&] { return compat::RetainedString(prefix + other); });
    }
    const auto prefix = contracts::detail::allocateContractStorage([&] { return std::string(kind.view()) + ":"; });
    const auto namePrefix = contracts::detail::allocateContractStorage([&] {
        return name.size() ? std::string(name.view()) + ":" : std::string{};
    });
    return contracts::detail::allocateContractStorage([&] { return compat::RetainedString(prefix + namePrefix + part); });
}
Value ngRigControlGeometry(Value controls) {
    Array mechanisms;
    for (auto mechanism : array(controls, "mechanisms")) {
        auto frame = row({{literal("name"), get(mechanism, "name")}});
        for (const auto key : {literal("contact_curves"), literal("source_frame")})
            if (auto* value = object(mechanism).if_contains(key.view())) object(frame)[key] = *value;
        if (object(frame).size() > 1) mechanisms.append(frame);
    }
    return row({{literal("mechanisms"), Value(mechanisms)}});
}
Value ngRigReviewControls(Value controls, Value review, Value& operations) {
    operations = Value{};
    Array rows, mechanisms;
    auto record = [&](RetainedStringSlice id, Value item) {
        rows.append(row({{literal("id"), Value(string(id))},
            {literal("enabled"), Value(ngRigReviewEnabled(review, id))}, {literal("plan"), ngRigReviewCompact(item)}}));
    };
    for (auto mechanism : array(controls, "mechanisms")) {
        const auto name = get(mechanism, "name").as_string();
        const String id = contracts::detail::allocateContractStorage([&] {
            return String("mechanism:" + std::string(name.view()));
        });
        record(id, row({{literal("parameter"), Value(name)}, {literal("axisX"), get(mechanism, "axisX")},
            {literal("axisY"), get(mechanism, "axisY")}}));
        Array kept;
        for (auto operation : array(mechanism, "operations")) {
            auto operationId = ngRigReviewOperationId(literal("control"), operation, name);
            record(operationId, row({{literal("parameter"), Value(name)}, {literal("part"), get(operation, "part")},
                {literal("keys"), Value(get(operation, "keys").as_array().size())}}));
            if (ngRigReviewEnabled(review, id) && ngRigReviewEnabled(review, operationId)) kept.append(operation);
        }
        if (kept.size()) {
            auto copy = duplicate(mechanism);
            copy[literal("operations")] = Value(kept);
            mechanisms.append(Value(copy));
        }
    }
    auto result = duplicate(controls);
    result[literal("mechanisms")] = Value(mechanisms);
    for (const auto key : {literal("masks"), literal("draw_order")}) {
        Array kept;
        for (auto operation : array(controls, key.view())) {
            auto id = ngRigReviewOperationId(key.view() == "masks" ? literal("mask") : literal("zsort"), operation);
            record(id, operation);
            if (ngRigReviewEnabled(review, id)) kept.append(operation);
        }
        result[key] = Value(kept);
    }
    operations = Value(rows);
    return Value(result);
}
Value ngRigReviewChanges(Value before, Value after) {
    Object oldItems, newItems;
    for (auto item : before.as_array()) oldItems[get(item, "id").as_string()] = item;
    for (auto item : after.as_array()) newItems[get(item, "id").as_string()] = item;
    compat::Slice<String> ids;
    for (const auto& entry : oldItems) ids.append(entry.name);
    for (const auto& entry : newItems) if (!oldItems.if_contains(entry.name.view())) ids.append(entry.name);
    if (ids.size() > 1)
        std::sort(ids.begin(), ids.end(), [](const String& a, const String& b) { return a.view() < b.view(); });
    Array changes;
    for (const auto& id : ids) {
        auto* oldItem = oldItems.if_contains(id.view());
        auto* newItem = newItems.if_contains(id.view());
        if (oldItem && newItem && *oldItem == *newItem) continue;
        Object fields;
        if (oldItem && newItem) {
            for (const auto entry : object(*newItem)) {
                const auto* previous = object(*oldItem).if_contains(entry.name.view());
                if (!previous || *previous != entry.item)
                    fields[entry.name] = row({{literal("before"), previous ? *previous : Value{}},
                        {literal("after"), entry.item}});
            }
            for (const auto entry : object(*oldItem))
                if (!object(*newItem).if_contains(entry.name.view()))
                    fields[entry.name] = row({{literal("before"), entry.item}, {literal("after"), Value{}}});
        }
        changes.append(row({{literal("id"), Value(id)},
            {literal("change"), Value(!oldItem ? literal("added") : !newItem ? literal("removed") : literal("modified"))},
            {literal("fields"), Value(fields)}, {literal("before"), oldItem ? *oldItem : Value{}},
            {literal("after"), newItem ? *newItem : Value{}}}));
    }
    return Value(changes);
}
} // namespace nijigenerate::autorig::deterministic::review
