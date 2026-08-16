module nijigenerate.viewport.depth.draw.manifest;

import nijigenerate.io.depthimage : DepthDrawAlphaDepthFocusedRule, DepthImageChannel, DepthImageConvolution,
    ngDepthImageChannelFromString, ngDepthImageChannelName, ngDepthImageConvolutionFromString,
    ngDepthImageConvolutionName, ngNormalizeDepthImageCustomRadius;
import nijigenerate.viewport.depth.draw.binding;
import nijigenerate.viewport.depth.draw.layer;
import nijigenerate.viewport.depth.draw.session;
import nijilive;
import std.conv : to;
import std.exception : enforce;
import std.file : exists, readText, write;
import std.format : format;
import std.json : JSONType, JSONValue, parseJSON;
import std.path : buildPath, isAbsolute;

struct DepthDrawManifestValidation {
    string[] missingSourceLayerIds;
    string[] missingTargetLayerIds;
    string[] missingBindingLayerIds;
    string[] duplicateLayerIds;
    string[] duplicateBindingKeys;
    string missingSelectedLayerId;
    ulong missingSelectedGridUuid;

    bool ok() const {
        return missingSourceLayerIds.length == 0 && missingTargetLayerIds.length == 0 &&
            missingBindingLayerIds.length == 0 && duplicateLayerIds.length == 0 &&
            duplicateBindingKeys.length == 0 &&
            missingSelectedLayerId.length == 0 && missingSelectedGridUuid == 0;
    }
}

private bool stringArrayContains(const(string)[] values, string value) {
    foreach (candidate; values) {
        if (candidate == value) return true;
    }
    return false;
}

private JSONValue vec2ToJson(vec2 value) {
    JSONValue result = JSONValue.emptyArray;
    result.array ~= JSONValue(cast(double)value.x);
    result.array ~= JSONValue(cast(double)value.y);
    return result;
}

private vec2 vec2FromJson(JSONValue value, string name, vec2 fallback = vec2(0, 0)) {
    if (value.type == JSONType.null_) return fallback;
    enforce(value.type == JSONType.array && value.array.length == 2, name ~ " must be [x, y]");
    return vec2(jsonFloat(value.array[0], name ~ "[0]"), jsonFloat(value.array[1], name ~ "[1]"));
}

private JSONValue rectToJson(DepthDrawRect rect) {
    JSONValue result = JSONValue.emptyArray;
    result.array ~= JSONValue(cast(long)rect.left);
    result.array ~= JSONValue(cast(long)rect.top);
    result.array ~= JSONValue(cast(long)rect.width);
    result.array ~= JSONValue(cast(long)rect.height);
    return result;
}

private DepthDrawRect rectFromJson(JSONValue value, string name) {
    enforce(value.type == JSONType.array && value.array.length == 4, name ~ " must be [left, top, width, height]");
    DepthDrawRect rect;
    rect.left = cast(int)jsonFloat(value.array[0], name ~ "[0]");
    rect.top = cast(int)jsonFloat(value.array[1], name ~ "[1]");
    rect.width = cast(int)jsonFloat(value.array[2], name ~ "[2]");
    rect.height = cast(int)jsonFloat(value.array[3], name ~ "[3]");
    return rect;
}

private JSONValue displayToJson(DepthDrawDisplayOptions display) {
    JSONValue[string] item;
    item["showNormalImage"] = JSONValue(display.showNormalImage);
    item["showRawDepth"] = JSONValue(display.showRawDepth);
    item["showCoverage"] = JSONValue(display.showCoverage);
    item["showComposite"] = JSONValue(display.showComposite);
    item["showLayerPlanes"] = JSONValue(display.showLayerPlanes);
    item["showDepthRanges"] = JSONValue(display.showDepthRanges);
    item["showMissingVertices"] = JSONValue(display.showMissingVertices);
    item["showWinningLayer"] = JSONValue(display.showWinningLayer);
    item["useGpuPreview"] = JSONValue(display.useGpuPreview);
    return JSONValue(item);
}

private DepthDrawDisplayOptions displayFromJson(JSONValue value, string name) {
    DepthDrawDisplayOptions display;
    if (value.type == JSONType.null_) return display;
    enforce(value.type == JSONType.object, name ~ " must be an object");
    auto object = value.object;
    display.showNormalImage = jsonBool(object.get("showNormalImage", JSONValue(display.showNormalImage)),
        name ~ ".showNormalImage", display.showNormalImage);
    display.showRawDepth = jsonBool(object.get("showRawDepth", JSONValue(display.showRawDepth)),
        name ~ ".showRawDepth", display.showRawDepth);
    display.showCoverage = jsonBool(object.get("showCoverage", JSONValue(display.showCoverage)),
        name ~ ".showCoverage", display.showCoverage);
    display.showComposite = jsonBool(object.get("showComposite", JSONValue(display.showComposite)),
        name ~ ".showComposite", display.showComposite);
    display.showLayerPlanes = jsonBool(object.get("showLayerPlanes", JSONValue(display.showLayerPlanes)),
        name ~ ".showLayerPlanes", display.showLayerPlanes);
    display.showDepthRanges = jsonBool(object.get("showDepthRanges", JSONValue(display.showDepthRanges)),
        name ~ ".showDepthRanges", display.showDepthRanges);
    display.showMissingVertices = jsonBool(object.get("showMissingVertices", JSONValue(display.showMissingVertices)),
        name ~ ".showMissingVertices", display.showMissingVertices);
    display.showWinningLayer = jsonBool(object.get("showWinningLayer", JSONValue(display.showWinningLayer)),
        name ~ ".showWinningLayer", display.showWinningLayer);
    display.useGpuPreview = jsonBool(object.get("useGpuPreview", JSONValue(display.useGpuPreview)),
        name ~ ".useGpuPreview", display.useGpuPreview);
    return display;
}

private float jsonFloat(JSONValue value, string name, float fallback = 0.0f) {
    if (value.type == JSONType.null_) return fallback;
    final switch (value.type) {
        case JSONType.float_:
            return cast(float)value.floating;
        case JSONType.integer:
            return cast(float)value.integer;
        case JSONType.uinteger:
            return cast(float)value.uinteger;
        case JSONType.null_:
            return fallback;
        case JSONType.object:
        case JSONType.array:
        case JSONType.string:
        case JSONType.true_:
        case JSONType.false_:
            enforce(false, name ~ " must be a number");
    }
    assert(0);
}

private bool jsonBool(JSONValue value, string name, bool fallback = false) {
    if (value.type == JSONType.null_) return fallback;
    final switch (value.type) {
        case JSONType.true_:
            return true;
        case JSONType.false_:
            return false;
        case JSONType.null_:
            return fallback;
        case JSONType.float_:
        case JSONType.integer:
        case JSONType.uinteger:
        case JSONType.object:
        case JSONType.array:
        case JSONType.string:
            enforce(false, name ~ " must be a bool");
    }
    assert(0);
}

private string jsonString(JSONValue value, string name, string fallback = null) {
    if (value.type == JSONType.null_) return fallback;
    enforce(value.type == JSONType.string, name ~ " must be a string");
    return value.str;
}

private string mergePolicyName(DepthMergePolicy policy) {
    final switch (policy) {
        case DepthMergePolicy.Replace: return "Replace";
        case DepthMergePolicy.Frontmost: return "Frontmost";
        case DepthMergePolicy.Backmost: return "Backmost";
        case DepthMergePolicy.Add: return "Add";
        case DepthMergePolicy.KeepExistingWhereMissing: return "KeepExistingWhereMissing";
    }
}

private DepthMergePolicy mergePolicyFromString(string value) {
    switch (value) {
        case "Replace": return DepthMergePolicy.Replace;
        case "Frontmost": return DepthMergePolicy.Frontmost;
        case "Backmost": return DepthMergePolicy.Backmost;
        case "Add": return DepthMergePolicy.Add;
        case "KeepExistingWhereMissing": return DepthMergePolicy.KeepExistingWhereMissing;
        default: throw new Exception("Unknown DepthDraw merge policy: " ~ value);
    }
}

private string cleanupKindName(DepthDrawLayerCleanupKind kind) {
    final switch (kind) {
        case DepthDrawLayerCleanupKind.AlphaDepthGapFill: return "AlphaDepthGapFill";
        case DepthDrawLayerCleanupKind.ContourRepair: return "ContourRepair";
    }
}

private DepthDrawLayerCleanupKind cleanupKindFromString(string value) {
    switch (value) {
        case "AlphaDepthGapFill": return DepthDrawLayerCleanupKind.AlphaDepthGapFill;
        case "ContourRepair": return DepthDrawLayerCleanupKind.ContourRepair;
        default: throw new Exception("Unknown DepthDraw cleanup operation: " ~ value);
    }
}

private JSONValue cleanupOperationsToJson(const(DepthDrawLayerCleanupOperation)[] operations) {
    JSONValue result = JSONValue.emptyArray;
    foreach (operation; operations) {
        JSONValue[string] item;
        item["kind"] = JSONValue(cleanupKindName(operation.kind));
        item["contourThickness"] = JSONValue(cast(long)operation.contourThickness);
        JSONValue rules = JSONValue.emptyArray;
        foreach (rule; operation.focusedRules) {
            JSONValue[string] ruleItem;
            ruleItem["layerIndex"] = JSONValue(cast(long)rule.layerIndex);
            ruleItem["x"] = JSONValue(cast(long)rule.x);
            ruleItem["y"] = JSONValue(cast(long)rule.y);
            ruleItem["w"] = JSONValue(cast(long)rule.w);
            ruleItem["h"] = JSONValue(cast(long)rule.h);
            ruleItem["lift"] = JSONValue(cast(long)rule.lift);
            ruleItem["radius"] = JSONValue(cast(long)rule.radius);
            rules.array ~= JSONValue(ruleItem);
        }
        item["focusedRules"] = rules;
        result.array ~= JSONValue(item);
    }
    return result;
}

private int jsonBoundedInt(JSONValue value, string name, int minimum, int maximum, int fallback = 0) {
    auto number = jsonFloat(value, name, fallback);
    if (number <= minimum) return minimum;
    if (number >= maximum) return maximum;
    return cast(int)number;
}

private DepthDrawLayerCleanupOperation[] cleanupOperationsFromJson(
    JSONValue value,
    string name,
    int layerWidth,
    int layerHeight,
) {
    if (value.type == JSONType.null_) return null;
    enforce(value.type == JSONType.array, name ~ " must be an array");
    DepthDrawLayerCleanupOperation[] result;
    foreach (i, entry; value.array) {
        auto itemName = "%s[%s]".format(name, i);
        enforce(entry.type == JSONType.object, itemName ~ " must be an object");
        auto object = entry.object;
        DepthDrawLayerCleanupOperation operation;
        operation.kind = cleanupKindFromString(jsonString(object.get("kind", JSONValue(null)), itemName ~ ".kind"));
        auto contourThickness = jsonFloat(
            object.get("contourThickness", JSONValue(2)), itemName ~ ".contourThickness", 2);
        operation.contourThickness = contourThickness >= DepthDrawMaxContourThickness
            ? DepthDrawMaxContourThickness
            : ngNormalizeDepthDrawContourThickness(cast(int)contourThickness);
        auto rulesValue = object.get("focusedRules", JSONValue.emptyArray);
        enforce(rulesValue.type == JSONType.array, itemName ~ ".focusedRules must be an array");
        foreach (ruleIndex, ruleValue; rulesValue.array) {
            auto ruleName = "%s.focusedRules[%s]".format(itemName, ruleIndex);
            enforce(ruleValue.type == JSONType.object, ruleName ~ " must be an object");
            auto ruleObject = ruleValue.object;
            DepthDrawAlphaDepthFocusedRule rule;
            auto maxX = layerWidth > 0 ? layerWidth : int.max;
            auto maxY = layerHeight > 0 ? layerHeight : int.max;
            rule.layerIndex = jsonBoundedInt(
                ruleObject.get("layerIndex", JSONValue(0)), ruleName ~ ".layerIndex", 0, int.max);
            rule.x = jsonBoundedInt(ruleObject.get("x", JSONValue(0)), ruleName ~ ".x", 0, maxX);
            rule.y = jsonBoundedInt(ruleObject.get("y", JSONValue(0)), ruleName ~ ".y", 0, maxY);
            rule.w = jsonBoundedInt(ruleObject.get("w", JSONValue(0)), ruleName ~ ".w", 0, maxX);
            rule.h = jsonBoundedInt(ruleObject.get("h", JSONValue(0)), ruleName ~ ".h", 0, maxY);
            rule.lift = jsonBoundedInt(ruleObject.get("lift", JSONValue(0)), ruleName ~ ".lift", 0, 255);
            rule.radius = jsonBoundedInt(ruleObject.get("radius", JSONValue(0)), ruleName ~ ".radius",
                0, DepthDrawMaxFocusedRuleRadius);
            if (layerWidth > 0 && layerHeight > 0)
                rule = ngNormalizeDepthDrawFocusedRule(rule, layerWidth, layerHeight);
            operation.focusedRules ~= rule;
        }
        result ~= operation;
    }
    return result;
}

JSONValue ngDepthDrawSessionToManifest(DepthDrawSession session) {
    JSONValue[string] root;
    root["version"] = JSONValue(1L);
    if (session !is null) {
        root["selectedLayerId"] = JSONValue(session.selectedLayerId);
        root["selectedGridUuid"] = JSONValue(session.selectedGridUuid.to!string);
        root["display"] = displayToJson(session.display);
        root["sourceIdentity"] = JSONValue(session.sourceIdentity);
        root["documentWidth"] = JSONValue(cast(long)session.documentWidth);
        root["documentHeight"] = JSONValue(cast(long)session.documentHeight);
    } else {
        root["selectedLayerId"] = JSONValue("");
        root["selectedGridUuid"] = JSONValue("0");
        root["display"] = displayToJson(DepthDrawDisplayOptions());
        root["sourceIdentity"] = JSONValue("");
        root["documentWidth"] = JSONValue(0L);
        root["documentHeight"] = JSONValue(0L);
    }

    JSONValue layers = JSONValue.emptyArray;
    if (session !is null) {
        foreach (layer; session.layers) {
            JSONValue[string] item;
            item["id"] = JSONValue(layer.id);
            item["sourcePath"] = JSONValue(layer.sourcePath);
            item["layerPath"] = JSONValue(layer.layerPath);
            item["displayName"] = JSONValue(layer.displayName);
            item["width"] = JSONValue(cast(long)layer.width);
            item["height"] = JSONValue(cast(long)layer.height);
            item["bounds"] = rectToJson(layer.bounds);
            item["opacity"] = JSONValue(cast(double)layer.opacity);
            item["visible"] = JSONValue(layer.visible);
            item["enabled"] = JSONValue(layer.enabled);
            item["xyOffset"] = vec2ToJson(layer.xyOffset);
            item["xyScale"] = vec2ToJson(layer.xyScale);
            item["zOffset"] = JSONValue(cast(double)layer.zOffset);
            item["zScale"] = JSONValue(cast(double)layer.zScale);
            item["backDepth"] = JSONValue(cast(double)layer.backDepth);
            item["frontDepth"] = JSONValue(cast(double)layer.frontDepth);
            item["sampleDepthScale"] = JSONValue(cast(double)layer.sampleDepthScale);
            item["invert"] = JSONValue(layer.invert);
            item["channel"] = JSONValue(ngDepthImageChannelName(layer.channel));
            item["convolution"] = JSONValue(ngDepthImageConvolutionName(layer.convolution));
            item["customRadius"] = JSONValue(cast(long)layer.customRadius);
            item["alphaThreshold"] = JSONValue(cast(double)layer.alphaThreshold);
            item["cleanupOperations"] = cleanupOperationsToJson(layer.cleanupOperations);
            layers.array ~= JSONValue(item);
        }
    }
    root["layers"] = layers;

    JSONValue bindings = JSONValue.emptyArray;
    if (session !is null) {
        foreach (binding; session.bindings) {
            JSONValue[string] item;
            item["layerId"] = JSONValue(binding.layerId);
            item["targetNodeUuid"] = JSONValue(binding.targetNodeUuid.to!string);
            item["targetGridUuid"] = JSONValue(binding.targetGridUuid.to!string);
            item["order"] = JSONValue(cast(long)binding.order);
            item["enabled"] = JSONValue(binding.enabled);
            item["useNormalLayerAlpha"] = JSONValue(binding.useNormalLayerAlpha);
            item["coverageThreshold"] = JSONValue(cast(double)binding.coverageThreshold);
            item["mergePolicy"] = JSONValue(mergePolicyName(binding.mergePolicy));
            bindings.array ~= JSONValue(item);
        }
    }
    root["bindings"] = bindings;
    return JSONValue(root);
}

DepthDrawSession ngDepthDrawSessionFromManifest(JSONValue manifest) {
    enforce(manifest.type == JSONType.object, "DepthDraw manifest must be an object");
    auto session = new DepthDrawSession();
    session.selectedLayerId = jsonString(manifest.object.get("selectedLayerId", JSONValue("")), "selectedLayerId", "");
    session.selectedGridUuid = jsonString(manifest.object.get("selectedGridUuid", JSONValue("0")),
        "selectedGridUuid", "0").to!ulong;
    session.display = displayFromJson(manifest.object.get("display", JSONValue(null)), "display");
    session.sourceIdentity = jsonString(
        manifest.object.get("sourceIdentity", JSONValue("")), "sourceIdentity", "");
    session.documentWidth = cast(int)jsonFloat(
        manifest.object.get("documentWidth", JSONValue(0)), "documentWidth");
    session.documentHeight = cast(int)jsonFloat(
        manifest.object.get("documentHeight", JSONValue(0)), "documentHeight");

    auto layersValue = manifest.object.get("layers", JSONValue.emptyArray);
    enforce(layersValue.type == JSONType.array, "DepthDraw manifest layers must be an array");
    foreach (entry; layersValue.array) {
        enforce(entry.type == JSONType.object, "DepthDraw manifest layer must be an object");
        auto object = entry.object;
        DepthDrawLayer layer;
        layer.id = jsonString(object.get("id", JSONValue(null)), "layer.id");
        layer.sourcePath = jsonString(object.get("sourcePath", JSONValue(null)), "layer.sourcePath");
        layer.layerPath = jsonString(object.get("layerPath", JSONValue(null)), "layer.layerPath");
        layer.displayName = jsonString(object.get("displayName", JSONValue(null)), "layer.displayName");
        layer.width = cast(int)jsonFloat(object.get("width", JSONValue(0)), "layer.width");
        layer.height = cast(int)jsonFloat(object.get("height", JSONValue(0)), "layer.height");
        layer.bounds = rectFromJson(object.get("bounds", rectToJson(DepthDrawRect(0, 0, layer.width, layer.height))), "layer.bounds");
        layer.opacity = jsonFloat(object.get("opacity", JSONValue(1.0)), "layer.opacity");
        layer.visible = jsonBool(object.get("visible", JSONValue(true)), "layer.visible", true);
        layer.enabled = jsonBool(object.get("enabled", JSONValue(true)), "layer.enabled", true);
        layer.xyOffset = vec2FromJson(object.get("xyOffset", JSONValue(null)), "layer.xyOffset", vec2(0, 0));
        layer.xyScale = vec2FromJson(object.get("xyScale", JSONValue(null)), "layer.xyScale", vec2(1, 1));
        layer.zOffset = jsonFloat(object.get("zOffset", JSONValue(0.0)), "layer.zOffset");
        layer.zScale = jsonFloat(object.get("zScale", JSONValue(1.0)), "layer.zScale");
        layer.backDepth = jsonFloat(object.get("backDepth", JSONValue(-1.0)), "layer.backDepth");
        layer.frontDepth = jsonFloat(object.get("frontDepth", JSONValue(1.0)), "layer.frontDepth");
        layer.sampleDepthScale = jsonFloat(
            object.get("sampleDepthScale", JSONValue(1.0)), "layer.sampleDepthScale");
        layer.invert = jsonBool(object.get("invert", JSONValue(false)), "layer.invert");
        layer.channel = ngDepthImageChannelFromString(
            jsonString(object.get("channel", JSONValue("AverageRGB")), "layer.channel"));
        layer.convolution = ngDepthImageConvolutionFromString(
            jsonString(object.get("convolution", JSONValue("Gaussian3x3")), "layer.convolution"));
        layer.customRadius = ngNormalizeDepthImageCustomRadius(
            cast(int)jsonFloat(object.get("customRadius", JSONValue(3)), "layer.customRadius"));
        layer.alphaThreshold = jsonFloat(object.get("alphaThreshold", JSONValue(0.01)), "layer.alphaThreshold");
        layer.cleanupOperations = cleanupOperationsFromJson(
            object.get("cleanupOperations", JSONValue.emptyArray), "layer.cleanupOperations",
            layer.width, layer.height);
        session.layers ~= layer;
    }

    auto bindingsValue = manifest.object.get("bindings", JSONValue.emptyArray);
    enforce(bindingsValue.type == JSONType.array, "DepthDraw manifest bindings must be an array");
    foreach (entry; bindingsValue.array) {
        enforce(entry.type == JSONType.object, "DepthDraw manifest binding must be an object");
        auto object = entry.object;
        DepthDrawBinding binding;
        binding.layerId = jsonString(object.get("layerId", JSONValue(null)), "binding.layerId");
        binding.targetNodeUuid = jsonString(object.get("targetNodeUuid", JSONValue("0")), "binding.targetNodeUuid").to!ulong;
        binding.targetGridUuid = jsonString(object.get("targetGridUuid", JSONValue("0")), "binding.targetGridUuid").to!ulong;
        binding.order = cast(int)jsonFloat(object.get("order", JSONValue(0)), "binding.order");
        binding.enabled = jsonBool(object.get("enabled", JSONValue(true)), "binding.enabled", true);
        binding.useNormalLayerAlpha = jsonBool(
            object.get("useNormalLayerAlpha", JSONValue(true)), "binding.useNormalLayerAlpha", true);
        binding.coverageThreshold = jsonFloat(object.get("coverageThreshold", JSONValue(0.01)), "binding.coverageThreshold");
        binding.mergePolicy = mergePolicyFromString(
            jsonString(object.get("mergePolicy", JSONValue("Frontmost")), "binding.mergePolicy"));
        session.bindings ~= binding;
    }
    session.normalizeGpuPreviewSampling();
    return session;
}

void ngSaveDepthDrawManifest(DepthDrawSession session, string path) {
    write(path, ngDepthDrawSessionToManifest(session).toString());
}

DepthDrawSession ngLoadDepthDrawManifest(string path) {
    return ngDepthDrawSessionFromManifest(parseJSON(readText(path)));
}

private string resolveManifestSourcePath(string sourcePath, string sourceBaseDir) {
    if (sourcePath.length == 0 || sourcePath.isAbsolute || sourceBaseDir.length == 0) return sourcePath;
    return buildPath(sourceBaseDir, sourcePath);
}

DepthDrawManifestValidation ngValidateDepthDrawSessionManifest(
    DepthDrawSession session,
    bool delegate(ulong gridUuid) hasTargetGrid = null,
    string sourceBaseDir = null
) {
    DepthDrawManifestValidation result;
    if (session is null) return result;

    string[] seenLayerIds;
    foreach (layer; session.layers) {
        if (layer.id.length > 0) {
            if (stringArrayContains(seenLayerIds, layer.id)) {
                if (!stringArrayContains(result.duplicateLayerIds, layer.id)) result.duplicateLayerIds ~= layer.id;
            } else {
                seenLayerIds ~= layer.id;
            }
        }

        auto sourcePath = resolveManifestSourcePath(layer.sourcePath, sourceBaseDir);
        if (sourcePath.length > 0 && !exists(sourcePath)) {
            result.missingSourceLayerIds ~= layer.id;
        }
    }

    if (session.selectedLayerId.length > 0 && session.layerById(session.selectedLayerId) is null) {
        result.missingSelectedLayerId = session.selectedLayerId;
    }

    string[] seenBindingKeys;
    foreach (binding; session.bindings) {
        if (!binding.enabled) continue;
        auto bindingKey = "%s\0%s".format(binding.layerId, binding.targetGridUuid);
        if (stringArrayContains(seenBindingKeys, bindingKey)) {
            if (!stringArrayContains(result.duplicateBindingKeys, bindingKey)) {
                result.duplicateBindingKeys ~= bindingKey;
            }
        } else {
            seenBindingKeys ~= bindingKey;
        }
        if (binding.layerId.length == 0 || session.layerById(binding.layerId) is null) {
            result.missingBindingLayerIds ~= binding.layerId;
        }
        if (binding.targetGridUuid == 0) {
            result.missingTargetLayerIds ~= binding.layerId;
            continue;
        }
        if (hasTargetGrid !is null && !hasTargetGrid(binding.targetGridUuid)) {
            result.missingTargetLayerIds ~= binding.layerId;
        }
    }

    if (session.selectedGridUuid != 0) {
        if (!session.hasTargetGrid(session.selectedGridUuid)) {
            result.missingSelectedGridUuid = session.selectedGridUuid;
        } else if (hasTargetGrid !is null && !hasTargetGrid(session.selectedGridUuid)) {
            result.missingSelectedGridUuid = session.selectedGridUuid;
        }
    }

    return result;
}
