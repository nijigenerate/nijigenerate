module nijigenerate.panels.autorig;

import bindbc.imgui;
import core.thread : Thread;
import core.time : msecs;
import i18n;
import nijigenerate : EditMode;
import nijigenerate.api.mcp.task : ngMcpProcessQueue, ngRunInMainThread, ngMcpSetExternalCommandsBlocked,
    ngMcpExternalCommandsBlocked;
import nijigenerate.autorig;
import nijigenerate.autorig.deterministic.processor : AnimeFrontViewRigProcessor;
import nijigenerate.autorig.deterministic.editor : ngApplyFaceProjection;
import nijigenerate.autorig.deterministic.native : ngRigNativeStage;
import nijigenerate.autorig.deterministic.evidence : ngRigMaterialRoleCandidates;
import nijigenerate.autorig.deterministic.templates : ngRigMaterialRoles;
import nijigenerate.autorig.deterministic.contracts : ngRigGet, ngRigString, ngRigPoint, ngRigUnsigned, ngRigNumber;
import nijigenerate.autorig.deterministic.presentation;
import nijigenerate.autorig.deterministic.review : RigMaterialField, ngRigEditedMaterialOverride,
    ngRigMaterialForcedStatic;
import std.string : endsWith, startsWith, toLower;
import std.algorithm.searching : canFind;
import nijigenerate.core.actionstack : incActionPushGroup, incActionPopGroup;
import nijigenerate.core.path : incGetAppConfigPath;
import nijigenerate.panels : Panel, incPanel, incAddPanel, incFindPanelByName;
import nijigenerate.utils.crashdump : installNativeCrashDumpThreadHandler;
import nijigenerate.widgets : incButtonColored, incInputText, incInputTextMultiline;
import std.algorithm.sorting : sort;
import std.conv : to;
import std.file : read;
import std.json : parseJSON, JSONValue, JSONType;
import std.path : buildPath;
import std.string : toStringz;
import std.format : format;

private AutoRigSessionManager sharedSessions;

private void runActionOnMainThread(void delegate() action) {
    auto current = Thread.getThis();
    if (current is null || current.isMainThread) action();
    else ngRunInMainThread!void(action);
}

/** The registration point used by built-in and future AutoRig processors. */
AutoRigSessionManager ngAutoRigSessionManager() {
    if (sharedSessions is null) {
        sharedSessions = new AutoRigSessionManager(buildPath(incGetAppConfigPath(), "autorig", "runs"));
        sharedSessions.setTaskActionBoundary((string taskId, void delegate() execute) {
            runActionOnMainThread({ incActionPushGroup(); });
            scope(exit) runActionOnMainThread({ incActionPopGroup(); });
            execute();
        });
        sharedSessions.setEditorDispatcher((void delegate() action) {
            runActionOnMainThread(action);
        });
        sharedSessions.registerProcessor(new AnimeFrontViewRigProcessor(
              (projection, target, context) => ngApplyFaceProjection(projection, target, context),
              (stage, state, program, model, context) => ngRigNativeStage(stage, state, program, model, context)));
        sharedSessions.registerProcessorAlias("deterministic-rig", "anime-front-view-rig");
    }
    return sharedSessions;
}

void ngRegisterAutoRigProcessor(AutoRigProcessor processor) {
    ngAutoRigSessionManager().registerProcessor(processor);
}

class AutoRigPanel : Panel {
private:
    AutoRigWorkflowManager workflows;
    AutoRigWorkflowRun[] runs;
    string selectedProviderId;
    string selectedWorkflowId;
    string[string] inputDrafts;
    string[string] inputObserved;
    struct InputTextCache { ulong revision; string text; }
    InputTextCache[string] inputTextCache;
    struct ContextDraft {
        string name;
        string value;
        AutoRigValueKind kind = AutoRigValueKind.Path;
    }
    ContextDraft[string] contextDrafts;
    Thread worker;
    AutoRigWorkflowRun activeRun;
    uint controlledRoot;
    bool previousDriversEnabled;
    string lastError;
    struct MaterialDraft {
        uint attempt, compiledAttempt;
        ulong revision;
        string[] names, paths, roles, features, sides, reasons, semanticSources;
        bool[] disabled, changed, reset, active, forcedStatic, requestedStatic;
        ubyte[] editedFields;
        bool unresolvedOnly;
        string filter;
        string[] landmarks, landmarkReasons;
        double[] landmarkX, landmarkY;
        bool[] landmarkChanged, landmarkReset;
    }
    MaterialDraft[string] materialDrafts;
    string[] materialRoleChoices;
    struct ReviewDraft {
        uint attempt;
        RigReviewTable[] tables;
        string[] operationIds, operationLabels;
        bool[] enabled;
    }
    ReviewDraft[string] reviewDrafts;
    struct WorkflowOptionsDraft { ulong revision; JSONValue options; bool previews; }
    WorkflowOptionsDraft[string] workflowOptionsDrafts;

    AutoRigWorkflowManager workflowManager() {
        if (workflows is null) workflows = new AutoRigWorkflowManager(ngAutoRigSessionManager());
        return workflows;
    }

    void finishWorker() {
        if (worker !is null && !worker.isRunning()) {
            worker.join();
            worker = null;
            activeRun = null;
            ngMcpSetExternalCommandsBlocked(false);
            restoreDriverState();
        }
    }

    void restoreDriverState() {
        import nijigenerate.project : incActivePuppet, incArmedParameter;
        auto puppet = incActivePuppet();
        if (puppet !is null && puppet.root.uuid == controlledRoot && incArmedParameter() is null)
            puppet.enableDrivers = previousDriversEnabled;
        controlledRoot = 0;
    }

    void startRun(AutoRigWorkflowRun run, string stepId = null, bool force = true) {
        finishWorker();
        if (worker !is null) return;
        lastError = null;
        activeRun = run;
        import nijigenerate.project : incActivePuppet;
        auto puppet = incActivePuppet();
        controlledRoot = puppet is null ? 0 : puppet.root.uuid;
        previousDriversEnabled = puppet !is null && puppet.enableDrivers;
        if (puppet !is null) puppet.enableDrivers = false;
        worker = new Thread({
            installNativeCrashDumpThreadHandler();
            try {
                if (stepId.length) run.executeStep(stepId, true);
                else run.execute(force);
            } catch (Throwable error) {
                synchronized (this) lastError = error.msg;
            }
        });
        ngMcpSetExternalCommandsBlocked(true);
        try worker.start();
        catch (Throwable error) {
            ngMcpSetExternalCommandsBlocked(false);
            worker = null;
            activeRun = null;
            restoreDriverState();
            throw error;
        }
    }

    string presetLabel(AutoRigWorkflowPreset preset) {
        auto owner = ngAutoRigSessionManager().processor(preset.providerId);
        return _(owner.displayName()) ~ " / " ~
            (preset.spec.label.length ? _(preset.spec.label) : preset.spec.id);
    }

    void renderPresetPicker() {
        auto presets = workflowManager().listPresets();
        presets.sort!((a, b) => a.spec.preferred != b.spec.preferred ? a.spec.preferred :
            a.providerId < b.providerId || a.providerId == b.providerId && a.spec.id < b.spec.id);
        bool selectionFound;
        foreach (preset; presets)
            if (preset.providerId == selectedProviderId && preset.spec.id == selectedWorkflowId)
                selectionFound = true;
        if (!selectionFound) {
            selectedProviderId = presets.length ? presets[0].providerId : null;
            selectedWorkflowId = presets.length ? presets[0].spec.id : null;
        }
        string selectedLabel = _("No workflows registered");
        string description;
        foreach (preset; presets)
            if (preset.providerId == selectedProviderId && preset.spec.id == selectedWorkflowId) {
                selectedLabel = presetLabel(preset);
                description = _(preset.spec.description);
            }

        igBeginDisabled(!presets.length || worker !is null);
        if (incButtonColored("+", ImVec2(24, 24))) {
            try {
                auto run = workflowManager().create(selectedProviderId, selectedWorkflowId);
                runs ~= run;
                synchronized (this) lastError = null;
            } catch (Exception error) {
                synchronized (this) lastError = error.msg;
            }
        }
        igEndDisabled();
        if (igIsItemHovered()) igSetTooltip("%s", _("Add workflow session").toStringz());
        igSameLine();
        ImVec2 space;
        igGetContentRegionAvail(&space);
        igSetNextItemWidth(space.x > 40 ? space.x - 34 : 1);
        if (igBeginCombo("##AutoRigWorkflow", selectedLabel.toStringz())) {
            foreach (preset; presets) {
                auto label = presetLabel(preset) ~ "##" ~ preset.providerId ~ "/" ~ preset.spec.id;
                bool selected = preset.providerId == selectedProviderId && preset.spec.id == selectedWorkflowId;
                if (igSelectable(label.toStringz(), selected)) {
                    selectedProviderId = preset.providerId;
                    selectedWorkflowId = preset.spec.id;
                }
            }
            igEndCombo();
        }
        if (description.length) igTextWrapped("%s", description.toStringz());
    }

    string draftKey(AutoRigWorkflowRun run, string taskId, string portId) {
        return run.id() ~ "/" ~ taskId ~ "/" ~ portId;
    }

    string inputText(AutoRigValue value) {
        final switch (value.kind) {
            case AutoRigValueKind.FileName:
            case AutoRigValueKind.Path: return value.text;
            case AutoRigValueKind.Json: return value.json.toString();
            case AutoRigValueKind.Blob: return value.text;
        }
    }

    string cachedInputText(string key, ulong revision, string delegate() load) {
        auto cached = key in inputTextCache;
        if (cached is null || cached.revision != revision) {
            inputTextCache[key] = InputTextCache(revision, load());
        }
        return inputTextCache[key].text;
    }

    string valueKindLabel(AutoRigValueKind kind) {
        final switch (kind) {
            case AutoRigValueKind.FileName: return _("File name");
            case AutoRigValueKind.Path: return _("File path");
            case AutoRigValueKind.Json: return _("JSON");
            case AutoRigValueKind.Blob: return _("Binary data");
        }
    }

    AutoRigValue draftValue(AutoRigValueKind kind, string draft) {
        final switch (kind) {
            case AutoRigValueKind.FileName:
                return AutoRigValue.fileName(draft);
            case AutoRigValueKind.Path:
                return AutoRigValue.path(draft);
            case AutoRigValueKind.Json:
                return AutoRigValue.jsonValue(parseJSON(draft));
            case AutoRigValueKind.Blob:
                return AutoRigValue.blob(cast(ubyte[])read(draft));
        }
    }

    void applyInput(IAutoRigInputContext context, AutoRigPortSpec port, string draft) {
        context.setValue(port.id, draftValue(port.kind, draft));
    }

    void renderSessionValue(AutoRigWorkflowRun run, string name, AutoRigValueKind kind,
        string current, bool contextValue) {
        auto key = draftKey(run, contextValue ? "session-context" : "workflow-input", name);
        if ((key in inputDrafts) is null || (key in inputObserved) !is null &&
            inputObserved[key] != current && inputDrafts[key] == inputObserved[key]) inputDrafts[key] = current;
        inputObserved[key] = current;
        igPushID(key.toStringz());
        igTextUnformatted((name ~ " (" ~ valueKindLabel(kind) ~ ")").toStringz());
        auto draft = inputDrafts[key];
        if (kind == AutoRigValueKind.Json) incInputTextMultiline("##value", draft, ImVec2(0, 80));
        else incInputText("##value", draft);
        inputDrafts[key] = draft;
        if (incButtonColored(__("Apply"))) {
            try {
                auto value = draftValue(kind, draft);
                if (contextValue) run.setContextValue(name, value);
                else run.setInput(name, value);
                synchronized (this) lastError = null;
            } catch (Exception error) {
                synchronized (this) lastError = error.msg;
            }
        }
        igPopID();
    }

    void renderSessionConfiguration(AutoRigWorkflowRun run) {
        igBeginDisabled(worker !is null);
        scope(exit) igEndDisabled();
        if (run.renderInputUI()) return;
        if (igTreeNodeEx(__("Workflow arguments"), ImGuiTreeNodeFlags.DefaultOpen)) {
            foreach (port; run.inputPorts()) {
                if (run.workflowId() == "model-to-rig" && port.id == "options") {
                    auto cached = run.id() in workflowOptionsDrafts;
                    if (cached is null || cached.revision != run.inputRevision()) {
                        auto options = run.input("options").json;
                        workflowOptionsDrafts[run.id()] = WorkflowOptionsDraft(run.inputRevision(),options,
                            ngRigGet(options,"render",JSONValue(false)).boolean);
                        cached = run.id() in workflowOptionsDrafts;
                    }
                    if (igCheckbox(_("Capture source appearance for verification").toStringz(),&cached.previews)) {
                        // Compile edits are task-local; preserve them when updating a shared workflow option.
                        cached.options = run.session().inputContext(run.stepTaskId("compile")).value("options").json;
                        cached.options["render"] = JSONValue(cached.previews);
                        run.setInput("options",AutoRigValue.jsonValue(cached.options));
                    }
                    igTextWrapped("%s",_("Material classification and landmark positions are edited in the compile task.").toStringz());
                    continue;
                }
                if (port.description.length) igTextWrapped("%s", _(port.description).toStringz());
                auto key = draftKey(run, "workflow-input", port.id);
                auto current = cachedInputText(key, run.inputRevision(),
                    () => run.hasInput(port.id) ? inputText(run.input(port.id)) : "");
                renderSessionValue(run, port.id, port.kind, current, false);
            }
            igTreePop();
        }
        if (igTreeNode(__("Session context / artifacts"))) {
            auto names = run.session().contextNames();
            names.sort();
            foreach (name; names) {
                auto info = run.session().contextInfo(name);
                auto key = draftKey(run, "session-context", name);
                auto current = cachedInputText(key, info.revision,
                    () => info.kind == AutoRigValueKind.Json ? inputText(run.contextValue(name)) : info.text);
                renderSessionValue(run, name, info.kind, current, true);
                if (info.kind == AutoRigValueKind.Blob)
                    igTextUnformatted(format(_("%s bytes"), info.byteLength).toStringz());
            }
            auto draft = run.id() in contextDrafts;
            if (draft is null) { contextDrafts[run.id()] = ContextDraft.init; draft = run.id() in contextDrafts; }
            incInputText(_("Name"), draft.name);
            if (igBeginCombo(__("Type"), valueKindLabel(draft.kind).toStringz())) {
                foreach (kind; [AutoRigValueKind.Path, AutoRigValueKind.Json,
                    AutoRigValueKind.Blob, AutoRigValueKind.FileName]) {
                    if (igSelectable(valueKindLabel(kind).toStringz(), kind == draft.kind)) draft.kind = kind;
                }
                igEndCombo();
            }
            incInputText(_("Value / artifact path"), draft.value);
            igTextWrapped("%s", _("Path references an artifact; Blob imports file contents; Json stores structured data.").toStringz());
            if (incButtonColored(__("Add context value"))) {
                try {
                    run.setContextValue(draft.name, draftValue(draft.kind, draft.value));
                    *draft = ContextDraft.init;
                    synchronized (this) lastError = null;
                } catch (Exception error) {
                    synchronized (this) lastError = error.msg;
                }
            }
            igTreePop();
        }
    }

    void renderSessionOutput(AutoRigWorkflowRun run) {
        if (!igTreeNode(__("Workflow output"))) return;
        scope(exit) igTreePop();
        if (run.renderOutputUI()) return;
        if (run.workflowId() == "model-to-rig") {
            igTextWrapped("%s",_("The resulting rig is applied to the editor model. Inspect task changes and verification below.").toStringz());
            return;
        }
        foreach (port; run.outputPorts()) {
            igTextUnformatted(port.id.toStringz());
            if (!run.hasOutput(port.id)) igTextUnformatted(_("Waiting for output").toStringz());
            else {
                auto artifact = run.outputArtifact(port.id);
                auto label = port.kind == AutoRigValueKind.Json ? _("JSON artifact ready") :
                    port.kind == AutoRigValueKind.Blob ? _("Binary artifact") : artifact.value.text;
                igTextUnformatted(label.toStringz());
                if (port.kind == AutoRigValueKind.Blob)
                    igTextUnformatted(format(_("%s bytes"), artifact.byteLength).toStringz());
            }
        }
    }

    void renderDefaultInputs(AutoRigWorkflowRun run, string taskId) {
        auto context = run.session().inputContext(taskId);
        foreach (port; context.ports()) {
            bool builtIn = run.workflowId() == "model-to-rig";
            if (builtIn && port.id == "options") {
                igTextWrapped("%s",_("Source capture is configured in workflow arguments; classification is edited in the compile task.").toStringz());
                continue;
            }
            if (port.id == "review") {
                igTextWrapped("%s",_("Operation settings are edited beside the task result below.").toStringz());
                continue;
            }
            igPushID(port.id.toStringz());
            auto label = !builtIn ? port.id ~ " (" ~ valueKindLabel(port.kind) ~ ")" :
                port.id == "state" ? _("Upstream processing result") : port.id == "program" ? _("Rig plan") :
                port.id == "model" ? _("Model checkpoint") : port.id == "materials" ? _("Material classification") : port.id;
            igTextUnformatted(label.toStringz());
            if (port.description.length) igTextUnformatted(_(port.description).toStringz());
            if (context.isConnected(port.id)) {
                auto value = !context.hasValue(port.id) ? _("Waiting for dependency") :
                    port.kind == AutoRigValueKind.Json ? builtIn ? _("Input from preceding task") : _("JSON artifact ready") :
                    port.kind == AutoRigValueKind.Blob ? _("Binary artifact") : inputText(context.value(port.id));
                igTextUnformatted(value.toStringz());
                foreach (connection; run.session().taskSpec(taskId).connections) if (connection.input == port.id) {
                    auto source = run.session().task(connection.sourceTask,false);
                    auto sourceSpec = run.session().taskSpec(connection.sourceTask);
                    igTextWrapped("%s",format(_("From %s / %s (attempt %s)"),_(sourceSpec.label),
                        connection.sourceOutput,source.attempt).toStringz());
                    bool hasReview;
                    foreach (output; sourceSpec.outputs) if (output.id == "review") hasReview = true;
                    if (hasReview && igTreeNode(__("Preceding stage result"))) {
                        auto cacheKey = draftKey(run,connection.sourceTask,"input-review");
                        auto cached = cacheKey in reviewDrafts;
                        if (worker is null && (cached is null || cached.attempt != source.attempt)) {
                            ReviewDraft draft; draft.attempt = source.attempt;
                            foreach (artifact; run.session().task(connection.sourceTask).artifacts)
                                if (artifact.portId == "review" || artifact.portId == "review-layout")
                                    draft.tables ~= ngRigReviewTables(ngCopyAutoRigValue(artifact.value).json,
                                        null,(message) => _(message));
                            reviewDrafts[cacheKey] = draft; cached = cacheKey in reviewDrafts;
                        }
                        if (cached !is null && cached.tables.length) renderReviewTables(cached.tables);
                        else igTextUnformatted(_("Waiting for task result").toStringz());
                        igTreePop();
                    }
                }
            } else {
                auto key = draftKey(run, taskId, port.id);
                auto current = cachedInputText(key, run.session().inputRevision(taskId),
                    () => context.hasValue(port.id) ? inputText(context.value(port.id)) : "");
                if ((key in inputDrafts) is null ||
                    (key in inputObserved) !is null && inputObserved[key] != current &&
                    inputDrafts[key] == inputObserved[key])
                    inputDrafts[key] = current;
                inputObserved[key] = current;
                auto draft = inputDrafts[key];
                igBeginDisabled(!context.canEdit(port.id));
                incInputText("##value", draft);
                inputDrafts[key] = draft;
                if (incButtonColored(__("Apply"))) {
                    try {
                        applyInput(context, port, draft);
                        synchronized (this) lastError = null;
                    } catch (Exception error) {
                        synchronized (this) lastError = error.msg;
                    }
                }
                igEndDisabled();
            }
            igPopID();
        }
    }

    void renderDefaultOutputs(AutoRigWorkflowRun run, string taskId) {
        auto context = run.session().outputContext(taskId);
        foreach (port; context.ports()) {
            igTextUnformatted((port.id ~ " (" ~ port.kind.to!string ~ ")").toStringz());
            if (context.hasValue(port.id)) {
                auto label = port.kind == AutoRigValueKind.Json ? _("JSON artifact ready") :
                    port.kind == AutoRigValueKind.Blob ? _("Binary artifact") : inputText(context.value(port.id));
                igTextUnformatted(label.toStringz());
            }
        }
        foreach (artifact; context.snapshot().artifacts) {
            auto label = (artifact.preview ? _("Preview: ") : _("Output")) ~ " " ~ artifact.portId;
            igTextUnformatted(label.toStringz());
        }
    }

    void renderReviewTables(ref RigReviewTable[] tables) {
        import std.algorithm : min;
        foreach (index,ref table; tables) {
            igPushID(cast(int)index);
            igTextUnformatted(table.title.toStringz());
            auto pages = (table.rows.length + 31) / 32;
            if (table.page >= pages) table.page = 0;
            if (pages > 1) {
                if (incButtonColored(__("Previous")) && table.page) --table.page;
                igSameLine(); if (incButtonColored(__("Next")) && table.page + 1 < pages) ++table.page;
                igSameLine(); igText("%u / %u",cast(uint)table.page + 1,cast(uint)pages);
            }
            auto flags = ImGuiTableFlags.RowBg | ImGuiTableFlags.Borders | ImGuiTableFlags.Resizable |
                ImGuiTableFlags.ScrollX | ImGuiTableFlags.SizingFixedFit;
            if (igBeginTable("##Review",cast(int)table.columns.length,flags)) {
                foreach (column; table.columns) igTableSetupColumn(column.toStringz());
                igTableHeadersRow();
                foreach (i; table.page * 32 .. min(table.rows.length,(table.page + 1) * 32)) {
                    igTableNextRow();
                    foreach (cell; table.rows[i]) { igTableNextColumn(); igTextUnformatted(cell.toStringz()); }
                }
                igEndTable();
            }
            igSeparator(); igPopID();
        }
    }

    bool renderRigOutput(AutoRigWorkflowRun run, string taskId) {
        auto spec = run.session().taskSpec(taskId);
        bool supported;
        foreach (port; spec.outputs) if (port.id == "review") supported = true;
        if (!supported) return false;
        auto task = run.session().task(taskId,false);
        auto key = draftKey(run,taskId,"review");
        auto cached = key in reviewDrafts;
        // Decode the small review artifact once per completed attempt, never per frame.
        if (worker is null && (cached is null || cached.attempt != task.attempt)) {
            ReviewDraft draft; draft.attempt = task.attempt;
            string[ulong] materialNames;
            if (run.workflowId() == "model-to-rig") {
                auto sourceId = run.stepTaskId("source");
                if (run.session().hasOutput(sourceId,"review"))
                    materialNames = ngRigReviewMaterialNames(run.session().output(sourceId,"review").json);
            }
            auto artifacts = run.session().task(taskId).artifacts;
            bool hasReport;
            foreach (artifact; artifacts) if (artifact.portId == "report") hasReport = true;
            foreach (artifact; artifacts) {
                if (artifact.portId == "report" || artifact.portId == "review-layout" ||
                    artifact.portId == "review" && !hasReport)
                    draft.tables ~= ngRigReviewTables(ngCopyAutoRigValue(artifact.value).json,
                        materialNames.dup,(message) => _(message));
                if (artifact.portId == "review-operations")
                    foreach (operation; ngCopyAutoRigValue(artifact.value).json.array) {
                        draft.operationIds ~= operation["id"].str;
                        auto plan = operation["plan"]; string targets;
                        foreach (field; ["part","source","target"]) if (auto target = field in plan.object) {
                            auto id = target.type == JSONType.uinteger ? target.uinteger : cast(ulong)target.integer;
                            auto name = id in materialNames;
                            targets ~= (targets.length ? " → " : "") ~ (name is null ? _("Unresolved target") : *name);
                        }
                        auto operationId = operation["id"].str;
                        auto parameter = ngRigString(plan,"parameter","");
                        auto kind = operationId.startsWith("mechanism:") ? _("Parameter") :
                            operationId.startsWith("control:") ? _("Parameter binding") :
                            operationId.startsWith("weld:") ? _("Shoulder welding") :
                            operationId.startsWith("mask:") ? _("Clipping mask") : _("Draw order");
                        auto description = kind ~ ": " ~ parameter ~ (parameter.length && targets.length ? " / " : "") ~ targets;
                        if (auto keys = "keys" in plan.object) description ~= format(_(" (%s keys)"),ngRigUnsigned(*keys));
                        if (auto zsort = "relative_zsort" in plan.object)
                            description ~= format(" (%.2f)",ngRigNumber(*zsort));
                        draft.operationLabels ~= description;
                        draft.enabled ~= operation["enabled"].boolean;
                    }
            }
            reviewDrafts[key] = draft; cached = key in reviewDrafts;
        }
        if (cached is null) { igTextUnformatted(_("Waiting for task result").toStringz()); return true; }
        if (cached.operationIds.length) {
            igTextWrapped("%s",_("Enable or disable individual operations, then rerun this task and its dependents.").toStringz());
            igBeginDisabled(worker !is null);
            if (igBeginChild("##Operations",ImVec2(0,200))) foreach (i,id; cached.operationIds) {
                igPushID(id.toStringz());
                igCheckbox("##enabled",&cached.enabled[i]); igSameLine();
                igTextWrapped("%s",cached.operationLabels[i].toStringz()); igPopID();
            }
            igEndChild();
            if (incButtonColored(__("Apply and rerun downstream"))) {
                JSONValue[] disabled;
                foreach (i,id; cached.operationIds) if (!cached.enabled[i]) disabled ~= JSONValue(id);
                try {
                    auto context = run.session().inputContext(taskId);
                    auto settings = context.hasValue("review") ? context.value("review").json :
                        JSONValue(cast(JSONValue[string])null);
                    settings["disabled"] = JSONValue(disabled);
                    context.setValue("review",AutoRigValue.jsonValue(settings));
                    startRun(run,null,false);
                } catch (Exception error) { synchronized (this) lastError = error.msg; }
            }
            igEndDisabled();
        }
        if (task.state == AutoRigTaskState.Stale)
            igTextWrapped("%s",_("Previous result. Settings changed; rerun to update it.").toStringz());
        if (cached.tables.length) renderReviewTables(cached.tables);
        else igTextUnformatted(_("Waiting for task result").toStringz());
        return true;
    }

    void renderMaterialRoles(AutoRigWorkflowRun run, string taskId) {
        auto context = run.session().inputContext(taskId);
        if (!context.hasValue("materials")) {
            igTextUnformatted(_("Observe the model first to configure material roles.").toStringz());
            return;
        }
        auto source = run.session().task(run.stepTaskId("source"), false);
        auto compiled = run.session().task(taskId,false);
        auto revision = run.session().inputRevision(taskId);
        auto cached = run.id() in materialDrafts;
        if (cached is null || cached.attempt != source.attempt || cached.compiledAttempt != compiled.attempt ||
            cached.revision != revision) {
            MaterialDraft draft;
            draft.attempt = source.attempt;
            draft.compiledAttempt = compiled.attempt; draft.revision = revision;
            // Acquire one owned snapshot per observation, never one per UI frame.
            auto materials = context.value("materials").json;
            foreach (artifact; run.session().task(taskId).artifacts) if (artifact.portId == "review") {
                auto report = ngCopyAutoRigValue(artifact.value).json;
                if (auto classification = "classification" in report.object) materials = *classification;
            }
            auto options = context.value("options").json;
            foreach (artifact; run.session().task(taskId).artifacts) if (artifact.portId == "evidence") {
                auto evidence = ngCopyAutoRigValue(artifact.value).json;
                if (auto landmarks = "landmarks" in evidence.object) {
                    auto names = landmarks.object.keys; names.sort;
                    foreach (name; names) {
                        auto item = (*landmarks)[name]; auto point = ngRigPoint(item["xy"]);
                        if (auto overrides = "landmarks" in options.object)
                            if (auto overridePoint = name in overrides.object) point = ngRigPoint(*overridePoint);
                        draft.landmarks ~= name; draft.landmarkX ~= point[0]; draft.landmarkY ~= point[1];
                        draft.landmarkReasons ~= ngRigString(item,"method",ngRigString(item,"provenance",""));
                        draft.landmarkChanged ~= false; draft.landmarkReset ~= false;
                    }
                }
            }
            foreach (material; materials.array) {
                auto name = material["name"].str, path = material["path"].str;
                auto candidates = ngRigMaterialRoleCandidates(name);
                string role = ngRigString(material,"role",candidates.length == 1 ? candidates[0] : "");
                auto feature = ngRigString(material,"feature","");
                auto side = ngRigString(material,"side_override","");
                bool disabled = ngRigGet(material,"static",JSONValue(!material["active"].boolean)).boolean;
                if (auto overrides = "materials" in options.object) if (auto entry = path in overrides.object) {
                    if (auto stationary = "static" in entry.object) disabled = stationary.boolean;
                    if (auto chosen = "role" in entry.object) role = chosen.str;
                    feature = ngRigString(*entry,"feature",feature); side = ngRigString(*entry,"side",side);
                }
                auto semanticSource = ngRigString(material,"semantic_source");
                bool forcedStatic = ngRigMaterialForcedStatic(material["active"].boolean,role,semanticSource);
                draft.requestedStatic ~= disabled;
                draft.semanticSources ~= semanticSource;
                disabled = disabled || forcedStatic;
                draft.names ~= name; draft.paths ~= path; draft.roles ~= role;
                draft.features ~= feature; draft.sides ~= side; draft.disabled ~= disabled;
                draft.active ~= material["active"].boolean;
                draft.forcedStatic ~= forcedStatic;
                draft.reasons ~= ngRigReviewEvidenceLabel(ngRigString(material,"semantic_source",""),(message) => _(message)) ~
                    " / " ~ ngRigString(material,"owner","") ~ " / " ~ ngRigString(material,"chart","");
                draft.changed ~= false; draft.reset ~= false;
                draft.editedFields ~= 0;
            }
            materialDrafts[run.id()] = draft;
            cached = run.id() in materialDrafts;
        }
        if (!materialRoleChoices.length) {
            materialRoleChoices = [""];
            foreach (rule; ngRigMaterialRoles()["rules"].array) materialRoleChoices ~= rule["id"].str;
        }
        igTextWrapped("%s", _("Inspect the actual classification and its evidence. Disabling rigging keeps the artwork static. Only edited rows become overrides.").toStringz());
        igCheckbox(_("Show unresolved only").toStringz(), &cached.unresolvedOnly);
        incInputText(_("Filter materials"),cached.filter);
        if (igBeginChild("##MaterialRoles", ImVec2(0, 240))) {
            if (igBeginTable("##MaterialClassification",6,ImGuiTableFlags.RowBg | ImGuiTableFlags.Borders |
                ImGuiTableFlags.Resizable | ImGuiTableFlags.ScrollX | ImGuiTableFlags.SizingFixedFit)) {
            foreach (header; [_("Material"),_("Role"),_("Static artwork"),_("Feature"),_("Model side"),
                _("Classification evidence")]) igTableSetupColumn(header.toStringz());
            igTableHeadersRow();
            foreach (i, path; cached.paths) {
                if (cached.unresolvedOnly && cached.roles[i].length) continue;
                if (cached.filter.length && !path.toLower.canFind(cached.filter.toLower) &&
                    !cached.names[i].toLower.canFind(cached.filter.toLower)) continue;
                igPushID(path.toStringz());
                igTableNextRow(); igTableNextColumn();
                igTextUnformatted(cached.names[i].toStringz());
                if (igIsItemHovered()) igSetTooltip("%s", path.toStringz());
                igTableNextColumn(); igSetNextItemWidth(150);
                auto label = cached.roles[i].length ? ngRigReviewClassLabel(cached.roles[i],(message) => _(message)) : _("Choose role");
                if (igBeginCombo("##role", label.toStringz())) {
                    foreach (choice; materialRoleChoices) {
                        auto display = choice.length ? ngRigReviewClassLabel(choice,(message) => _(message)) : _("Choose role");
                        if (igSelectable(display.toStringz(), choice == cached.roles[i])) {
                            cached.roles[i] = choice; cached.changed[i] = true; cached.reset[i] = false;
                            cached.editedFields[i] |= RigMaterialField.role;
                            cached.forcedStatic[i] = ngRigMaterialForcedStatic(cached.active[i],choice,
                                cached.semanticSources[i]);
                            cached.disabled[i] = cached.forcedStatic[i] || cached.requestedStatic[i];
                        }
                    }
                    igEndCombo();
                }
                igTableNextColumn();
                igBeginDisabled(cached.forcedStatic[i]);
                if (igCheckbox("##static",&cached.disabled[i])) {
                    cached.requestedStatic[i] = cached.disabled[i];
                    cached.changed[i] = true; cached.reset[i] = false;
                    cached.editedFields[i] |= RigMaterialField.stationary;
                }
                igEndDisabled();
                auto featureLabel = ngRigReviewClassLabel(cached.features[i],(message) => _(message));
                igTableNextColumn(); igSetNextItemWidth(150);
                if (igBeginCombo("##feature",featureLabel.toStringz())) {
                    foreach (feature; ["", "mouth", "mouth_tongue", "mouth_upper_teeth", "mouth_lower_teeth",
                        "mouth_outline", "mouth_upper_lip", "mouth_lower_lip", "brow", "sclera", "iris", "corner",
                        "upper", "lower", "fold", "nose"])
                        if (igSelectable(ngRigReviewClassLabel(feature,(message) => _(message)).toStringz(),
                            feature == cached.features[i])) {
                            cached.features[i] = feature; cached.changed[i] = true; cached.reset[i] = false;
                            cached.editedFields[i] |= RigMaterialField.feature;
                        }
                    igEndCombo();
                }
                igTableNextColumn(); igSetNextItemWidth(110);
                if (igBeginCombo("##side",cached.sides[i].length ? ngRigReviewClassLabel(cached.sides[i],(message) => _(message)).toStringz() :
                    _("Automatic").toStringz())) {
                    foreach (side; ["","L","R"]) if (igSelectable(side.length ? ngRigReviewClassLabel(side,(message) => _(message)).toStringz() :
                        _("Automatic").toStringz(),side == cached.sides[i])) {
                        cached.sides[i] = side; cached.changed[i] = true; cached.reset[i] = false;
                        cached.editedFields[i] |= RigMaterialField.side;
                    }
                    igEndCombo();
                }
                igTableNextColumn(); igTextUnformatted(cached.reasons[i].toStringz());
                if (incButtonColored(__("Reset to automatic"))) { cached.changed[i] = true; cached.reset[i] = true; }
                if (cached.changed[i]) igTextUnformatted(_("Unapplied changes").toStringz());
                igPopID();
            }
            igEndTable();
            }
        }
        igEndChild();
        if (cached.landmarks.length && igTreeNode(__("Anatomical landmarks"))) {
            igTextWrapped("%s",_("Edit measured landmark positions in model coordinates. The scaffold is solved again from these inputs.").toStringz());
            foreach (i,name; cached.landmarks) {
                igPushID(name.toStringz());
                igTextUnformatted(name.toStringz());
                if (igInputDouble("X",&cached.landmarkX[i])) {
                    cached.landmarkChanged[i] = true; cached.landmarkReset[i] = false;
                }
                if (igInputDouble("Y",&cached.landmarkY[i])) {
                    cached.landmarkChanged[i] = true; cached.landmarkReset[i] = false;
                }
                igTextWrapped("%s",cached.landmarkReasons[i].toStringz());
                if (incButtonColored(__("Reset to automatic"))) {
                    cached.landmarkChanged[i] = true; cached.landmarkReset[i] = true;
                }
                if (cached.landmarkChanged[i]) igTextUnformatted(_("Unapplied changes").toStringz());
                igPopID();
            }
            igTreePop();
        }
        bool apply = incButtonColored(_("Apply review settings").toStringz());
        igSameLine(); bool rerun = incButtonColored(__("Apply and rerun downstream"));
        if (apply || rerun) try {
            auto options = context.value("options").json;
            JSONValue[string] overrides;
            if (auto previous = "materials" in options.object) overrides = previous.object;
            foreach (i, path; cached.paths) {
                if (!cached.changed[i]) continue;
                if (cached.reset[i]) { overrides.remove(path); continue; }
                auto entry = path in overrides;
                overrides[path] = ngRigEditedMaterialOverride(entry is null ? JSONValue.init : *entry,
                    cached.editedFields[i],cached.roles[i],cached.disabled[i],cached.features[i],cached.sides[i]);
            }
            options["materials"] = JSONValue(overrides);
            JSONValue[string] landmarks;
            if (auto previous = "landmarks" in options.object) landmarks = previous.object.dup;
            foreach (i,name; cached.landmarks) if (cached.landmarkChanged[i]) {
                if (cached.landmarkReset[i]) landmarks.remove(name);
                else landmarks[name] = JSONValue([cached.landmarkX[i],cached.landmarkY[i]]);
            }
            options["landmarks"] = JSONValue(landmarks);
            context.setValue("options", AutoRigValue.jsonValue(options));
            if (rerun) startRun(run,null,false);
        } catch (Exception error) { synchronized (this) lastError = error.msg; }
    }

    void renderTask(AutoRigWorkflowRun run, AutoRigWorkflowStep step) {
        auto taskId = run.stepTaskId(step.id);
        auto task = run.session().task(taskId, false);
        auto state = task.state;
        igPushID(step.id.toStringz());
        auto spec = run.session().taskSpec(taskId);
        auto title = spec.label.length ? _(spec.label) : step.id;
        igBeginDisabled(worker !is null);
        if (incButtonColored("\ue037", ImVec2(24, 24))) startRun(run, step.id);
        igEndDisabled();
        igSameLine();
        bool open = renderStateTree(title, state.to!string, "task", ImGuiTreeNodeFlags.None);
        if (open) {
            if (igTreeNodeEx(__("Input"), ImGuiTreeNodeFlags.DefaultOpen)) {
                igBeginDisabled(worker !is null);
                if (step.taskId == "compile-rig") renderMaterialRoles(run, taskId);
                else if (!run.session().renderInputUI(taskId)) renderDefaultInputs(run, taskId);
                igEndDisabled();
                igTreePop();
            }
            if (igTreeNodeEx(__("Output"), ImGuiTreeNodeFlags.DefaultOpen)) {
                if (!renderRigOutput(run,taskId) && !run.session().renderOutputUI(taskId)) renderDefaultOutputs(run, taskId);
                igTreePop();
            }
            auto message = task.message;
            if (message.length) igTextWrapped("%s", message.toStringz());
            igTreePop();
        }
        igPopID();
    }

    bool renderStateTree(string title, string state, string id, ImGuiTreeNodeFlags flags) {
        ImVec4 color;
        string icon, status;
        switch (state) {
            case "Running":
                color = ImVec4(0, 0.4, 0.8, 1);
                icon = "\ue1c4"; status = _("Running");
                break;
            case "Succeeded":
                color = ImVec4(0, 0.9, 0, 1);
                icon = "\ue92f"; status = _("Completed");
                break;
            case "Failed":
                color = ImVec4(0.9, 0, 0, 1);
                icon = "\uf8b6"; status = _("Failed");
                break;
            case "Canceled":
                color = ImVec4(0.8, 0.4, 0, 1);
                icon = "\ue5c9"; status = _("Canceled");
                break;
            case "Stale":
                color = ImVec4(0.8, 0.4, 0, 1);
                icon = "\uef4a"; status = _("Needs rerun");
                break;
            default:
                color = ImVec4(0.8, 0.4, 0, 1);
                icon = "\uef4a"; status = _("Pending");
                break;
        }
        // Match AutoMeshBatch's status symbols and colors, with stable tree IDs.
        igTextColored(color, "%s", icon.toStringz());
        if (igIsItemHovered()) igSetTooltip("%s", status.toStringz());
        igSameLine(0, 0);
        auto label = title ~ "###" ~ id;
        return igTreeNodeEx(label.toStringz(), flags);
    }

    bool renderRun(AutoRigWorkflowRun run) {
        igPushID(run.id().toStringz());
        scope(exit) igPopID();
        auto snapshot = run.snapshot(false);
        auto label = _(run.displayName()) ~ " #" ~ run.id()[0 .. 8];
        igBeginDisabled(activeRun is run || run.session().isBusy());
        bool remove = incButtonColored("\ue872##delete", ImVec2(24, 24));
        igEndDisabled();
        if (igIsItemHovered()) igSetTooltip("%s", _("Delete session").toStringz());
        if (remove) return true;
        igSameLine();
        bool running = worker !is null && activeRun is run;
        igBeginDisabled(worker !is null && !running || running && run.session().isCanceled());
        if (incButtonColored(running ? "\ue5c9" : "\ue037", ImVec2(24, 24))) {
            if (running) run.cancel();
            else startRun(run);
        }
        igEndDisabled();
        if (igIsItemHovered()) igSetTooltip("%s", (running ? _("Cancel") : _("Run batch")).toStringz());
        igSameLine();
        bool open = renderStateTree(label, snapshot.state.to!string, "run", ImGuiTreeNodeFlags.DefaultOpen);
        if (open) {
            renderSessionConfiguration(run);
            renderSessionOutput(run);
            foreach (step; run.orderedSteps()) renderTask(run, step);
            igTreePop();
        }
        return false;
    }

    void removeRun(string runId) {
        workflowManager().remove(runId);
        AutoRigWorkflowRun[] remaining;
        foreach (run; runs) if (run.id() != runId) remaining ~= run;
        runs = remaining;
        materialDrafts.remove(runId);
        foreach (key; reviewDrafts.keys) if (key.startsWith(runId ~ "/")) reviewDrafts.remove(key);
        contextDrafts.remove(runId);
        workflowOptionsDrafts.remove(runId);
        foreach (key; inputDrafts.keys) if (key.startsWith(runId ~ "/")) inputDrafts.remove(key);
        foreach (key; inputObserved.keys) if (key.startsWith(runId ~ "/")) inputObserved.remove(key);
        foreach (key; inputTextCache.keys) if (key.startsWith(runId ~ "/")) inputTextCache.remove(key);
    }

protected:
    override void onInit() {
        import nijigenerate.commands.puppet.tool : ngSetAutoRigCommandHandlers;
        ngSetAutoRigCommandHandlers(&executeImportedModel,&runStatus,&resumeWorkflow,&removeRun);
    }

    override void onUpdate() {
        finishWorker();
        renderPresetPicker();
        string error;
        synchronized (this) error = lastError;
        if (error.length) igTextWrapped("%s", error.toStringz());
        igSeparator();
        string removedRun;
        if (igBeginChild("##AutoRigSessions", ImVec2(0, 0)))
            foreach (run; runs) if (renderRun(run)) removedRun = run.id();
        igEndChild();
        if (removedRun.length) removeRun(removedRun);
    }

public:
    string resumeWorkflow(string runId, string stepId) {
        finishWorker();
        import std.exception : enforce;
        enforce(worker is null,"An AutoRig workflow is already running");
        auto run = workflowManager().reopen(runId); bool present;
        foreach (existing; runs) if (existing.id() == run.id()) present = true;
        if (!present) runs ~= run;
        startRun(run,stepId,false); return run.id();
    }

    string executeImportedModel(JSONValue options, JSONValue context) {
        finishWorker();
        import std.exception : enforce;
        enforce(worker is null,"An AutoRig workflow is already running");
        auto run = workflowManager().createConfigured("anime-front-view-rig","model-to-rig",
            ["options":AutoRigValue.jsonValue(options)],context);
        runs ~= run; startRun(run,null,false); return run.id();
    }

    JSONValue runStatus(string runId) {
        finishWorker();
        import core.memory : GC;
        if (!runId.length) {
            auto gc = GC.stats();
            ulong jsonBytes, blobBytes, cpuBytes;
            bool busy;
            foreach (run; runs) {
                auto info = run.session().memoryInfo();
                jsonBytes += info["json_bytes"].uinteger;
                blobBytes += info["blob_bytes"].uinteger;
                cpuBytes += info["cpu_bytes"].uinteger;
                busy = busy || run.session().isBusy();
            }
            return JSONValue(["state":JSONValue(busy ? "Running" : "Idle"),
                "memory":JSONValue(["json_bytes":JSONValue(jsonBytes),"blob_bytes":JSONValue(blobBytes),
                    "cpu_bytes":JSONValue(cpuBytes),
                    "gc_used_bytes":JSONValue(cast(ulong)gc.usedSize),
                    "gc_free_bytes":JSONValue(cast(ulong)gc.freeSize)])]);
        }
        foreach (run; runs) if (run.id() == runId) {
            auto snapshot = run.snapshot(); JSONValue[] steps;
            foreach (step; run.orderedSteps()) {
                auto state = run.session().task(run.stepTaskId(step.id));
                steps ~= JSONValue(["id":JSONValue(step.id),"state":JSONValue(state.state.to!string),
                    "attempt":JSONValue(state.attempt),"message":JSONValue(state.message)]);
            }
            auto gc = GC.stats();
            auto memory = run.session().memoryInfo();
            memory["gc_used_bytes"] = JSONValue(cast(ulong)gc.usedSize);
            memory["gc_free_bytes"] = JSONValue(cast(ulong)gc.freeSize);
            auto result = JSONValue(["run_id":JSONValue(runId),"state":JSONValue(snapshot.state.to!string),
                "message":JSONValue(snapshot.message),"steps":JSONValue(steps),"memory":memory]);
            import nijigenerate.project : incActivePuppet, incArmedParameter;
            auto puppet = incActivePuppet();
            result["editor"] = JSONValue(["panel_visible":JSONValue(visible),
                "worker_active":JSONValue(worker !is null),
                "commands_blocked":JSONValue(ngMcpExternalCommandsBlocked()),
                "drivers_enabled":JSONValue(puppet !is null && puppet.enableDrivers),
                "armed_parameter":JSONValue(incArmedParameter() is null ? 0u : incArmedParameter().uuid)]);
            import nijigenerate.panels.resource : ngResourcePanelReadback;
            result["resource_view"] = ngResourcePanelReadback();
            foreach (key, value; ngAutoRigWorkflowDiagnostics(run).object) result[key] = value;
            return result;
        }
        throw new Exception("Unknown AutoRig workflow run: " ~ runId);
    }

    this() {
        super("AutoRig", _("AutoRig"), true);
        activeModes = EditMode.ModelEdit;
    }

    void stop() {
        if (worker !is null) {
            if (activeRun !is null) activeRun.cancel();
            while (worker.isRunning()) {
                ngMcpProcessQueue();
                Thread.sleep(1.msecs);
            }
            worker.join();
        }
        worker = null;
        activeRun = null;
        ngMcpSetExternalCommandsBlocked(false);
        restoreDriverState();
        if (workflows !is null) workflows.disposeAll();
        runs = null;
        inputDrafts = null;
        inputObserved = null;
        inputTextCache = null;
        contextDrafts = null;
        materialDrafts = null;
        reviewDrafts = null;
        workflowOptionsDrafts = null;
    }
}

void ngAutoRigStopAll() {
    auto panel = cast(AutoRigPanel)incFindPanelByName("AutoRig");
    if (panel !is null) panel.stop();
    if (sharedSessions !is null) sharedSessions.disposeAll();
}

/** Reap completed workers even when their panel is hidden or inactive. */
void ngAutoRigPollWorker() {
    auto panel = cast(AutoRigPanel)incFindPanelByName("AutoRig");
    if (panel !is null) panel.finishWorker();
}

mixin incPanel!AutoRigPanel;
