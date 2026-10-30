// Required Phobos std.file/std.path/std.conv behavior is adapted under BSL-1.0.
// Copyright notices and license: native/port/licenses/phobos-depth-manifest.txt.
#include "manifest.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/core/settings_json_string.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/compat/phobos_unsigned_conversion.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/utils/crashdump_file_name.hpp>
#include <nijigenerate/utils/crashdump_file_error.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cfenv>
#include <limits>
#include <stdexcept>
#include <exception>
#include <new>
#include <source_location>
#if defined(_M_X64) || defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#include <xmmintrin.h>
#endif
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace nijigenerate::viewport::depth::draw::manifest {
using namespace io::depthimage;
namespace {
using Value = core::settings::SettingsJsonValue;
using Object = core::settings::SettingsJsonObject;
using Array = core::settings::SettingsJsonArray;
using Vec2 = nicxlive::core::math::Vec2;
std::string_view text(const DepthDrawString& value) { return value.view(); }
// Only pure bytes/backing work belongs here; providers and OS calls stay outside.
template<class Make> decltype(auto) allocateManifestMemory(Make&& make) {
    try { return make(); }
    catch (const std::bad_alloc&) { throw compat::ngStaticError<compat::OutOfMemoryError>(); }
    catch (const std::length_error&) { throw compat::ngStaticError<compat::OutOfMemoryError>(); }
}
std::string joinText(std::string_view first, std::string_view second) {
    return allocateManifestMemory([&] {
        auto result = std::string(first);
        result.append(second);
        return result;
    });
}
template<class Integer> std::string decimalText(Integer value) {
    return allocateManifestMemory([&] { return std::to_string(value); });
}
DepthDrawString retain(std::string value) {
    return allocateManifestMemory([&] { return DepthDrawString(std::move(value)); });
}
DepthDrawString copyEnforceMessage(std::string_view message) {
    if (message.empty()) return DepthDrawString::fromStaticLiteral("Enforcement failed");
    return allocateManifestMemory([&] { return DepthDrawString(std::string(message)); });
}
template<std::size_t N> DepthDrawString retain(const char (&value)[N]) {
    return DepthDrawString::fromStaticLiteral(value);
}
template<std::size_t N> core::settings::SettingsString jsonKey(const char (&value)[N]) {
    return core::settings::SettingsString(retain(value));
}
Value stringValue(DepthDrawString value) {
    return Value(core::settings::SettingsString(value));
}
compat::RetainedString manifestSourceFile() {
    return compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/manifest.d");
}
template<std::size_t N> void enforce(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) throw compat::ThrowableException(
        copyEnforceMessage(std::string_view(message, N - 1)), manifestSourceFile(), line);
}
template<class Message> void enforceLazy(bool condition, Message&& message, std::size_t line) {
    if (!condition) {
        const auto bytes = message();
        throw compat::ThrowableException(copyEnforceMessage(bytes), manifestSourceFile(), line);
    }
}
Value get(Object object, std::string_view key, Value fallback) {
    if (object.isOrdered()) throw core::settings::SettingsJsonError::fromStaticLiteral(
        "JSONValue object is ordered, cannot return by ref");
    const auto found = object.if_contains(key);
    return found ? *found : fallback;
}
Object object(Value value) {
    return value.sourceObjectRef();
}
int toInt(float value) {
#if defined(_M_X64) || defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
    // The source truncating conversion uses the actual integer-indefinite
    // result and floating exception flags, not an early sentinel substitution.
    return _mm_cvtt_ss2si(_mm_set_ss(value));
#else
    if (!std::isfinite(value) || value < -2147483648.0f || value >= 2147483648.0f) {
        (void)std::feraiseexcept(FE_INVALID);
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
#endif
}
std::uint64_t parseUlong(DepthDrawString value) {
    return compat::phobos::ngUnsignedDecimal64(text(value));
}
bool stringArrayContains(compat::Slice<DepthDrawString> values, DepthDrawString value) {
    for (auto candidate : values) if (text(candidate) == text(value)) return true;
    return false;
}
float jsonFloat(Value value, std::string_view name, float fallback = 0.0f) {
    if (value.is_null()) return fallback;
    if (value.is_double()) return static_cast<float>(value.as_double());
    if (value.is_int64()) return static_cast<float>(value.as_int64());
    if (value.is_uint64()) return static_cast<float>(value.as_uint64());
    throw compat::ThrowableException(copyEnforceMessage(joinText(name, " must be a number")),
        manifestSourceFile(), 129);
}
bool jsonBool(Value value, std::string_view name, bool fallback = false) {
    if (value.is_null()) return fallback;
    if (value.is_bool()) return value.as_bool();
    throw compat::ThrowableException(copyEnforceMessage(joinText(name, " must be a bool")),
        manifestSourceFile(), 149);
}
DepthDrawString jsonString(Value value, std::string_view name, DepthDrawString fallback = {}) {
    if (value.is_null()) return fallback;
    enforceLazy(value.is_string(), [&] { return joinText(name, " must be a string"); }, 156);
    const auto& valueString = value.as_string();
    return valueString.retainedString();
}
Value vec2ToJson(Vec2 value) { return Array{Value(static_cast<double>(value.x)), Value(static_cast<double>(value.y))}; }
Vec2 vec2FromJson(Value value, std::string_view name, Vec2 fallback = {0, 0}) {
    if (value.is_null()) return fallback;
    enforceLazy(value.is_array() && value.as_array().size() == 2,
        [&] { return joinText(name, " must be [x, y]"); }, 50);
    const auto element = [&](std::size_t index, const char* suffix) {
        const auto item = value.as_array()[index];
        const auto itemName = joinText(name, suffix);
        return jsonFloat(item, itemName);
    };
    const auto x = element(0, "[0]");
    const auto y = element(1, "[1]");
    return {x, y};
}
Value rectToJson(DepthDrawRect rect) {
    return Array{Value(static_cast<std::int64_t>(rect.left)), Value(static_cast<std::int64_t>(rect.top)),
        Value(static_cast<std::int64_t>(rect.width)), Value(static_cast<std::int64_t>(rect.height))};
}
DepthDrawRect rectFromJson(Value value, std::string_view name) {
    enforceLazy(value.is_array() && value.as_array().size() == 4,
        [&] { return joinText(name, " must be [left, top, width, height]"); }, 64);
    DepthDrawRect rect;
    const auto element = [&](std::size_t index, const char* suffix) {
        const auto item = value.as_array()[index];
        const auto itemName = joinText(name, suffix);
        return toInt(jsonFloat(item, itemName));
    };
    rect.left = element(0, "[0]");
    rect.top = element(1, "[1]");
    rect.width = element(2, "[2]");
    rect.height = element(3, "[3]");
    return rect;
}
Value displayToJson(DepthDrawDisplayOptions display) {
    Object item;
    item[jsonKey("showNormalImage")] = Value(display.showNormalImage);
    item[jsonKey("showRawDepth")] = Value(display.showRawDepth);
    item[jsonKey("showCoverage")] = Value(display.showCoverage);
    item[jsonKey("showComposite")] = Value(display.showComposite);
    item[jsonKey("showLayerPlanes")] = Value(display.showLayerPlanes);
    item[jsonKey("showDepthRanges")] = Value(display.showDepthRanges);
    item[jsonKey("showMissingVertices")] = Value(display.showMissingVertices);
    item[jsonKey("showWinningLayer")] = Value(display.showWinningLayer);
    item[jsonKey("useGpuPreview")] = Value(display.useGpuPreview);
    return item;
}
DepthDrawDisplayOptions displayFromJson(Value value, std::string_view name) {
    DepthDrawDisplayOptions display;
    if (value.is_null()) return display;
    enforceLazy(value.is_object(), [&] { return joinText(name, " must be an object"); }, 90);
    const auto values = object(value);
#define NG_DEPTH_DISPLAY_FIELD(field) do { \
    const auto item = get(values, #field, Value(display.field)); \
    const auto itemName = joinText(name, "." #field); \
    const auto fallback = display.field; \
    display.field = jsonBool(item, itemName, fallback); \
} while (false)
    NG_DEPTH_DISPLAY_FIELD(showNormalImage);
    NG_DEPTH_DISPLAY_FIELD(showRawDepth);
    NG_DEPTH_DISPLAY_FIELD(showCoverage);
    NG_DEPTH_DISPLAY_FIELD(showComposite);
    NG_DEPTH_DISPLAY_FIELD(showLayerPlanes);
    NG_DEPTH_DISPLAY_FIELD(showDepthRanges);
    NG_DEPTH_DISPLAY_FIELD(showMissingVertices);
    NG_DEPTH_DISPLAY_FIELD(showWinningLayer);
    NG_DEPTH_DISPLAY_FIELD(useGpuPreview);
#undef NG_DEPTH_DISPLAY_FIELD
    return display;
}
DepthDrawString mergePolicyName(DepthMergePolicy value) {
    switch (value) {
        case DepthMergePolicy::Replace: return retain("Replace");
        case DepthMergePolicy::Frontmost: return retain("Frontmost");
        case DepthMergePolicy::Backmost: return retain("Backmost");
        case DepthMergePolicy::Add: return retain("Add");
        case DepthMergePolicy::KeepExistingWhereMissing: return retain("KeepExistingWhereMissing");
    }
    throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        manifestSourceFile(), 161);
}
DepthMergePolicy mergePolicyFromString(DepthDrawString value) {
    auto s = text(value);
    if (s == "Replace") return DepthMergePolicy::Replace;
    if (s == "Frontmost") return DepthMergePolicy::Frontmost;
    if (s == "Backmost") return DepthMergePolicy::Backmost;
    if (s == "Add") return DepthMergePolicy::Add;
    if (s == "KeepExistingWhereMissing") return DepthMergePolicy::KeepExistingWhereMissing;
    throw compat::ThrowableException(retain(joinText("Unknown DepthDraw merge policy: ", s)),
        manifestSourceFile(), 177);
}
DepthDrawString cleanupKindName(DepthDrawLayerCleanupKind kind) {
    switch (kind) {
        case DepthDrawLayerCleanupKind::AlphaDepthGapFill: return retain("AlphaDepthGapFill");
        case DepthDrawLayerCleanupKind::ContourRepair: return retain("ContourRepair");
    }
    throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        manifestSourceFile(), 182);
}
DepthDrawLayerCleanupKind cleanupKindFromString(DepthDrawString value) {
    if (text(value) == "AlphaDepthGapFill") return DepthDrawLayerCleanupKind::AlphaDepthGapFill;
    if (text(value) == "ContourRepair") return DepthDrawLayerCleanupKind::ContourRepair;
    throw compat::ThrowableException(retain(joinText(
        "Unknown DepthDraw cleanup operation: ", text(value))), manifestSourceFile(), 192);
}
Value cleanupOperationsToJson(compat::Slice<DepthDrawLayerCleanupOperation> operations) {
    Array result;
    for (auto operation : operations) {
        Object item;
        item[jsonKey("kind")] = stringValue(cleanupKindName(operation.kind));
        item[jsonKey("contourThickness")] = Value(static_cast<std::int64_t>(operation.contourThickness));
        Array rules;
        for (auto rule : operation.focusedRules) {
            Object ruleItem;
#define NG_DEPTH_RULE_FIELD(field) ruleItem[jsonKey(#field)] = Value(static_cast<std::int64_t>(rule.field))
            NG_DEPTH_RULE_FIELD(layerIndex); NG_DEPTH_RULE_FIELD(x); NG_DEPTH_RULE_FIELD(y);
            NG_DEPTH_RULE_FIELD(w); NG_DEPTH_RULE_FIELD(h); NG_DEPTH_RULE_FIELD(lift); NG_DEPTH_RULE_FIELD(radius);
#undef NG_DEPTH_RULE_FIELD
            rules.append(Value(ruleItem));
        }
        item[jsonKey("focusedRules")] = Value(rules);
        result.append(Value(item));
    }
    return result;
}
int jsonBoundedInt(Value value, std::string_view name, int minimum, int maximum, int fallback = 0) {
    auto number = jsonFloat(value, name, static_cast<float>(fallback));
    if (number <= minimum) return minimum;
    if (number >= maximum) return maximum;
    return toInt(number);
}
compat::Slice<DepthDrawLayerCleanupOperation> cleanupOperationsFromJson(Value value,
    std::string_view name, int layerWidth, int layerHeight) {
    if (value.is_null()) return {};
    enforceLazy(value.is_array(), [&] { return joinText(name, " must be an array"); }, 234);
    compat::Slice<DepthDrawLayerCleanupOperation> result;
    const auto values = value.as_array();
    for (std::size_t i = 0; i < values.size(); ++i) {
        auto entry = values[i];
        auto itemName = allocateManifestMemory([&] { return std::string(name) + "[" + std::to_string(i) + "]"; });
        enforceLazy(entry.is_object(), [&] { return joinText(itemName, " must be an object"); }, 238);
        auto valuesObject = object(entry);
        DepthDrawLayerCleanupOperation operation;
        auto kindValue = get(valuesObject, "kind", Value(nullptr));
        auto kindName = joinText(itemName, ".kind");
        operation.kind = cleanupKindFromString(jsonString(kindValue, kindName));
        auto thicknessValue = get(valuesObject, "contourThickness", Value(2));
        auto thicknessName = joinText(itemName, ".contourThickness");
        auto thickness = jsonFloat(thicknessValue, thicknessName, 2);
        operation.contourThickness = thickness >= DepthDrawMaxContourThickness ? DepthDrawMaxContourThickness :
            ngNormalizeDepthDrawContourThickness(toInt(thickness));
        auto rulesValue = get(valuesObject, "focusedRules", Value(Array{}));
        enforceLazy(rulesValue.is_array(), [&] { return joinText(itemName, ".focusedRules must be an array"); }, 248);
        const auto rules = rulesValue.as_array();
        for (std::size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex) {
            auto ruleValue = rules[ruleIndex];
            auto ruleName = allocateManifestMemory([&] {
                return itemName + ".focusedRules[" + std::to_string(ruleIndex) + "]";
            });
            enforceLazy(ruleValue.is_object(), [&] { return joinText(ruleName, " must be an object"); }, 251);
            const auto ruleObject = object(ruleValue);
            DepthDrawAlphaDepthFocusedRule rule;
            auto maxX = layerWidth > 0 ? layerWidth : std::numeric_limits<int>::max();
            auto maxY = layerHeight > 0 ? layerHeight : std::numeric_limits<int>::max();
            const auto bounded = [&](const char* key, int maximum) {
                auto fieldValue = get(ruleObject, key, Value(0));
                auto fieldName = allocateManifestMemory([&] { return ruleName + "." + key; });
                return jsonBoundedInt(fieldValue, fieldName, 0, maximum);
            };
            rule.layerIndex = bounded("layerIndex", std::numeric_limits<int>::max());
            rule.x = bounded("x", maxX);
            rule.y = bounded("y", maxY);
            rule.w = bounded("w", maxX);
            rule.h = bounded("h", maxY);
            rule.lift = bounded("lift", 255);
            rule.radius = bounded("radius", DepthDrawMaxFocusedRuleRadius);
            if (layerWidth > 0 && layerHeight > 0) rule = ngNormalizeDepthDrawFocusedRule(rule, layerWidth, layerHeight);
            operation.focusedRules.append(rule);
        }
        result.append(operation);
    }
    return result;
}
} // namespace
Value ngDepthDrawSessionToManifest(std::shared_ptr<DepthDrawSession> session) {
    Object root;
    root[jsonKey("version")] = Value(std::int64_t{1});
    if (session != nullptr) {
        root[jsonKey("selectedLayerId")] = stringValue(session->selectedLayerId);
        root[jsonKey("selectedGridUuid")] = stringValue(retain(decimalText(session->selectedGridUuid)));
        root[jsonKey("display")] = displayToJson(session->display);
        root[jsonKey("sourceIdentity")] = stringValue(session->sourceIdentity);
        root[jsonKey("documentWidth")] = Value(static_cast<std::int64_t>(session->documentWidth));
        root[jsonKey("documentHeight")] = Value(static_cast<std::int64_t>(session->documentHeight));
    } else {
        root[jsonKey("selectedLayerId")] = stringValue(retain(""));
        root[jsonKey("selectedGridUuid")] = stringValue(retain("0"));
        root[jsonKey("display")] = displayToJson(DepthDrawDisplayOptions());
        root[jsonKey("sourceIdentity")] = stringValue(retain(""));
        root[jsonKey("documentWidth")] = Value(std::int64_t{0});
        root[jsonKey("documentHeight")] = Value(std::int64_t{0});
    }

    Value layers = Value(Array{});
    if (session != nullptr) {
        const auto sessionLayers = session->layers;
        for (auto layer : sessionLayers) {
            Object item;
            item[jsonKey("id")] = stringValue(layer.id);
            item[jsonKey("sourcePath")] = stringValue(layer.sourcePath);
            item[jsonKey("layerPath")] = stringValue(layer.layerPath);
            item[jsonKey("displayName")] = stringValue(layer.displayName);
            item[jsonKey("width")] = Value(static_cast<std::int64_t>(layer.width));
            item[jsonKey("height")] = Value(static_cast<std::int64_t>(layer.height));
            item[jsonKey("bounds")] = rectToJson(layer.bounds);
            item[jsonKey("opacity")] = Value(static_cast<double>(layer.opacity));
            item[jsonKey("visible")] = Value(layer.visible);
            item[jsonKey("enabled")] = Value(layer.enabled);
            item[jsonKey("xyOffset")] = vec2ToJson(layer.xyOffset);
            item[jsonKey("xyScale")] = vec2ToJson(layer.xyScale);
            item[jsonKey("zOffset")] = Value(static_cast<double>(layer.zOffset));
            item[jsonKey("zScale")] = Value(static_cast<double>(layer.zScale));
            item[jsonKey("backDepth")] = Value(static_cast<double>(layer.backDepth));
            item[jsonKey("frontDepth")] = Value(static_cast<double>(layer.frontDepth));
            item[jsonKey("sampleDepthScale")] = Value(static_cast<double>(layer.sampleDepthScale));
            item[jsonKey("invert")] = Value(layer.invert);
            item[jsonKey("channel")] = stringValue(ngDepthImageChannelNameSource(layer.channel));
            item[jsonKey("convolution")] = stringValue(ngDepthImageConvolutionNameSource(layer.convolution));
            item[jsonKey("customRadius")] = Value(static_cast<std::int64_t>(layer.customRadius));
            item[jsonKey("alphaThreshold")] = Value(static_cast<double>(layer.alphaThreshold));
            item[jsonKey("cleanupOperations")] = cleanupOperationsToJson(layer.cleanupOperations);
            layers.as_array().append(Value(item));
        }
    }
    root[jsonKey("layers")] = layers;

    Value bindings = Value(Array{});
    if (session != nullptr) {
        const auto sessionBindings = session->bindings;
        for (auto binding : sessionBindings) {
            Object item;
            item[jsonKey("layerId")] = stringValue(binding.layerId);
            item[jsonKey("targetNodeUuid")] = stringValue(retain(decimalText(binding.targetNodeUuid)));
            item[jsonKey("targetGridUuid")] = stringValue(retain(decimalText(binding.targetGridUuid)));
            item[jsonKey("order")] = Value(static_cast<std::int64_t>(binding.order));
            item[jsonKey("enabled")] = Value(binding.enabled);
            item[jsonKey("useNormalLayerAlpha")] = Value(binding.useNormalLayerAlpha);
            item[jsonKey("coverageThreshold")] = Value(static_cast<double>(binding.coverageThreshold));
            item[jsonKey("mergePolicy")] = stringValue(mergePolicyName(binding.mergePolicy));
            bindings.as_array().append(Value(item));
        }
    }
    root[jsonKey("bindings")] = bindings;
    return Value(root);
}

std::shared_ptr<DepthDrawSession> ngDepthDrawSessionFromManifest(Value manifest) {
    enforce(manifest.is_object(), "DepthDraw manifest must be an object", 345);
    auto session = std::allocate_shared<DepthDrawSession>(
        nicxlive::core::d_runtime::SourceAllocator<DepthDrawSession>{});
    session->selectedLayerId = jsonString(get(object(manifest), "selectedLayerId", stringValue(retain(""))), "selectedLayerId", retain(""));
    session->selectedGridUuid = parseUlong(jsonString(get(object(manifest), "selectedGridUuid", stringValue(retain("0"))),
        "selectedGridUuid", retain("0")));
    session->display = displayFromJson(get(object(manifest), "display", Value(nullptr)), "display");
    session->sourceIdentity = jsonString(
        get(object(manifest), "sourceIdentity", stringValue(retain(""))), "sourceIdentity", retain(""));
    session->documentWidth = toInt(jsonFloat(
        get(object(manifest), "documentWidth", Value(0)), "documentWidth"));
    session->documentHeight = toInt(jsonFloat(
        get(object(manifest), "documentHeight", Value(0)), "documentHeight"));

    auto layersValue = get(object(manifest), "layers", Value(Array{}));
    enforce(layersValue.is_array(), "DepthDraw manifest layers must be an array", 359);
    for (auto entry : layersValue.as_array()) {
        enforce(entry.is_object(), "DepthDraw manifest layer must be an object", 361);
        auto valuesObject = object(entry);
        DepthDrawLayer layer;
        layer.id = jsonString(get(valuesObject, "id", Value(nullptr)), "layer.id");
        layer.sourcePath = jsonString(get(valuesObject, "sourcePath", Value(nullptr)), "layer.sourcePath");
        layer.layerPath = jsonString(get(valuesObject, "layerPath", Value(nullptr)), "layer.layerPath");
        layer.displayName = jsonString(get(valuesObject, "displayName", Value(nullptr)), "layer.displayName");
        layer.width = toInt(jsonFloat(get(valuesObject, "width", Value(0)), "layer.width"));
        layer.height = toInt(jsonFloat(get(valuesObject, "height", Value(0)), "layer.height"));
        layer.bounds = rectFromJson(get(valuesObject, "bounds", rectToJson(DepthDrawRect{0, 0, layer.width, layer.height})), "layer.bounds");
        layer.opacity = jsonFloat(get(valuesObject, "opacity", Value(1.0)), "layer.opacity");
        layer.visible = jsonBool(get(valuesObject, "visible", Value(true)), "layer.visible", true);
        layer.enabled = jsonBool(get(valuesObject, "enabled", Value(true)), "layer.enabled", true);
        layer.xyOffset = vec2FromJson(get(valuesObject, "xyOffset", Value(nullptr)), "layer.xyOffset", Vec2{0, 0});
        layer.xyScale = vec2FromJson(get(valuesObject, "xyScale", Value(nullptr)), "layer.xyScale", Vec2{1, 1});
        layer.zOffset = jsonFloat(get(valuesObject, "zOffset", Value(0.0)), "layer.zOffset");
        layer.zScale = jsonFloat(get(valuesObject, "zScale", Value(1.0)), "layer.zScale");
        layer.backDepth = jsonFloat(get(valuesObject, "backDepth", Value(-1.0)), "layer.backDepth");
        layer.frontDepth = jsonFloat(get(valuesObject, "frontDepth", Value(1.0)), "layer.frontDepth");
        layer.sampleDepthScale = jsonFloat(
            get(valuesObject, "sampleDepthScale", Value(1.0)), "layer.sampleDepthScale");
        layer.invert = jsonBool(get(valuesObject, "invert", Value(false)), "layer.invert");
        layer.channel = ngDepthImageChannelFromStringSource(
            jsonString(get(valuesObject, "channel", stringValue(retain("AverageRGB"))), "layer.channel"));
        layer.convolution = ngDepthImageConvolutionFromStringSource(
            jsonString(get(valuesObject, "convolution", stringValue(retain("Gaussian3x3"))), "layer.convolution"));
        layer.customRadius = ngNormalizeDepthImageCustomRadius(
            toInt(jsonFloat(get(valuesObject, "customRadius", Value(3)), "layer.customRadius")));
        layer.alphaThreshold = jsonFloat(get(valuesObject, "alphaThreshold", Value(0.01)), "layer.alphaThreshold");
        layer.cleanupOperations = cleanupOperationsFromJson(
            get(valuesObject, "cleanupOperations", Value(Array{})), "layer.cleanupOperations",
            layer.width, layer.height);
        session->layers.append(layer);
    }

    auto bindingsValue = get(object(manifest), "bindings", Value(Array{}));
    enforce(bindingsValue.is_array(), "DepthDraw manifest bindings must be an array", 397);
    for (auto entry : bindingsValue.as_array()) {
        enforce(entry.is_object(), "DepthDraw manifest binding must be an object", 399);
        auto valuesObject = object(entry);
        DepthDrawBinding binding;
        binding.layerId = jsonString(get(valuesObject, "layerId", Value(nullptr)), "binding.layerId");
        binding.targetNodeUuid = parseUlong(jsonString(get(valuesObject, "targetNodeUuid", stringValue(retain("0"))), "binding.targetNodeUuid"));
        binding.targetGridUuid = parseUlong(jsonString(get(valuesObject, "targetGridUuid", stringValue(retain("0"))), "binding.targetGridUuid"));
        binding.order = toInt(jsonFloat(get(valuesObject, "order", Value(0)), "binding.order"));
        binding.enabled = jsonBool(get(valuesObject, "enabled", Value(true)), "binding.enabled", true);
        binding.useNormalLayerAlpha = jsonBool(
            get(valuesObject, "useNormalLayerAlpha", Value(true)), "binding.useNormalLayerAlpha", true);
        binding.coverageThreshold = jsonFloat(get(valuesObject, "coverageThreshold", Value(0.01)), "binding.coverageThreshold");
        binding.mergePolicy = mergePolicyFromString(
            jsonString(get(valuesObject, "mergePolicy", stringValue(retain("Frontmost"))), "binding.mergePolicy"));
        session->bindings.append(binding);
    }
    session->normalizeGpuPreviewSampling();
    return session;
}

namespace {
std::string serialize(Value value) {
    return core::settings::ngSettingsJsonString(value);
}
bool dirSeparator(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}
bool unc(std::string_view path) {
    return path.size() >= 3 && dirSeparator(path[0]) && dirSeparator(path[1]) && !dirSeparator(path[2]);
}
std::size_t uncRoot(std::string_view path) {
    std::size_t i = 3;
    while (i < path.size() && !dirSeparator(path[i])) ++i;
    if (i < path.size()) {
        auto j = i;
        do { ++j; } while (j < path.size() && dirSeparator(path[j]));
        if (j < path.size()) {
            do { ++j; } while (j < path.size() && !dirSeparator(path[j]));
            i = j;
        }
    }
    return i;
}
DepthDrawString resolveManifestSourcePath(DepthDrawString sourcePath, DepthDrawString sourceBaseDir) {
    const auto source = text(sourcePath), base = text(sourceBaseDir);
#ifdef _WIN32
    const bool absolute = (source.size() >= 3 && source[1] == ':' && dirSeparator(source[2])) || unc(source);
#else
    const bool absolute = !source.empty() && source[0] == '/';
#endif
    if (source.empty() || absolute || base.empty()) return sourcePath;
    auto pos = base.size();
    bool useSeparator = false;
    if (dirSeparator(source[0])) {
#ifdef _WIN32
        if (base.size() >= 2 && base[1] == ':') pos = 2;
        else if (unc(base)) pos = uncRoot(base);
        else pos = 0;
        if (pos > 0 && dirSeparator(base[pos - 1])) --pos;
#else
        pos = 0;
#endif
    } else if (!dirSeparator(base[pos - 1])) useSeparator = true;
    return allocateManifestMemory([&] {
        std::string result(base.substr(0, pos));
        if (useSeparator) {
#ifdef _WIN32
            result += '\\';
#else
            result += '/';
#endif
        }
        result += source;
        return retain(std::move(result));
    });
}
#ifdef _WIN32
using NativeFileName = utils::crashdump::detail::CrashFileName<wchar_t>;
#else
using NativeFileName = utils::crashdump::detail::CrashFileName<char>;
#endif
NativeFileName filePath(DepthDrawString value) {
    // Consume the existing actual tempCString lowering, preserving null and
    // counted input before the native API consumes its first terminator.
    return NativeFileName(compat::RetainedStringSlice(value));
}
bool fileExists(DepthDrawString value) {
    const auto path = filePath(value);
#ifdef _WIN32
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat status;
    return ::stat(path.c_str(), &status) == 0;
#endif
}
#ifdef _WIN32
void fileEnforce(bool condition, DepthDrawString path, std::size_t line) {
    if (!condition) {
        const auto code = GetLastError();
        utils::crashdump::detail::failCrashFile(compat::RetainedStringSlice(path), code, line);
    }
}
#endif
std::string readText(DepthDrawString value) {
    const auto path = filePath(value);
    std::string result;
#ifdef _WIN32
    auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    fileEnforce(handle != INVALID_HANDLE_VALUE, value, 424);
    compat::ngRunWithScopeExit([&] {
        DWORD high;
        const auto low = GetFileSize(handle, &high);
        fileEnforce(low != INVALID_FILE_SIZE, value, 427);
        const auto size = (static_cast<std::uint64_t>(high) << 32) | low;
        allocateManifestMemory([&] {
            result.resize(static_cast<std::size_t>(std::min<std::uint64_t>(size,
                std::numeric_limits<std::size_t>::max())));
        });
        std::size_t total = 0;
        while (total != result.size()) {
            auto chunk = static_cast<DWORD>(std::min<std::size_t>(result.size() - total, 0xffff0000));
            DWORD count = 0;
            fileEnforce(ReadFile(handle, result.data() + total, chunk, &count, nullptr) != 0 && count == chunk,
                value, 437);
            total += count;
        }
    }, [&] {
        fileEnforce(CloseHandle(handle) != 0, value, 425);
    });
#else
    const auto fail = [&](std::size_t line) {
        utils::crashdump::detail::failCrashFile(compat::RetainedStringSlice(value),
            static_cast<std::uint32_t>(errno), line);
    };
    const auto fd = ::open(path.c_str(), O_RDONLY);
    if (fd == -1) fail(350);
    try {
        struct stat status;
        if (::fstat(fd, &status) != 0) fail(354);
        const auto initial = status.st_size == 0 ? std::size_t{4096} : [&] {
            const auto extended = std::bit_cast<std::int64_t>(
                static_cast<std::uint64_t>(status.st_size) + 1);
            if (extended < 0) throw compat::phobos::ConvOverflowException();
            return static_cast<std::size_t>(std::min<std::uint64_t>(
                static_cast<std::uint64_t>(extended), std::numeric_limits<std::size_t>::max() / 2));
        }();
        allocateManifestMemory([&] { result.resize(initial); });
        std::size_t used = 0;
        for (;;) {
            const auto count = ::read(fd, result.data() + used, result.size() - used);
            if (count == -1) fail(366);
            if (count == 0) break;
            used += static_cast<std::size_t>(count);
            if (used < result.size()) continue;
            if (used > std::numeric_limits<std::size_t>::max() - 16384)
                compat::ngAssertUnreachable("Overflow on binary operator +", "std/checkedint.d", 1476);
            allocateManifestMemory([&] { result.resize(used + 16384); });
        }
        result.resize(used);
    } catch (...) {
        (void)::close(fd);
        throw;
    }
    (void)::close(fd);
#endif
    const auto starts = [&](std::initializer_list<unsigned char> bom) {
        return result.size() >= bom.size() && std::equal(bom.begin(), bom.end(), result.begin(),
            [](unsigned char a, char b) { return a == static_cast<unsigned char>(b); });
    };
    if (starts({0, 0, 0xfe, 0xff}) || starts({0xff, 0xfe, 0, 0}))
        throw compat::UTFException(
            compat::RetainedString::fromStaticLiteral("UTF-8 requested. BOM is for UTF-32"),
            compat::RetainedString::fromStaticLiteral("std/file.d"), 532);
    if (starts({0xfe, 0xff}) || starts({0xff, 0xfe}))
        throw compat::UTFException(
            compat::RetainedString::fromStaticLiteral("UTF-8 requested. BOM is for UTF-16"),
            compat::RetainedString::fromStaticLiteral("std/file.d"), 530);
    compat::phobos::detail::UnicodeCursor<std::string_view, char> cursor{std::string_view(result)};
    while (!cursor.empty()) {
        (void)cursor.front();
        cursor.popFront();
    }
    return result;
}
void writeText(DepthDrawString value, std::string_view bytes) {
    const auto path = filePath(value);
#ifdef _WIN32
    auto handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    fileEnforce(handle != INVALID_HANDLE_VALUE, value, 873);
    std::size_t sum = 0;
    while (sum != bytes.size()) {
        auto chunk = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - sum, std::size_t{1} << 30));
        DWORD written = 0;
        WriteFile(handle, bytes.data() + sum, chunk, &written, nullptr);
        if (written != chunk) break;
        sum += written;
    }
    fileEnforce(sum == bytes.size() && CloseHandle(handle) != 0, value, 898);
#else
    const auto fd = ::open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd == -1) utils::crashdump::detail::failCrashFile(
        compat::RetainedStringSlice(value), static_cast<std::uint32_t>(errno), 840);
    try {
        std::size_t total = 0;
        while (total != bytes.size()) {
            const auto count = std::min(bytes.size() - total, std::size_t{1} << 30);
            const auto written = ::write(fd, bytes.data() + total, count);
            if (written != static_cast<ssize_t>(count)) break;
            total += static_cast<std::size_t>(written);
        }
        if (total != bytes.size()) utils::crashdump::detail::failCrashFile(
            compat::RetainedStringSlice(value), static_cast<std::uint32_t>(errno), 854);
    } catch (...) {
        (void)::close(fd);
        throw;
    }
    if (::close(fd) != 0) utils::crashdump::detail::failCrashFile(
        compat::RetainedStringSlice(value), static_cast<std::uint32_t>(errno), 856);
#endif
}
} // namespace
bool DepthDrawManifestValidation::ok() const {
    return missingSourceLayerIds.empty() && missingTargetLayerIds.empty() && missingBindingLayerIds.empty() &&
        duplicateLayerIds.empty() && duplicateBindingKeys.empty() && text(missingSelectedLayerId).empty() &&
        missingSelectedGridUuid == 0;
}
void ngSaveDepthDrawManifest(std::shared_ptr<DepthDrawSession> session, DepthDrawString path) {
    writeText(path, ngDepthDrawManifestToJson(ngDepthDrawSessionToManifest(session)));
}
std::string ngDepthDrawManifestToJson(Value manifest) { return serialize(manifest); }
void ngDepthDrawWriteFileBytes(DepthDrawString path, std::span<const std::uint8_t> bytes) {
    writeText(path, bytes.empty() ? std::string_view{} :
        std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}
std::shared_ptr<DepthDrawSession> ngLoadDepthDrawManifest(DepthDrawString path) {
    auto bytes = readText(path);
    const auto input = retain(std::move(bytes));
    autorig::json::detail::FirstPassReader reader{core::settings::SettingsString(input)};
    return ngDepthDrawSessionFromManifest(reader.parseRoot());
}
DepthDrawManifestValidation ngValidateDepthDrawSessionManifest(std::shared_ptr<DepthDrawSession> session,
    compat::SharedDelegate<bool(std::uint64_t)> hasTargetGrid, DepthDrawString sourceBaseDir) {
    DepthDrawManifestValidation result;
    if (!session) return result;
    compat::Slice<DepthDrawString> seenLayerIds;
    const auto layers = session->layers;
    for (auto layer : layers) {
        if (!text(layer.id).empty()) {
            if (stringArrayContains(seenLayerIds, layer.id)) {
                if (!stringArrayContains(result.duplicateLayerIds, layer.id)) result.duplicateLayerIds.append(layer.id);
            } else seenLayerIds.append(layer.id);
        }
        auto sourcePath = resolveManifestSourcePath(layer.sourcePath, sourceBaseDir);
        if (!text(sourcePath).empty() && !fileExists(sourcePath)) result.missingSourceLayerIds.append(layer.id);
    }
    if (!text(session->selectedLayerId).empty() && !session->layerById(session->selectedLayerId))
        result.missingSelectedLayerId = session->selectedLayerId;
    compat::Slice<DepthDrawString> seenBindingKeys;
    const auto bindings = session->bindings;
    for (auto binding : bindings) {
        if (!binding.enabled) continue;
        auto bindingKey = allocateManifestMemory([&] {
            std::string key(text(binding.layerId));
            key.push_back('\0'); key += std::to_string(binding.targetGridUuid);
            return retain(std::move(key));
        });
        if (stringArrayContains(seenBindingKeys, bindingKey)) {
            if (!stringArrayContains(result.duplicateBindingKeys, bindingKey)) result.duplicateBindingKeys.append(bindingKey);
        } else seenBindingKeys.append(bindingKey);
        if (text(binding.layerId).empty() || !session->layerById(binding.layerId))
            result.missingBindingLayerIds.append(binding.layerId);
        if (binding.targetGridUuid == 0) { result.missingTargetLayerIds.append(binding.layerId); continue; }
        if (hasTargetGrid && !hasTargetGrid(binding.targetGridUuid)) result.missingTargetLayerIds.append(binding.layerId);
    }
    if (session->selectedGridUuid != 0) {
        if (!session->hasTargetGrid(session->selectedGridUuid)) result.missingSelectedGridUuid = session->selectedGridUuid;
        else if (hasTargetGrid && !hasTargetGrid(session->selectedGridUuid)) result.missingSelectedGridUuid = session->selectedGridUuid;
    }
    return result;
}
} // namespace nijigenerate::viewport::depth::draw::manifest
