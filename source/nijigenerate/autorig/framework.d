module nijigenerate.autorig.framework;

import core.atomic : atomicLoad, atomicStore;
import core.memory : pageSize;
import core.thread.fiber : Fiber;
import std.conv : to;
import std.exception : enforce;
import std.file : exists, read, readText;
import std.json : JSONValue, JSONType;
import nijigenerate.autorig.json : parseJSON = ngParseAutoRigJson;
import std.path : absolutePath, buildPath, baseName, asNormalizedPath;
import std.uuid : randomUUID;

/** Values exchanged between tasks. FileName is a name, while Path identifies a resource. */
enum AutoRigValueKind { FileName, Path, Json, Blob }

/** Marks message IDs for gettext extraction without localizing worker data. */
string ngAutoRigMessage(string message) { return message; }

struct AutoRigValue {
    AutoRigValueKind kind;
    string text;
    JSONValue json;
private:
    ubyte[] mutableBytes;
    immutable(ubyte)[] immutableBytes;
    string encodedJson;
public:

    /** Materialize an owned mutable view only for callers that edit raw bytes. */
    @property ref ubyte[] bytes() {
        if (immutableBytes !is null) {
            mutableBytes = immutableBytes.dup;
            immutableBytes = null;
        }
        return mutableBytes;
    }

    private size_t blobByteLength() const {
        return immutableBytes !is null ? immutableBytes.length : mutableBytes.length;
    }

    static AutoRigValue fileName(string value) {
        return AutoRigValue(AutoRigValueKind.FileName, value);
    }

    static AutoRigValue path(string value) {
        return AutoRigValue(AutoRigValueKind.Path, value);
    }

    static AutoRigValue jsonValue(JSONValue value) {
        AutoRigValue result;
        result.kind = AutoRigValueKind.Json;
        result.json = value;
        return result;
    }

    private static AutoRigValue jsonSnapshot(JSONValue value) {
        AutoRigValue result;
        result.kind = AutoRigValueKind.Json;
        // Retain pointer-free text instead of millions of GC-scanned JSON nodes.
        // Consumers still receive an independently owned, exact-double JSON tree.
        result.encodedJson = value.toString();
        return result;
    }

    static AutoRigValue blob(ubyte[] value) {
        AutoRigValue result;
        result.kind = AutoRigValueKind.Blob;
        // Immutable payloads can cross worker boundaries without sharing mutable arrays.
        result.immutableBytes = value.idup;
        return result;
    }

    ubyte[] readBlob() {
        enforce(kind == AutoRigValueKind.Blob, "AutoRig value is not a blob");
        return text.length ? cast(ubyte[])read(text) :
            immutableBytes !is null ? immutableBytes.dup : mutableBytes.dup;
    }

    /** A read-only task can share the snapshot without copying its entire payload. */
    immutable(ubyte)[] readImmutableBlob() {
        enforce(kind == AutoRigValueKind.Blob, "AutoRig value is not a blob");
        return text.length ? (cast(ubyte[])read(text)).idup :
            immutableBytes !is null ? immutableBytes : mutableBytes.idup;
    }
}

/** Lightweight UI metadata; reading it never copies JSON or binary payloads. */
struct AutoRigValueInfo {
    AutoRigValueKind kind;
    string text;
    size_t byteLength;
    ulong revision;
}

private AutoRigValueInfo valueInfo(AutoRigValue value, ulong revision) {
    return AutoRigValueInfo(value.kind, value.text,
        value.encodedJson.length ? value.encodedJson.length : value.blobByteLength(), revision);
}

private JSONValue copyJson(JSONValue value) {
    if (value.type == JSONType.object) {
        JSONValue[string] result;
        foreach (key,entry; value.object) result[key] = copyJson(entry);
        return JSONValue(result);
    }
    if (value.type == JSONType.array) {
        auto result = new JSONValue[value.array.length];
        foreach (i,entry; value.array) result[i] = copyJson(entry);
        return JSONValue(result);
    }
    return value;
}

private AutoRigValue copyValue(AutoRigValue value) {
    final switch (value.kind) {
        case AutoRigValueKind.FileName: return AutoRigValue.fileName(value.text);
        case AutoRigValueKind.Path: return AutoRigValue.path(value.text);
        case AutoRigValueKind.Json: return AutoRigValue.jsonValue(value.text.length ?
            parseJSON(readText(value.text)) : value.encodedJson.length ?
            parseJSON(value.encodedJson) : copyJson(value.json));
        case AutoRigValueKind.Blob:
            return value.text.length || value.immutableBytes !is null ? value : AutoRigValue.blob(value.mutableBytes);
    }
}

AutoRigValue ngCopyAutoRigValue(AutoRigValue value) { return copyValue(value); }

/** Wraps one actual task invocation in the host editor's undo boundary. */
alias AutoRigTaskActionBoundary = void delegate(string taskId, void delegate() execute);

/** Dispatches a task's editor mutation to the host UI thread. */
alias AutoRigEditorDispatcher = void delegate(void delegate() action);

struct AutoRigPortSpec {
    string id;
    AutoRigValueKind kind;
    bool required = true;
    string mediaType;
    string description;
    string viewHint;
}

struct AutoRigConnection {
    string input;
    string sourceTask;
    string sourceOutput;
}

struct AutoRigTaskSpec {
    string id;
    string label;
    string[] dependencies;
    AutoRigPortSpec[] inputs;
    AutoRigPortSpec[] outputs;
    AutoRigConnection[] connections;
    // A failed stage may explicitly publish a rollback checkpoint for later diagnostics.
    bool retainFailureOutputs;
    // Explicit opt-in for processors that establish their own editor action group.
    bool ownsActionBoundary;
}

/** A workflow preset wires task calls, including calls to other processors. */
struct AutoRigWorkflowStep {
    string id;
    string processorId;
    string taskId;
    string[] dependencies;
}

struct AutoRigWorkflowInputBinding {
    string workflowInput;
    string targetStep;
    string targetTask;
    string targetPort;
}

struct AutoRigWorkflowConnection {
    string sourceStep;
    string sourcePort;
    string targetStep;
    string targetTask;
    string targetPort;
}

struct AutoRigWorkflowOutputBinding {
    string workflowOutput;
    string sourceStep;
    string sourcePort;
}

struct AutoRigWorkflowSpec {
    string id;
    string label;
    string description;
    AutoRigPortSpec[] inputs;
    AutoRigPortSpec[] outputs;
    AutoRigWorkflowStep[] steps;
    AutoRigWorkflowInputBinding[] inputBindings;
    AutoRigWorkflowConnection[] connections;
    AutoRigWorkflowOutputBinding[] outputBindings;
    AutoRigValue[string] inputDefaults;
    bool preferred;
}

enum AutoRigTaskState { Pending, Running, Succeeded, Failed, Canceled, Stale }

struct AutoRigArtifact {
    string taskId;
    string portId;
    uint attempt;
    AutoRigValueKind kind;
    string storagePath;
    string mediaType;
    string viewHint;
    bool committed;
    bool preview;
private:
    AutoRigValue value_;
public:
    @property AutoRigValue value() { return copyValue(value_); }
    private @property void value(AutoRigValue item) { value_ = item; }
    @property size_t byteLength() { return valueInfo(value_, 0).byteLength; }
}

struct AutoRigTaskSnapshot {
    string taskId;
    uint attempt;
    AutoRigTaskState state;
    string message;
    AutoRigArtifact[] artifacts;
}

/** Session-owned task input contract, available before and between attempts. */
interface IAutoRigInputContext {
    string taskId();
    AutoRigPortSpec[] ports();
    bool isConnected(string portId);
    bool canEdit(string portId);
    bool hasValue(string portId);
    AutoRigValue value(string portId);
    void setValue(string portId, AutoRigValue value);
}

/** Session-owned task output contract, including previews of active attempts. */
interface IAutoRigOutputContext {
    string taskId();
    AutoRigPortSpec[] ports();
    AutoRigTaskSnapshot snapshot();
    bool hasValue(string portId);
    AutoRigValue value(string portId);
    AutoRigArtifact artifact(string portId);
}

/** Optional processor-owned immediate-mode editors, as in AutoMesh.configure(). */
interface IAutoRigInputEditor {
    void configureInput(string taskId, IAutoRigInputContext context);
}

interface IAutoRigOutputViewer {
    void viewOutput(string taskId, IAutoRigOutputContext context);
}

/** Read-only session UI contract; values are returned as owned copies. */
interface IAutoRigSessionViewContext {
    string runId();
    string workflowId();
    AutoRigPortSpec[] inputPorts();
    bool hasInput(string portId);
    AutoRigValue input(string portId);
    string[] contextNames();
    AutoRigValue contextValue(string name);
    AutoRigPortSpec[] outputPorts();
    bool hasOutput(string portId);
    AutoRigValue output(string portId);
    AutoRigArtifact outputArtifact(string portId);
}

interface IAutoRigSessionEditContext : IAutoRigSessionViewContext {
    bool canEdit();
    void setInput(string portId, AutoRigValue value);
    void setContextValue(string name, AutoRigValue value);
}

interface IAutoRigSessionEditor {
    void configureSession(string workflowId, IAutoRigSessionEditContext context);
}

interface IAutoRigSessionViewer {
    void viewSession(string workflowId, IAutoRigSessionViewContext context);
}

/** Processor implementations share D code and expose each program as a task function. */
abstract class AutoRigProcessor {
    abstract string procId();
    abstract string displayName();
    abstract AutoRigTaskSpec[] tasks();
    AutoRigWorkflowSpec[] workflows() { return null; }
    abstract void executeTask(string taskId, AutoRigTaskContext context);

    bool renderSessionInputUI(string workflowId, IAutoRigSessionEditContext context) {
        auto ui = cast(IAutoRigSessionEditor)this;
        if (ui is null) return false;
        ui.configureSession(workflowId, context);
        return true;
    }

    bool renderSessionOutputUI(string workflowId, IAutoRigSessionViewContext context) {
        auto ui = cast(IAutoRigSessionViewer)this;
        if (ui is null) return false;
        ui.viewSession(workflowId, context);
        return true;
    }

    bool renderInputUI(string taskId, IAutoRigInputContext context) {
        auto ui = cast(IAutoRigInputEditor)this;
        if (ui is null) return false;
        ui.configureInput(taskId, context);
        return true;
    }

    bool renderOutputUI(string taskId, IAutoRigOutputContext context) {
        auto ui = cast(IAutoRigOutputViewer)this;
        if (ui is null) return false;
        ui.viewOutput(taskId, context);
        return true;
    }
}

/** Session-owned CPU data. Implementations synchronize metadata reads and own their buffers. */
interface AutoRigWorkspace {
    JSONValue memoryInfo();
    void dispose();
}

/** A task sees logical port names; the session owns output locations and lifetimes. */
class AutoRigTaskContext {
private:
    AutoRigTaskSpec spec;
    string attemptDirectory;
    AutoRigValue[string] inputs;
    AutoRigValue[string] outputs;
    AutoRigArtifact[string] artifacts;
    string[string] reservedPreviews;
    uint previewVersion;
    bool delegate() canceled;
    void delegate(AutoRigArtifact) onPreview;
    AutoRigEditorDispatcher editorDispatcher;
    string reportedFailure;
    string sessionId_;
    AutoRigValue[string] sessionValues;
    AutoRigWorkspace delegate(string, AutoRigWorkspace delegate()) workspaceProvider;

    this(AutoRigTaskSpec spec, string directory, AutoRigValue[string] inputs,
        bool delegate() canceled, void delegate(AutoRigArtifact) onPreview,
        AutoRigEditorDispatcher editorDispatcher) {
        this.spec = spec;
        this.attemptDirectory = directory;
        this.inputs = inputs;
        this.canceled = canceled;
        this.onPreview = onPreview;
        this.editorDispatcher = editorDispatcher;
    }

    AutoRigPortSpec requireOutput(string portId, AutoRigValueKind kind) {
        foreach (port; spec.outputs)
            if (port.id == portId) {
                enforce(port.kind == kind, "AutoRig output kind mismatch: " ~ portId);
                enforce((portId in outputs) is null, "AutoRig output already published: " ~ portId);
                return port;
            }
        throw new Exception("Unknown AutoRig output: " ~ portId);
    }

    void publish(AutoRigPortSpec port, AutoRigValue value, string storagePath) {
        outputs[port.id] = value;
        artifacts[port.id] = AutoRigArtifact(spec.id, port.id, 0, value.kind,
            storagePath, port.mediaType, port.viewHint, false, false);
        artifacts[port.id].value = value;
    }

public:
    string sessionId() { return sessionId_; }
    /** Only the executing worker uses workspace data; the UI reads metadata only. */
    AutoRigWorkspace workspace(string name, AutoRigWorkspace delegate() create) {
        enforce(workspaceProvider !is null, "AutoRig workspace is unavailable");
        return workspaceProvider(name, create);
    }
    /** Named session data and artifact references, owned by this task attempt. */
    AutoRigValue contextValue(string name) {
        auto value = name in sessionValues;
        enforce(value !is null, "Missing AutoRig session context: " ~ name);
        return copyValue(*value);
    }

    string[] contextNames() { return sessionValues.keys; }

    bool hasInput(string portId) { return (portId in inputs) !is null; }

    AutoRigValue input(string portId) {
        auto found = portId in inputs;
        enforce(found !is null, "Missing AutoRig input: " ~ portId);
        return copyValue(*found);
    }

    bool isCanceled() { return canceled !is null && canceled(); }

    void reportFailure(string message) {
        enforce(spec.retainFailureOutputs && message.length>0,"Task does not support failure checkpoints");
        reportedFailure = message;
    }

    void runOnMainThread(void delegate() action) {
        enforce(action !is null, "AutoRig editor action is null");
        if (editorDispatcher !is null) editorDispatcher(action);
        else action();
    }

    string outputPath(string portId, string extension = "") {
        requireOutput(portId, AutoRigValueKind.Path);
        enforce(extension.length == 0 ||
            (extension[0] == '.' && extension.length <= 20 && safeId(extension[1 .. $])),
            "Invalid AutoRig output extension");
        return buildPath(attemptDirectory, portId ~ extension);
    }

    void publishPath(string portId, string path) {
        auto port = requireOutput(portId, AutoRigValueKind.Path);
        enforce(exists(path), "AutoRig output path does not exist: " ~ portId);
        // Published paths are owned by this attempt; imported user paths remain inputs.
        enforce(path == outputPath(portId) || path.length > outputPath(portId).length &&
            path[0 .. outputPath(portId).length] == outputPath(portId) &&
            path[outputPath(portId).length] == '.', "AutoRig output path is not allocated by the session");
        publish(port, AutoRigValue.path(path), path);
    }

    void publishFileName(string portId, string name) {
        auto port = requireOutput(portId, AutoRigValueKind.FileName);
        enforce(safeFileName(name), "Invalid AutoRig file name");
        publish(port, AutoRigValue.fileName(name), null);
    }

    void publishJson(string portId, JSONValue value) {
        publish(requireOutput(portId, AutoRigValueKind.Json), AutoRigValue.jsonSnapshot(value), null);
    }

    void publishBlob(string portId, ubyte[] value) {
        publish(requireOutput(portId, AutoRigValueKind.Blob), AutoRigValue.blob(value), null);
    }

    void previewJson(string id, JSONValue value, string mediaType = "application/json") {
        previewValue(id, AutoRigValue.jsonSnapshot(value), mediaType);
    }

    void previewBlob(string id, ubyte[] value, string mediaType = "application/octet-stream") {
        previewValue(id, AutoRigValue.blob(value), mediaType);
    }

    private void previewValue(string id, AutoRigValue value, string mediaType) {
        enforce(safeId(id), "Invalid AutoRig preview name");
        AutoRigArtifact artifact;
        artifact.taskId = spec.id; artifact.portId = id; artifact.kind = value.kind;
        artifact.mediaType = mediaType; artifact.preview = true; artifact.value = value;
        if (onPreview !is null) onPreview(artifact);
    }

    string previewPath(string id, string extension) {
        enforce(safeId(id) && extension.length > 1 && extension[0] == '.' &&
            safeId(extension[1 .. $]), "Invalid AutoRig preview name");
        auto location = buildPath(attemptDirectory,
            "preview-" ~ id ~ "-" ~ (++previewVersion).to!string ~ extension);
        reservedPreviews[id] = location;
        return location;
    }

    void publishPreview(string id, string path, string mediaType) {
        auto reserved = id in reservedPreviews;
        enforce(reserved !is null && *reserved == path && exists(path),
            "AutoRig preview path was not allocated or written");
        reservedPreviews.remove(id);
        if (onPreview !is null) {
            auto artifact = AutoRigArtifact(spec.id, id, 0, AutoRigValueKind.Path,
                path, mediaType, null, false, true);
            artifact.value = AutoRigValue.path(path);
            onPreview(artifact);
        }
    }
}

private bool safeId(string id) {
    if (id.length == 0 || id == "." || id == "..") return false;
    foreach (ch; id)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')) return false;
    return true;
}

private bool safeFileName(string name) {
    if (name.length == 0 || name == "." || name == ".." ||
        name[$ - 1] == '.' || name[$ - 1] == ' ') return false;
    foreach (ch; name)
        if (ch < ' ' || ch == '/' || ch == '\\' || ch == ':' || ch == '*' ||
            ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') return false;
    return true;
}

/** A single run owns every task attempt and the files created for it. */
class AutoRigSession {
private:
    AutoRigProcessor processor;
    string id_;
    string directory_;
    AutoRigTaskSpec[string] specs;
    AutoRigValue[string][string] suppliedInputs;
    AutoRigValue[string][string] committedOutputs;
    AutoRigTaskSnapshot[string] snapshots;
    AutoRigInputContext[string] inputContexts;
    AutoRigOutputContext[string] outputContexts;
    AutoRigTaskActionBoundary actionBoundary;
    AutoRigEditorDispatcher editorDispatcher;
    shared bool cancelRequested;
    bool busy;
    AutoRigValue[string] sessionValues;
    ulong[string] inputRevisions;
    ulong contextRevision_;
    AutoRigWorkspace[string] workspaces;

    this(AutoRigProcessor processor, string id, string directory,
        AutoRigTaskActionBoundary actionBoundary, AutoRigEditorDispatcher editorDispatcher) {
        this.processor = processor;
        this.actionBoundary = actionBoundary;
        this.editorDispatcher = editorDispatcher;
        id_ = id;
        directory_ = directory;
        foreach (spec; processor.tasks()) {
            enforce(safeId(spec.id) && (spec.id in specs) is null, "Invalid or duplicate AutoRig task ID");
            bool[string] inputs;
            bool[string] outputs;
            foreach (port; spec.inputs) {
                enforce(safeId(port.id) && (port.id in inputs) is null, "Invalid or duplicate input port");
                inputs[port.id] = true;
            }
            foreach (port; spec.outputs) {
                enforce(safeId(port.id) && (port.id in outputs) is null, "Invalid or duplicate output port");
                outputs[port.id] = true;
            }
            specs[spec.id] = spec;
            snapshots[spec.id] = AutoRigTaskSnapshot(spec.id, 0, AutoRigTaskState.Pending);
            inputContexts[spec.id] = new AutoRigInputContext(this, spec.id);
            outputContexts[spec.id] = new AutoRigOutputContext(this, spec.id);
        }
        foreach (spec; specs) {
            foreach (dependency; spec.dependencies)
                enforce((dependency in specs) !is null && dependency != spec.id, "Invalid AutoRig dependency");
            bool[string] connected;
            foreach (connection; spec.connections) {
                enforce((connection.input in connected) is null, "Duplicate AutoRig input connection");
                connected[connection.input] = true;
                enforce((connection.sourceTask in specs) !is null &&
                    connection.sourceTask != spec.id, "Invalid AutoRig connection source");
                enforce(hasPort(spec.inputs, connection.input), "Unknown AutoRig connected input");
                auto source = specs[connection.sourceTask];
                enforce(hasPort(source.outputs, connection.sourceOutput), "Unknown AutoRig source output");
                enforce(portKind(spec.inputs, connection.input) ==
                    portKind(source.outputs, connection.sourceOutput), "AutoRig connection kind mismatch");
            }
        }
        ubyte[string] visits;
        foreach (taskId; specs.keys) validateDependencies(taskId, visits);
    }

    void validateDependencies(string taskId, ref ubyte[string] visits) {
        auto status = taskId in visits;
        if (status !is null && *status == 2) return;
        enforce(status is null || *status != 1, "AutoRig task dependency cycle at " ~ taskId);
        visits[taskId] = 1;
        auto spec = specs[taskId];
        foreach (dependency; spec.dependencies) validateDependencies(dependency, visits);
        foreach (connection; spec.connections) validateDependencies(connection.sourceTask, visits);
        visits[taskId] = 2;
    }

    void invalidateDependents(string changed) {
        foreach (spec; specs) {
            bool affected = false;
            foreach (dependency; spec.dependencies)
                if (dependency == changed) affected = true;
            foreach (connection; spec.connections)
                if (connection.sourceTask == changed) affected = true;
            if (!affected || snapshots[spec.id].state == AutoRigTaskState.Stale) continue;
            if (snapshots[spec.id].state == AutoRigTaskState.Succeeded) {
                auto snapshot = snapshots[spec.id];
                snapshot.state = AutoRigTaskState.Stale;
                snapshots[spec.id] = snapshot;
            }
            committedOutputs.remove(spec.id);
            invalidateDependents(spec.id);
        }
    }

    void runTask(string taskId, ref bool[string] visiting, bool force) {
        enforce((taskId in specs) !is null, "Unknown AutoRig task: " ~ taskId);
        enforce((taskId in visiting) is null, "AutoRig task dependency cycle at " ~ taskId);
        if (!force && snapshots[taskId].state == AutoRigTaskState.Succeeded) return;
        if (!force && snapshots[taskId].state == AutoRigTaskState.Failed &&
            specs[taskId].retainFailureOutputs && (taskId in committedOutputs) !is null) return;
        visiting[taskId] = true;
        scope(exit) visiting.remove(taskId);
        auto spec = specs[taskId];
        foreach (dependency; spec.dependencies) runTask(dependency, visiting, false);
        foreach (connection; spec.connections) runTask(connection.sourceTask, visiting, false);
        enforce(!atomicLoad(cancelRequested), "AutoRig run canceled");

        AutoRigValue[string] inputs;
        if (auto supplied = taskId in suppliedInputs) inputs = supplied.dup;
        foreach (connection; spec.connections) {
            auto source = connection.sourceTask in committedOutputs;
            enforce(source !is null, "AutoRig dependency has no outputs: " ~ connection.sourceTask);
            auto value = connection.sourceOutput in *source;
            enforce(value !is null, "AutoRig dependency output missing: " ~ connection.sourceOutput);
            // Task input() returns an owned copy. The private input table can
            // reference committed storage without duplicating a whole model/tree.
            inputs[connection.input] = *value;
        }
        foreach (port; spec.inputs) {
            auto value = port.id in inputs;
            enforce(!port.required || value !is null, "Missing AutoRig input: " ~ port.id);
            if (value !is null) enforce(value.kind == port.kind, "AutoRig input kind mismatch: " ~ port.id);
        }

        AutoRigTaskSnapshot snapshot;
        synchronized (this) {
            invalidateDependents(taskId);
            committedOutputs.remove(taskId);
            snapshot = snapshots[taskId];
            snapshot.attempt++;
            snapshot.state = AutoRigTaskState.Running;
            snapshot.message = null;
            snapshot.artifacts = null;
            snapshots[taskId] = snapshot;
        }
        auto attemptDirectory = buildPath(directory_, taskId, "attempt-" ~ snapshot.attempt.to!string);
        auto context = new AutoRigTaskContext(spec, attemptDirectory, inputs,
            { return atomicLoad(cancelRequested); },
            (AutoRigArtifact artifact) {
                synchronized (this) {
                    auto current = snapshots[taskId];
                    artifact.attempt = current.attempt;
                    current.artifacts ~= artifact;
                    snapshots[taskId] = current;
                }
            }, editorDispatcher);
        context.sessionId_ = id_;
        context.workspaceProvider = (string name, AutoRigWorkspace delegate() create) {
            synchronized (this) {
                if (auto existing = name in workspaces) return *existing;
                auto workspace = create();
                enforce(workspace !is null, "AutoRig workspace factory returned null");
                workspaces[name] = workspace;
                return workspace;
            }
        };
        // Completed command closures must not retain an attempt's dependency
        // payloads or callbacks after the session commits its independent outputs.
        scope(exit) {
            context.inputs = null;
            context.outputs = null;
            context.artifacts = null;
            context.sessionValues = null;
            context.workspaceProvider = null;
            context.onPreview = null;
            context.canceled = null;
            context.editorDispatcher = null;
        }
        synchronized (this) foreach (name, value; sessionValues)
            context.sessionValues[name] = copyValue(value);
        try {
            void executeInFiber() {
                auto fiber = new Fiber({ processor.executeTask(taskId, context); },
                    pageSize * Fiber.defaultStackPages * 4);
                while (fiber.state != Fiber.State.TERM) fiber.call();
            }
            if (actionBoundary !is null && !spec.ownsActionBoundary)
                actionBoundary(taskId, &executeInFiber);
            else
                executeInFiber();
            enforce(!atomicLoad(cancelRequested), "AutoRig run canceled");
            foreach (port; spec.outputs)
                enforce(!port.required || (port.id in context.outputs) !is null,
                    "Missing AutoRig output: " ~ port.id);
            enforce(!context.reportedFailure.length,context.reportedFailure);
            snapshot.state = AutoRigTaskState.Succeeded;
            synchronized (this) committedOutputs[taskId] = context.outputs.dup;
        } catch (Exception error) {
            snapshot.state = atomicLoad(cancelRequested) ? AutoRigTaskState.Canceled : AutoRigTaskState.Failed;
            snapshot.message = error.msg;
            if (spec.retainFailureOutputs && context.reportedFailure.length && !atomicLoad(cancelRequested)) {
                bool complete = true;
                foreach (port; spec.outputs) if (port.required && (port.id in context.outputs) is null) complete = false;
                if (complete) synchronized (this) committedOutputs[taskId] = context.outputs.dup;
            }
        }
        synchronized (this) snapshot.artifacts = snapshots[taskId].artifacts;
        foreach (artifact; context.artifacts) {
            auto item = artifact;
            item.attempt = snapshot.attempt;
            item.committed = snapshot.state == AutoRigTaskState.Succeeded ||
                spec.retainFailureOutputs && (taskId in committedOutputs) !is null;
            snapshot.artifacts ~= item;
        }
        synchronized (this) snapshots[taskId] = snapshot;
        enforce(snapshot.state == AutoRigTaskState.Succeeded ||
            spec.retainFailureOutputs && (taskId in committedOutputs) !is null,
            "AutoRig task " ~ taskId ~ " failed: " ~ snapshot.message);
    }

public:
    /** Outputs remain owned by the live session; reopening requires no disk read. */
    void restoreCommittedOutputs() {
        enforce(!busy, "Cannot restore an executing AutoRig session");
    }

    string id() { return id_; }
    string directory() { return directory_; }
    string processorId() { return processor.procId(); }
    bool isBusy() { synchronized (this) return busy; }

    /** Retained artifact payload sizes, without copying or parsing the payloads. */
    JSONValue memoryInfo() {
        synchronized (this) {
            ulong jsonBytes, blobBytes, previewBytes, cpuBytes;
            JSONValue[] entries;
            foreach (id, snapshot; snapshots) foreach (artifact; snapshot.artifacts) {
                auto size = artifact.byteLength;
                if (artifact.kind == AutoRigValueKind.Json) jsonBytes += size;
                if (artifact.kind == AutoRigValueKind.Blob) blobBytes += size;
                if (artifact.preview) previewBytes += size;
                entries ~= JSONValue(["task":JSONValue(id), "port":JSONValue(artifact.portId),
                    "kind":JSONValue(artifact.kind.to!string), "preview":JSONValue(artifact.preview),
                    "bytes":JSONValue(cast(ulong)size)]);
            }
            JSONValue[string] workspaceInfo;
            foreach (name, workspace; workspaces) {
                auto info = workspace.memoryInfo();
                workspaceInfo[name] = info;
                jsonBytes += info["json_bytes"].uinteger;
                cpuBytes += info["cpu_bytes"].uinteger;
            }
            return JSONValue(["json_bytes":JSONValue(jsonBytes), "blob_bytes":JSONValue(blobBytes),
                "cpu_bytes":JSONValue(cpuBytes), "workspaces":JSONValue(workspaceInfo),
                "preview_bytes":JSONValue(previewBytes), "artifacts":JSONValue(entries)]);
        }
    }
    /** Release payloads even when an old command retains a reference to this session. */
    void dispose() {
        synchronized (this) {
            enforce(!busy, "Cannot dispose a running AutoRig session");
            import std.file : exists, isDir, isSymlink, rmdirRecurse;
            import std.path : absolutePath, baseName;
            auto ownedDirectory = absolutePath(directory_);
            enforce(ownedDirectory.baseName == id_, "Invalid AutoRig session directory");
            if (exists(ownedDirectory)) {
                enforce(isDir(ownedDirectory) && !isSymlink(ownedDirectory),
                    "AutoRig session directory must be an owned directory");
                rmdirRecurse(ownedDirectory);
            }
            foreach (workspace; workspaces) workspace.dispose();
            workspaces = null;
            committedOutputs = null;
            suppliedInputs = null;
            sessionValues = null;
            foreach (ref snapshot; snapshots) snapshot.artifacts = null;
        }
    }
    bool isCanceled() { return atomicLoad(cancelRequested); }

    void setContextValue(string name, AutoRigValue value) {
        synchronized (this) {
            enforce(!busy, "Cannot change AutoRig context during execution");
            enforce(name.length > 0, "AutoRig context name is empty");
            sessionValues[name] = copyValue(value);
            ++contextRevision_;
            foreach (taskId, ref snapshot; snapshots) {
                if (snapshot.state == AutoRigTaskState.Succeeded) snapshot.state = AutoRigTaskState.Stale;
            }
            committedOutputs = null;
        }
    }

    AutoRigValue contextValue(string name) {
        synchronized (this) {
            auto value = name in sessionValues;
            enforce(value !is null, "Missing AutoRig session context: " ~ name);
            return copyValue(*value);
        }
    }

    string[] contextNames() { synchronized (this) return sessionValues.keys; }

    AutoRigValueInfo contextInfo(string name) {
        synchronized (this) {
            auto value = name in sessionValues;
            enforce(value !is null, "Missing AutoRig session context: " ~ name);
            return valueInfo(*value, contextRevision_);
        }
    }

    ulong inputRevision(string taskId) {
        synchronized (this) return inputRevisions.get(taskId, 0);
    }

    void invalidateTask(string taskId) {
        synchronized (this) {
            enforce(!busy, "Cannot invalidate AutoRig tasks during execution");
            enforce((taskId in specs) !is null, "Unknown AutoRig task");
            invalidateDependents(taskId);
            committedOutputs.remove(taskId);
            auto snapshot = snapshots[taskId];
            snapshot.state = AutoRigTaskState.Stale;
            snapshots[taskId] = snapshot;
        }
    }

    void setInput(string taskId, string portId, AutoRigValue value) {
        synchronized (this) {
            enforce(!busy, "Cannot change AutoRig inputs during execution");
            auto spec = taskId in specs;
            enforce(spec !is null && hasPort(spec.inputs, portId), "Unknown AutoRig input");
            enforce(portKind(spec.inputs, portId) == value.kind, "AutoRig input kind mismatch");
            foreach (connection; spec.connections)
                enforce(connection.input != portId, "Connected AutoRig input cannot be supplied directly");
            suppliedInputs[taskId][portId] = copyValue(value);
            ++inputRevisions[taskId];
            if ((taskId in committedOutputs) !is null)
                invalidateDependents(taskId);
            committedOutputs.remove(taskId);
            auto snapshot = snapshots[taskId];
            if (snapshot.state == AutoRigTaskState.Succeeded) snapshot.state = AutoRigTaskState.Stale;
            snapshots[taskId] = snapshot;
        }
    }

    void execute(string taskId, bool force = false) {
        executeGroup([taskId], force);
    }

    private void finishExecution(string[] taskIds, ref uint[string] initialAttempts) {
        synchronized (this) {
            if (atomicLoad(cancelRequested)) foreach (taskId; taskIds) {
                if ((taskId in snapshots) is null) continue;
                if (snapshots[taskId].state == AutoRigTaskState.Succeeded &&
                    snapshots[taskId].attempt > initialAttempts.get(taskId,0)) continue;
                snapshots[taskId].state = AutoRigTaskState.Canceled;
                snapshots[taskId].message = "AutoRig run canceled";
            }
            // Acknowledge only a stopped execution; preserve requests made before startup.
            atomicStore(cancelRequested, false);
            busy = false;
        }
    }

    void executeGroup(string[] taskIds, bool force = false) {
        uint[string] initialAttempts;
        synchronized (this) {
            enforce(!busy, "AutoRig session is already executing");
            foreach (taskId; taskIds)
                if (auto snapshot = taskId in snapshots) initialAttempts[taskId] = snapshot.attempt;
            busy = true;
        }
        scope(exit) finishExecution(taskIds,initialAttempts);
        // Retry failed dependencies once per execution, including those hidden
        // behind a succeeded step that consumed a retained failure checkpoint.
        synchronized (this) {
            bool[string] inspected;
            void prepare(string taskId) {
                enforce((taskId in specs) !is null, "Unknown AutoRig task: " ~ taskId);
                if ((taskId in inspected) !is null) return;
                inspected[taskId] = true;
                auto spec = specs[taskId];
                foreach (dependency; spec.dependencies) prepare(dependency);
                foreach (connection; spec.connections) prepare(connection.sourceTask);
                if (snapshots[taskId].state == AutoRigTaskState.Failed) {
                    invalidateDependents(taskId);
                    committedOutputs.remove(taskId);
                    snapshots[taskId].state = AutoRigTaskState.Stale;
                }
            }
            foreach (taskId; taskIds) prepare(taskId);
        }
        bool[string] visiting;
        foreach (taskId; taskIds) {
            runTask(taskId, visiting, force);
        }
    }

    void cancel() { synchronized (this) atomicStore(cancelRequested, true); }

    AutoRigTaskSnapshot task(string taskId, bool includeArtifacts = true) {
        synchronized (this) {
            auto found = taskId in snapshots;
            enforce(found !is null, "Unknown AutoRig task");
            auto result = *found;
            result.artifacts = includeArtifacts ? result.artifacts.dup : null;
            return result;
        }
    }

    AutoRigTaskSpec taskSpec(string taskId) {
        auto found = taskId in specs;
        enforce(found !is null, "Unknown AutoRig task");
        auto result = *found;
        result.dependencies = result.dependencies.dup;
        result.inputs = result.inputs.dup;
        result.outputs = result.outputs.dup;
        result.connections = result.connections.dup;
        return result;
    }

    bool hasSuppliedInput(string taskId, string portId) {
        auto spec = taskSpec(taskId);
        enforce(hasPort(spec.inputs, portId), "Unknown AutoRig input");
        synchronized (this) {
            auto values = taskId in suppliedInputs;
            return values !is null && (portId in *values) !is null;
        }
    }

    AutoRigValue suppliedInput(string taskId, string portId) {
        auto spec = taskSpec(taskId);
        enforce(hasPort(spec.inputs, portId), "Unknown AutoRig input");
        synchronized (this) {
            auto values = taskId in suppliedInputs;
            enforce(values !is null && (portId in *values) !is null, "AutoRig input has no supplied value");
            return copyValue((*values)[portId]);
        }
    }

    bool hasOutput(string taskId, string portId) {
        auto spec = taskSpec(taskId);
        enforce(hasPort(spec.outputs, portId), "Unknown AutoRig output");
        synchronized (this) {
            auto values = taskId in committedOutputs;
            return values !is null && (portId in *values) !is null;
        }
    }

    IAutoRigInputContext inputContext(string taskId) {
        auto found = taskId in inputContexts;
        enforce(found !is null, "Unknown AutoRig task");
        return *found;
    }

    IAutoRigOutputContext outputContext(string taskId) {
        auto found = taskId in outputContexts;
        enforce(found !is null, "Unknown AutoRig task");
        return *found;
    }

    bool renderInputUI(string taskId) {
        return processor.renderInputUI(taskId, inputContext(taskId));
    }

    bool renderOutputUI(string taskId) {
        return processor.renderOutputUI(taskId, outputContext(taskId));
    }

    AutoRigValue output(string taskId, string portId) {
        synchronized (this) {
            auto taskOutputs = taskId in committedOutputs;
            enforce(taskOutputs !is null, "AutoRig task has no committed outputs");
            auto found = portId in *taskOutputs;
            enforce(found !is null, "Unknown AutoRig output");
            return copyValue(*found);
        }
    }

    AutoRigArtifact outputArtifact(string taskId, string portId) {
        synchronized (this) {
            enforce((taskId in committedOutputs) !is null,
                "AutoRig task has no committed outputs");
            auto found = taskId in snapshots;
            enforce(found !is null, "Unknown AutoRig task");
            foreach (artifact; found.artifacts)
                if (artifact.portId == portId && artifact.committed && !artifact.preview) {
                    return artifact;
                }
            throw new Exception("Unknown committed AutoRig artifact: " ~ portId);
        }
    }
}

/** Editable input state for one task instance; connected ports remain read-only. */
class AutoRigInputContext : IAutoRigInputContext {
private:
    AutoRigSession session_;
    string taskId_;

    this(AutoRigSession session, string taskId) {
        session_ = session;
        taskId_ = taskId;
    }

    bool findConnection(string portId, out AutoRigConnection connection) {
        auto spec = session_.taskSpec(taskId_);
        enforce(hasPort(spec.inputs, portId), "Unknown AutoRig input");
        foreach (item; spec.connections)
            if (item.input == portId) {
                connection = item;
                return true;
            }
        return false;
    }

public:
    override string taskId() { return taskId_; }
    override AutoRigPortSpec[] ports() { return session_.taskSpec(taskId_).inputs; }
    override bool isConnected(string portId) {
        AutoRigConnection source;
        return findConnection(portId, source);
    }
    override bool canEdit(string portId) { return !session_.isBusy() && !isConnected(portId); }

    override bool hasValue(string portId) {
        AutoRigConnection source;
        if (findConnection(portId, source))
            return session_.hasOutput(source.sourceTask, source.sourceOutput);
        return session_.hasSuppliedInput(taskId_, portId);
    }

    override AutoRigValue value(string portId) {
        AutoRigConnection source;
        if (findConnection(portId, source))
            return session_.output(source.sourceTask, source.sourceOutput);
        return session_.suppliedInput(taskId_, portId);
    }

    override void setValue(string portId, AutoRigValue value) {
        session_.setInput(taskId_, portId, value);
    }
}

/** Output state for one task instance, including in-progress or failed artifacts. */
class AutoRigOutputContext : IAutoRigOutputContext {
private:
    AutoRigSession session_;
    string taskId_;

    this(AutoRigSession session, string taskId) {
        session_ = session;
        taskId_ = taskId;
    }

public:
    override string taskId() { return taskId_; }
    override AutoRigPortSpec[] ports() { return session_.taskSpec(taskId_).outputs; }
    override AutoRigTaskSnapshot snapshot() { return session_.task(taskId_); }
    override bool hasValue(string portId) { return session_.hasOutput(taskId_, portId); }
    override AutoRigValue value(string portId) { return session_.output(taskId_, portId); }
    override AutoRigArtifact artifact(string portId) { return session_.outputArtifact(taskId_, portId); }
}

private bool hasPort(AutoRigPortSpec[] ports, string id) {
    foreach (port; ports) if (port.id == id) return true;
    return false;
}

private AutoRigValueKind portKind(AutoRigPortSpec[] ports, string id) {
    foreach (port; ports) if (port.id == id) return port.kind;
    throw new Exception("Unknown AutoRig port: " ~ id);
}

/** Registers built-in processors and allocates isolated run directories. */
class AutoRigSessionManager {
private:
    string root;
    AutoRigProcessor[string] processors;
    string[string] processorAliases;
    AutoRigSession[string] sessions;
    AutoRigSession[string] closedSessions;
    AutoRigTaskActionBoundary actionBoundary;
    AutoRigEditorDispatcher editorDispatcher;

public:
    this(string root) {
        enforce(root.length > 0, "AutoRig workspace root is empty");
        this.root = absolutePath(root);
    }

    void registerProcessor(AutoRigProcessor processor) {
        enforce(processor !is null && safeId(processor.procId()), "Invalid AutoRig processor");
        enforce((processor.procId() in processors) is null, "Duplicate AutoRig processor");
        processors[processor.procId()] = processor;
    }

    void registerProcessorAlias(string previousId, string currentId) {
        enforce(safeId(previousId) && (previousId in processors) is null &&
            (previousId in processorAliases) is null, "Invalid or duplicate AutoRig processor alias");
        enforce((currentId in processors) !is null, "Unknown AutoRig processor alias target");
        processorAliases[previousId] = currentId;
    }

    void setTaskActionBoundary(AutoRigTaskActionBoundary boundary) {
        actionBoundary = boundary;
    }

    void setEditorDispatcher(AutoRigEditorDispatcher dispatcher) {
        editorDispatcher = dispatcher;
    }

    AutoRigProcessor[] listProcessors() {
        AutoRigProcessor[] result;
        foreach (processor; processors) result ~= processor;
        return result;
    }

    AutoRigSession create(string processorId) {
        return createAt(processorId, root);
    }

    AutoRigSession createAt(string processorId, string workspaceRoot) {
        return createWithProcessor(processor(processorId), workspaceRoot);
    }

    AutoRigSession createWithProcessor(AutoRigProcessor processor, string workspaceRoot) {
        enforce(processor !is null && safeId(processor.procId()), "Invalid AutoRig processor");
        auto runId = randomUUID().toString();
        auto session = new AutoRigSession(processor, runId, buildPath(workspaceRoot, runId),
            actionBoundary, editorDispatcher);
        sessions[runId] = session;
        return session;
    }

    AutoRigSession reopenWithProcessor(AutoRigProcessor processor, string runId) {
        auto found = runId in closedSessions;
        enforce(found !is null && (runId in sessions) is null, "Unknown closed AutoRig session");
        auto session = *found;
        enforce(session.processorId() == processor.procId(), "AutoRig processor identity mismatch");
        closedSessions.remove(runId);
        sessions[runId] = session;
        return session;
    }

    AutoRigProcessor processor(string processorId) {
        if (auto currentId = processorId in processorAliases) processorId = *currentId;
        auto found = processorId in processors;
        enforce(found !is null, "Unknown AutoRig processor");
        return *found;
    }

    string rootDirectory() { return root; }

    AutoRigSession get(string runId) {
        auto found = runId in sessions;
        enforce(found !is null, "Unknown AutoRig run");
        return *found;
    }

    void close(string runId) {
        auto session = get(runId);
        enforce(!session.isBusy(), "Cannot close a running AutoRig session");
        closedSessions[runId] = session;
        sessions.remove(runId);
    }

    void remove(string runId) {
        auto active = runId in sessions;
        auto closed = runId in closedSessions;
        enforce(active !is null || closed !is null, "Unknown AutoRig session");
        auto session = active !is null ? *active : *closed;
        enforce(!session.isBusy(), "Cannot delete a running AutoRig session");
        session.dispose();
        sessions.remove(runId);
        closedSessions.remove(runId);
    }

    void disposeAll() {
        foreach (session; sessions) enforce(!session.isBusy(), "Cannot dispose a running AutoRig session");
        foreach (session; closedSessions) enforce(!session.isBusy(), "Cannot dispose a running AutoRig session");
        foreach (runId; sessions.keys) remove(runId);
        foreach (runId; closedSessions.keys) remove(runId);
    }
}
