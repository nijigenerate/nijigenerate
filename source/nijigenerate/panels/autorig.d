module nijigenerate.panels.autorig;

import bindbc.imgui;
import core.thread : Thread;
import core.time : msecs;
import i18n;
import nijigenerate : EditMode;
import nijigenerate.api.mcp.task : ngMcpProcessQueue, ngRunInMainThread;
import nijigenerate.autorig;
import nijigenerate.autorig.deterministic.processor : DeterministicRigProcessor;
import nijigenerate.autorig.deterministic.editor : ngApplyFaceProjection;
import nijigenerate.autorig.deterministic.native : ngRigNativeStage;
import nijigenerate.autorig.deterministic.evidence : ngRigMaterialRoleCandidates;
import nijigenerate.autorig.deterministic.templates : ngRigMaterialRoles;
import std.string : endsWith;
import nijigenerate.core.actionstack : incActionPushGroup, incActionPopGroup;
import nijigenerate.core.path : incGetAppConfigPath;
import nijigenerate.panels : Panel, incPanel, incAddPanel, incFindPanelByName;
import nijigenerate.utils.crashdump : installNativeCrashDumpThreadHandler;
import nijigenerate.widgets : incButtonColored, incInputText;
import std.algorithm.sorting : sort;
import std.conv : to;
import std.file : read;
import std.json : parseJSON, JSONValue;
import std.path : buildPath;
import std.string : toStringz;
import std.math : sin;

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
        sharedSessions.registerProcessor(new DeterministicRigProcessor(
              (projection, target, context) => ngApplyFaceProjection(projection, target, context),
              (stage, state, program, model, context) => ngRigNativeStage(stage, state, program, model, context)));
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
    Thread worker;
    AutoRigWorkflowRun activeRun;
    string lastError;
    struct MaterialDraft {
        uint attempt;
        string[] names, paths, roles;
        bool unresolvedOnly = true;
    }
    MaterialDraft[string] materialDrafts;

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
    }

    string presetLabel(AutoRigWorkflowPreset preset) {
        auto owner = ngAutoRigSessionManager().processor(preset.providerId);
        return owner.displayName() ~ " / " ~
            (preset.spec.label.length ? preset.spec.label : preset.spec.id);
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
                description = preset.spec.description;
            }

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
        igSameLine();
        igBeginDisabled(!presets.length || worker !is null);
        if (incButtonColored("\ue037", ImVec2(24, 24))) {
            try {
                auto run = workflowManager().create(selectedProviderId, selectedWorkflowId);
                runs ~= run;
                startRun(run);
                synchronized (this) lastError = null;
            } catch (Exception error) {
                synchronized (this) lastError = error.msg;
            }
        }
        igEndDisabled();
        if (igIsItemHovered()) igSetTooltip("%s", _("Run workflow on the imported model").toStringz());
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

    void applyInput(IAutoRigInputContext context, AutoRigPortSpec port, string draft) {
        final switch (port.kind) {
            case AutoRigValueKind.FileName:
                context.setValue(port.id, AutoRigValue.fileName(draft));
                break;
            case AutoRigValueKind.Path:
                context.setValue(port.id, AutoRigValue.path(draft));
                break;
            case AutoRigValueKind.Json:
                context.setValue(port.id, AutoRigValue.jsonValue(parseJSON(draft)));
                break;
            case AutoRigValueKind.Blob:
                context.setValue(port.id, AutoRigValue.blob(cast(ubyte[])read(draft)));
                break;
        }
    }

    void renderDefaultInputs(AutoRigWorkflowRun run, string taskId) {
        auto context = run.session().inputContext(taskId);
        foreach (port; context.ports()) {
            igPushID(port.id.toStringz());
            igTextUnformatted((port.id ~ " (" ~ port.kind.to!string ~ ")").toStringz());
            if (port.description.length) igTextUnformatted(port.description.toStringz());
            if (context.isConnected(port.id)) {
                auto value = !context.hasValue(port.id) ? _("Waiting for dependency") :
                    port.kind == AutoRigValueKind.Json ? _("JSON artifact ready") : inputText(context.value(port.id));
                igTextUnformatted(value.toStringz());
            } else {
                auto key = draftKey(run, taskId, port.id);
                auto current = context.hasValue(port.id) ? inputText(context.value(port.id)) : "";
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
            auto label = (artifact.preview ? _("Preview: ") : _("File: ")) ~ artifact.storagePath;
            igTextUnformatted(label.toStringz());
        }
    }

    void renderMaterialRoles(AutoRigWorkflowRun run, string taskId) {
        auto context = run.session().inputContext(taskId);
        if (!context.hasValue("materials")) {
            igTextUnformatted(_("Observe the model first to configure material roles.").toStringz());
            return;
        }
        auto source = run.session().task(run.stepTaskId("source"));
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
        string[] choices = ["", "static"];
        foreach (rule; ngRigMaterialRoles()["rules"].array) choices ~= rule["id"].str;
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
                    foreach (choice; choices) {
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
            foreach (i, path; cached.paths) if (cached.roles[i].length) {
                overrides[path] = cached.roles[i] == "static" ? JSONValue(["static":JSONValue(true)]) :
                    JSONValue(["role":JSONValue(cached.roles[i])]);
            }
            options["materials"] = JSONValue(overrides);
            context.setValue("options", AutoRigValue.jsonValue(options));
        }
    }

    void renderTask(AutoRigWorkflowRun run, AutoRigWorkflowStep step) {
        auto taskId = run.stepTaskId(step.id);
        auto state = run.session().task(taskId).state;
        igPushID(step.id.toStringz());
        auto spec = run.session().taskSpec(taskId);
        auto title = spec.label.length ? spec.label : step.id;
        bool open = renderStateTree(title, state.to!string, "task", ImGuiTreeNodeFlags.None);
        igSameLine();
        igBeginDisabled(worker !is null);
        if (incButtonColored("\ue037", ImVec2(24, 24))) startRun(run, step.id);
        igEndDisabled();
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
            auto message = run.session().task(taskId).message;
            if (message.length) igTextWrapped("%s", message.toStringz());
            igTreePop();
        }
        igPopID();
    }

    bool renderStateTree(string title, string state, string id, ImGuiTreeNodeFlags flags) {
        ImVec4 color = *igGetStyleColorVec4(ImGuiCol.Text);
        string marker, status;
        switch (state) {
            case "Running":
                auto phase = igGetTime();
                float pulse = cast(float)(.75 + .25 * sin(phase * 4));
                color = ImVec4(.35f * pulse, .75f * pulse, 1f * pulse, 1);
                marker = ["[|] ", "[/] ", "[-] ", "[\\] "][cast(size_t)(phase * 8) % 4];
                status = _("Running");
                break;
            case "Succeeded":
                color = ImVec4(.35f, .85f, .45f, 1);
                marker = "[OK] "; status = _("Completed");
                break;
            case "Failed":
                color = ImVec4(1, .4f, .35f, 1);
                marker = "[!] "; status = _("Failed");
                break;
            case "Canceled":
                color = ImVec4(.95f, .7f, .3f, 1);
                marker = "[-] "; status = _("Canceled");
                break;
            case "Stale":
                color = ImVec4(.95f, .7f, .3f, 1);
                marker = "[~] "; status = _("Needs rerun");
                break;
            default:
                color = *igGetStyleColorVec4(ImGuiCol.TextDisabled);
                marker = "[ ] "; status = _("Pending");
                break;
        }
        // A stable ID preserves the expanded state while the status animates or changes.
        auto label = marker ~ title ~ " [" ~ status ~ "]###" ~ id;
        igPushStyleColor(ImGuiCol.Text, color);
        bool open = igTreeNodeEx(label.toStringz(), flags);
        igPopStyleColor();
        return open;
    }

    void renderRun(AutoRigWorkflowRun run) {
        igPushID(run.id().toStringz());
        auto snapshot = run.snapshot();
        auto label = snapshot.workflowId ~ " #" ~ run.id()[0 .. 8];
        bool open = renderStateTree(label, snapshot.state.to!string, "run", ImGuiTreeNodeFlags.DefaultOpen);
        igSameLine();
        igBeginDisabled(worker !is null);
        if (incButtonColored("\ue037", ImVec2(24, 24))) startRun(run);
        igEndDisabled();
        if (open) {
            foreach (step; run.orderedSteps()) renderTask(run, step);
            igTreePop();
        }
        igPopID();
    }

protected:
    override void onInit() {
        import nijigenerate.commands.puppet.tool : ngSetAutoRigCommandHandlers;
        ngSetAutoRigCommandHandlers(&executeImportedModel,&runStatus,&resumeWorkflow);
    }

    override void onUpdate() {
        finishWorker();
        renderPresetPicker();
        string error;
        synchronized (this) error = lastError;
        if (error.length) igTextWrapped("%s", error.toStringz());
        igSeparator();
        if (igBeginChild("##AutoRigSessions", ImVec2(0, 0)))
            foreach (run; runs) renderRun(run);
        igEndChild();
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

    string executeImportedModel(JSONValue options) {
        finishWorker();
        import std.exception : enforce;
        enforce(worker is null,"An AutoRig workflow is already running");
        auto run = workflowManager().create("deterministic-rig","model-to-rig");
        run.setInput("options",AutoRigValue.jsonValue(options));
        runs ~= run; startRun(run); return run.id();
    }

    JSONValue runStatus(string runId) {
        foreach (run; runs) if (run.id() == runId) {
            auto snapshot = run.snapshot(); JSONValue[] steps;
            foreach (step; run.orderedSteps()) {
                auto state = run.session().task(run.stepTaskId(step.id));
                steps ~= JSONValue(["id":JSONValue(step.id),"state":JSONValue(state.state.to!string),
                    "attempt":JSONValue(state.attempt),"message":JSONValue(state.message)]);
            }
            return JSONValue(["run_id":JSONValue(runId),"state":JSONValue(snapshot.state.to!string),
                "message":JSONValue(snapshot.message),"steps":JSONValue(steps)]);
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
