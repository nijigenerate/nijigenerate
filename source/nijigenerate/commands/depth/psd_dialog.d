module nijigenerate.commands.depth.psd_dialog;

import i18n;
import std.conv : to;
import std.json : JSONType, JSONValue;
import nijigenerate.actions : Action;
import nijigenerate.commands.base;
import nijigenerate.core.actionstack : incActionPush;
import nijigenerate.io.depthmap_psd : PsdDepthChannel, PsdDepthConvolution, PsdDepthMissingPolicy;
import nijilive : Node;
import nijigenerate.windows.psddepthmap : PSDDepthMapWindow, PsdDepthDialogLayerPixels,
    PsdDepthDialogLayerState, PsdDepthDialogOverallPreview, PsdDepthDialogPartData,
    PsdDepthDialogPartLayerData, PsdDepthDialogSettingsState, ngActivePsdDepthMapWindow;

enum PsdDepthDialogCommand {
    InspectPsdDepthDialog,
    GetPsdDepthDialogPartData,
    SetPsdDepthDialogColorSource,
    SetPsdDepthDialogInvert,
    SetPsdDepthDialogBackDepth,
    SetPsdDepthDialogFrontDepth,
    SetPsdDepthDialogDepthScale,
    SetPsdDepthDialogChannel,
    SetPsdDepthDialogSampling,
    SetPsdDepthDialogCustomRadius,
    SetPsdDepthDialogAlphaThreshold,
    SetPsdDepthDialogMissingPolicy,
    SetPsdDepthDialogContourRepair,
    SetPsdDepthDialogSurfaceSmoothing,
    FillPsdDepthDialogAlphaDepthGaps,
    SetPsdDepthDialogGpuComposition,
    SetPsdDepthDialogDirectGridMatch,
    SetPsdDepthDialogProblemFilter,
    SetPsdDepthDialogLayerMappingAuto,
    SetPsdDepthDialogLayerMappingIgnored,
    SetPsdDepthDialogLayerMappingTarget,
    SetPsdDepthDialogTargetEnabled,
    SetPsdDepthDialogLayerEnabled,
    SetPsdDepthDialogLayerVisible,
    SetPsdDepthDialogLayerDepthEnabled,
    SetPsdDepthDialogLayerDepthInverted,
    SetPsdDepthDialogLayerDepthScale,
    SetPsdDepthDialogLayerDepthOffset,
    ResetPsdDepthDialogLayerDepthTransform,
    ApplyPsdDepthDialog,
    CancelPsdDepthDialog,
}

final class PsdDepthDialogCommandScope : CommandScope {
    override string id() const {
        return "psd-depth-dialog";
    }

    override string description() const {
        return "PSD depth import dialog";
    }

    override CommandScopeTransition[] completionCommands() const {
        return [CommandScopeTransition(
            ngCommandIdFromKey(PsdDepthDialogCommand.ApplyPsdDepthDialog),
            "Apply the PSD depth import and close the dialog"
        )];
    }

    override CommandScopeTransition[] cancellationCommands() const {
        return [CommandScopeTransition(
            ngCommandIdFromKey(PsdDepthDialogCommand.CancelPsdDepthDialog),
            "Discard dialog changes and close the dialog"
        )];
    }
}

Command[PsdDepthDialogCommand] commands;

private PSDDepthMapWindow contextDialog(Context ctx) {
    return ngActivePsdDepthMapWindow();
}

@ShortcutHidden
@CommandScopes!(PsdDepthDialogCommandScope)()
class InspectPsdDepthDialogCommand : ExCommand!() {
    this() {
        super(
            _("Inspect PSD Depth Dialog"),
            _("Read the settings, source layers, and target UUIDs of the displayed PSD depth dialog.")
        );
    }

    override bool runnable(Context ctx) {
        auto dialog = contextDialog(ctx);
        return dialog !is null && dialog.dialogCommandsAvailable();
    }

    override ExCommandResult!JSONValue run(Context ctx) {
        if (!runnable(ctx)) {
            return ExCommandResult!JSONValue(
                false,
                JSONValue(null),
                "PSD depth import dialog is not displayed"
            );
        }

        auto state = contextDialog(ctx).captureDialogSettingsState();
        JSONValue[string] settings;
        settings["colorSourcePath"] = JSONValue(state.settings.colorSourcePath);
        settings["invert"] = JSONValue(state.settings.invert);
        settings["backDepth"] = JSONValue(state.settings.backDepth);
        settings["frontDepth"] = JSONValue(state.settings.frontDepth);
        settings["depthScale"] = JSONValue(state.settings.depthScale);
        settings["channel"] = JSONValue(state.settings.channel.to!string);
        settings["sampling"] = JSONValue(state.settings.convolution.to!string);
        settings["customRadius"] = JSONValue(state.settings.customRadius);
        settings["alphaThreshold"] = JSONValue(state.settings.alphaThreshold);
        settings["missingPolicy"] = JSONValue(state.settings.missingPolicy.to!string);
        settings["contourRepair"] = JSONValue(state.settings.repairContourBand);
        settings["surfaceSmoothing"] = JSONValue(state.settings.smoothWavySurface);
        settings["gpuComposition"] = JSONValue(state.settings.useGpuComposition);
        settings["directGridMatch"] = JSONValue(state.settings.matchDirectGridName);
        settings["problemFilter"] = JSONValue(state.onlyProblemLayers);

        JSONValue[] layers;
        foreach (layer; state.layers) {
            JSONValue[string] entry;
            entry["layerPath"] = JSONValue(layer.layerPath);
            entry["targetGridUuid"] = JSONValue(layer.targetGridUuid);
            entry["visible"] = JSONValue(layer.visible);
            entry["enabled"] = JSONValue(layer.enabled);
            entry["depthEnabled"] = JSONValue(layer.depthEnabled);
            entry["depthInverted"] = JSONValue(layer.invert);
            entry["depthOffset"] = JSONValue(layer.depthOffset);
            entry["depthScale"] = JSONValue(layer.depthScale);
            entry["outlierPruneEnabled"] = JSONValue(layer.outlierPruneEnabled);
            entry["puppetFitEnabled"] = JSONValue(layer.puppetFitEnabled);
            layers ~= JSONValue(entry);
        }

        JSONValue[string] result;
        result["settings"] = JSONValue(settings);
        result["layers"] = JSONValue(layers);
        return ExCommandResult!JSONValue(true, JSONValue(result));
    }
}

private JSONValue finiteFloatToJson(float value) {
    import std.math : isFinite;

    return value.isFinite ? JSONValue(cast(double)value) : JSONValue(null);
}

private JSONValue floatArrayToJson(const(float)[] values) {
    JSONValue result = JSONValue.emptyArray;
    foreach (value; values) result.array ~= finiteFloatToJson(value);
    return result;
}

private JSONValue stringArrayToJson(const(string)[] values) {
    JSONValue result = JSONValue.emptyArray;
    foreach (value; values) result.array ~= JSONValue(value);
    return result;
}

private JSONValue boolArrayToJson(const(bool)[] values) {
    JSONValue result = JSONValue.emptyArray;
    foreach (value; values) result.array ~= JSONValue(value);
    return result;
}

private JSONValue vertexPositionsToJson(ref PsdDepthDialogPartData part) {
    JSONValue result = JSONValue.emptyArray;
    foreach (i; 0 .. part.vertexX.length) {
        if (i >= part.vertexY.length) break;
        result.array ~= JSONValue([
            finiteFloatToJson(part.vertexX[i]),
            finiteFloatToJson(part.vertexY[i]),
        ]);
    }
    return result;
}

private JSONValue colorStatisticsToJson(ref PsdDepthDialogPartLayerData layer) {
    ulong[4] sums;
    size_t pixels;
    foreach (i; 0 .. layer.colorRgba.length / 4) {
        auto offset = i * 4;
        if (layer.colorRgba[offset + 3] == 0) continue;
        foreach (channel; 0 .. 4) sums[channel] += layer.colorRgba[offset + channel];
        pixels++;
    }

    JSONValue average = JSONValue.emptyArray;
    foreach (sum; sums) {
        average.array ~= JSONValue(cast(long)(pixels ? (sum + pixels / 2) / pixels : 0));
    }
    JSONValue[string] result;
    result["nonTransparentPixels"] = JSONValue(cast(long)pixels);
    result["averageRgba8"] = average;
    return JSONValue(result);
}

private bool appendPngContent(
    ref JSONValue[] content,
    const(ubyte)[] rgba,
    int width,
    int height,
    out long contentIndex,
    out string error
) {
    import core.stdc.stdlib : free;
    import imagefmt : IF_ERROR, IF_PNG, write_image_mem;
    import std.base64 : Base64;

    contentIndex = -1;
    error = null;
    if (width <= 0 || height <= 0 ||
        rgba.length != cast(size_t)width * cast(size_t)height * 4) return true;

    int encodeError;
    ubyte[] pngData = write_image_mem(IF_PNG, width, height, rgba, 4, encodeError);
    if (encodeError) {
        error = "PNG encode failed: " ~ IF_ERROR[encodeError].to!string;
        return false;
    }
    scope(exit) free(pngData.ptr);

    JSONValue[string] image;
    image["type"] = JSONValue("image");
    image["mimeType"] = JSONValue("image/png");
    image["data"] = JSONValue(Base64.encode(cast(const(ubyte)[])pngData));
    contentIndex = cast(long)content.length;
    content ~= JSONValue(image);
    return true;
}

private bool appendFloat32ResourceContent(
    ref JSONValue[] content,
    const(float)[] values,
    string uri,
    out long contentIndex
) {
    import core.stdc.string : memcpy;
    import std.base64 : Base64;

    contentIndex = -1;
    if (values.length == 0) return true;

    ubyte[] bytes;
    bytes.length = values.length * float.sizeof;
    foreach (i, value; values) {
        uint bits;
        memcpy(&bits, &value, float.sizeof);
        auto offset = i * float.sizeof;
        bytes[offset + 0] = cast(ubyte)(bits & 0xFF);
        bytes[offset + 1] = cast(ubyte)((bits >> 8) & 0xFF);
        bytes[offset + 2] = cast(ubyte)((bits >> 16) & 0xFF);
        bytes[offset + 3] = cast(ubyte)((bits >> 24) & 0xFF);
    }

    JSONValue[string] resource;
    resource["uri"] = JSONValue(uri);
    resource["mimeType"] = JSONValue("application/vnd.nijigenerate.depth-map.f32le");
    resource["blob"] = JSONValue(Base64.encode(cast(const(ubyte)[])bytes));

    JSONValue[string] item;
    item["type"] = JSONValue("resource");
    item["resource"] = JSONValue(resource);
    contentIndex = cast(long)content.length;
    content ~= JSONValue(item);
    return true;
}

private JSONValue imageReference(
    long contentIndex,
    int width,
    int height,
    int left = 0,
    int top = 0
) {
    JSONValue[string] result;
    result["available"] = JSONValue(contentIndex >= 0);
    result["contentIndex"] = JSONValue(contentIndex);
    result["mimeType"] = JSONValue(contentIndex >= 0 ? "image/png" : "");
    result["width"] = JSONValue(width);
    result["height"] = JSONValue(height);
    result["left"] = JSONValue(left);
    result["top"] = JSONValue(top);
    return JSONValue(result);
}

@ShortcutHidden
@CommandScopes!(PsdDepthDialogCommandScope)()
class GetPsdDepthDialogPartDataCommand : ExCommand!() {
    this() {
        super(
            _("Get PSD Depth Dialog Part Data"),
            _("Return composed vertex depths, preview images, and color/depth layer images for " ~
                "the PSD depth dialog parts selected by Context.nodes.")
        );
    }

    override bool runnable(Context ctx) {
        auto dialog = contextDialog(ctx);
        return dialog !is null && dialog.dialogCommandsAvailable();
    }

    override ExCommandResult!JSONValue run(Context ctx) {
        if (!runnable(ctx)) {
            return ExCommandResult!JSONValue(
                false,
                JSONValue(null),
                "PSD depth import dialog is not displayed"
            );
        }

        PsdDepthDialogPartData[] parts;
        string error;
        if (!contextDialog(ctx).captureDialogContextPartData(ctx, parts, error)) {
            return ExCommandResult!JSONValue(false, JSONValue(null), error);
        }

        JSONValue[] content = [JSONValue(null)];
        PsdDepthDialogOverallPreview overallPreview;
        if (!contextDialog(ctx).captureDialogOverallPreview(overallPreview, error)) {
            return ExCommandResult!JSONValue(false, JSONValue(null), error);
        }
        long overallPreviewContentIndex;
        if (!appendPngContent(
            content,
            overallPreview.rgba,
            overallPreview.width,
            overallPreview.height,
            overallPreviewContentIndex,
            error
        )) {
            return ExCommandResult!JSONValue(false, JSONValue(null), error);
        }
        long overallDepthContentIndex;
        appendFloat32ResourceContent(
            content,
            overallPreview.depths,
            "nijigenerate://psd-depth-dialog/overall-depth.f32le",
            overallDepthContentIndex
        );

        JSONValue partEntries = JSONValue.emptyArray;
        foreach (ref part; parts) {
            JSONValue[string] depth;
            depth["values"] = floatArrayToJson(part.depths);
            depth["baseValues"] = floatArrayToJson(part.baseDepths);
            depth["vertexPositions"] = vertexPositionsToJson(part);
            depth["winnerLayerPaths"] = stringArrayToJson(part.winnerLayerPaths);
            depth["missingVertexMask"] = boolArrayToJson(part.missingVertexMask);
            depth["sampledVertices"] = JSONValue(cast(long)part.sampledVertices);
            depth["missingVertices"] = JSONValue(cast(long)part.missingVertices);
            depth["min"] = finiteFloatToJson(part.minDepth);
            depth["max"] = finiteFloatToJson(part.maxDepth);

            long previewContentIndex;
            if (!appendPngContent(
                content,
                part.previewRgba,
                part.previewWidth,
                part.previewHeight,
                previewContentIndex,
                error
            )) {
                return ExCommandResult!JSONValue(false, JSONValue(null), error);
            }

            JSONValue layerEntries = JSONValue.emptyArray;
            foreach (ref layer; part.layers) {
                long colorContentIndex;
                if (!appendPngContent(
                    content,
                    layer.colorRgba,
                    layer.width,
                    layer.height,
                    colorContentIndex,
                    error
                )) {
                    return ExCommandResult!JSONValue(false, JSONValue(null), error);
                }
                long depthContentIndex;
                if (!appendPngContent(
                    content,
                    layer.depthRgba,
                    layer.width,
                    layer.height,
                    depthContentIndex,
                    error
                )) {
                    return ExCommandResult!JSONValue(false, JSONValue(null), error);
                }

                JSONValue[string] stats;
                stats["hasDepth"] = JSONValue(layer.depthStats.hasDepth);
                stats["maskedPixels"] = JSONValue(cast(long)layer.depthStats.maskedPixels);
                stats["zeroPixels"] = JSONValue(cast(long)layer.depthStats.zeroPixels);
                stats["minDepth01"] = finiteFloatToJson(layer.depthStats.minDepth01);
                stats["maxDepth01"] = finiteFloatToJson(layer.depthStats.maxDepth01);
                stats["rangeDepth01"] = finiteFloatToJson(layer.depthStats.rangeDepth01);
                stats["adjacentDelta01"] = finiteFloatToJson(layer.depthStats.adjacentDelta01);

                JSONValue[string] layerEntry;
                layerEntry["layerPath"] = JSONValue(layer.layerPath);
                layerEntry["layerName"] = JSONValue(layer.layerName);
                layerEntry["colorLayerPath"] = JSONValue(layer.colorLayerPath);
                layerEntry["colorLayerName"] = JSONValue(layer.colorLayerName);
                layerEntry["sourcePath"] = JSONValue(layer.sourcePath);
                layerEntry["visible"] = JSONValue(layer.visible);
                layerEntry["enabled"] = JSONValue(layer.enabled);
                layerEntry["depthEnabled"] = JSONValue(layer.depthEnabled);
                layerEntry["depthStats"] = JSONValue(stats);
                layerEntry["colorStatistics"] = colorStatisticsToJson(layer);
                layerEntry["colorImage"] = imageReference(
                    colorContentIndex, layer.width, layer.height, layer.left, layer.top);
                layerEntry["depthImage"] = imageReference(
                    depthContentIndex, layer.width, layer.height, layer.left, layer.top);
                layerEntries.array ~= JSONValue(layerEntry);
            }

            JSONValue[string] partEntry;
            partEntry["targetGridUuid"] = JSONValue(part.targetGridUuid);
            partEntry["targetGridName"] = JSONValue(part.targetGridName);
            partEntry["targetType"] = JSONValue(part.targetType);
            partEntry["skipped"] = JSONValue(part.skipped);
            partEntry["documentWidth"] = JSONValue(part.documentWidth);
            partEntry["documentHeight"] = JSONValue(part.documentHeight);
            partEntry["coverageSources"] = JSONValue(cast(long)part.coverageSources);
            partEntry["depth"] = JSONValue(depth);
            partEntry["previewImage"] = imageReference(
                previewContentIndex,
                part.previewWidth,
                part.previewHeight,
                part.previewLeft,
                part.previewTop
            );
            partEntry["layers"] = layerEntries;
            partEntries.array ~= JSONValue(partEntry);
        }

        JSONValue[string] metadata;
        auto overallPreviewImage = imageReference(
            overallPreviewContentIndex,
            overallPreview.width,
            overallPreview.height
        );
        overallPreviewImage.object["yaw"] = finiteFloatToJson(overallPreview.yaw);
        overallPreviewImage.object["pitch"] = finiteFloatToJson(overallPreview.pitch);
        overallPreviewImage.object["zoom"] = finiteFloatToJson(overallPreview.zoom);
        overallPreviewImage.object["pan"] = JSONValue([
            finiteFloatToJson(overallPreview.panX),
            finiteFloatToJson(overallPreview.panY),
        ]);
        metadata["overallPreviewImage"] = overallPreviewImage;
        JSONValue[string] overallDepth;
        overallDepth["available"] = JSONValue(overallDepthContentIndex >= 0);
        overallDepth["contentIndex"] = JSONValue(overallDepthContentIndex);
        overallDepth["mimeType"] = JSONValue(
            overallDepthContentIndex >= 0
                ? "application/vnd.nijigenerate.depth-map.f32le"
                : ""
        );
        overallDepth["width"] = JSONValue(overallPreview.width);
        overallDepth["height"] = JSONValue(overallPreview.height);
        overallDepth["encoding"] = JSONValue("float32-le");
        overallDepth["layout"] = JSONValue("row-major-top-left");
        overallDepth["valueSpace"] = JSONValue("composed-depth");
        overallDepth["missingValue"] = JSONValue("NaN");
        overallDepth["coveredPixels"] = JSONValue(cast(long)overallPreview.coveredPixels);
        overallDepth["min"] = finiteFloatToJson(overallPreview.minDepth);
        overallDepth["max"] = finiteFloatToJson(overallPreview.maxDepth);
        metadata["overallDepth"] = JSONValue(overallDepth);
        metadata["parts"] = partEntries;
        JSONValue[string] textContent;
        textContent["type"] = JSONValue("text");
        textContent["text"] = JSONValue(JSONValue(metadata).toString());
        content[0] = JSONValue(textContent);

        long imageCount;
        long resourceCount;
        foreach (item; content) {
            if (item.type != JSONType.object || !("type" in item.object)) continue;
            if (item["type"].str == "image") imageCount++;
            else if (item["type"].str == "resource") resourceCount++;
        }
        JSONValue[string] meta;
        meta["partCount"] = JSONValue(cast(long)parts.length);
        meta["imageCount"] = JSONValue(imageCount);
        meta["resourceCount"] = JSONValue(resourceCount);

        JSONValue[string] result;
        result["mcpDirectToolResult"] = JSONValue(true);
        result["content"] = JSONValue(content);
        result["_meta"] = JSONValue(meta);
        return ExCommandResult!JSONValue(true, JSONValue(result));
    }
}

private class PsdDepthDialogSettingsChangeAction : Action {
private:
    PSDDepthMapWindow dialog;
    PsdDepthDialogSettingsState oldState;
    PsdDepthDialogSettingsState newState;
    string changeKey;
    string changeLabel;
    bool mergeable;

public:
    this(
        PSDDepthMapWindow dialog,
        PsdDepthDialogSettingsState oldState,
        PsdDepthDialogSettingsState newState,
        string changeKey,
        string changeLabel,
        bool mergeable
    ) {
        this.dialog = dialog;
        this.oldState = oldState;
        this.newState = newState;
        this.changeKey = changeKey;
        this.changeLabel = changeLabel;
        this.mergeable = mergeable;
    }

    void rollback() {
        dialog.applyDialogSettingsState(oldState);
    }

    void redo() {
        dialog.applyDialogSettingsState(newState);
    }

    string describe() {
        return changeLabel;
    }

    string describeUndo() {
        return changeLabel;
    }

    string getName() {
        return "PsdDepthDialogSettingsChangeAction";
    }

    bool merge(Action other) {
        auto action = cast(PsdDepthDialogSettingsChangeAction)other;
        if (action is null || !canMerge(other)) return false;
        newState = action.newState;
        return true;
    }

    bool canMerge(Action other) {
        auto action = cast(PsdDepthDialogSettingsChangeAction)other;
        return mergeable && action !is null && action.mergeable &&
            action.dialog is dialog && action.changeKey == changeKey;
    }
}

private class PsdDepthDialogLayerChangeAction : Action {
private:
    PSDDepthMapWindow dialog;
    PsdDepthDialogLayerState oldState;
    PsdDepthDialogLayerState newState;
    string changeKey;
    string changeLabel;
    bool stageForApply;
    bool mergeable;

public:
    this(
        PSDDepthMapWindow dialog,
        PsdDepthDialogLayerState oldState,
        PsdDepthDialogLayerState newState,
        string changeKey,
        string changeLabel,
        bool stageForApply,
        bool mergeable
    ) {
        this.dialog = dialog;
        this.oldState = oldState;
        this.newState = newState;
        this.changeKey = changeKey;
        this.changeLabel = changeLabel;
        this.stageForApply = stageForApply;
        this.mergeable = mergeable;
    }

    void rollback() {
        dialog.applyDialogLayerState(oldState, stageForApply);
    }

    void redo() {
        dialog.applyDialogLayerState(newState, stageForApply);
    }

    string describe() {
        return changeLabel;
    }

    string describeUndo() {
        return changeLabel;
    }

    string getName() {
        return "PsdDepthDialogLayerChangeAction";
    }

    bool merge(Action other) {
        auto action = cast(PsdDepthDialogLayerChangeAction)other;
        if (action is null || !canMerge(other)) return false;
        newState = action.newState;
        return true;
    }

    bool canMerge(Action other) {
        auto action = cast(PsdDepthDialogLayerChangeAction)other;
        return mergeable && action !is null && action.mergeable &&
            action.dialog is dialog && action.changeKey == changeKey &&
            action.oldState.layerPath == oldState.layerPath &&
            action.oldState.targetGridUuid == oldState.targetGridUuid;
    }
}

private class PsdDepthDialogPixelsChangeAction : Action {
private:
    PSDDepthMapWindow dialog;
    PsdDepthDialogLayerPixels[] oldState;
    PsdDepthDialogLayerPixels[] newState;

public:
    this(
        PSDDepthMapWindow dialog,
        PsdDepthDialogLayerPixels[] oldState,
        PsdDepthDialogLayerPixels[] newState
    ) {
        this.dialog = dialog;
        this.oldState = oldState;
        this.newState = newState;
    }

    void rollback() {
        dialog.applyDialogLayerPixels(oldState);
    }

    void redo() {
        dialog.applyDialogLayerPixels(newState);
    }

    string describe() {
        return _("Filled alpha-depth gaps");
    }

    string describeUndo() {
        return _("Alpha-depth gap fill was reverted");
    }

    string getName() {
        return "PsdDepthDialogPixelsChangeAction";
    }

    bool merge(Action other) {
        return false;
    }

    bool canMerge(Action other) {
        return false;
    }
}

@CommandScopes!(PsdDepthDialogCommandScope)()
private abstract class PsdDepthDialogSettingsValueCommand(T, string valueDescription) :
    ExCommand!(TW!(T, "value", valueDescription)) {
private:
    string changeKey;
    string changeLabel;
    bool mergeable;

protected:
    this(string label, string description, string changeKey, string changeLabel, bool mergeable = false) {
        super(label, description);
        this.changeKey = changeKey;
        this.changeLabel = changeLabel;
        this.mergeable = mergeable;
    }

    abstract void update(ref PsdDepthDialogSettingsState state);

public:
    override bool runnable(Context ctx) {
        auto dialog = contextDialog(ctx);
        return dialog !is null && dialog.dialogCommandsAvailable();
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx)) return CommandResult(false, "PSD depth import dialog is not displayed");
        auto dialog = contextDialog(ctx);
        auto oldState = dialog.captureDialogSettingsState();
        auto nextState = oldState;
        update(nextState);
        if (!dialog.applyDialogSettingsState(nextState)) return CommandResult(true);
        incActionPush(new PsdDepthDialogSettingsChangeAction(
            dialog, oldState, nextState, changeKey, changeLabel, mergeable));
        return CommandResult(true);
    }
}

@ShortcutHidden
class SetPsdDepthDialogColorSourceCommand :
    PsdDepthDialogSettingsValueCommand!(string, "Color source image path; empty uses active target artwork") {
    this() { super(_("Set PSD Depth Color Source"), _("Set the color source used by the displayed PSD depth dialog."),
        "color-source", _("Changed PSD depth color source")); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.colorSourcePath = value; }
}

@ShortcutHidden
class SetPsdDepthDialogInvertCommand :
    PsdDepthDialogSettingsValueCommand!(bool, "Whether white and black depth interpretation is inverted") {
    this() { super(_("Set PSD Depth Inversion"), _("Set global depth inversion in the displayed PSD depth dialog."),
        "invert-depth", _("Changed PSD depth inversion")); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.invert = value; }
}

@ShortcutHidden
class SetPsdDepthDialogBackDepthCommand :
    PsdDepthDialogSettingsValueCommand!(float, "Depth assigned to the back of the imported range") {
    this() { super(_("Set PSD Back Depth"), _("Set the back depth in the displayed PSD depth dialog."),
        "back-depth", _("Changed PSD back depth"), true); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.backDepth = value; }
}

@ShortcutHidden
class SetPsdDepthDialogFrontDepthCommand :
    PsdDepthDialogSettingsValueCommand!(float, "Depth assigned to the front of the imported range") {
    this() { super(_("Set PSD Front Depth"), _("Set the front depth in the displayed PSD depth dialog."),
        "front-depth", _("Changed PSD front depth"), true); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.frontDepth = value; }
}

@ShortcutHidden
class SetPsdDepthDialogDepthScaleCommand :
    PsdDepthDialogSettingsValueCommand!(float, "Multiplier applied to imported depth values") {
    this() { super(_("Set PSD Depth Scale"), _("Set the global depth scale in the displayed PSD depth dialog."),
        "depth-scale", _("Changed PSD depth scale"), true); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.depthScale = value; }
}

@ShortcutHidden
class SetPsdDepthDialogChannelCommand :
    PsdDepthDialogSettingsValueCommand!(PsdDepthChannel, "Image channel used to read depth") {
    this() { super(_("Set PSD Depth Channel"), _("Set the depth channel in the displayed PSD depth dialog."),
        "channel", _("Changed PSD depth channel")); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.channel = value; }
}

@ShortcutHidden
class SetPsdDepthDialogSamplingCommand :
    PsdDepthDialogSettingsValueCommand!(PsdDepthConvolution, "Sampling filter used to read depth") {
    this() { super(_("Set PSD Depth Sampling"), _("Set depth sampling in the displayed PSD depth dialog."),
        "sampling", _("Changed PSD depth sampling")); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.convolution = value; }
}

@ShortcutHidden
class SetPsdDepthDialogCustomRadiusCommand :
    PsdDepthDialogSettingsValueCommand!(int, "Radius used by custom sampling filters") {
    this() { super(_("Set PSD Sampling Radius"), _("Set the custom sampling radius in the displayed PSD depth dialog."),
        "custom-radius", _("Changed PSD depth sampling radius"), true); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.customRadius = value; }
}

@ShortcutHidden
class SetPsdDepthDialogAlphaThresholdCommand :
    PsdDepthDialogSettingsValueCommand!(float, "Minimum alpha accepted as a valid depth sample") {
    this() { super(_("Set PSD Alpha Threshold"), _("Set the alpha threshold in the displayed PSD depth dialog."),
        "alpha-threshold", _("Changed PSD depth alpha threshold"), true); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.alphaThreshold = value; }
}

@ShortcutHidden
class SetPsdDepthDialogMissingPolicyCommand :
    PsdDepthDialogSettingsValueCommand!(PsdDepthMissingPolicy, "Policy for vertices without a valid depth pixel") {
    this() { super(_("Set PSD Missing Pixel Policy"), _("Set missing-pixel handling in the displayed PSD depth dialog."),
        "missing-policy", _("Changed PSD missing depth policy")); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.missingPolicy = value; }
}

@ShortcutHidden
class SetPsdDepthDialogContourRepairCommand :
    PsdDepthDialogSettingsValueCommand!(bool, "Whether the contour band is repaired") {
    this() { super(_("Set PSD Contour Repair"), _("Set contour repair in the displayed PSD depth dialog."),
        "repair-contour", _("Changed PSD contour repair")); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.repairContourBand = value; }
}

@ShortcutHidden
class SetPsdDepthDialogSurfaceSmoothingCommand :
    PsdDepthDialogSettingsValueCommand!(bool, "Whether wavy depth surfaces are smoothed") {
    this() { super(_("Set PSD Surface Smoothing"), _("Set surface smoothing in the displayed PSD depth dialog."),
        "smooth-surface", _("Changed PSD surface smoothing")); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.smoothWavySurface = value; }
}

@ShortcutHidden
class SetPsdDepthDialogGpuCompositionCommand :
    PsdDepthDialogSettingsValueCommand!(bool, "Whether composition must use the GPU path") {
    this() { super(_("Set PSD GPU Composition"), _("Set GPU composition in the displayed PSD depth dialog."),
        "gpu-composition", _("Changed PSD GPU composition")); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.useGpuComposition = value; }
}

@ShortcutHidden
class SetPsdDepthDialogDirectGridMatchCommand :
    PsdDepthDialogSettingsValueCommand!(bool, "Whether layer names may directly match GridDeformer names") {
    this() { super(_("Set PSD Direct Grid Match"), _("Set direct grid-name matching in the displayed PSD depth dialog."),
        "direct-grid-match", _("Changed PSD direct grid matching")); }
    override void update(ref PsdDepthDialogSettingsState state) { state.settings.matchDirectGridName = value; }
}

@ShortcutHidden
class SetPsdDepthDialogProblemFilterCommand :
    PsdDepthDialogSettingsValueCommand!(bool, "Whether Source / Mapping shows only problem layers") {
    this() { super(_("Set PSD Problem Layer Filter"), _("Set the problem-layer filter in the displayed PSD depth dialog."),
        "problem-filter", _("Changed PSD problem layer filter")); }
    override void update(ref PsdDepthDialogSettingsState state) { state.onlyProblemLayers = value; }
}

private CommandResult applyMappingChange(
    Context ctx,
    string layerPath,
    Node target,
    bool ignored,
    bool automatic
) {
    auto dialog = contextDialog(ctx);
    if (dialog is null || !dialog.dialogCommandsAvailable())
        return CommandResult(false, "PSD depth import dialog is not displayed");
    auto oldState = dialog.captureDialogSettingsState();
    auto nextState = dialog.captureDialogSettingsState();
    nextState.settings.ignoredLayerPaths.remove(layerPath);
    nextState.settings.layerTargetGridUuidOverrides.remove(layerPath);
    if (ignored) {
        nextState.settings.ignoredLayerPaths[layerPath] = true;
    } else if (!automatic) {
        if (target is null || !dialog.dialogMappingTargetAvailable(target))
            return CommandResult(false, "PSD depth mapping target is not available");
        nextState.settings.layerTargetGridUuidOverrides[layerPath] = target.uuid.to!string;
    }
    if (!dialog.applyDialogSettingsState(nextState)) return CommandResult(true);
    incActionPush(new PsdDepthDialogSettingsChangeAction(
        dialog, oldState, nextState, "mapping:" ~ layerPath, _("Changed PSD depth layer mapping"), false));
    return CommandResult(true);
}

@ShortcutHidden
@CommandScopes!(PsdDepthDialogCommandScope)()
class SetPsdDepthDialogLayerMappingAutoCommand :
    ExCommand!(TW!(string, "layerPath", "PSD source layer path to return to automatic mapping")) {
    this() { super(_("Use Automatic PSD Layer Mapping"),
        _("Return a Source / Mapping layer to automatic target matching.")); }
    override bool runnable(Context ctx) { auto dialog = contextDialog(ctx); return dialog !is null && dialog.dialogCommandsAvailable(); }
    override CommandResult run(Context ctx) { return applyMappingChange(ctx, layerPath, null, false, true); }
}

@ShortcutHidden
@CommandScopes!(PsdDepthDialogCommandScope)()
class SetPsdDepthDialogLayerMappingIgnoredCommand :
    ExCommand!(TW!(string, "layerPath", "PSD source layer path to ignore")) {
    this() { super(_("Ignore PSD Source Layer"), _("Ignore a layer in Source / Mapping.")); }
    override bool runnable(Context ctx) { auto dialog = contextDialog(ctx); return dialog !is null && dialog.dialogCommandsAvailable(); }
    override CommandResult run(Context ctx) { return applyMappingChange(ctx, layerPath, null, true, false); }
}

@ShortcutHidden
@CommandScopes!(PsdDepthDialogCommandScope)()
class SetPsdDepthDialogLayerMappingTargetCommand : ExCommand!(
    TW!(string, "layerPath", "PSD source layer path to remap"),
    TW!(Node, "target", "GridDeformer or PathDeformer selected as the mapping target")
) {
    this() { super(_("Remap PSD Source Layer"), _("Assign a Source / Mapping layer to a specific target.")); }
    override bool runnable(Context ctx) {
        auto dialog = contextDialog(ctx);
        return dialog !is null && dialog.dialogCommandsAvailable() && dialog.dialogMappingTargetAvailable(target);
    }
    override CommandResult run(Context ctx) { return applyMappingChange(ctx, layerPath, target, false, false); }
}

@ShortcutHidden
@CommandScopes!(PsdDepthDialogCommandScope)()
class SetPsdDepthDialogTargetEnabledCommand :
    ExCommand!(TW!(bool, "value", "Whether the Context target GridDeformer or PathDeformer is used")) {
    this() { super(_("Set PSD Target Enabled"),
        _("Enable or disable the Context target in Source / Mapping.")); }
    override bool runnable(Context ctx) {
        auto dialog = contextDialog(ctx);
        ulong uuid;
        return dialog !is null && dialog.dialogCommandsAvailable() &&
            dialog.dialogContextTargetUuid(ctx, uuid);
    }
    override CommandResult run(Context ctx) {
        auto dialog = contextDialog(ctx);
        ulong uuid;
        if (dialog is null || !dialog.dialogCommandsAvailable() ||
            !dialog.dialogContextTargetUuid(ctx, uuid)) {
            return CommandResult(false, "PSD depth target is not selected in Context");
        }
        auto oldState = dialog.captureDialogSettingsState();
        auto nextState = dialog.captureDialogSettingsState();
        auto key = uuid.to!string;
        if (value) nextState.settings.disabledGridUuids.remove(key);
        else nextState.settings.disabledGridUuids[key] = true;
        if (!dialog.applyDialogSettingsState(nextState)) return CommandResult(true);
        incActionPush(new PsdDepthDialogSettingsChangeAction(
            dialog, oldState, nextState, "target-enabled:" ~ key,
            _("Changed PSD depth target state"), false));
        return CommandResult(true);
    }
}

@CommandScopes!(PsdDepthDialogCommandScope)()
private abstract class PsdDepthDialogLayerValueCommand(T, string valueDescription) :
    ExCommand!(TW!(string, "layerPath", "PSD composed layer path"), TW!(T, "value", valueDescription)) {
private:
    string changeKey;
    string changeLabel;
    bool stageForApply;
    bool mergeable;

protected:
    this(string label, string description, string changeKey, string changeLabel,
        bool stageForApply, bool mergeable = false) {
        super(label, description);
        this.changeKey = changeKey;
        this.changeLabel = changeLabel;
        this.stageForApply = stageForApply;
        this.mergeable = mergeable;
    }
    abstract void update(ref PsdDepthDialogLayerState state);

public:
    override bool runnable(Context ctx) {
        auto dialog = contextDialog(ctx);
        PsdDepthDialogLayerState state;
        return dialog !is null && dialog.dialogCommandsAvailable() &&
            dialog.captureDialogContextLayerState(ctx, layerPath, state);
    }
    override CommandResult run(Context ctx) {
        auto dialog = contextDialog(ctx);
        PsdDepthDialogLayerState oldState;
        if (dialog is null || !dialog.dialogCommandsAvailable() ||
            !dialog.captureDialogContextLayerState(ctx, layerPath, oldState)) {
            return CommandResult(false, "PSD depth dialog layer is not selected in Context");
        }
        auto nextState = oldState;
        update(nextState);
        if (!dialog.applyDialogLayerState(nextState, stageForApply)) return CommandResult(true);
        incActionPush(new PsdDepthDialogLayerChangeAction(
            dialog, oldState, nextState, changeKey ~ ":" ~ layerPath,
            changeLabel, stageForApply, mergeable));
        return CommandResult(true);
    }
}

@ShortcutHidden
class SetPsdDepthDialogLayerEnabledCommand :
    PsdDepthDialogLayerValueCommand!(bool, "Whether the composed layer is used") {
    this() { super(_("Set PSD Layer Enabled"), _("Enable or disable the selected composed layer."),
        "layer-enabled", _("Changed PSD depth layer state"), false); }
    override void update(ref PsdDepthDialogLayerState state) { state.enabled = value; state.visible = value; }
}

@ShortcutHidden
class SetPsdDepthDialogLayerVisibleCommand :
    PsdDepthDialogLayerValueCommand!(bool, "Whether the selected layer is shown in 3D Adjust") {
    this() { super(_("Set PSD Layer Visible"), _("Show or hide the selected layer in 3D Adjust."),
        "layer-visible", _("Changed PSD depth layer visibility"), false); }
    override void update(ref PsdDepthDialogLayerState state) { state.visible = value; }
}

@ShortcutHidden
class SetPsdDepthDialogLayerDepthEnabledCommand :
    PsdDepthDialogLayerValueCommand!(bool, "Whether the selected layer supplies its own depth") {
    this() { super(_("Set PSD Layer Depth Enabled"), _("Enable or attach depth for the selected layer."),
        "layer-depth-enabled", _("Changed PSD layer depth attachment"), false); }
    override void update(ref PsdDepthDialogLayerState state) { state.depthEnabled = value; }
}

@ShortcutHidden
class SetPsdDepthDialogLayerDepthInvertedCommand :
    PsdDepthDialogLayerValueCommand!(bool, "Whether depth is inverted for the selected layer") {
    this() { super(_("Set PSD Layer Depth Inversion"), _("Invert depth for the selected layer."),
        "layer-invert", _("Changed PSD layer depth inversion"), true); }
    override void update(ref PsdDepthDialogLayerState state) { state.invert = value; }
}

@ShortcutHidden
class SetPsdDepthDialogLayerDepthScaleCommand :
    PsdDepthDialogLayerValueCommand!(float, "Z scale applied to the selected layer") {
    this() { super(_("Set PSD Layer Z Scale"), _("Set Z scale for the selected layer."),
        "layer-z-scale", _("Changed PSD layer depth scale"), true, true); }
    override void update(ref PsdDepthDialogLayerState state) { state.depthScale = value; }
}

@ShortcutHidden
class SetPsdDepthDialogLayerDepthOffsetCommand :
    PsdDepthDialogLayerValueCommand!(float, "Z offset applied to the selected layer") {
    this() { super(_("Set PSD Layer Z Offset"), _("Set Z offset for the selected layer."),
        "layer-z-offset", _("Changed PSD layer depth offset"), true, true); }
    override void update(ref PsdDepthDialogLayerState state) { state.depthOffset = value; }
}

@ShortcutHidden
@CommandScopes!(PsdDepthDialogCommandScope)()
class ResetPsdDepthDialogLayerDepthTransformCommand :
    ExCommand!(TW!(string, "layerPath", "PSD composed layer path to reset")) {
    this() { super(_("Reset PSD Layer Z Transform"), _("Reset inversion, Z scale, and Z offset for the selected layer.")); }
    override bool runnable(Context ctx) {
        auto dialog = contextDialog(ctx);
        PsdDepthDialogLayerState state;
        return dialog !is null && dialog.dialogCommandsAvailable() &&
            dialog.captureDialogContextLayerState(ctx, layerPath, state);
    }
    override CommandResult run(Context ctx) {
        auto dialog = contextDialog(ctx);
        PsdDepthDialogLayerState oldState;
        if (dialog is null || !dialog.captureDialogContextLayerState(ctx, layerPath, oldState))
            return CommandResult(false, "PSD depth dialog layer is not selected in Context");
        auto nextState = oldState;
        nextState.invert = false;
        nextState.depthScale = 1.0f;
        nextState.depthOffset = 0.0f;
        if (!dialog.applyDialogLayerState(nextState, true)) return CommandResult(true);
        incActionPush(new PsdDepthDialogLayerChangeAction(
            dialog, oldState, nextState, "layer-z-reset:" ~ layerPath,
            _("Reset PSD layer depth transform"), true, false));
        return CommandResult(true);
    }
}

@ShortcutHidden
@CommandScopes!(PsdDepthDialogCommandScope)()
class FillPsdDepthDialogAlphaDepthGapsCommand : ExCommand!() {
    this() {
        super(_("Fill PSD Depth Dialog Alpha-Depth Gaps"),
            _("Fill alpha-depth gaps in the displayed PSD depth import dialog."));
    }

    override bool runnable(Context ctx) {
        auto dialog = contextDialog(ctx);
        return dialog !is null && dialog.dialogCommandsAvailable();
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx)) return CommandResult(false, "PSD depth import dialog is not displayed");
        auto dialog = contextDialog(ctx);
        auto oldState = dialog.captureDialogLayerPixels();
        if (!dialog.applyDialogAlphaDepthGapFill()) return CommandResult(false, "Alpha-depth gap fill failed");
        auto newState = dialog.captureDialogLayerPixels();
        if (!PSDDepthMapWindow.dialogLayerPixelsEqual(oldState, newState)) {
            incActionPush(new PsdDepthDialogPixelsChangeAction(dialog, oldState, newState));
        }
        return CommandResult(true);
    }
}

@ShortcutHidden
@CommandScopes!(PsdDepthDialogCommandScope)()
class ApplyPsdDepthDialogCommand : ExCommand!() {
    this() {
        super(_("Apply PSD Depth Dialog"), _("Apply the displayed PSD depth import dialog."));
    }

    override bool runnable(Context ctx) {
        auto dialog = contextDialog(ctx);
        return dialog !is null && dialog.dialogCommandsAvailable();
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx)) return CommandResult(false, "PSD depth import dialog is not displayed");
        auto dialog = contextDialog(ctx);
        if (!dialog.applyDialogResult()) return CommandResult(false, "PSD depth import dialog apply failed");
        return CommandResult(true);
    }
}

@ShortcutHidden
@CommandScopes!(PsdDepthDialogCommandScope)()
class CancelPsdDepthDialogCommand : ExCommand!() {
    this() {
        super(_("Cancel PSD Depth Dialog"), _("Cancel the displayed PSD depth import dialog."));
    }

    override bool runnable(Context ctx) {
        auto dialog = contextDialog(ctx);
        return dialog !is null && dialog.dialogCommandsAvailable();
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx)) return CommandResult(false, "PSD depth import dialog is not displayed");
        auto dialog = contextDialog(ctx);
        dialog.cancelDialog();
        return CommandResult(true);
    }
}

void ngInitCommands(T)() if (is(T == PsdDepthDialogCommand)) {
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!PsdDepthDialogCommand) {
        mixin(registerCommand!(name));
    }
}
