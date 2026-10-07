# AutoRig panel status

The processor is `AnimeFrontViewRigProcessor`, registered as
`anime-front-view-rig` and displayed as `Anime Front View Rig`. It targets anime
front-view standing character artwork. The previous `deterministic-rig` ID is
accepted as a compatibility alias, without adding a second
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
Session rows put trash and play buttons before their label; task rows put play
before the task label. The trash button removes the session and its cached input
drafts and in-memory artifacts. Running sessions cannot be deleted. Deletion
does not undo changes already applied to the editor model.
Its options default to `{}`. Import a model before executing it;
Names, ancestors, clipping receivers and alpha proximity classify materials
automatically. Role overrides in the compile input are optional.
The workflow play button retries failed tasks and resumes pending or stale tasks
while preserving completed stages. Individual step buttons
explicitly retry the selected step and invalidate downstream results.
Connected JSON and output JSON display artifact availability,
rather than copying and formatting the full data every frame.

`Session context / artifacts` accepts named `Path`, `Json`, `Blob`, and
`FileName` values. Path references an external artifact; Blob imports file
contents into owned memory; Json carries structured context. Apply edits before
execution. Context values are available to every task through
`AutoRigTaskContext.contextValue(name)` and `contextNames()`, independently of
declared task input ports. A task receives an owned snapshot, with deep copies
of JSON and binary data. Context changes are rejected during execution and mark
completed tasks stale. Arguments, context, task results and previews are owned
by the live session in memory. Closing and reopening a session in the same
editor process retains these values. They do not survive an editor restart.
Path references retain their paths rather than copying external files.

Processors may implement `IAutoRigSessionEditor.configureSession(workflowId,
IAutoRigSessionEditContext)` and `IAutoRigSessionViewer.viewSession(workflowId,
IAutoRigSessionViewContext)` to supply session-level editing and display UI.
The view contract exposes run identity, workflow input/output ports, owned
values, named context and output artifact metadata. The edit contract adds
`canEdit()`, `setInput()` and `setContextValue()`; setters retain validation,
ownership and invalidation behavior. UI callbacks run on the main thread and
receive the selected workflow provider's session, including for cross-processor
workflows. The panel dispatches through `renderSessionInputUI()` and
`renderSessionOutputUI()`; returning false uses the generic panel UI. The default
output view displays artifact availability and byte counts without copying full payloads.
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
resumes the workflow from its in-memory artifacts in the current editor process.
Model snapshot hashes and editor signatures are checked before reuse. Native
depth refresh completes before each model snapshot is acquired.
AutoRig explicitly authors zero deformation keys as well as moving keys.
Neck inference measures head-local alpha cross-section widths. A narrow separate
neck uses its inferior attachment; neck material that continues into the torso
uses the fitted narrow-to-wide transition. The same geometry works when neck and
body share a Part. Clothing is fallback support when skin is absent. Evidence
records the coordinate frame, sampled width profile, inferred material structure
and provenance; covered or unresolved narrow regions remain estimates. This
matches the Python riglib.neck implementation and does not use model identities.
AutoRig tasks do not save models, JSON, or images to disk. Native model snapshots
use the engine's memory serialization API; retry and failure restoration use
the engine's memory loader. JSON values remain owned data and images remain PNG
bytes. File saving remains an explicit editor command performed outside AutoRig.

The workflow header and each step display the current execution state:

- Running: blue activity icon, matching AutoMeshBatch.
- Completed: green success icon, matching AutoMeshBatch.
- Failed: red failure icon, matching AutoMeshBatch.
- Canceled: orange cancel icon.
- Needs rerun: orange waiting icon.
- Pending: orange waiting icon, matching AutoMeshBatch.

Status icons precede plain row titles and expose translated status tooltips.
Expanded input and output sections remain open when the state changes. Status
comes from synchronized workflow and task snapshots without querying worker model
data. While a session is running, its play button switches to Cancel, as in
AutoMeshBatch. Cancellation is cooperative; the worker finishes rollback before
the session can be retried or deleted.

Rendering connected JSON and model inputs uses availability labels instead of
copying their payloads. Session context metadata exposes kind, byte count and a
revision without copying JSON or model bytes. Editable JSON text is cached until
its input revision changes; user drafts remain independent of that cache.
The workflow caches its dependency order and status-only snapshots omit preview
artifact arrays. Worker-facing value getters still return owned copies.
Anime Front View Rig stores source point clouds and alpha runs once as immutable
CPU numeric arrays owned by the session. They are calculation inputs, not JSON
checkpoint contents. Observations, evidence and stage results are separate,
content-addressed in-memory JSON artifacts; native state checkpoints contain
small runtime fields and artifact references. Unchanged artifacts share storage.
Retries materialize an owned calculation view from the checkpoint's references,
including the original CPU support, without reading the PSD or writing files.
Session deletion explicitly releases these artifacts and buffers even if an old
editor command still references the session object. Memory status includes
`cpu_bytes` and per-workspace artifact metadata as well as `json_bytes`.

Material support classification uses exact nearest-alpha distances. Its spatial
index prunes subtrees by their full two-dimensional bounds, including for query
points outside a part's silhouette. This changes exploration cost without
sampling away support points or changing the nearest-distance calculation.
`tests/autorig_alpha_support.d` checks the index against exhaustive search using
real PSD-derived cloud JSON. Run status exposes `compile_profile` phase start
times in milliseconds for input restoration, material classification, evidence
derivation, program compilation and artifact retention.

Committed JSON and JSON previews retain serialized text in memory rather than
large trees of GC-scanned arrays and objects. Task readers restore independent
JSON trees with exact double precision. Private dependency tables share committed
storage until a reader consumes it; this does not expose mutable task data.
`tests/autorig_json_storage.d` compares retained memory and collection time using
an actual PSD-derived cloud JSON input and checks precision and reader isolation.
`ToolCommand_GetAutoRigStatus` reports retained payload bytes by stage and port
plus GC used/free bytes. These totals do not include in-flight task copies, native
allocations, or GPU textures. Preview bytes are a subset of the JSON/blob totals.
`ToolCommand_GetAutoRigMemoryStatus` accepts `runId` and `collectGarbage`; an empty
run ID measures all panel sessions. Explicit collection is rejected while a run
is executing. `ToolCommand_DeleteAutoRigSession` deletes a stopped session's
artifacts without changing the editor model. The `cycles` mode of the JSON storage
test checks repeated creation/deletion and collection using real cloud data.
Run `out/autorig-context-tests.exe --ui-performance` after compiling
`tests/autorig_framework.d` to check metadata ownership and measure payload-copy
avoidance. This benchmark measures data retrieval, not viewport frame rate.

Run the standalone solver and pipeline regression checks from the development
PowerShell with `./build-aux/test-autorig.ps1`. The script compiles current sources
and stops on compile, link or test failure. Owned JSON checkpoints preserve double
values during reloading and do not change the process-wide numeric locale.
