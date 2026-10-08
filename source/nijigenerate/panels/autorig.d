module nijigenerate.panels.autorig;

import bindbc.imgui;
import core.thread : Thread;
import core.time : msecs;
import i18n;
import nijigenerate : EditMode;
import nijigenerate.api.mcp.task : ngMcpProcessQueue, ngRunInMainThread;
import nijigenerate.autorig;
import nijigenerate.autorig.deterministic.processor : AnimeFrontViewRigProcessor;
import nijigenerate.autorig.deterministic.editor : ngApplyFaceProjection;
import nijigenerate.autorig.deterministic.native : ngRigNativeStage;
import nijigenerate.autorig.deterministic.evidence : ngRigMaterialRoleCandidates;
import nijigenerate.autorig.deterministic.templates : ngRigMaterialRoles;
import std.string : endsWith, startsWith;
import nijigenerate.core.actionstack : incActionPushGroup, incActionPopGroup;
import nijigenerate.core.path : incGetAppConfigPath;
import nijigenerate.panels : Panel, incPanel, incAddPanel, incFindPanelByName;
import nijigenerate.utils.crashdump : installNativeCrashDumpThreadHandler;
import nijigenerate.widgets : incButtonColored, incInputText, incInputTextMultiline;
import nijigenerate.widgets.modal : Modal, incModalAdd, incModalCloseTop;
import std.algorithm.sorting : sort;
import std.conv : to;
import std.file : read;
import std.json : parseJSON, JSONValue;
import std.path : buildPath;
import std.string : toStringz;
import std.format : format;

private AutoRigSessionManager sharedSessions;

private class AutoRigProgressWindow : Modal {
    AutoRigPanel owner;
    AutoRigWorkflowRun run;

    this(AutoRigPanel owner, AutoRigWorkflowRun run) {
        super(_("AutoRig"), false);
        this.owner = owner;
        this.run = run;
        flags |= ImGuiWindowFlags.AlwaysAutoResize;
    }

    protected override void onUpdate() {
        owner.finishWorker();
        if (owner.worker is null) {
            igCloseCurrentPopup();
            incModalCloseTop();
            return;
        }
        owner.renderRun(run);
    }
}

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
            foreach (stage; ["observe-model","prepare-source-groups","mesh-parts","register-source-uv","build-native-rig","bake-depth-angles",
                "prepare-shoulders","prepare-feature-composites","compile-domain-layout","weld-shoulders",
                "apply-rig-controls","apply-shape-corrections",
                "validate-depth-inputs","verify-saved-rig"]) if (taskId.endsWith(stage)) {
                // These stages restore a checkpoint, then own their action group.
                execute(); return;
            }
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
    string lastError;
    struct MaterialDraft {
        uint attempt;
        string[] names, paths, roles;
        bool unresolvedOnly = true;
    }
    MaterialDraft[string] materialDrafts;
    string[] materialRoleChoices;

    AutoRigWorkflowManager workflowManager() {
        if (workflows is null) workflows = new AutoRigWorkflowManager(ngAutoRigSessionManager());
        return workflows;
    }

    void finishWorker() {
        if (worker !is null && !worker.isRunning()) {
            worker.join();
            worker = null;
            activeRun = null;
        }
    }

    void startRun(AutoRigWorkflowRun run, string stepId = null) {
        finishWorker();
        if (worker !is null) return;
        lastError = null;
        activeRun = run;
        worker = new Thread({
            installNativeCrashDumpThreadHandler();
            try {
                if (stepId.length) run.executeStep(stepId, true);
                else run.execute(false);
            } catch (Throwable error) {
                synchronized (this) lastError = error.msg;
            }
        });
        worker.start();
        incModalAdd(new AutoRigProgressWindow(this, run));
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
            igPushID(port.id.toStringz());
            igTextUnformatted((port.id ~ " (" ~ valueKindLabel(port.kind) ~ ")").toStringz());
            if (port.description.length) igTextUnformatted(_(port.description).toStringz());
            if (context.isConnected(port.id)) {
                auto value = !context.hasValue(port.id) ? _("Waiting for dependency") :
                    port.kind == AutoRigValueKind.Json ? _("JSON artifact ready") :
                    port.kind == AutoRigValueKind.Blob ? _("Binary artifact") : inputText(context.value(port.id));
                igTextUnformatted(value.toStringz());
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

    void renderMaterialRoles(AutoRigWorkflowRun run, string taskId) {
        auto context = run.session().inputContext(taskId);
        if (!context.hasValue("materials")) {
            igTextUnformatted(_("Observe the model first to configure material roles.").toStringz());
            return;
        }
        auto source = run.session().task(run.stepTaskId("source"), false);
        auto cached = run.id() in materialDrafts;
        if (cached is null || cached.attempt != source.attempt) {
            MaterialDraft draft;
            draft.attempt = source.attempt;
            // Acquire one owned snapshot per observation, never one per UI frame.
            auto materials = context.value("materials").json;
            auto options = context.value("options").json;
            foreach (material; materials.array) {
                if (!material["active"].boolean) continue;
                auto name = material["name"].str, path = material["path"].str;
                auto candidates = ngRigMaterialRoleCandidates(name);
                string role = candidates.length == 1 ? candidates[0] : "";
                if (auto overrides = "materials" in options.object) if (auto entry = path in overrides.object) {
                    if (auto stationary = "static" in entry.object) if (stationary.boolean) role = "static";
                    if (role != "static") if (auto chosen = "role" in entry.object) role = chosen.str;
                }
                draft.names ~= name; draft.paths ~= path; draft.roles ~= role;
            }
            materialDrafts[run.id()] = draft;
            cached = run.id() in materialDrafts;
        }
        if (!materialRoleChoices.length) {
            materialRoleChoices = ["", "static"];
            foreach (rule; ngRigMaterialRoles()["rules"].array) materialRoleChoices ~= rule["id"].str;
        }
        igTextWrapped("%s", _("Materials are classified automatically from names, hierarchy, clipping and alpha support. These overrides are optional. Static keeps the material unrigged.").toStringz());
        igCheckbox(_("Show unresolved only").toStringz(), &cached.unresolvedOnly);
        if (igBeginChild("##MaterialRoles", ImVec2(0, 240))) {
            foreach (i, path; cached.paths) {
                if (cached.unresolvedOnly && cached.roles[i].length) continue;
                igPushID(path.toStringz());
                igTextUnformatted(cached.names[i].toStringz());
                if (igIsItemHovered()) igSetTooltip("%s", path.toStringz());
                auto label = cached.roles[i].length ? cached.roles[i] : _("Choose role");
                if (igBeginCombo("##role", label.toStringz())) {
                    foreach (choice; materialRoleChoices) {
                        auto display = choice.length ? choice : _("Choose role");
                        if (igSelectable(display.toStringz(), choice == cached.roles[i])) cached.roles[i] = choice;
                    }
                    igEndCombo();
                }
                igPopID();
            }
        }
        igEndChild();
        if (incButtonColored(_("Apply material roles").toStringz())) {
            auto options = context.value("options").json;
            JSONValue[string] overrides;
            if (auto previous = "materials" in options.object) overrides = previous.object;
            foreach (i, path; cached.paths) {
                if (cached.roles[i].length) {
                    overrides[path] = cached.roles[i] == "static" ? JSONValue(["static":JSONValue(true)]) :
                        JSONValue(["role":JSONValue(cached.roles[i])]);
                } else overrides.remove(path);
            }
            options["materials"] = JSONValue(overrides);
            context.setValue("options", AutoRigValue.jsonValue(options));
        }
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
                if (!run.session().renderOutputUI(taskId)) renderDefaultOutputs(run, taskId);
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
        contextDrafts.remove(runId);
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
        startRun(run,stepId); return run.id();
    }

    string executeImportedModel(JSONValue options, JSONValue context) {
        finishWorker();
        import std.exception : enforce;
        enforce(worker is null,"An AutoRig workflow is already running");
        auto run = workflowManager().create("anime-front-view-rig","model-to-rig");
        run.setInput("options",AutoRigValue.jsonValue(options));
        run.setContext(context);
        runs ~= run; startRun(run); return run.id();
    }

    JSONValue runStatus(string runId) {
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
            import nijigenerate.panels.resource : ngResourcePanelReadback;
            result["resource_view"] = ngResourcePanelReadback();
            auto compileTask = run.session().task(run.stepTaskId("compile"));
            JSONValue[] compileProfile;
            foreach (artifact; compileTask.artifacts)
                if (artifact.preview && artifact.portId == "compile-profile")
                    compileProfile ~= artifact.value.json;
            if (compileProfile.length) result["compile_profile"] = JSONValue(compileProfile);
            auto verifyId = run.stepTaskId("verify");
            if (!run.session().isBusy() && run.session().hasOutput(verifyId,"report")) {
                auto report = run.session().output(verifyId,"report").json;
                JSONValue[string] summary;
                foreach (key; ["passed","error","readback_verified","finish_stages",
                    "all_finish_stages_succeeded","numerical_stages_passed","rig_complete"])
                    if (auto entry = key in report.object) summary[key] = *entry;
                if (auto findings = "numerical_findings" in report.object) {
                    summary["numerical_findings_count"] = JSONValue(cast(ulong) (*findings).array.length);
                    // Keep status diagnostics bounded without copying the full validation report.
                    import std.algorithm : min;
                    auto entries = (*findings).array;
                    summary["numerical_findings"] = JSONValue(entries[0 .. min(entries.length, 32)].dup);
                }
                result["verification"] = JSONValue(summary);
            }
            return result;
        }
        throw new Exception("Unknown AutoRig workflow run: " ~ runId);
    }

    this() {
        super("AutoRig", _("AutoRig"), true);
        activeModes = EditMode.ModelEdit;
    }

    void stop() {
        if (worker is null) return;
        if (activeRun !is null) activeRun.cancel();
        while (worker.isRunning()) {
            ngMcpProcessQueue();
            Thread.sleep(1.msecs);
        }
        worker.join();
        worker = null;
        activeRun = null;
    }
}

void ngAutoRigStopAll() {
    auto panel = cast(AutoRigPanel)incFindPanelByName("AutoRig");
    if (panel !is null) panel.stop();
}

mixin incPanel!AutoRigPanel;
