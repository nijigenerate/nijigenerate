#include "presentation.hpp"
#include "presentation_conversion.hpp"
#include <nijigenerate/autorig/framework_primitives.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <array>
#include <string>
#include <utility>
#include <initializer_list>
#include <compat/source_allocator.hpp>

namespace nijigenerate::autorig::deterministic::presentation {
namespace {
using Value = SettingsJsonValue;
using Object = core::settings::SettingsJsonObject;
using Array = core::settings::SettingsJsonArray;
using String = core::settings::SettingsString;
using Row = compat::Slice<RetainedStringSlice>;
using namespace contracts;
// Escaping Source array literals allocate before evaluating their elements.
// Producers are synchronous reference-only closures, never erased or retained.
template<class... Element> Row literalRow(Element&&... element) {
    Row result;
    result.resize(sizeof...(Element));
    std::size_t index = 0;
    (static_cast<void>(result[index++] = element()), ...);
    return result;
}
String dash() { return compat::RetainedString::fromStaticLiteral("\xe2\x80\x94"); }
String retainedMessage(String message) { return message; }
template<std::size_t N> String retainedMessage(const char (&message)[N]) {
    return compat::RetainedString::fromStaticLiteral(message);
}
template<std::size_t N> std::pair<std::string_view, String> labelEntry(
    std::string_view key, const char (&message)[N]) {
    return {key, compat::RetainedString::fromStaticLiteral(message)};
}
Object& object(Value& value) {
    return value.sourceObjectRef();
}
struct JsonMemberReference {
    Object::iterator entry;
    Value* value = nullptr;
    explicit operator bool() const { return value != nullptr; }
    Value& operator*() const { return *value; }
    Value* operator->() const { return value; }
};
JsonMemberReference findMember(Object members, std::string_view key) {
    JsonMemberReference result;
    for (auto entry = members.begin(); entry != members.end(); ++entry) {
        auto& member = *entry;
        if (member.key() == key) {
            result.entry = entry;
            result.value = &member.value();
        }
    }
    return result;
}
Value get(Value value, std::string_view key) {
    return value.sourceIndexValue(key);
}
template<std::size_t N> Value optional(Value value, const char (&key)[N], Value fallback = {}) {
    return ngRigGet(value, retainedMessage(key), fallback);
}
template<std::size_t N> bool equalField(Value previous, Value current, const char (&key)[N]) {
    const auto before = get(previous, key);
    const auto after = get(current, key);
    return before == after;
}
template<std::size_t N> bool equalOptionalField(Value previous, Value current, const char (&key)[N]) {
    const auto before = optional(previous, key);
    const auto after = optional(current, key);
    return before == after;
}
Array array(Value value, std::string_view key) { return get(value, key).as_array(); }
Array array(Value value) { return value.as_array(); }
String str(Value value, std::string_view key) { return get(value, key).as_string(); }
template<std::size_t N> String text(Value value, const char (&key)[N], String fallback = String::emptyLiteral()) {
    return String(ngRigString(value, retainedMessage(key), fallback));
}
void setName(RigReviewMaterialNames& names, std::uint64_t id, RetainedStringSlice value) {
    if (!names) names = std::allocate_shared<RigReviewMaterialNameTable>(
        nicxlive::core::d_runtime::SourceAllocator<RigReviewMaterialNameTable>{});
    (*names)[id] = value;
}
String joinText(const compat::Slice<String>& parts, std::string_view separator) {
    if (parts.empty()) return {};
    std::size_t length = (parts.size() - 1) * separator.size();
    for (const auto& part : parts) length += part.size();
    auto result = contracts::detail::allocateContractStorage([&] { return std::string(length, '\0'); });
    std::size_t position = 0;
    const auto put = [&](std::string_view bytes, std::size_t sourceLine) {
        for (const auto byte : bytes) {
            if (position >= result.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                position, result.size(), compat::RetainedString::fromStaticLiteral("std/array.d"), sourceLine);
            result[position++] = byte;
        }
    };
    bool first = true;
    for (const auto& part : parts) {
        if (!first) put(separator, 2263);
        put(part.view(), first ? 2258 : 2265);
        first = false;
    }
    return result.empty() ? String{} : contracts::detail::allocateContractStorage([&] {
        return String(std::move(result));
    });
}
void appendText(String& text, std::string_view suffix) {
    const auto previous = text.view();
    auto result = contracts::detail::allocateContractStorage([&] {
        std::string bytes(previous); bytes.append(suffix);
        return String(std::move(bytes));
    });
    text = std::move(result);
}
std::string concatenateBytes(std::initializer_list<std::string_view> parts) {
    return contracts::detail::allocateContractStorage([&] {
        std::string result;
        for (const auto part : parts) result.append(part);
        return result;
    });
}
String numericText(Value value) {
    if (value.is_null()) return dash();
    if (value.is_string()) return value.as_string();
    if (value.is_bool()) return value.as_bool() ?
        String(compat::RetainedString::fromStaticLiteral("\xe2\x9c\x93")) : dash();
    if (value.is_int64()) {
        const auto number = value.as_int64();
        return contracts::detail::allocateContractStorage([&] { return String(std::to_string(number)); });
    }
    if (value.is_uint64()) {
        const auto number = value.as_uint64();
        return contracts::detail::allocateContractStorage([&] { return String(std::to_string(number)); });
    }
    if (value.is_double()) {
        const std::array<core::translation::TypedFormatArgument, 1> arguments{
            core::translation::ngFormatArgument(value.as_double())};
        auto formatted = core::translation::ngFormatTyped("%.2f", arguments);
        return contracts::detail::allocateContractStorage([&] { return String(std::move(formatted)); });
    }
    if (value.is_array()) {
        const auto entries = value.as_array();
        compat::Slice<String> parts;
        for (auto entry : entries) parts.append(numericText(entry));
        return joinText(parts, ", ");
    }
    return dash();
}
std::uint64_t parseId(std::string_view text) {
    return detail::ngUnsignedNodeId(text);
}
} // namespace

RigReviewMaterialNames ngRigReviewMaterialNames(Value report) {
    RigReviewMaterialNames names;
    if (!report.is_object()) return names;
    const auto classification = findMember(object(report), "classification");
    if (!classification || !classification->is_array()) return names;
    for (auto row : array(*classification)) {
        if (!row.is_object()) continue;
        const auto id = findMember(object(row), "uuid");
        const auto path = findMember(object(row), "path");
        const auto name = findMember(object(row), "name");
        if (!id || (!id->is_int64() && !id->is_uint64())) continue;
        const auto label = path && path->is_string() ? path : name;
        if (label && label->is_string()) setName(names, ngRigUnsigned(*id), label->as_string());
    }
    return names;
}

compat::Slice<RigReviewTable> ngRigReviewTables(Value report, RigReviewMaterialNames names,
    RigReviewTranslate translate) {
    if (!report.is_object()) return {};
    auto t = [&](auto&& message) -> String {
        auto marked = framework::ngAutoRigMessage(retainedMessage(std::forward<decltype(message)>(message)));
        return String(translate ? translate(marked) : marked);
    };
    auto name = [&](Value id) -> String {
        const auto key = ngRigUnsigned(id);
        if (names) if (auto it = names->find(key); it != names->end()) return String(it->second);
        return t("Unresolved target");
    };
    auto makeTable = [&]<std::size_t N>(const char (&title)[N], std::initializer_list<String> columns) {
        RigReviewTable table;
        table.title = t(title);
        table.columns.resize(columns.size());
        std::size_t index = 0;
        for (const auto& column : columns) table.columns[index++] = t(column);
        return table;
    };
    compat::Slice<RigReviewTable> tables;
    if (auto rows = findMember(object(report), "classification")) {
        auto table = makeTable("Material classification", {
            retainedMessage("Material"),
            retainedMessage("Role"),
            retainedMessage("Feature"),
            retainedMessage("Rigging"),
            retainedMessage("Classification evidence")});
        for (auto row : array(*rows)) {
            const auto id = ngRigUnsigned(get(row, "uuid"));
            const auto label = str(row, "name");
            setName(names, id, label);
            const bool stationary = optional(row, "static", Value(!get(row, "active").as_bool())).as_bool();
            const auto role = text(row, "role");
            table.rows.append(literalRow([&] { return str(row, "name"); },
                [&] { return ngRigReviewClassLabel(role, translate); },
                [&] { return ngRigReviewClassLabel(text(row, "feature"), translate); },
                [&] { return t(stationary ? retainedMessage("Static artwork") : retainedMessage("Enabled")); },
                [&] { return ngRigReviewEvidenceLabel(text(row, "semantic_source"), translate); }));
        }
        tables.append(table);
    }
    if (auto rows = findMember(object(report), "carriers")) {
        auto table = makeTable("Rig targets and bone assignments", {
            retainedMessage("Material"),
            retainedMessage("Model side"),
            retainedMessage("Bones")});
        for (auto row : array(*rows)) table.rows.append(literalRow(
            [&] { return text(row, "path", name(get(row, "part"))); },
            [&] { return ngRigReviewClassLabel(text(row, "side"), translate); },
            [&] { return numericText(optional(row, "bones")); }));
        tables.append(table);
    }
    if (auto scaffold = findMember(object(report), "scaffold"))
        if (auto rows = findMember(object(*scaffold), "bones")) {
            auto table = makeTable("Anatomical scaffold", {
                retainedMessage("Bone"),
                retainedMessage("Parent"),
                retainedMessage("Bone head position"),
                retainedMessage("Bone tail position"),
                retainedMessage("Bone rest angle")});
            for (auto row : array(*rows)) table.rows.append(literalRow([&] { return str(row, "id"); },
                [&] { return numericText(get(row, "parent")); }, [&] { return numericText(get(row, "head")); },
                [&] { return numericText(get(row, "tail")); },
                [&] { return numericText(optional(row, "rest_roll")); }));
            tables.append(table);
        }
    if (auto hierarchy = findMember(object(report), "hierarchy"))
        if (auto groups = findMember(object(*hierarchy), "groups")) {
            auto table = makeTable("Planned hierarchy", {
                retainedMessage("Group"),
                retainedMessage("Parent"),
                retainedMessage("Bone"),
                retainedMessage("Position")});
            for (auto group : array(*groups)) {
                auto parent = get(group, "parent");
                table.rows.append(literalRow([&] { return str(group, "id"); },
                    [&] { return text(parent, "group",
                        findMember(object(parent), "node") ? name(get(parent, "node")) : dash()); },
                    [&] { return str(group, "bone"); }, [&] { return numericText(get(group, "origin")); }));
            }
            tables.append(table);
        }
    if (auto rows = findMember(object(report), "changes")) {
        for (auto row : array(*rows)) for (const auto key : {"before", "after"}) {
            auto item = get(row, key);
            if (!item.is_object() || !str(item, "id").view().starts_with("node:")) continue;
            const auto id = parseId(str(item, "id").view().substr(5));
            const auto label = str(item, "name");
            setName(names, id, label);
        }
        auto table = makeTable("Model changes", {
            retainedMessage("Target"),
            retainedMessage("Action"),
            retainedMessage("Changed item"),
            retainedMessage("Before"),
            retainedMessage("After")});
        for (auto row : array(*rows)) {
            const auto action = str(row, "change");
            auto item = get(row, action.view() == "removed" ? "before" : "after");
            const auto target = str(item, "name");
            if (action.view() != "modified") {
                table.rows.append(literalRow([&] { return target; },
                    [&] { return t(action.view() == "added" ? retainedMessage("Added") : retainedMessage("Removed")); },
                    [&] { return ngRigReviewClassLabel(text(item, "type", retainedMessage("Parameter")), translate); },
                    [&] { return action.view() == "removed" ? target : dash(); },
                    [&] { return action.view() == "added" ? target : dash(); }));
                if (action.view() == "added")
                    for (const auto field : {retainedMessage("parent_name"), retainedMessage("rest_head"),
                        retainedMessage("rest_tail"), retainedMessage("rest_roll"), retainedMessage("minimum"),
                        retainedMessage("maximum"), retainedMessage("vertices"), retainedMessage("triangles")})
                        if (const auto value = findMember(object(item), field.view()))
                            table.rows.append(literalRow([&] { return target; }, [&] { return t("Added"); },
                                [&] { return ngRigReviewFieldLabel(field, translate); }, [&] { return dash(); },
                                [&] { return numericText(*value); }));
                continue;
            }
            bool visible = false;
            for (const auto field : {retainedMessage("name"), retainedMessage("type"), retainedMessage("parent_name"),
                retainedMessage("enabled"), retainedMessage("zsort"), retainedMessage("translation"),
                retainedMessage("rotation"), retainedMessage("scale"), retainedMessage("vertices"),
                retainedMessage("triangles"), retainedMessage("vertex_bounds"), retainedMessage("opacity"),
                retainedMessage("rest_head"), retainedMessage("rest_tail"), retainedMessage("rest_roll"),
                retainedMessage("inherit_parent"), retainedMessage("minimum"), retainedMessage("maximum"),
                retainedMessage("default"), retainedMessage("axisX"), retainedMessage("axisY"),
                retainedMessage("maximum_offset")}) {
                auto fields = get(row, "fields");
                const auto change = findMember(object(fields), field.view());
                if (!change) continue;
                table.rows.append(literalRow([&] { return target; }, [&] { return t("Modified"); },
                    [&] { return ngRigReviewFieldLabel(field, translate); },
                    [&] { return numericText(get(*change, "before")); },
                    [&] { return numericText(get(*change, "after")); }));
                visible = true;
            }
            for (const auto field : {retainedMessage("bindings"), retainedMessage("welding"), retainedMessage("masks")}) {
                auto fields = get(row, "fields");
                if (const auto change = findMember(object(fields), field.view())) {
                    auto summary = [&](Value items, bool updated) -> String {
                        if (!items.is_array()) return dash();
                        compat::Slice<String> entries;
                        for (auto entry : array(items)) {
                            const auto id = optional(entry, "target", optional(entry, "source"));
                            auto value = text(entry, "target_name", id.is_null() ? String::emptyLiteral() : name(id));
                            if (field.view() == "bindings") {
                                const auto property = str(entry, "property");
                                const auto authored = numericText(get(entry, "authored_keys"));
                                const auto keys = t("keys");
                                const auto maximum = numericText(optional(entry, "maximum_offset"));
                                const std::array<core::translation::TypedFormatArgument, 4> arguments{
                                    core::translation::ngFormatArgument(property),
                                    core::translation::ngFormatArgument(authored),
                                    core::translation::ngFormatArgument(keys),
                                    core::translation::ngFormatArgument(maximum)};
                                appendText(value, core::translation::ngFormatTyped(" / %s / %s %s / %s", arguments));
                            }
                            if (field.view() == "bindings" && updated) for (auto previous : array(*change, "before"))
                                if (equalField(previous, entry, "target") &&
                                    equalField(previous, entry, "property") &&
                                    !equalOptionalField(previous, entry, "content_sha256")) {
                                    const auto label = t("Updated values");
                                    appendText(value, concatenateBytes({" / ",label.view()}));
                                }
                            if (field.view() == "welding") {
                                const auto paired = numericText(optional(entry, "paired_vertices"));
                                const auto pairedLabel = t("Paired vertices");
                                const auto weightLabel = t("Weight");
                                const auto weight = numericText(optional(entry, "weight"));
                                appendText(value, concatenateBytes({" / ",paired.view()," ",pairedLabel.view(),
                                    " / ",weightLabel.view()," ",weight.view()}));
                                if (updated) for (auto previous : array(*change, "before"))
                                    if (equalField(previous, entry, "target") &&
                                        !equalOptionalField(previous, entry, "indices_sha256")) {
                                        const auto label = t("Correspondence updated");
                                        appendText(value, concatenateBytes({" / ",label.view()}));
                                    }
                            }
                            entries.append(value);
                        }
                        return entries.empty() ? dash() : joinText(entries, "\n");
                    };
                    table.rows.append(literalRow([&] { return target; }, [&] { return t("Modified"); },
                        [&] { return ngRigReviewFieldLabel(field, translate); },
                        [&] { return summary(get(*change, "before"), false); },
                        [&] { return summary(get(*change, "after"), true); }));
                    visible = true;
                }
            }
            if (!visible) table.rows.append(literalRow([&] { return target; }, [&] { return t("Modified"); },
                [&] { return t("Mesh or deformation values"); }, [&] { return t("Previous values"); },
                [&] { return t("Updated values"); }));
        }
        if (!table.rows.size()) table.rows.append(literalRow([&] { return t("No model changes"); },
            [&] { return dash(); }, [&] { return dash(); }, [&] { return dash(); }, [&] { return dash(); }));
        tables.append(table);
    }
    if (auto passed = findMember(object(report), "passed")) {
        auto table = makeTable("Verification", {
            retainedMessage("Check"),
            retainedMessage("Result")});
        table.rows.append(literalRow([&] { return t("Numerical rig verification"); },
            [&] { return t(passed->as_bool() ? retainedMessage("Passed") : retainedMessage("Failed")); }));
        for (const auto field : {retainedMessage("bones"), retainedMessage("baked_keys"),
                retainedMessage("intermediate_samples")})
            if (const auto value = findMember(object(report), field.view()))
                table.rows.append(literalRow([&] { return ngRigReviewFieldLabel(field, translate); },
                    [&] { return numericText(*value); }));
        if (auto findings = findMember(object(report), "numerical_findings"))
            for (auto finding : array(*findings))
                table.rows.append(literalRow([&] { return text(finding, "parameter", t("Model")); },
                    [&] { return str(finding, "message"); }));
        if (auto error = findMember(object(report), "error")) table.rows.append(literalRow(
            [&] { return t("Error"); }, [&] { return error->as_string(); }));
        if (auto visual = findMember(object(report), "visual_review_required"))
            if (visual->as_bool()) table.rows.append(literalRow(
                [&] { return t("Visual review"); }, [&] { return t("Required"); }));
        tables.append(table);
    }
    if (auto details = findMember(object(report), "details"))
        if (auto pairs = findMember(object(*details), "shoulder_pairs")) {
            auto table = makeTable("Shoulder welding", {
                retainedMessage("Source"),
                retainedMessage("Target"),
                retainedMessage("Result"),
                retainedMessage("Paired vertices")});
            for (auto pair : array(*pairs)) table.rows.append(literalRow(
                [&] { return name(get(pair, "source")); }, [&] { return name(get(pair, "target")); },
                [&] { return ngRigReviewClassLabel(text(pair, "status", get(pair, "matching").as_bool() ?
                    retainedMessage("matching") : retainedMessage("not_matching")), translate); },
                [&] { return numericText(optional(pair, "paired_vertices")); }));
            if (table.rows.size()) tables.append(table);
        }
    if (auto details = findMember(object(report), "details"))
        if (auto depth = findMember(object(*details), "depth_validation")) {
            auto table = makeTable("Depth verification", {
                retainedMessage("Check"),
                retainedMessage("Result")});
            if (optional(*depth, "applicable", Value(false)).as_bool()) {
                table.rows.append(literalRow([&] { return t("Depth units"); }, [&] { return t("Verified"); }));
                table.rows.append(literalRow([&] { return t("Observed depth scale"); },
                    [&] { return numericText(get(*depth, "observed_scale")); }));
                table.rows.append(literalRow([&] { return t("Planned depth scale"); },
                    [&] { return numericText(get(*depth, "compiled_scale")); }));
                table.rows.append(literalRow([&] { return t("Depth surfaces"); }, [&] {
                    const auto count = array(*depth, "surfaces").size();
                    return contracts::detail::allocateContractStorage([&] { return String(std::to_string(count)); });
                }));
            } else table.rows.append(literalRow(
                [&] { return t("Depth surfaces"); }, [&] { return t("Not applicable"); }));
            tables.append(table);
        }
    return tables;
}

RetainedStringSlice ngRigReviewFieldLabel(RetainedStringSlice value, RigReviewTranslate translate) {
    static const std::pair<std::string_view, String> labels[] = {
        labelEntry("name", "Name"),
        labelEntry("type", "Node type"),
        labelEntry("parent_name", "Parent"),
        labelEntry("enabled", "Enabled"),
        labelEntry("zsort", "Draw order"),
        labelEntry("translation", "Position"),
        labelEntry("rotation", "Rotation"),
        labelEntry("scale", "Scale"),
        labelEntry("vertices", "Vertices"),
        labelEntry("triangles", "Triangles"),
        labelEntry("vertex_bounds", "Mesh bounds"),
        labelEntry("opacity", "Opacity"),
        labelEntry("rest_head", "Bone head position"),
        labelEntry("rest_tail", "Bone tail position"),
        labelEntry("rest_roll", "Bone rest angle"),
        labelEntry("inherit_parent", "Inherit parent motion"),
        labelEntry("bindings", "Parameter bindings"),
        labelEntry("welding", "Welding"),
        labelEntry("masks", "Clipping masks"),
        labelEntry("minimum", "Minimum"),
        labelEntry("maximum", "Maximum"),
        labelEntry("default", "Default value"),
        labelEntry("axisX", "Horizontal keys"),
        labelEntry("axisY", "Vertical keys"),
        labelEntry("bones", "Bones"),
        labelEntry("baked_keys", "Baked keys"),
        labelEntry("intermediate_samples", "Intermediate samples"),
    };
    RetainedStringSlice result = value;
    for (const auto& [key, label] : labels) if (value.view() == key) {
        result = framework::ngAutoRigMessage(label);
        break;
    }
    return translate ? translate(result) : result;
}

RetainedStringSlice ngRigReviewClassLabel(RetainedStringSlice value, RigReviewTranslate translate) {
    static const std::pair<std::string_view, String> labels[] = {
        labelEntry("face", "Face"),
        labelEntry("face_feature", "Facial feature"),
        labelEntry("neck", "Neck"),
        labelEntry("torso", "Torso"),
        labelEntry("arm", "Arm"),
        labelEntry("hand", "Hand"),
        labelEntry("leg", "Leg"),
        labelEntry("foot", "Foot"),
        labelEntry("sleeve", "Sleeve"),
        labelEntry("bodice", "Upper clothing"),
        labelEntry("waistwear", "Waist clothing"),
        labelEntry("thigh_accessory", "Thigh accessory"),
        labelEntry("ankle_accessory", "Ankle accessory"),
        labelEntry("skirt", "Front skirt"),
        labelEntry("skirt_back", "Back skirt"),
        labelEntry("apron", "Apron"),
        labelEntry("shoulder", "Shoulder"),
        labelEntry("ear", "Ear"),
        labelEntry("headwear", "Head accessory"),
        labelEntry("tail", "Tail"),
        labelEntry("chest_accessory", "Chest accessory"),
        labelEntry("pelvis_accessory", "Hip accessory"),
        labelEntry("local", "Local mechanism"),
        labelEntry("background", "Background"),
        labelEntry("Part", "Material"),
        labelEntry("GridDeformer", "Grid deformer"),
        labelEntry("DepthBone", "Bone"),
        labelEntry("DynamicComposite", "Dynamic composite"),
        labelEntry("Composite", "Composite"),
        labelEntry("Node", "Group"),
        labelEntry("Parameter", "Parameter"),
        labelEntry("L", "Model left"),
        labelEntry("R", "Model right"),
        labelEntry("Both", "Both sides"),
        labelEntry("hair_front", "Front hair"),
        labelEntry("hair_side", "Side hair"),
        labelEntry("hair_back", "Back hair"),
        labelEntry("sclera", "Eye white"),
        labelEntry("iris", "Iris"),
        labelEntry("upper", "Upper eyelid"),
        labelEntry("lower", "Lower eyelid"),
        labelEntry("corner", "Eye corner"),
        labelEntry("brow", "Eyebrow"),
        labelEntry("fold", "Eyelid fold"),
        labelEntry("nose", "Nose"),
        labelEntry("mouth", "Mouth"),
        labelEntry("mouth_tongue", "Tongue"),
        labelEntry("mouth_upper_teeth", "Upper teeth"),
        labelEntry("mouth_lower_teeth", "Lower teeth"),
        labelEntry("mouth_outline", "Mouth outline"),
        labelEntry("mouth_upper_lip", "Upper lip"),
        labelEntry("mouth_lower_lip", "Lower lip"),
        labelEntry("disabled_by_user", "Disabled by user"),
        labelEntry("not_matching", "No matching seam"),
        labelEntry("matching", "Matching seam"),
        labelEntry("no_native_vertex_pairs", "No matching vertices"),
        labelEntry("applied_readback_verified", "Applied and verified"),
    };
    RetainedStringSlice result = value;
    if (!value.size()) result = framework::ngAutoRigMessage(compat::RetainedString::fromStaticLiteral("None"));
    for (const auto& [key, label] : labels) if (value.view() == key) {
        result = framework::ngAutoRigMessage(label);
        break;
    }
    return translate ? translate(result) : result;
}

RetainedStringSlice ngRigReviewEvidenceLabel(RetainedStringSlice value, RigReviewTranslate translate) {
    static const std::pair<std::string_view, String> labels[] = {
        labelEntry("explicit_override", "Explicit review setting"),
        labelEntry("model_name_and_ancestry_candidate", "Material name and parent hierarchy"),
        labelEntry("full_body_backdrop_alpha_perimeter", "Background detected from alpha extent"),
        labelEntry("imported_clipping_receiver", "Inherited from clipping receiver"),
        labelEntry("local_mechanism_without_anatomy", "Local mechanism without anatomical support"),
        labelEntry("alpha_proximal_support_candidate", "Nearest anatomical alpha support"),
    };
    RetainedStringSlice result = value;
    if (!value.size()) result = framework::ngAutoRigMessage(
        compat::RetainedString::fromStaticLiteral("Not classified yet"));
    for (const auto& [key, label] : labels) if (value.view() == key) {
        result = framework::ngAutoRigMessage(label);
        break;
    }
    return translate ? translate(result) : result;
}
} // namespace nijigenerate::autorig::deterministic::presentation
