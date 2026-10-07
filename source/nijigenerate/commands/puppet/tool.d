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
import std.json : JSONValue;

private __gshared string delegate(JSONValue, JSONValue) autoRigExecute;
private __gshared JSONValue delegate(string) autoRigStatus;
private __gshared string delegate(string,string) autoRigResume;
private __gshared void delegate(string) autoRigRemove;

void ngSetAutoRigCommandHandlers(string delegate(JSONValue, JSONValue) execute, JSONValue delegate(string) status,
    string delegate(string,string) resume, void delegate(string) remove = null) {
    autoRigExecute = execute; autoRigStatus = status; autoRigResume = resume;
    autoRigRemove = remove;
}

@McpHidden
@GuiDialog
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
                return ngRunCommand(cmd, ctx);
            }
        }
        return CommandResult(false, "Import canceled");
    }
}

@EffectImport
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

@EffectTextureRegenerate
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

@EffectTextureRegenerate
class RebleedTextureCommand : ExCommand!() {
    this() { super(_("Bleed textures..."), _("Bleed texture.")); }

    override
    CommandResult run(Context ctx) {
        incRebleedTextures();
        return CommandResult(true);
    }
}

@EffectTextureRegenerate
class RegenerateMipmapsCommand : ExCommand!() {
    this() { super(_("Generate Mipmaps..."), _("Generate mipmaps.")); }

    override
    CommandResult run(Context ctx) {
        incRegenerateMipmaps();
        return CommandResult(true);
    }
}

@EffectStructuralEdit
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

@EffectRepair
class AttemptRepairPuppetCommand : ExCommand!() {
    this() { super(_("Attempt full repair..."), _("Attempt full repair...")); }

    override
    CommandResult run(Context ctx) {
        if (!ctx.hasPuppet || !ctx.puppet) return CommandResult(false, "No puppet");
        incAttemptRepairPuppet(ctx.puppet);
        return CommandResult(true);
    }
}

@EffectRepair
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

@EffectStructuralEdit
class ExecuteAutoRigCommand : ExCommand!(TW!(string,"options","AutoRig options JSON; defaults to an empty object."),
    TW!(string,"context","Named session values: {name: {kind: Path|Json|Blob|FileName, value: ...}}.")) {
    this(string options = "{}", string context = "{}") {
        super(_("Execute AutoRig"),_("Run AutoRig on the imported model."),options,context);
    }
    override CommandResult run(Context ctx) {
        if (!ctx.hasPuppet || ctx.puppet is null) return CommandResult(false,"No imported model is open");
        import std.json : parseJSON, JSONValue;
        if (autoRigExecute is null) return CommandResult(false,"AutoRig panel is unavailable");
        auto id = autoRigExecute(parseJSON(options.length ? options : "{}"),
            parseJSON(context.length ? context : "{}"));
        return new ExCommandResult!JSONValue(true,JSONValue(["run_id":JSONValue(id)]));
    }
}

class GetAutoRigStatusCommand : ExCommand!(TW!(string,"runId","AutoRig run UUID.")) {
    this(string runId = "") { super(_("AutoRig Status"),_("Read AutoRig workflow progress."),runId); }
    override CommandResult run(Context ctx) {
        if (autoRigStatus is null) return CommandResult(false,"AutoRig panel is unavailable");
        return new ExCommandResult!JSONValue(true,autoRigStatus(runId));
    }
}

class GetAutoRigMemoryStatusCommand : ExCommand!(TW!(string,"runId","AutoRig run UUID; empty measures all panel sessions."),
    TW!(bool,"collectGarbage","Explicitly collect unused memory for a stopped session before measuring.")) {
    this(string runId = "", bool collectGarbage = false) {
        super(_("AutoRig memory status"),_("Read retained artifact sizes and garbage collector memory."),
            runId,collectGarbage);
    }
    override CommandResult run(Context ctx) {
        import std.json : JSONValue;
        if (autoRigStatus is null) return CommandResult(false,"AutoRig panel is unavailable");
        auto status = autoRigStatus(runId);
        if (collectGarbage) {
            if (status["state"].str == "Running") return CommandResult(false,"AutoRig is still running");
            import core.memory : GC;
            import std.datetime.stopwatch : StopWatch;
            auto timer = StopWatch(); timer.start();
            GC.collect();
            timer.stop();
            auto memory = GC.stats();
            status["memory"]["gc_used_after_collection_bytes"] = JSONValue(cast(ulong)memory.usedSize);
            status["memory"]["gc_free_after_collection_bytes"] = JSONValue(cast(ulong)memory.freeSize);
            status["memory"]["collection_microseconds"] = JSONValue(timer.peek.total!"usecs");
        }
        return new ExCommandResult!JSONValue(true,status);
    }
}

class DeleteAutoRigSessionCommand : ExCommand!(TW!(string,"runId","Stopped AutoRig session UUID.")) {
    this(string runId = "") {
        super(_("Delete AutoRig session"),_("Delete the session artifacts while keeping the editor model."),runId);
    }
    override CommandResult run(Context ctx) {
        if (autoRigRemove is null) return CommandResult(false,"AutoRig panel is unavailable");
        autoRigRemove(runId);
        return CommandResult(true);
    }
}

@EffectStructuralEdit
class ResumeAutoRigCommand : ExCommand!(TW!(string,"runId","Saved AutoRig run UUID."),
    TW!(string,"stepId","Optional workflow step to retry; empty resumes the workflow.")) {
    this(string runId = "", string stepId = "") {
        super(_("Resume AutoRig"),_("Resume a saved AutoRig workflow."),runId,stepId);
    }
    override CommandResult run(Context ctx) {
        if (autoRigResume is null) return CommandResult(false,"AutoRig panel is unavailable");
        auto id = autoRigResume(runId,stepId);
        return new ExCommandResult!JSONValue(true,JSONValue(["run_id":JSONValue(id)]));
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
    ExecuteAutoRig,
    GetAutoRigStatus,
    GetAutoRigMemoryStatus,
    DeleteAutoRigSession,
    ResumeAutoRig,
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
