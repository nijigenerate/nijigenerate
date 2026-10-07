# AutoRig panel status

The processor is `AnimeFrontViewRigProcessor`, registered as
`anime-front-view-rig` and displayed as `Anime Front View Rig`. It targets anime
front-view standing character artwork. The previous `deterministic-rig` ID is
accepted as a compatibility alias for saved sessions, without adding a second
processor or workflow to the picker.

Processor, workflow and task labels and descriptions are gettext message IDs,
translated by the panel at display time. `ngAutoRigMessage()` marks literals
for `genpot.sh` extraction without introducing an i18n dependency into pure
processor computation or standalone tests. Internal IDs and persisted values
remain language-independent. Japanese AutoRig messages are provided in
`tl/ja.po`; `gentl.sh` compiles the locale catalogs for application loading.

The only rig workflow is `Imported model to anatomical rig`. The plus button
beside the selector adds a pending session. Expand the session to configure
`Workflow arguments`, apply the values, then use its play button to execute.
Its options default to `{}`. Import a model before executing it;
Names, ancestors, clipping receivers and alpha proximity classify materials
automatically. Role overrides in the compile input are optional.
The workflow play button retries failed tasks and resumes pending or stale tasks
while preserving completed stages. Individual step buttons
explicitly retry the selected step and invalidate downstream results.
Connected JSON and output JSON display artifact availability and file locations,
rather than copying and formatting the full data every frame.

`Session context / artifacts` accepts named `Path`, `Json`, `Blob`, and
`FileName` values. Path references an external artifact; Blob imports file
contents into owned memory; Json carries structured context. Apply edits before
execution. Context values are available to every task through
`AutoRigTaskContext.contextValue(name)` and `contextNames()`, independently of
declared task input ports. A task receives an owned snapshot, with deep copies
of JSON and binary data. Context changes are rejected during execution and mark
completed tasks stale. Arguments and context are stored in `workflow.json` and
restored when reopening a session; existing records without context remain valid.
Path references retain their paths rather than copying external files.

Processors may implement `IAutoRigSessionEditor.configureSession(workflowId,
IAutoRigSessionEditContext)` and `IAutoRigSessionViewer.viewSession(workflowId,
IAutoRigSessionViewContext)` to supply session-level editing and display UI.
The view contract exposes run identity, workflow input/output ports, owned
values, named context and output artifact metadata. The edit contract adds
`canEdit()`, `setInput()` and `setContextValue()`; setters retain validation,
persistence and invalidation behavior. UI callbacks run on the main thread and
receive the selected workflow provider's session, including for cross-processor
workflows. The panel dispatches through `renderSessionInputUI()` and
`renderSessionOutputUI()`; returning false uses the generic panel UI. The default
output view displays committed artifact paths without loading full JSON outputs.
Task-level `IAutoRigInputEditor` and `IAutoRigOutputViewer` remain independent.

This preset contains fifteen stages: model observation, evidence compilation,
source group preparation, shoulder measurement, Part meshing, source UV
registration, feature composite meshing, shared domain compilation, native rig
construction, shoulder welding, local controls, depth validation, angle baking,
fixed-foot and cheek corrections, and saved-rig verification. Numerical fitting and face projection
remain processor tasks and are not published as separate workflow presets.

Finishing stages retain their failure state and error while passing a verified
rollback checkpoint to the following stages. The final report records every
attempt; a failed stage never counts as successful completion. Optional
`{"render":true}` options capture the source neutral image, compare the saved
neutral image, and publish fifteen head pose previews. Visual acceptance is
recorded separately from numerical validation.

The local API provides `ToolCommand_ExecuteAutoRig` with an `options` JSON string
and optional `context` JSON string, for example
`{"reference":{"kind":"Path","value":"C:/rig/reference.json"}}`,
and `ToolCommand_GetAutoRigStatus` with the returned `runId`. Both use the same
panel workflow and worker; production stages invoke editor commands directly.
`ToolCommand_ResumeAutoRig` accepts `runId` and `stepId`; an empty `stepId`
resumes the workflow from its committed artifacts, including after restarting
the editor. Checkpoint hashes, task identities and artifact paths are verified
before reuse. Native depth refresh completes before each checkpoint is saved.
AutoRig explicitly authors zero deformation keys as well as moving keys.
Workflow inputs, including optional overrides and rendering, persist across
editor restarts. JSON outputs are loaded from their committed artifact only
when consumed, rather than retaining a full copy for every completed stage.
The task snapshot and a JSON output named `result` use separate files.

The workflow header and each step display the current execution state:

- Running: blue text pulses with a rotating activity marker.
- Completed: green text with an `[OK]` marker.
- Failed: red text with an `[!]` marker.
- Canceled: amber text with a `[-]` marker.
- Needs rerun: amber text with a `[~]` marker for stale results.
- Pending: subdued text with an empty marker.

Status labels and markers accompany the colors. Expanded input and output sections
remain open when the state changes or the activity marker animates. Status comes
from synchronized workflow and task snapshots; animation uses the UI clock and
does not query the worker's model data.

Run the standalone solver and pipeline regression checks from the development
PowerShell with `./build-aux/test-autorig.ps1`. The script compiles current sources
and stops on compile, link or test failure. Owned JSON checkpoints preserve double
values during reloading and do not change the process-wide numeric locale.
