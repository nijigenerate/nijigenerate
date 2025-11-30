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
    CommandResult run(Context ctx) {
        const TFD_Filter[] filters = [
            { ["*.inp"], "nijilive Puppet (*.inp)" }
        ];

        if (string path = incShowImportDialog(filters, _("Import..."))) {
            auto cmd = cast(ImportSessionDataCommand)commands[ToolCommand.ImportSessionData];
            if (cmd) {
                cmd.path = path;
                return cmd.run(ctx);
            }
        }
        return CommandResult(false, "Import canceled");
    }
}

class ImportSessionDataCommand : ExCommand!(TW!(string, "path", "file path of INP file.")) {
    this(string path) { super(_("Import Inochi Session Data"), _("Import INP Session Data."), path); }

    override
    CommandResult run(Context ctx) {
        if (!ctx.hasPuppet) return CommandResult(false, "No puppet");
        if (path) {
            Puppet p = inLoadPuppet!ExPuppet(path);

            if ("com.inochi2d.inochi-session.bindings" in p.extData) {
                ctx.puppet.extData["com.inochi2d.inochi-session.bindings"] = p.extData["com.inochi2d.inochi-session.bindings"].dup;
                incSetStatus(_("Successfully overwrote Inochi Session tracking data..."));
                destroy!false(p);
                return CommandResult(true);
            } else {
                incDialog(__("Error"), _("There was no Inochi Session data to import!"));
            }

            destroy!false(p);
            return CommandResult(false, "Session data missing");
        }
        return CommandResult(false, "Path not provided");
    }
}

class PremultTextureCommand : ExCommand!() {
    this() { super(_("Premultiply textures"), _("Premultiply texture.")); }

    override
    CommandResult run(Context ctx) {
        if (!ctx.hasPuppet) return CommandResult(false, "No puppet");
        import nijigenerate.utils.repair : incPremultTextures;
        incPremultTextures(ctx.puppet);
        return CommandResult(true);
    }
}

class RebleedTextureCommand : ExCommand!() {
    this() { super(_("Bleed textures..."), _("Bleed texture.")); }

    override
    CommandResult run(Context ctx) {
        incRebleedTextures();
        return CommandResult(true);
    }
}

class RegenerateMipmapsCommand : ExCommand!() {
    this() { super(_("Generate Mipmaps..."), _("Generate mipmaps.")); }

    override
    CommandResult run(Context ctx) {
        incRegenerateMipmaps();
        return CommandResult(true);
    }
}

class GenerateFakeLayerNameCommand : ExCommand!() {
    this() { super(_("Generate fake layer name info..."), _("Generate fake layer name.")); }

    override
    CommandResult run(Context ctx) {
        if (!ctx.hasPuppet || !ctx.puppet) return CommandResult(false, "No puppet");
        auto parts = ctx.puppet.getAllParts();
        foreach(ref part; parts) {
            auto expart = cast(ExPart)part;
            if (expart) {
                expart.layerPath = "/"~part.name;
            }
        }
        return CommandResult(true);
    }
}

class AttemptRepairPuppetCommand : ExCommand!() {
    this() { super(_("Attempt full repair..."), _("Attempt full repair...")); }

    override
    CommandResult run(Context ctx) {
        if (!ctx.hasPuppet || !ctx.puppet) return CommandResult(false, "No puppet");
        incAttemptRepairPuppet(ctx.puppet);
        return CommandResult(true);
    }
}

class RegenerateNodeIDsCommand : ExCommand!() {
    this() { super(_("Regenerate Node IDs"), _("Regenerate Node IDs")); }

    override
    CommandResult run(Context ctx) {
        if (!ctx.hasPuppet || !ctx.puppet) return CommandResult(false, "No puppet");
        incRegenerateNodeIDs(ctx.puppet.root);
        return CommandResult(true);
    }
}

class ModelEditModeCommand : ExCommand!() {
    this() { super(_("Edit Puppet"), _("Switch to model-edit mode")); }

    override
    CommandResult run(Context ctx) {
        bool alreadySelected = incEditMode == EditMode.ModelEdit;
        if (!alreadySelected) {
            incSetEditMode(EditMode.ModelEdit);
        }
        return CommandResult(true);
    }
}


class AnimEditModeCommand : ExCommand!() {
    this() { super(_("Edit Animation"), _("Switch to anim-edit mode")); }

    override
    CommandResult run(Context ctx) {
        bool alreadySelected = incEditMode == EditMode.AnimEdit;
        if (!alreadySelected) {
            incSetEditMode(EditMode.AnimEdit);
        }
        return CommandResult(true);
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
