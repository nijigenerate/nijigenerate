module nijigenerate.commands.puppet.view;

import nijigenerate.commands.base;
import nijigenerate.core.window;
import nijilive;
import nijilive.core.diff_collect : DifferenceEvaluationResult;
import tinyfiledialogs;
import nijigenerate.io;
import i18n;
import std.path;
import nijigenerate.project;
import nijigenerate.core.settings;
import std.json : JSONValue;


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

private bool incCaptureLiveViewport(out int width, out int height, out ubyte[] textureData, out string message) {
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

    inBeginScene();
        puppet.update();
        puppet.draw();
    inEndScene();

    textureData = new ubyte[inViewportDataLength()];
    inDumpViewport(textureData);
    inTexUnPremuliply(textureData);
    return true;
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
class CaptureLiveScreenshotCommand : ExCommand!() {
    this() { super(_("Capture Live Screenshot"), _("Capture current live viewport and return an MCP image content item with mimeType image/png.")); }

    override
    ExCommandResult!JSONValue run(Context ctx) {
        import core.stdc.stdlib : free;
        import imagefmt : IF_PNG, IF_ERROR, write_image_mem;
        import std.base64 : Base64;
        import std.conv : to;

        int width, height;
        ubyte[] textureData;
        string message;
        if (!incCaptureLiveViewport(width, height, textureData, message))
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
