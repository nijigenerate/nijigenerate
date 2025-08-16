module nijigenerate.commands.puppet.tool;

import nijigenerate.commands.base;
import nijigenerate.core.window;
import nijilive;
import nijigenerate.ext;
import tinyfiledialogs;
import nijigenerate.io;
import i18n;
import std.path;
import nijigenerate.project;
import nijigenerate.core.settings;
import nijigenerate.core.tasks;
import nijigenerate.widgets.dialog;
import nijigenerate.utils.repair;

class ShowImportSessionDataDialogCommand : ExCommand!() {
    this() { super(_("Import Inochi Session Data"), _("Shows \"Import Session Data\" dialog.")); }

    override
    void run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.inp"], "nijilive Puppet (*.inp)" }
        ];

        if (string path = incShowImportDialog(filters, _("Import..."))) {
            auto cmd = cast(ImportSessionDataCommand)commands[ToolCommand.ImportSessionData];
            if (cmd) {
                cmd.path = path;
                cmd.run(ctx);
            }
        }
    }
}

class ImportSessionDataCommand : ExCommand!(TW!(string, "path", "file path of INP file.")) {
    this(string path) { super(_("Import Inochi Session Data"), _("Import INP Session Data."), path); }

    override
    void run(Context ctx) {
        if (!ctx.hasPuppet) return;
        if (path) {
            Puppet p = inLoadPuppet!ExPuppet(path);

            if ("com.inochi2d.inochi-session.bindings" in p.extData) {
                ctx.puppet.extData["com.inochi2d.inochi-session.bindings"] = p.extData["com.inochi2d.inochi-session.bindings"].dup;
                incSetStatus(_("Successfully overwrote Inochi Session tracking data..."));
            } else {
                incDialog(__("Error"), _("There was no Inochi Session data to import!"));
            }

            destroy!false(p);
        }
    }
}

class PremultTextureCommand : ExCommand!() {
    this() { super(_("Premultiply textures"), _("Premultiply texture.")); }

    override
    void run(Context ctx) {
        if (!ctx.hasPuppet) return;
        import nijigenerate.utils.repair : incPremultTextures;
        incPremultTextures(ctx.puppet);
    }
}

class RebleedTextureCommand : ExCommand!() {
    this() { super(_("Bleed textures..."), _("Bleed texture.")); }

    override
    void run(Context ctx) {
        incRebleedTextures();
    }
}

class RegenerateMipmapsCommand : ExCommand!() {
    this() { super(_("Generate Mipmaps..."), _("Generate mipmaps.")); }

    override
    void run(Context ctx) {
        incRegenerateMipmaps();
    }
}

class GenerateFakeLayerNameCommand : ExCommand!() {
    this() { super(_("Generate fake layer name info..."), _("Generate fake layer name.")); }

    override
    void run(Context ctx) {
        if (!ctx.hasPuppet || !ctx.puppet) return;
        auto parts = ctx.puppet.getAllParts();
        foreach(ref part; parts) {
            auto expart = cast(ExPart)part;
            if (expart) {
                expart.layerPath = "/"~part.name;
            }
        }
    }
}

class AttemptRepairPuppetCommand : ExCommand!() {
    this() { super(_("Attempt full repair..."), _("Attempt full repair...")); }

    override
    void run(Context ctx) {
        if (!ctx.hasPuppet || !ctx.puppet) return;
        incAttemptRepairPuppet(ctx.puppet);
    }
}

class RegenerateNodeIDsCommand : ExCommand!() {
    this() { super(_("Regenerate Node IDs"), _("Regenerate Node IDs")); }

    override
    void run(Context ctx) {
        if (!ctx.hasPuppet || !ctx.puppet) return;
        incRegenerateNodeIDs(ctx.puppet.root);
    }
}

class ModelEditModeCommand : ExCommand!() {
    this() { super(_("Edit Puppet"), _("Switch to model-edit mode")); }

    override
    void run(Context ctx) {
        bool alreadySelected = incEditMode == EditMode.ModelEdit;
        if (!alreadySelected) {
            incSetEditMode(EditMode.ModelEdit);
        }
    }
}


class AnimEditModeCommand : ExCommand!() {
    this() { super(_("Edit Animation"), _("Switch to anim-edit mode")); }

    override
    void run(Context ctx) {
        bool alreadySelected = incEditMode == EditMode.AnimEdit;
        if (!alreadySelected) {
            incSetEditMode(EditMode.AnimEdit);
        }
    }
}

enum ToolCommand {
    ShowImportSessionDataDialog,
    ImportSessionData,
    PremultTexture,
    RebleedTexture,
    RegenerateMipmaps,
    GenerateFakeLayerName,
    AttemptRepairPuppet,
    RegenerateNodeIDs,
    ModelEditMode,
    AnimEditMode,
}


Command[ToolCommand] commands;

void ngInitCommands(T)() if (is(T == ToolCommand))
{
    import std.traits : EnumMembers;
    static foreach (name; EnumMembers!ToolCommand) {
        static if (__traits(compiles, { mixin(registerCommand!(name)); }))
            mixin(registerCommand!(name));
    }
    mixin(registerCommand!(ToolCommand.ImportSessionData, ""));
}
