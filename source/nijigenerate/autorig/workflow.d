module nijigenerate.autorig.workflow;

import nijigenerate.autorig.framework;
import std.conv : to;
import std.exception : enforce;
import std.file : write, readText;
import std.json : JSONValue;
import nijigenerate.autorig.json : parseJSON = ngParseAutoRigJson;
import std.path : buildPath;

enum AutoRigWorkflowState { Pending, Running, Succeeded, Failed, Canceled }

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
    AutoRigWorkflowSpec workflow;
    AutoRigTaskSpec[] flattened;
    AutoRigProcessor[string] owners;
    string[string] originalTaskIds;
    string[string] selectedTaskIds;
    string[string][string] includedTasks;

    this(AutoRigSessionManager manager, string providerId, AutoRigWorkflowSpec workflow) {
        this.providerId = providerId;
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
        foreach (binding; workflow.inputBindings) {
            auto publicPort = port(workflow.inputs, binding.workflowInput);
            auto target = targetId(binding.targetStep, binding.targetTask);
            auto targetPort = port(taskSpec(target).inputs, binding.targetPort);
            enforce(publicPort.kind == targetPort.kind, "AutoRig workflow input kind mismatch");
            auto key = target ~ ":" ~ binding.targetPort;
            enforce((key in bound) is null, "Duplicate AutoRig workflow input binding");
            bound[key] = true;
        }
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

/** Workflow control is a view of one session, not a second run. */
class AutoRigWorkflowRun {
private:
    AutoRigSession session_;
    WorkflowProcessor processor;
    JSONValue[string] suppliedInputs;

    this(AutoRigSession session, WorkflowProcessor processor) {
        session_ = session;
        this.processor = processor;
        writeRecord();
    }

    void writeRecord() {
        JSONValue[string] record;
        record["runId"] = JSONValue(id());
        record["providerId"] = JSONValue(processor.providerId);
        record["workflowId"] = JSONValue(processor.workflow.id);
        auto current = snapshot();
        record["state"] = JSONValue(current.state.to!string);
        record["message"] = JSONValue(current.message);
        JSONValue[string] steps;
        foreach (step; processor.workflow.steps)
            steps[step.id] = JSONValue(processor.selectedTaskIds[step.id]);
        record["steps"] = JSONValue(steps);
        record["inputs"] = JSONValue(suppliedInputs);
        write(buildPath(directory(), "workflow.json"), JSONValue(record).toString());
    }

public:
    string id() { return session_.id(); }
    string directory() { return session_.directory(); }
    AutoRigSession session() { return session_; }
    string stepTaskId(string stepId) {
        auto found = stepId in processor.selectedTaskIds;
        enforce(found !is null, "Unknown workflow step");
        return *found;
    }

    AutoRigWorkflowStep[] orderedSteps() {
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
        return result;
    }

    void setInput(string portId, AutoRigValue value) {
        enforce(!session_.isBusy(), "Workflow is running");
        auto spec = port(processor.workflow.inputs, portId);
        enforce(spec.kind == value.kind, "AutoRig workflow input kind mismatch");
        foreach (binding; processor.workflow.inputBindings)
            if (binding.workflowInput == portId)
                session_.setInput(processor.targetId(binding.targetStep, binding.targetTask),
                    binding.targetPort, value);
        JSONValue[string] stored;
        stored["kind"] = JSONValue(value.kind.to!string);
        final switch (value.kind) {
            case AutoRigValueKind.Json:
                stored["value"] = value.text.length ? parseJSON(readText(value.text)) :
                    parseJSON(value.json.toString());
                break;
            case AutoRigValueKind.Blob:
                stored["value"] = JSONValue(value.readBlob());
                break;
            case AutoRigValueKind.FileName:
            case AutoRigValueKind.Path:
                stored["value"] = JSONValue(value.text);
                break;
        }
        suppliedInputs[portId] = JSONValue(stored);
        writeRecord();
    }

    void execute(bool force = false) {
        string[] selected;
        foreach (step; orderedSteps()) selected ~= stepTaskId(step.id);
        try {
            session_.executeGroup(selected, force);
        } catch (Exception error) {
            writeRecord();
            throw error;
        }
        writeRecord();
    }

    void executeStep(string stepId, bool force = false) {
        try {
            session_.execute(stepTaskId(stepId), force);
        } catch (Exception error) {
            writeRecord();
            throw error;
        }
        writeRecord();
    }

    void cancel() { session_.cancel(); }

    AutoRigValue output(string portId) {
        foreach (binding; processor.workflow.outputBindings)
            if (binding.workflowOutput == portId)
                return session_.output(stepTaskId(binding.sourceStep), binding.sourcePort);
        throw new Exception("Unknown workflow output");
    }

    AutoRigWorkflowSnapshot snapshot() {
        AutoRigWorkflowSnapshot result;
        result.runId = id();
        result.providerId = processor.providerId;
        result.workflowId = processor.workflow.id;
        result.state = session_.isBusy() ? AutoRigWorkflowState.Running : AutoRigWorkflowState.Pending;
        bool allSucceeded = true;
        foreach (step; processor.workflow.steps) {
            result.steps[step.id] = session_.task(stepTaskId(step.id));
            auto task = result.steps[step.id];
            if (task.state != AutoRigTaskState.Succeeded) allSucceeded = false;
        }
        foreach (taskSpec; processor.flattened) {
            auto task = session_.task(taskSpec.id);
            if (task.state == AutoRigTaskState.Failed) {
                result.state = AutoRigWorkflowState.Failed;
                result.message = task.message;
            } else if (task.state == AutoRigTaskState.Canceled) {
                result.state = AutoRigWorkflowState.Canceled;
                result.message = task.message;
            }
        }
        if (session_.isCanceled()) result.state = AutoRigWorkflowState.Canceled;
        else if (allSucceeded) result.state = AutoRigWorkflowState.Succeeded;
        return result;
    }
}

class AutoRigWorkflowManager {
private:
    AutoRigSessionManager sessions;
    AutoRigWorkflowRun[string] runs;

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
        import std.uuid : UUID;
        enforce(UUID(runId).toString() == runId,"Invalid saved AutoRig run UUID");
        auto record = parseJSON(readText(buildPath(sessions.rootDirectory(),runId,"workflow.json")));
        enforce(record["runId"].str == runId,"Saved workflow identity mismatch");
        AutoRigWorkflowSpec spec; bool found;
        foreach (workflow; sessions.processor(record["providerId"].str).workflows())
            if (workflow.id == record["workflowId"].str) { spec = workflow; found = true; }
        enforce(found,"Saved AutoRig workflow is no longer registered");
        auto adapter = new WorkflowProcessor(sessions,record["providerId"].str,spec);
        foreach (step; spec.steps) enforce(record["steps"][step.id].str == adapter.selectedTaskIds[step.id],
            "Saved workflow graph differs from the current workflow");
        auto session = sessions.reopenWithProcessor(adapter,runId);
        auto run = new AutoRigWorkflowRun(session,adapter);
        foreach (portId,value; spec.inputDefaults) run.setInput(portId,value);
        if (auto inputs = "inputs" in record.object) foreach (portId,stored; inputs.object) {
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
            run.setInput(portId,value);
        }
        session.restoreCommittedOutputs(); run.writeRecord(); runs[runId] = run; return run;
    }

    void close(string runId) {
        get(runId);
        sessions.close(runId);
        runs.remove(runId);
    }
}
