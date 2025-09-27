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


class SetDefaultLayoutCommand : ExCommand!() {
    this() { super(_("Reset Layout"), _("Set default layout of panels.")); }

    override
    void run(Context ctx) {
        incSetDefaultLayout();
    }
}

class ShowSaveScreenshotDialogCommand : ExCommand!() {
    this() { super(_("Save Screenshot"), _("Shows \"Save screenshot\" dialog.")); }

    override
    void run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.png"], "PNG Image (*.png)" }
        ];
        
        string filename = incShowSaveDialog(filters, "", _("Save Screenshot..."));
        if (filename) {
            auto cmd = cast(SaveScreenshotCommand)commands[ViewCommand.SaveScreenshot];
            if (cmd) {
                cmd.filename = filename;
                cmd.run(ctx);
            }
        }
    }
}

class SaveScreenshotCommand : ExCommand!(TW!(string, "filename", "file path to save screenshot.")) {
    this(string filename) { super(_("Save Screenshot"), _("Save screenshot."), filename); }

    override
    void run(Context ctx) {
        if (filename) {
            string file = filename.setExtension("png");

            // Dump viewport to RGBA byte array
            int width, height;
            inGetViewport(width, height);
            Texture outTexture = new Texture(null, width, height);

            // Texture data
            inSetClearColor(0, 0, 0, 0);
            inBeginScene();
                incActivePuppet().update();
                incActivePuppet().draw();
            inEndScene();
            ubyte[] textureData = new ubyte[inViewportDataLength()];
            inDumpViewport(textureData);
            inTexUnPremuliply(textureData);
            incResetClearColor();
            
            // Write to texture
            outTexture.setData(textureData);

            outTexture.save(file);
        }
    }
}

class ShowStatusForNerdsCommand : ExCommand!() {
    this() { super(_("Show Stats for Nerds"), _("Show status for nerds.")); }
    override
    void run(Context ctx) {
        incShowStatsForNerds = !incShowStatsForNerds;
        incSettingsSet("NerdStats", incShowStatsForNerds);
    }
}

class ToggleDifferenceAggregationCommand : ExCommand!() {
    this() { super(_("Difference Aggregation Debug"), _("Toggle GPU difference aggregation for the selected node.")); }

    override
    void run(Context ctx) {
        ngDifferenceAggregationDebugEnabled = !ngDifferenceAggregationDebugEnabled;
        if (!ngDifferenceAggregationDebugEnabled) {
            ngDifferenceAggregationResolvedIndex = size_t.max;
            ngDifferenceAggregationResultValid = false;
            ngDifferenceAggregationResult = DifferenceEvaluationResult.init;
            ngDifferenceAggregationResultSerial = 0;
            inSetDifferenceAggregationEnabled(false);
        }
    }
}

enum ViewCommand {
    SetDefaultLayout,
    ShowSaveScreenshotDialog,
    SaveScreenshot,
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
