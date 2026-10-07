# AutoRig processor and session framework

## Current memory ownership

AutoRig sessions now keep model snapshots, JSON, binary artifacts and previews
in memory. Task publication does not write files or create run directories.
The native model port is a `Blob` snapshot produced by the engine memory API;
it is restored through the engine memory loader for retry or rollback. PNG
previews are captured through `CaptureLiveScreenshotCommand` and retained as
bytes. Session close/reopen is supported within the same application process;
disk-based recovery and automatic model saving have been removed. The original
disk-storage design described below is superseded by this ownership model.

This document describes the first D framework for built-in AutoRig processors. A processor contains shared implementation code and exposes its programs as named task functions. The first intended processor is a D port of `nijigenerate-deterministic-rig`.

Processors may also publish workflow presets. A workflow connects task calls from its provider or other registered processors. The workflow manager lists these presets and creates one `AutoRigSession` when a user invokes one.

## Boundary

`AutoRigProcessor` declares a stable processor ID, display name, task specifications, and `executeTask(taskId, context)`. A processor may call any shared D code internally. The framework runs selected tasks and their dependencies in one session. There is no per-task process invocation or CLI argument mapping.

The framework lives in `source/nijigenerate/autorig/framework.d`. It is independent of the editor's layout and deform UI. Either mode can create a session and supply inputs through the same API. A task declares model-related inputs only when it needs them.

## Task contract

Each `AutoRigTaskSpec` has an ID, dependencies, input and output ports, and connections from an earlier task's output to its input. Port IDs are local to a task. The four value kinds are:

| Kind | Meaning |
| --- | --- |
| `FileName` | A filename value without a directory. It does not imply that a file exists. |
| `Path` | A reference to a file or directory. User-supplied paths remain external inputs; output paths are allocated inside the run. |
| `Json` | A structured JSON value. It is available in memory and saved as JSON for inspection. |
| `Blob` | Binary bytes. They are available in memory and saved as a binary artifact. |

Connections require matching kinds. The framework checks task and port IDs, connections, required inputs, and required outputs. Connections imply execution dependencies; explicit dependencies cover ordering without data transfer. Cycles fail before a task is run.

For example, a D port may declare `prepare-psd` with a `Path` input and `Json` observation output. `derive-evidence` can take that observation as `Json` and return `Json` evidence. A later task can request a `Path` output for an INX model. This does not require every task to share one input schema.

## Session and artifact ownership

`AutoRigSessionManager` registers built-in processors and creates runs beneath an application-selected root. Each run gets a generated ID and directory. Every task attempt gets a separate directory:

```text
<root>/<run-id>/<task-id>/attempt-<number>/
    <output-port>.json | .blob | .name | <allocated path>
    preview-<preview-id>-<version>.<extension>
    result.json
```

Task code reads inputs by port name and writes outputs with `publishJson`, `publishBlob`, `publishFileName`, or `outputPath` followed by `publishPath`. It never chooses a run directory. A failed attempt retains its files and `result.json` for inspection, but its outputs are not passed to downstream tasks. A successful attempt commits its declared outputs together. A new attempt does not overwrite the previous one.

Changing a supplied input invalidates that task and downstream results. Re-executing a task with `force=true` similarly invalidates its dependents. `executeGroup` runs selected tasks in dependency order, reusing valid completed dependencies. `cancel()` sets a cooperative cancellation flag; processors doing long work should check `context.isCanceled()` regularly. A canceled or failed run may be retried in the same session.

`previewJson`, `previewBlob`, and `previewPath`/`publishPreview` expose versioned, noncommitted artifacts during execution. `session.task(taskId)` returns task state, attempt number, message, and artifact records, including previews. A viewer can choose its display from the artifact kind, MIME type, and optional port `viewHint`.

## Task UI contract

A processor may implement `IAutoRigInputEditor` and/or `IAutoRigOutputViewer`, following AutoMesh's processor-owned `configure()` pattern. Their `configureInput(taskId, context)` and `viewOutput(taskId, context)` methods draw the editor and result viewer. These methods are called from the editor UI thread through `session.renderInputUI(taskId)` and `session.renderOutputUI(taskId)`. Both return `false` when no custom implementation exists, allowing a generic port or artifact viewer. The framework does not create an editor panel or impose an ImGui dependency on processors without UI.

The session owns one stable `AutoRigInputContext` and `AutoRigOutputContext` per task instance. They implement `IAutoRigInputContext` and `IAutoRigOutputContext`; the same interfaces are passed to custom renderers and can be retrieved directly with `session.inputContext(taskId)` and `session.outputContext(taskId)`. They represent task state rather than UI-only wrappers. `AutoRigTaskContext` remains the short-lived execution context for one attempt.

The input context lists declared ports and exposes `isConnected`, `canEdit`, `hasValue`, `value`, and `setValue`. `setValue` uses `AutoRigSession.setInput`, so it validates the kind, rejects edits to connected ports or during execution, and invalidates downstream results. Connected values can be inspected but are read-only. The output context lists output ports and exposes the current task snapshot, committed values, and committed output artifacts. Its snapshot includes versioned previews and failed-attempt files, so a viewer can show progress without treating those files as committed outputs.

In a workflow, a UI call on a task instance is dispatched to the originating processor with its original task ID. The context still carries the task instance ID and the one workflow session, so repeated calls to the same task remain distinct and cross-plugin editors use the same run's values and files.

## AutoRig panel

The AutoRig panel is available in both Layout and Deform. A built-in processor registers through `ngRegisterAutoRigProcessor(processor)` in `nijigenerate.panels.autorig`; the panel reads presets from the shared `AutoRigSessionManager`. The top dropdown lists every registered processor's workflow presets. Its Play button creates a new workflow session under the application's AutoRig run directory.

Each session is a tree with its own Play button. It lists workflow steps in dependency order, even if the preset declares them in another order. Session Play reruns the entire workflow in that order. A task's Play button reruns that task and any unfinished dependencies in the same session. The API's default `execute()` and `executeStep()` calls still reuse successful results; passing `force=true` gives Play behavior. Every actual task invocation runs in a Fiber, and panel Play runs the workflow on a separate thread. Like AutoMesh, processors should perform computation in the worker and call `context.runOnMainThread()` for editor mutations. The panel's session manager opens and closes each task's `GroupAction` on the main thread with `incActionPushGroup()` / `incActionPopGroup()`. This covers internal dependencies and tasks from other processors, making each task's actions one Undo entry while preserving its own nested group calls. A failed task still closes its group, so any actions it emitted remain undoable. Tasks that reuse a successful result create no Undo entry. The Input and Output branches call the processor-owned UI interfaces when present. Otherwise they show a generic port editor for `FileName`, `Path`, and `Json`, a file loader for `Blob`, and output values plus artifact paths. The panel reports parse, file, and task errors without discarding the session.

`close(runId)` removes an idle in-memory session from the manager and leaves the run directory intact. Reopening a run from disk, retention policy, and cleanup will be added with the UI and persistence layer. The current session API is synchronous; a UI caller must schedule execution outside the render loop and route model mutations through the editor's existing action boundary.

## Workflow presets and calls

`AutoRigProcessor.workflows()` returns zero or more `AutoRigWorkflowSpec` values. The workflow ID is local to its provider; callers identify a preset by `(providerId, workflowId)`. A preset declares its public input and output ports, named steps, step ordering, and bindings. Each step specifies a processor ID and task ID. A connection names the source step's output and the destination step's input. An input binding maps a public workflow input to a task input. The optional `targetTask` field permits binding an input of the selected task's internal dependency; an empty value means the selected task itself.

`AutoRigWorkflowManager.listPresets()` provides the user-facing preset list. `create(providerId, workflowId)` validates the complete graph and creates a single session. Validation checks processor and task availability, port kinds, required input coverage, duplicate bindings, and dependency cycles before creating files. `AutoRigWorkflowRun` is a controller for that session: its ID and directory are the session ID and directory. `setInput()` supplies public inputs before execution. `execute()` runs the tasks in dependency order; a failed or canceled call can be retried, reusing completed tasks. `session()`, `stepTaskId()`, `snapshot()`, `output()`, `cancel()`, and the manager's `close()` provide execution and inspection control. Workflow state is derived from session task states.

Each step becomes a task instance in the same session. Its instance ID includes the step ID, so a preset can call the same processor task more than once with different inputs. Internal task dependencies are included under that step. The session owns all attempts and outputs:

```text
<root>/<run-id>/workflow.json
<root>/<run-id>/<task-instance-id>/attempt-<number>/...
```

Cross-step values use ordinary task connections in that same session. The session passes only committed outputs, checks matching kinds, and invalidates downstream results when an upstream input changes. JSON is copied at the connection boundary; Blob and Path artifacts stay in the one run directory. `workflow.json` records the preset identity and step-to-task-instance mapping. Task attempt records hold output and preview provenance.

For example, a processor can publish both a one-step `prepare-only` preset and a `prepare-and-consume` preset. The latter can connect its own `prepare` task's JSON output to another processor's `consume` task. The provider defines the preset; the second processor only needs to publish the named task contract.

## Example

```d
class ExampleRigProcessor : AutoRigProcessor {
    override string procId() { return "example-rig"; }
    override string displayName() { return "Example rig"; }

    override AutoRigTaskSpec[] tasks() {
        return [AutoRigTaskSpec("prepare", "Prepare", null,
            [AutoRigPortSpec("source", AutoRigValueKind.Json)],
            [AutoRigPortSpec("result", AutoRigValueKind.Json)], null)];
    }

    override void executeTask(string taskId, AutoRigTaskContext context) {
        auto source = context.input("source").json;
        context.publishJson("result", source);
    }
}
```

The standalone framework test is in `tests/autorig_framework.d`. Run it with:

```powershell
C:\opt\ldc-1.41\bin\dub.exe build --root=tests --compiler=C:\opt\ldc-1.41\bin\ldc2.exe --skip-registry=all
.\out\autorig-framework-tests.exe
```

`AnimeFrontViewRigProcessor` now registers stage-local OSQP face projection and
grid-depth fitting presets, plus a projection-to-editor workflow using the
existing undoable deformation command. See [OSQP integration](osqp-integration.md)
for inputs, build requirements, checks and the remaining full-rig migration.
