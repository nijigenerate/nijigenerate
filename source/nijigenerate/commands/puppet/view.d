module nijigenerate.commands.puppet.view;

import nijigenerate.commands.base;
import nijigenerate.core.window;
import nijilive;
import nijilive.core.diff_collect : DifferenceEvaluationResult;
import tinyfiledialogs;
import nijigenerate.io;
import i18n;
import std.path;
import std.conv : to;
import std.algorithm : max, min;
import nijigenerate.project;
import nijigenerate.core.settings;
import nijilive.math : vec2, vec2u;
static import std.json;
import std.json : JSONType, JSONValue;


@McpHidden
@GuiLayout
@EffectLayoutReset
class SetDefaultLayoutCommand : ExCommand!() {
    this() { super(_("Reset Layout"), _("Set default layout of panels.")); }

    override
    CommandResult run(Context ctx) {
        incSetDefaultLayout();
        return CommandResult(true);
    }
}

@McpHidden
@GuiDialog
class ShowSaveScreenshotDialogCommand : ExCommand!() {
    this() { super(_("Save Screenshot"), _("Shows \"Save screenshot\" dialog.")); }

    override
    CommandResult run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.png"], "PNG Image (*.png)" }
        ];
        
        string filename = incShowSaveDialog(filters, "", _("Save Screenshot..."));
        if (filename) {
            auto cmd = cast(SaveScreenshotCommand)commands[ViewCommand.SaveScreenshot];
            if (cmd) {
                cmd.filename = filename;
                return cmd.run(ctx);
            }
        }
        return CommandResult(false, "Screenshot save canceled");
    }
}

private struct ScreenshotOverlayObject {
    uint uuid;
    string kind;
}

private bool readOverlayUuid(JSONValue value, out uint uuid) {
    import std.conv : to;

    if (value.type == JSONType.integer && value.integer >= 0) {
        uuid = cast(uint)value.integer;
        return true;
    }
    if (value.type == JSONType.string) {
        try {
            uuid = to!uint(value.str);
            return true;
        } catch (Exception) {
            return false;
        }
    }
    return false;
}

private bool parseScreenshotOverlayObjects(JSONValue value, out ScreenshotOverlayObject[] overlays, out string message) {
    if (value.type == JSONType.null_)
        return true;
    if (value.type != JSONType.array) {
        message = "overlayObjects must be an array";
        return false;
    }

    foreach (entry; value.array) {
        if (entry.type != JSONType.object) {
            message = "overlayObjects entries must be objects";
            return false;
        }

        auto obj = entry.object;
        ScreenshotOverlayObject overlay;

        if ("uuid" in obj) {
            if (!readOverlayUuid(obj["uuid"], overlay.uuid)) {
                message = "overlayObjects[].uuid must be a numeric UUID";
                return false;
            }

            if ("overlay" in obj && obj["overlay"].type == JSONType.string) {
                overlay.kind = obj["overlay"].str;
            } else if ("type" in obj && obj["type"].type == JSONType.string) {
                overlay.kind = obj["type"].str;
            } else if ("kind" in obj && obj["kind"].type == JSONType.string) {
                overlay.kind = obj["kind"].str;
            } else {
                message = "overlayObjects[] requires overlay/type/kind as bounds or mesh";
                return false;
            }
        } else {
            bool found = false;
            foreach (key, kindValue; obj) {
                if (kindValue.type != JSONType.string) continue;
                try {
                    import std.conv : to;
                    overlay.uuid = to!uint(key);
                    overlay.kind = kindValue.str;
                    found = true;
                    break;
                } catch (Exception) {
                }
            }
            if (!found) {
                message = "overlayObjects[] must be {uuid, overlay} or {\"<uuid>\": \"bounds|mesh\"}";
                return false;
            }
        }

        if (overlay.kind != "bounds" && overlay.kind != "mesh") {
            message = "overlayObjects[] overlay must be bounds or mesh";
            return false;
        }
        overlays ~= overlay;
    }

    return true;
}

private void drawScreenshotBoundsOverlay(Node node) {
    import nijilive.core.dbg : inDbgDrawLines, inDbgLineWidth, inDbgSetBuffer;

    auto bounds = node.getCombinedBounds!(true)();
    auto points = Vec3Array([
        vec3(bounds.x, bounds.y, 0),
        vec3(bounds.z, bounds.y, 0),
        vec3(bounds.z, bounds.w, 0),
        vec3(bounds.x, bounds.w, 0),
    ]);
    ushort[] indices = [0, 1, 1, 2, 2, 3, 3, 0];
    inDbgSetBuffer(points, indices);
    inDbgLineWidth(3);
    inDbgDrawLines(vec4(1, 0.85, 0.1, 1));
    inDbgLineWidth(1);
}

private bool drawScreenshotDeformableMeshOverlay(Deformable deformable) {
    import nijilive.core.dbg : inDbgDrawLines, inDbgDrawPoints, inDbgPointsSize, inDbgSetBuffer;
    import nijilive.core.nodes.deformer.grid : GridDeformer;
    import std.algorithm : map, sort;
    import std.algorithm.iteration : uniq;
    import std.array : array;

    if (deformable is null || deformable.vertices.length == 0)
        return false;

    Vec3Array pointBuffer;
    auto vertices = deformable.vertices;
    bool haveDeform = deformable.deformation.length == vertices.length;

    if (auto grid = cast(GridDeformer)deformable) {
        auto baseVertsAoS = vertices.toArray();
        auto xs = baseVertsAoS.map!(v => v.x).array;
        auto ys = baseVertsAoS.map!(v => v.y).array;
        xs.sort();
        ys.sort();
        xs = xs.uniq.array;
        ys = ys.uniq.array;
        size_t cols = xs.length;
        size_t rows = ys.length;

        if (cols >= 2 && rows >= 2 && cols * rows == vertices.length) {
            Vec3Array lines;
            foreach (y; 0 .. rows) {
                foreach (x; 0 .. cols) {
                    size_t idx = y * cols + x;
                    vec2 startPos = baseVertsAoS[idx];
                    if (haveDeform) startPos += grid.deformation[idx];
                    auto start = vec3(startPos, 0);
                    if (x + 1 < cols) {
                        size_t nextIdx = idx + 1;
                        vec2 rightPos = baseVertsAoS[nextIdx];
                        if (haveDeform) rightPos += grid.deformation[nextIdx];
                        lines ~= start;
                        lines ~= vec3(rightPos, 0);
                    }
                    if (y + 1 < rows) {
                        size_t nextIdx = idx + cols;
                        vec2 downPos = baseVertsAoS[nextIdx];
                        if (haveDeform) downPos += grid.deformation[nextIdx];
                        lines ~= start;
                        lines ~= vec3(downPos, 0);
                    }
                }
            }
            if (lines.length > 0) {
                inDbgSetBuffer(lines);
                inDbgDrawLines(vec4(0.1, 0.9, 1, 1), grid.transform.matrix);
            }
        }
    } else if (vertices.length >= 2) {
        Vec3Array lines;
        foreach (i; 1 .. vertices.length) {
            auto prev = vertices[i - 1];
            auto next = vertices[i];
            if (haveDeform) {
                prev += deformable.deformation[i - 1];
                next += deformable.deformation[i];
            }
            lines ~= vec3(prev, 0);
            lines ~= vec3(next, 0);
        }
        if (lines.length > 0) {
            inDbgSetBuffer(lines);
            inDbgDrawLines(vec4(0.1, 0.9, 1, 1), deformable.transform.matrix);
        }
    }

    pointBuffer.length = vertices.length;
    foreach (i, point; vertices) {
        if (haveDeform) point += deformable.deformation[i];
        pointBuffer[i] = vec3(point, 0);
    }
    inDbgSetBuffer(pointBuffer);
    inDbgPointsSize(8);
    inDbgDrawPoints(vec4(0, 0, 0, 1), deformable.transform.matrix);
    inDbgPointsSize(4);
    inDbgDrawPoints(vec4(1, 1, 1, 1), deformable.transform.matrix);
    return true;
}

private bool drawScreenshotOverlays(Puppet puppet, JSONValue overlayObjects, out string message) {
    ScreenshotOverlayObject[] overlays;
    if (!parseScreenshotOverlayObjects(overlayObjects, overlays, message))
        return false;

    foreach (overlay; overlays) {
        auto node = puppet.find!(Node)(overlay.uuid);
        if (node is null) {
            message = "overlayObjects target node not found: " ~ overlay.uuid.to!string;
            return false;
        }

        if (overlay.kind == "bounds") {
            drawScreenshotBoundsOverlay(node);
        } else if (overlay.kind == "mesh") {
            auto drawable = cast(Drawable)node;
            if (drawable !is null) {
                drawable.drawMeshLines(vec4(0.1, 0.9, 1, 1));
                drawable.drawMeshPoints();
            } else if (!drawScreenshotDeformableMeshOverlay(cast(Deformable)node)) {
                message = "overlayObjects mesh target is not Drawable or Deformable: " ~ overlay.uuid.to!string;
                return false;
            }
        }
    }

    return true;
}

private bool resolveScreenshotParameter(Context ctx, out Parameter param, out string message) {
    param = null;

    if (ctx.hasArmedParameters && ctx.armedParameters.length > 0) {
        if (ctx.armedParameters.length != 1) {
            message = "context.parameterValue requires exactly one armed parameter";
            return false;
        }
        param = ctx.armedParameters[0];
    } else if (ctx.hasParameters && ctx.parameters.length > 0) {
        if (ctx.parameters.length != 1) {
            message = "context.parameterValue requires exactly one parameter";
            return false;
        }
        param = ctx.parameters[0];
    } else {
        param = incArmedParameter();
    }

    if (param is null) {
        message = "context.parameterValue requires a parameter";
        return false;
    }
    return true;
}

private bool resolveScreenshotParameterValue(Parameter param, vec2 value, out vec2 resolved, out string message) {
    if (param is null) {
        message = "context.parameterValue requires a parameter";
        return false;
    }

    uint x = param.getClosestAxisPointIndex(0, param.mapAxis(0, value.x));
    uint y = param.getClosestAxisPointIndex(1, param.mapAxis(1, value.y));
    vec2u keyPoint = vec2u(x, y);
    if (keyPoint.x >= param.axisPointCount(0) || keyPoint.y >= param.axisPointCount(1)) {
        message = "context.parameterValue resolved keypoint is out of range";
        return false;
    }

    resolved = param.getKeypointValue(keyPoint);

    import std.math : abs;
    enum float epsilon = 1e-5f;
    if (abs(resolved.x - value.x) > epsilon || abs(resolved.y - value.y) > epsilon) {
        message = "context.parameterValue does not match an existing key value";
        return false;
    }
    return true;
}

private bool applyScreenshotParameterContext(Context ctx, out Parameter param, out vec2 restoreValue, out string message) {
    param = null;
    restoreValue = vec2(0, 0);
    if (!ctx.hasParameterValue)
        return true;

    if (!resolveScreenshotParameter(ctx, param, message))
        return false;

    vec2 captureValue;
    if (!resolveScreenshotParameterValue(param, ctx.parameterValue, captureValue, message))
        return false;

    restoreValue = param.value;
    param.value = captureValue;
    return true;
}

private bool incCaptureLiveViewport(out int width, out int height, out ubyte[] textureData, out string message, JSONValue overlayObjects = JSONValue.init) {
    auto puppet = incActivePuppet();
    if (puppet is null) {
        message = "No active puppet";
        return false;
    }

    inGetViewport(width, height);
    if (width <= 0 || height <= 0) {
        message = "Viewport is empty";
        return false;
    }

    inSetClearColor(0, 0, 0, 0);
    scope(exit) incResetClearColor();

    bool overlaysOk = true;
    inBeginScene();
        puppet.update();
        puppet.draw();
        overlaysOk = drawScreenshotOverlays(puppet, overlayObjects, message);
    inEndScene();
    if (!overlaysOk)
        return false;

    textureData = new ubyte[inViewportDataLength()];
    inDumpViewport(textureData);
    inTexUnPremuliply(textureData);
    return true;
}

private JSONValue jsonVec2(vec2 value) {
    JSONValue result = JSONValue.emptyArray;
    result.array ~= JSONValue(cast(double)value.x);
    result.array ~= JSONValue(cast(double)value.y);
    return result;
}

private JSONValue jsonMat4(mat4 value) {
    JSONValue result = JSONValue.emptyArray;
    foreach (row; 0 .. 4) {
        JSONValue rowValue = JSONValue.emptyArray;
        foreach (col; 0 .. 4) {
            rowValue.array ~= JSONValue(cast(double)value[row][col]);
        }
        result.array ~= rowValue;
    }
    return result;
}

private vec2 clipToWorldPoint(mat4 clipToWorld, vec2 clip) {
    auto world = clipToWorld * vec4(clip.x, clip.y, 0, 1);
    if (world.w != 0)
        return (world.xy / world.w);
    return world.xy;
}

private mat4 clipToImageMatrix(int width, int height) {
    return mat4.translation(cast(float)width * 0.5f, cast(float)height * 0.5f, 0) *
        mat4.scaling(cast(float)width * 0.5f, -(cast(float)height) * 0.5f, 1);
}

private JSONValue buildScreenshotWorldCaptureMeta(int width, int height) {
    auto worldToClip = inGetCamera().matrix();
    auto clipToWorld = worldToClip.inverse;
    auto worldToImage = clipToImageMatrix(width, height) * worldToClip;
    auto imageToWorld = worldToImage.inverse;

    vec2 topLeft = clipToWorldPoint(clipToWorld, vec2(-1, 1));
    vec2 topRight = clipToWorldPoint(clipToWorld, vec2(1, 1));
    vec2 bottomRight = clipToWorldPoint(clipToWorld, vec2(1, -1));
    vec2 bottomLeft = clipToWorldPoint(clipToWorld, vec2(-1, -1));

    vec2 minPoint = vec2(
        min(min(topLeft.x, topRight.x), min(bottomRight.x, bottomLeft.x)),
        min(min(topLeft.y, topRight.y), min(bottomRight.y, bottomLeft.y))
    );
    vec2 maxPoint = vec2(
        max(max(topLeft.x, topRight.x), max(bottomRight.x, bottomLeft.x)),
        max(max(topLeft.y, topRight.y), max(bottomRight.y, bottomLeft.y))
    );

    JSONValue[string] corners;
    corners["topLeft"] = jsonVec2(topLeft);
    corners["topRight"] = jsonVec2(topRight);
    corners["bottomRight"] = jsonVec2(bottomRight);
    corners["bottomLeft"] = jsonVec2(bottomLeft);

    JSONValue[string] aabb;
    aabb["min"] = jsonVec2(minPoint);
    aabb["max"] = jsonVec2(maxPoint);

    JSONValue[string] capture;
    capture["coordinateSystem"] = JSONValue("nijigenerate-world");
    capture["pixelOrigin"] = JSONValue("top-left");
    capture["width"] = JSONValue(cast(long)width);
    capture["height"] = JSONValue(cast(long)height);
    capture["corners"] = JSONValue(corners);
    capture["aabb"] = JSONValue(aabb);
    capture["worldToClipMatrix"] = jsonMat4(worldToClip);
    capture["clipToWorldMatrix"] = jsonMat4(clipToWorld);
    capture["worldToImageMatrix"] = jsonMat4(worldToImage);
    capture["imageToWorldMatrix"] = jsonMat4(imageToWorld);
    capture["pixelToWorld"] = JSONValue("world = imageToWorldMatrix * [pixelX, pixelY, 0, 1]");
    return JSONValue(capture);
}

private JSONValue buildScreenshotOverlayMappings(Puppet puppet, JSONValue overlayObjects, int width, int height, out string message) {
    ScreenshotOverlayObject[] overlays;
    if (!parseScreenshotOverlayObjects(overlayObjects, overlays, message))
        return JSONValue.init;

    auto worldToImage = clipToImageMatrix(width, height) * inGetCamera().matrix();
    auto imageToWorld = worldToImage.inverse;

    JSONValue result = JSONValue.emptyArray;
    foreach (overlay; overlays) {
        auto node = puppet.find!(Node)(overlay.uuid);
        if (node is null) {
            message = "overlayObjects target node not found: " ~ overlay.uuid.to!string;
            return JSONValue.init;
        }

        auto targetToWorld = node.transform.matrix;
        auto worldToTarget = targetToWorld.inverse;
        auto targetToImage = worldToImage * targetToWorld;
        auto imageToTarget = targetToImage.inverse;

        JSONValue[string] entry;
        entry["uuid"] = JSONValue(cast(long)overlay.uuid);
        entry["overlay"] = JSONValue(overlay.kind);
        entry["coordinateSystem"] = JSONValue("target-local");
        entry["targetToWorldMatrix"] = jsonMat4(targetToWorld);
        entry["worldToTargetMatrix"] = jsonMat4(worldToTarget);
        entry["worldToImageMatrix"] = jsonMat4(worldToImage);
        entry["imageToWorldMatrix"] = jsonMat4(imageToWorld);
        entry["targetToImageMatrix"] = jsonMat4(targetToImage);
        entry["imageToTargetMatrix"] = jsonMat4(imageToTarget);
        entry["targetToImage"] = JSONValue("image = targetToImageMatrix * [targetX, targetY, 0, 1]");
        entry["imageToTarget"] = JSONValue("target = imageToTargetMatrix * [pixelX, pixelY, 0, 1]");
        result.array ~= JSONValue(entry);
    }
    return result;
}

@EffectFileWrite
class SaveScreenshotCommand : ExCommand!(TW!(string, "filename", "file path to save screenshot.")) {
    this(string filename) { super(_("Save Screenshot"), _("Save screenshot."), filename); }

    override
    CommandResult run(Context ctx) {
        if (filename) {
            string file = filename.setExtension("png");

            int width, height;
            ubyte[] textureData;
            string message;
            if (!incCaptureLiveViewport(width, height, textureData, message))
                return CommandResult(false, message);
            
            Texture outTexture = new Texture(null, width, height);
            outTexture.setData(textureData);
            outTexture.save(file);
            return CommandResult(true);
        }
        return CommandResult(false, "Filename not set");
    }
}

@ShortcutHidden
class CaptureLiveScreenshotCommand : ExCommand!(
    TW!(JSONValue, "overlayObjects", "Optional overlay objects. Use [{\"uuid\": 123, \"overlay\": \"bounds\"|\"mesh\"}] or [{\"123\": \"bounds\"|\"mesh\"}].")
) {
    this() {
        super(_("Capture Live Screenshot"), _("Capture current live viewport and return an MCP image content item with mimeType image/png."));
        overlayObjects = JSONValue.emptyArray;
    }

    override
    ExCommandResult!JSONValue run(Context ctx) {
        import core.stdc.stdlib : free;
        import imagefmt : IF_PNG, IF_ERROR, write_image_mem;
        import std.base64 : Base64;
        import std.conv : to;

        int width, height;
        ubyte[] textureData;
        string message;
        Parameter captureParam;
        vec2 restoreValue;
        bool restoreCaptureParam = ctx.hasParameterValue;
        if (!applyScreenshotParameterContext(ctx, captureParam, restoreValue, message))
            return ExCommandResult!JSONValue(false, JSONValue.init, message);
        scope(exit) {
            if (restoreCaptureParam && captureParam !is null)
                captureParam.value = restoreValue;
        }

        if (!incCaptureLiveViewport(width, height, textureData, message, overlayObjects))
            return ExCommandResult!JSONValue(false, JSONValue.init, message);

        int error;
        ubyte[] pngData = write_image_mem(IF_PNG, width, height, textureData, 4, error);
        if (error)
            return ExCommandResult!JSONValue(false, JSONValue.init, "PNG encode failed: " ~ IF_ERROR[error].to!string);
        scope(exit) free(pngData.ptr);

        JSONValue[string] image;
        image["type"] = JSONValue("image");
        image["mimeType"] = JSONValue("image/png");
        image["data"] = JSONValue(Base64.encode(cast(const(ubyte)[])pngData));

        JSONValue[string] meta;
        meta["width"] = JSONValue(cast(long)width);
        meta["height"] = JSONValue(cast(long)height);
        meta["worldCapture"] = buildScreenshotWorldCaptureMeta(width, height);
        if (overlayObjects.type == JSONType.array && overlayObjects.array.length > 0) {
            auto puppet = incActivePuppet();
            if (puppet is null)
                return ExCommandResult!JSONValue(false, JSONValue.init, "No active puppet");
            message = null;
            JSONValue overlayMappings = buildScreenshotOverlayMappings(puppet, overlayObjects, width, height, message);
            if (message.length > 0)
                return ExCommandResult!JSONValue(false, JSONValue.init, message);
            meta["overlayMappings"] = overlayMappings;
        }

        JSONValue[string] result;
        result["mcpDirectToolResult"] = JSONValue(true);
        result["content"] = JSONValue([JSONValue(image)]);
        result["_meta"] = JSONValue(meta);
        return ExCommandResult!JSONValue(true, JSONValue(result));
    }
}

class ShowStatusForNerdsCommand : ExCommand!() {
    this() { super(_("Show Stats for Nerds"), _("Show status for nerds.")); }
    override
    CommandResult run(Context ctx) {
        incShowStatsForNerds = !incShowStatsForNerds;
        incSettingsSet("NerdStats", incShowStatsForNerds);
        return CommandResult(true);
    }
}

class ToggleDifferenceAggregationCommand : ExCommand!() {
    this() { super(_("Difference Aggregation Debug"), _("Toggle GPU difference aggregation for the selected node.")); }

    override
    CommandResult run(Context ctx) {
        ngDifferenceAggregationDebugEnabled = !ngDifferenceAggregationDebugEnabled;
        if (!ngDifferenceAggregationDebugEnabled) {
            ngDifferenceAggregationResolvedIndex = size_t.max;
            ngDifferenceAggregationResultValid = false;
            ngDifferenceAggregationResult = DifferenceEvaluationResult.init;
            ngDifferenceAggregationResultSerial = 0;
            inSetDifferenceAggregationEnabled(false);
        }
        return CommandResult(true);
    }
}

enum ViewCommand {
    SetDefaultLayout,
    ShowSaveScreenshotDialog,
    SaveScreenshot,
    CaptureLiveScreenshot,
    ShowStatusForNerds,
    ToggleDifferenceAggregation,
}


Command[ViewCommand] commands;

void ngInitCommands(T)() if (is(T == ViewCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!ViewCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
    mixin(registerCommand!(ViewCommand.SaveScreenshot, ""));
}
