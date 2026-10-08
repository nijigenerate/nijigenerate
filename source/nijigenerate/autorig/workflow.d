module nijigenerate.autorig.workflow;

import nijigenerate.autorig.framework;
import std.conv : to;
import std.exception : enforce;
import std.json : JSONValue, JSONType;

enum AutoRigWorkflowState { Pending, Running, Succeeded, Failed, Canceled }

/** Optional native-rig diagnostics; generic workflows need neither step nor output. */
JSONValue ngAutoRigWorkflowDiagnostics(AutoRigWorkflowRun run) {
    JSONValue[string] result;
    foreach (step; run.orderedSteps()) {
        auto taskId = run.stepTaskId(step.id);
        if (step.id == "compile") {
            JSONValue[] profile;
            foreach (artifact; run.session().task(taskId).artifacts)
                if (artifact.preview && artifact.portId == "compile-profile") profile ~= artifact.value.json;
            if (profile.length) result["compile_profile"] = JSONValue(profile);
        }
        if (step.id != "verify" || run.session().isBusy()) continue;
        bool reportPort;
        foreach (output; run.session().taskSpec(taskId).outputs)
            if (output.id == "report" && output.kind == AutoRigValueKind.Json) reportPort = true;
        if (!reportPort || !run.session().hasOutput(taskId,"report")) continue;
        auto report = run.session().output(taskId,"report").json;
        if (report.type != JSONType.object) continue;
        JSONValue[string] summary;
        foreach (key; ["passed","error","readback_verified","finish_stages",
            "all_finish_stages_succeeded","numerical_stages_passed","rig_complete"])
            if (auto entry = key in report.object) summary[key] = *entry;
        if (auto findings = "numerical_findings" in report.object) {
            if (findings.type == JSONType.array) {
                import std.algorithm : min;
                auto entries = findings.array;
                summary["numerical_findings_count"] = JSONValue(cast(ulong)entries.length);
                summary["numerical_findings"] = JSONValue(entries[0 .. min(entries.length, 32)].dup);
            }
        }
        result["verification"] = JSONValue(summary);
    }
    return JSONValue(result);
}

struct AutoRigWorkflowPreset {
    string providerId;
    AutoRigWorkflowSpec spec;
}

struct AutoRigWorkflowSnapshot {
    string runId;
    string providerId;
    string workflowId;
    AutoRigWorkflowState state;
    string message;
    AutoRigTaskSnapshot[string] steps;
}

private AutoRigPortSpec port(AutoRigPortSpec[] ports, string id) {
    foreach (candidate; ports) if (candidate.id == id) return candidate;
    throw new Exception("Unknown AutoRig workflow port: " ~ id);
}

private string instanceId(string stepId, string taskId) {
    return stepId.length.to!string ~ "_" ~ stepId ~ "_" ~ taskId;
}

private class WorkflowProcessor : AutoRigProcessor {
    string providerId;
    AutoRigProcessor provider;
    AutoRigWorkflowSpec workflow;
    AutoRigTaskSpec[] flattened;
    AutoRigProcessor[string] owners;
    string[string] originalTaskIds;
    string[string] selectedTaskIds;
    string[string][string] includedTasks;

    this(AutoRigSessionManager manager, string providerId, AutoRigWorkflowSpec workflow) {
        provider = manager.processor(providerId);
        this.providerId = provider.procId();
        this.workflow = workflow;
        enforce(workflow.id.length && workflow.steps.length, "Empty AutoRig workflow");
        AutoRigWorkflowStep[string] steps;
        foreach (step; workflow.steps) {
            enforce(step.id.length && (step.id in steps) is null, "Invalid or duplicate workflow step");
            steps[step.id] = step;
            auto owner = manager.processor(step.processorId);
            AutoRigTaskSpec[string] catalog;
            foreach (task; owner.tasks()) {
                enforce(task.id.length && (task.id in catalog) is null, "Invalid processor task catalog");
                catalog[task.id] = task;
            }
            enforce((step.taskId in catalog) !is null, "Unknown workflow task: " ~ step.taskId);
            bool[string] visiting;
            includeTask(step.id, step.taskId, owner, catalog, visiting);
            selectedTaskIds[step.id] = instanceId(step.id, step.taskId);
        }
        foreach (step; workflow.steps) {
            auto selected = selectedTaskIds[step.id];
            foreach (dependency; step.dependencies) {
                enforce((dependency in steps) !is null, "Unknown workflow step dependency");
                updateDependency(selected, selectedTaskIds[dependency]);
            }
        }
        foreach (binding; workflow.connections) {
            enforce((binding.sourceStep in steps) !is null &&
                (binding.targetStep in steps) !is null, "Unknown workflow connection step");
            auto source = selectedTaskIds[binding.sourceStep];
            auto target = targetId(binding.targetStep, binding.targetTask);
            auto sourcePort = port(taskSpec(source).outputs, binding.sourcePort);
            auto targetPort = port(taskSpec(target).inputs, binding.targetPort);
            enforce(sourcePort.kind == targetPort.kind, "AutoRig workflow connection kind mismatch");
            auto task = taskSpec(target);
            task.connections ~= AutoRigConnection(binding.targetPort, source, binding.sourcePort);
            replaceTask(task);
        }
        bool[string] bound;
        bool[string] boundPublicInputs;
        foreach (binding; workflow.inputBindings) {
            auto publicPort = port(workflow.inputs, binding.workflowInput);
            auto target = targetId(binding.targetStep, binding.targetTask);
            auto targetPort = port(taskSpec(target).inputs, binding.targetPort);
            enforce(publicPort.kind == targetPort.kind, "AutoRig workflow input kind mismatch");
            auto key = target ~ ":" ~ binding.targetPort;
            enforce((key in bound) is null, "Duplicate AutoRig workflow input binding");
            bound[key] = true;
            boundPublicInputs[binding.workflowInput] = true;
        }
        foreach (publicPort; workflow.inputs)
            enforce(!publicPort.required || (publicPort.id in boundPublicInputs) !is null,
                "Missing required workflow input binding: " ~ publicPort.id);
        bool[string] outputs;
        foreach (binding; workflow.outputBindings) {
            auto publicPort = port(workflow.outputs, binding.workflowOutput);
            enforce((binding.workflowOutput in outputs) is null, "Duplicate workflow output binding");
            outputs[binding.workflowOutput] = true;
            enforce((binding.sourceStep in steps) !is null, "Unknown workflow output step");
            auto sourcePort = port(taskSpec(selectedTaskIds[binding.sourceStep]).outputs, binding.sourcePort);
            enforce(publicPort.kind == sourcePort.kind, "AutoRig workflow output kind mismatch");
        }
        foreach (publicPort; workflow.outputs)
            enforce(!publicPort.required || (publicPort.id in outputs) !is null,
                "Missing required workflow output binding");
        foreach (task; flattened) foreach (input; task.inputs) {
            bool connected;
            foreach (connection; task.connections) if (connection.input == input.id) connected = true;
            auto key = task.id ~ ":" ~ input.id;
            enforce(!input.required || connected || (key in bound) !is null,
                "Missing required workflow task input: " ~ key);
            enforce(!connected || (key in bound) is null, "Workflow input is both bound and connected");
        }
    }

    void includeTask(string stepId, string taskId, AutoRigProcessor owner,
        AutoRigTaskSpec[string] catalog, ref bool[string] visiting) {
        auto flatId = instanceId(stepId, taskId);
        if ((flatId in owners) !is null) return;
        enforce((taskId in visiting) is null, "AutoRig task dependency cycle");
        auto found = taskId in catalog;
        enforce(found !is null, "Unknown internal AutoRig task dependency");
        visiting[taskId] = true;
        scope(exit) visiting.remove(taskId);
        auto task = *found;
        task.dependencies = task.dependencies.dup;
        task.connections = task.connections.dup;
        foreach (dependency; task.dependencies) includeTask(stepId, dependency, owner, catalog, visiting);
        foreach (connection; task.connections)
            includeTask(stepId, connection.sourceTask, owner, catalog, visiting);
        foreach (ref dependency; task.dependencies) dependency = instanceId(stepId, dependency);
        foreach (ref connection; task.connections)
            connection.sourceTask = instanceId(stepId, connection.sourceTask);
        task.id = flatId;
        flattened ~= task;
        owners[flatId] = owner;
        originalTaskIds[flatId] = taskId;
        includedTasks[stepId][taskId] = flatId;
    }

    string targetId(string stepId, string taskId) {
        auto tasks = stepId in includedTasks;
        enforce(tasks !is null, "Unknown workflow target step");
        if (!taskId.length) return selectedTaskIds[stepId];
        auto target = taskId in *tasks;
        enforce(target !is null, "Workflow target task is not a dependency of its step");
        return *target;
    }

    AutoRigTaskSpec taskSpec(string id) {
        foreach (task; flattened) if (task.id == id) return task;
        throw new Exception("Unknown workflow task instance");
    }

    void replaceTask(AutoRigTaskSpec task) {
        foreach (ref current; flattened) if (current.id == task.id) {
            current = task;
            return;
        }
        throw new Exception("Unknown workflow task instance");
    }

    void updateDependency(string targetId, string sourceId) {
        auto task = taskSpec(targetId);
        task.dependencies ~= sourceId;
        replaceTask(task);
    }

    override string procId() { return "workflow-" ~ providerId ~ "-" ~ workflow.id; }
    override string displayName() { return workflow.label; }
    override AutoRigTaskSpec[] tasks() { return flattened; }
    override void executeTask(string taskId, AutoRigTaskContext context) {
        auto owner = taskId in owners;
        enforce(owner !is null, "Unknown workflow task owner");
        (*owner).executeTask(originalTaskIds[taskId], context);
    }

    override bool renderInputUI(string taskId, IAutoRigInputContext context) {
        auto owner = taskId in owners;
        enforce(owner !is null, "Unknown workflow task owner");
        return (*owner).renderInputUI(originalTaskIds[taskId], context);
    }

    override bool renderOutputUI(string taskId, IAutoRigOutputContext context) {
        auto owner = taskId in owners;
        enforce(owner !is null, "Unknown workflow task owner");
        return (*owner).renderOutputUI(originalTaskIds[taskId], context);
    }
}

private AutoRigValue restoredValue(JSONValue stored) {
    AutoRigValue value;
    value.kind = stored["kind"].str.to!AutoRigValueKind;
    final switch (value.kind) {
        case AutoRigValueKind.Json: value.json = stored["value"]; break;
        case AutoRigValueKind.Blob:
            foreach (entry; stored["value"].array) value.bytes ~= entry.integer.to!ubyte;
            break;
        case AutoRigValueKind.FileName:
        case AutoRigValueKind.Path: value.text = stored["value"].str; break;
    }
    return value;
}

class AutoRigWorkflowRun : IAutoRigSessionEditContext {
private:
    AutoRigSession session_;
    WorkflowProcessor processor;
    AutoRigValue[string] suppliedInputs;
    ulong inputRevision_;
    AutoRigWorkflowStep[] orderedSteps_;

    this(AutoRigSession session, WorkflowProcessor processor) {
        session_ = session;
        this.processor = processor;
        orderedSteps();
    }

public:
    string id() { return session_.id(); }
    string runId() { return id(); }
    string workflowId() { return processor.workflow.id; }
    string displayName() { return processor.workflow.label.length ? processor.workflow.label : workflowId(); }
    bool canEdit() { return !session_.isBusy(); }
    string[] contextNames() { return session_.contextNames(); }
    AutoRigValue contextValue(string name) { return session_.contextValue(name); }
    AutoRigPortSpec[] outputPorts() { return processor.workflow.outputs.dup; }
    bool hasOutput(string portId) {
        foreach (binding; processor.workflow.outputBindings)
            if (binding.workflowOutput == portId)
                return session_.outputContext(stepTaskId(binding.sourceStep)).hasValue(binding.sourcePort);
        return false;
    }
    AutoRigArtifact outputArtifact(string portId) {
        foreach (binding; processor.workflow.outputBindings)
            if (binding.workflowOutput == portId)
                return session_.outputArtifact(stepTaskId(binding.sourceStep), binding.sourcePort);
        throw new Exception("Unknown workflow output");
    }
    bool renderInputUI() {
        requireUIThread();
        return processor.provider.renderSessionInputUI(workflowId(), this);
    }
    bool renderOutputUI() {
        requireUIThread();
        return processor.provider.renderSessionOutputUI(workflowId(), this);
    }

private:
    void requireUIThread() {
        import core.thread : Thread;
        enforce(Thread.getThis() !is null && Thread.getThis().isMainThread,
            "AutoRig session UI must run on the main thread");
    }

public:
    string directory() { return session_.directory(); }
    AutoRigSession session() { return session_; }
    AutoRigPortSpec[] inputPorts() { return processor.workflow.inputs.dup; }
    bool hasInput(string portId) { return (portId in suppliedInputs) !is null; }
    ulong inputRevision() { return inputRevision_; }
    AutoRigValue input(string portId) {
        auto value = portId in suppliedInputs;
        enforce(value !is null, "Missing AutoRig workflow input: " ~ portId);
        return ngCopyAutoRigValue(*value);
    }
    void setContextValue(string name, AutoRigValue value) {
        session_.setContextValue(name, value);
    }
    void setContext(JSONValue values) {
        import std.json : JSONType;
        enforce(values.type == JSONType.object, "AutoRig session context must be an object");
        foreach (name, value; values.object) setContextValue(name, restoredValue(value));
    }
    string stepTaskId(string stepId) {
        auto found = stepId in processor.selectedTaskIds;
        enforce(found !is null, "Unknown workflow step");
        return *found;
    }

    AutoRigWorkflowStep[] orderedSteps() {
        if (orderedSteps_.length) return orderedSteps_.dup;
        size_t[string] selectedIndices;
        foreach (index, step; processor.workflow.steps)
            selectedIndices[stepTaskId(step.id)] = index;
        bool[string] visiting;
        bool[string] visited;
        AutoRigWorkflowStep[] result;
        void visit(string taskId) {
            if ((taskId in visited) !is null) return;
            enforce((taskId in visiting) is null, "AutoRig workflow dependency cycle");
            visiting[taskId] = true;
            auto task = processor.taskSpec(taskId);
            foreach (dependency; task.dependencies) visit(dependency);
            foreach (connection; task.connections) visit(connection.sourceTask);
            visiting.remove(taskId);
            visited[taskId] = true;
            if (auto index = taskId in selectedIndices)
                result ~= processor.workflow.steps[*index];
        }
        foreach (step; processor.workflow.steps) visit(stepTaskId(step.id));
        orderedSteps_ = result;
        return result.dup;
    }

    void setInput(string portId, AutoRigValue value) {
        enforce(!session_.isBusy(), "Workflow is running");
        auto spec = port(processor.workflow.inputs, portId);
        enforce(spec.kind == value.kind, "AutoRig workflow input kind mismatch");
        foreach (binding; processor.workflow.inputBindings)
            if (binding.workflowInput == portId)
                session_.setInput(processor.targetId(binding.targetStep, binding.targetTask),
                    binding.targetPort, value);
        suppliedInputs[portId] = ngCopyAutoRigValue(value);
        ++inputRevision_;
    }

    void execute(bool force = false) {
        string[] selected;
        foreach (step; orderedSteps()) selected ~= stepTaskId(step.id);
        session_.executeGroup(selected, force);
    }

    void executeStep(string stepId, bool force = false) {
        session_.execute(stepTaskId(stepId), force);
    }

    void cancel() { session_.cancel(); }

    AutoRigValue output(string portId) {
        foreach (binding; processor.workflow.outputBindings)
            if (binding.workflowOutput == portId)
                return session_.output(stepTaskId(binding.sourceStep), binding.sourcePort);
        throw new Exception("Unknown workflow output");
    }

    AutoRigWorkflowSnapshot snapshot(bool includeArtifacts = true) {
        AutoRigWorkflowSnapshot result;
        result.runId = id();
        result.providerId = processor.providerId;
        result.workflowId = processor.workflow.id;
        auto running = session_.isBusy();
        result.state = running ? AutoRigWorkflowState.Running : AutoRigWorkflowState.Pending;
        bool allSucceeded = true;
        foreach (step; processor.workflow.steps) {
            result.steps[step.id] = session_.task(stepTaskId(step.id), includeArtifacts);
            auto task = result.steps[step.id];
            if (task.state != AutoRigTaskState.Succeeded) allSucceeded = false;
        }
        foreach (taskSpec; processor.flattened) {
            auto task = session_.task(taskSpec.id, false);
            if (task.state == AutoRigTaskState.Failed) {
                allSucceeded = false;
                if (!running) result.state = AutoRigWorkflowState.Failed;
                result.message = task.message;
            } else if (task.state == AutoRigTaskState.Canceled) {
                allSucceeded = false;
                if (!running) result.state = AutoRigWorkflowState.Canceled;
                result.message = task.message;
            }
        }
        if (!running) {
            if (session_.isCanceled()) result.state = AutoRigWorkflowState.Canceled;
            else if (allSucceeded) result.state = AutoRigWorkflowState.Succeeded;
        }
        return result;
    }
}

class AutoRigWorkflowManager {
private:
    AutoRigSessionManager sessions;
    AutoRigWorkflowRun[string] runs;
    AutoRigWorkflowRun[string] closedRuns;

public:
    this(AutoRigSessionManager sessions) { this.sessions = sessions; }

    AutoRigWorkflowPreset[] listPresets() {
        AutoRigWorkflowPreset[] result;
        foreach (processor; sessions.listProcessors())
            foreach (workflow; processor.workflows())
                result ~= AutoRigWorkflowPreset(processor.procId(), workflow);
        return result;
    }

    AutoRigWorkflowRun create(string providerId, string workflowId) {
        AutoRigWorkflowSpec spec;
        bool found;
        foreach (workflow; sessions.processor(providerId).workflows())
            if (workflow.id == workflowId) {
                enforce(!found, "Duplicate AutoRig workflow preset ID");
                spec = workflow;
                found = true;
            }
        enforce(found, "Unknown AutoRig workflow preset");
        auto adapter = new WorkflowProcessor(sessions, providerId, spec);
        auto session = sessions.createWithProcessor(adapter, sessions.rootDirectory());
        auto run = new AutoRigWorkflowRun(session, adapter);
        foreach (portId, value; spec.inputDefaults) run.setInput(portId, value);
        runs[run.id()] = run;
        return run;
    }

    AutoRigWorkflowRun get(string runId) {
        auto found = runId in runs;
        enforce(found !is null, "Unknown AutoRig workflow run");
        return *found;
    }

    AutoRigWorkflowRun reopen(string runId) {
        if (auto known = runId in runs) return *known;
        auto saved = runId in closedRuns;
        enforce(saved !is null, "AutoRig session is no longer available in memory");
        auto run = *saved;
        sessions.reopenWithProcessor(run.processor, runId);
        closedRuns.remove(runId);
        runs[runId] = run;
        return run;
    }

    void close(string runId) {
        closedRuns[runId] = get(runId);
        sessions.close(runId);
        runs.remove(runId);
    }

    void remove(string runId) {
        enforce((runId in runs) !is null || (runId in closedRuns) !is null, "Unknown AutoRig workflow run");
        sessions.remove(runId);
        runs.remove(runId);
        closedRuns.remove(runId);
    }

    void disposeAll() {
        foreach (run; runs) enforce(!run.session().isBusy(), "Cannot dispose a running AutoRig workflow");
        foreach (run; closedRuns) enforce(!run.session().isBusy(), "Cannot dispose a running AutoRig workflow");
        foreach (runId; runs.keys) remove(runId);
        foreach (runId; closedRuns.keys) remove(runId);
    }
}
