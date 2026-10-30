# D23 MCP dispatch contract

Status: submitted for independent source-contract review; runtime acceptance is separate.

## Authoritative execution paths

The current D sources are `api/mcp/{server,task,helpers,http_transport,
resource_listing}.d`, `commands/base.d`, `commands/automesh/dynamic.d`,
`project.d`, `core/actionstack.d`, `panels/autorig.d`, and `app.d`. The installed mcp 1.0.1
`mcp/server.d` and `mcp/tools.d` define the protocol wrapper. D06, D07,
D12 and D21 are prerequisites, not substitutes for these call paths.

### Tool admission and UI application

1. HTTP authorization and JSON parsing precede protocol dispatch. The MCP
   server checks initialization, resolves tools/call name and arguments, and
   the tool validates its schema before invoking the registered callback.
2. Registration captures the existing command instance from AllCommandMaps.
   Each call copies the JSONValue descriptor into payloadCopy and queues a
   closure through ngRunInMainThread!CommandResult. This is not a new command
   instance per request, nor an immutable deep clone of the payload graph.
3. The closure reads the UI thread's externalCommandsBlocked flag. A blocked
   command failing ngCommandAllowedInCurrentContext returns the AutoRig error
   before context construction or argument mutation. A parameterized live
   screenshot is rejected separately when the object context contains the
   parameterValue key. Preserve that exact key test.
4. Normally buildContextFromPayload builds the current shortcut execution
   context, then applies explicit overrides against incActivePuppet at this
   execution point. While editor-owned, it instead creates an empty Context;
   presentation commands receive the current puppet. Top-level arguments
   are then applied to the captured instance using its declared argument
   types. Missing fields are not blanket-reset; invalid field types follow
   the original branch behavior. Persistent instance fields can affect the
   next request and must not be replaced with per-call default initialization.
5. The editor-owned branch calls ngRunCommand directly. The normal branch
   prepares the action scope, installs scope(exit) finish, checks runnable,
   and dispatches the concrete command through ngRunCommand. That function
   independently checks permission and runnable again before run. Preserve
   these calls and their order; do not collapse repeated predicate calls.
6. ngRunInMainThread returns the CommandResult to the server caller. Encoding
   then calls virtual waitForCompletion outside the queued closure, converts
   the completed result, and unwraps a nested mcpDirectToolResult marker when
   the object also contains content. MCP Tool.execute passes through objects
   with content or _meta; otherwise it wraps the JSON as text content.

The base CommandResult wait returns itself. AutoMeshApplyResult instead waits
for its completion using vibe yield or a condition, returns finalResult when
nonnull, and publishes only its first completion. Initial UI application,
action-boundary exit, asynchronous completion and HTTP reply are distinct
events. Do not keep an action scope open simply until the response is sent.

### Action boundaries and exceptions

Prepare affects names starting VertexCommand_ and exact EditCommand_Undo or
EditCommand_Redo. It flushes action groups, guards VertexEdit or DepthEdit
scope according to the current edit mode (otherwise the default guard), then
flushes again. Finish always flushes groups in the normal branch after the
scope-exit guard has been installed. Errors in context/argument preparation,
the editor-owned branch, or prepare itself do not acquire that later guard.
This is a flush/scope contract, not a rollback transaction.

The queue wrapper catches D Exception, publishes completion and rethrows to
the waiting caller. It does not claim to catch all Throwable/Error. Tool.execute
converts Exception from validation or its handler to content plus isError=true;
the MCP request wrapper separately handles protocol/internal errors. Preserve
the distinction between a failed CommandResult and an exception envelope.

## Concurrent requests and lifetime

The shared queue appends under its mutex; order is mutex acquisition/enqueue
order, not HTTP arrival order or JSON-RPC ID order. A drain detaches the whole
array under the mutex and executes its nonnull callbacks without that mutex.
Enqueue during execution belongs to a later drain. An uncaught exception from
a raw queued action stops that detached loop; there is no retry/requeue loop.

ngRunInMainThread always queues, even for a UI-thread caller. It allocates
request-specific lock, condition, completion, result and exception storage.
A vibe TaskFiber yields while waiting; an ordinary thread waits on the
condition. There is no inline main-thread fast path or timeout in this helper.
Caller interruption must not invalidate storage retained by its queued
closure. Internal worker dispatches are not filtered by externalCommandsBlocked.

app.d drains after shortcuts when incShouldProcess is true, before AutoRig
polling and panel/window updates. AutoRig stop also drains while waiting for
its worker, then joins and clears the ownership gate. Do not assume there is
exactly one drain per rendered frame, or that draining is disabled by AutoRig.

HTTP response slots are keyed by Fiber, not request ID. sendMessage takes
that fiber's slot and removes it, then broadcasts to SSE clients; failing
streams are removed. handlePost removes its slot again after handleMessage
returns and emits 204 for absent/null IDs. There is no scope-exit slot removal
around handleMessage in this source. Preserve lifetime/error boundaries without
inventing guarantees for exceptions escaping that call.

## Cancellation

The installed MCP server's notification handler recognizes initialized only;
other notifications have no action there. Neither the UI queue record nor
ngRunInMainThread carries a protocol request ID, cancel flag or removal API.
Thus a cancellation notification or HTTP disconnect is not evidence that a
queued application was withdrawn or a completed command was rolled back.
Interruption of the waiter can leave its closure runnable. Command-specific
AutoRig/AutoMesh cancellation is a separate job mechanism, not general MCP
cancellation. Native cancellation facilities must not silently change this
observable contract; any deliberate policy change needs separate authorization.

## Project changes and resource reads

Default context and UUID overrides are resolved when the queued closure runs.
A project switch before that point changes the active puppet used for lookup;
the request does not carry an admission-time project generation. Explicit
parameters/nodes arrays clear their has flags before resolving valid UUIDs;
missing UUIDs are skipped. Explicit parameters without armedParameters clear
the inherited armed flag. Parameter-value/key-point validation follows helpers.d,
including rejection of keyPoint and simultaneous parameterValue/paramValue.

Concrete resource callbacks retain a UUID but look up the current puppet
inside their queued callback. Template UUID reads, selector/find tree reads,
and binding-descriptor reads likewise use queued UI work. They do not create
a transaction with any earlier resources/list or with a following tool call.
No automatic stale-project rejection or global queue clearing appears here.

Project-load notification hooks call the three resource notifiers; notification
does not cancel requests or refresh all captured command fields. An already
started command/job retains its own references according to its implementation.
Do not infer that project switching cancels all asynchronous jobs.

There is an important exception to UI-only reads: handlePost invokes
rewriteResourcesListResponse after protocol handling, which directly calls
buildCurrentResourceList and incActivePuppet on that HTTP execution context.
incActivePuppet reads activeProject, declared as an ordinary module variable
inside a private block, hence D thread-local storage. The HTTP thread does
not thereby read the UI thread's activeProject. Do not label this path a UI snapshot or move it to
the UI queue without acknowledging an observable change. The native ownership
mapping must be settled using D06/D12 and actual thread-context witnesses.

## Native implementation and grouped verification obligations

Use portable synchronization and the configured event-loop adapter, with any
necessary OS facilities isolated below that adapter. Preserve retained closure,
JSON descriptor and command-instance lifetimes; do not emulate D collection
ownership with dangling stack captures or accidental deep copies.

The module/project validation must cover: concurrent calls to the same command
with omitted fields; FIFO and enqueue-during-drain; separate fiber response
slots; ordinary-thread and event-loop waits; failed results versus exceptions;
scope exit before asynchronous completion; AutoRig presentation and screenshot
branches; cancellation before/after enqueue and waiter interruption; project
switch before drain versus after job capture; UUID reuse/missing UUIDs; and the
resources/list thread-context exception. Compare actual D producers and native
implementations. This contract alone accepts no native function or runtime
scenario, and unresolved providers cannot be replaced with success stubs.
