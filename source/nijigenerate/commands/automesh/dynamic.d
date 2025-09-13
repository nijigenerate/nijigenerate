module nijigenerate.commands.automesh.dynamic;

import nijigenerate.commands.base;
import nijigenerate.viewport.vertex;                // ngAutoMeshProcessors, ngActiveAutoMeshProcessor
import nijigenerate.viewport.common.mesh;          // IncMesh, applyMeshToTarget
import nijigenerate.viewport.vertex.automesh;      // AutoMeshProcessor
import nijigenerate.project : incSelectedNodes;    // fallback when ctx has no nodes
import nijilive;                                   // Drawable, Node
import i18n;
import std.algorithm : map, filter;
import std.array : array;
import core.thread : Thread;
import core.thread.fiber : Fiber;
import core.sync.mutex : Mutex;
import nijigenerate.api.mcp.task : ngRunInMainThread, ngMcpEnqueueAction; // scheduling helpers
import nijigenerate.core.tasks : incTaskAdd, incTaskYield, incTaskProgress;
import nijigenerate.widgets.notification : NotificationPopup; // UI progress popup

// Stable key for per-processor AutoMesh commands
struct AutoMeshKey {
    string id; // processor type identifier (e.g., fully qualified type name)
    string toString() const { return id; }
    size_t toHash() const @safe nothrow @nogc {
        import core.internal.hash : hashOf; return hashOf(id);
    }
    bool opEquals(const AutoMeshKey rhs) const @safe nothrow @nogc { return id == rhs.id; }
}

// Utility to generate a stable identifier for a processor instance
private string _procId(AutoMeshProcessor p)
{
    // Use simple class name for id
    auto tn = typeid(cast(Object)p).toString();
    size_t lastDot = 0; bool hasDot = false;
    foreach (i, ch; tn) if (ch == '.') { lastDot = i; hasDot = true; }
    return hasDot ? tn[lastDot + 1 .. $] : tn;
}

// Command: Apply a specific AutoMesh processor to selected nodes
class ApplyAutoMeshCommand : ExCommand!(TW!(string, "processorId", "AutoMesh processor identifier")) {
    this(string id) {
        super("Apply AutoMesh %s".format(id), "Apply AutoMesh to selected nodes", id);
    }
    override bool runnable(Context ctx) {
        // Needs at least one selectable Drawable
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        foreach (n; ns) if (cast(Drawable)n) return true;
        return false;
    }
    override void run(Context ctx) {
        if (!runnable(ctx)) return;

        // Resolve processor
        AutoMeshProcessor chosen = null;
        foreach (p; ngAutoMeshProcessors()) if (_procId(p) == processorId) { chosen = p; break; }
        if (chosen is null) return;

        // Collect targets
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        Drawable[] targets = ns.filter!(n => cast(Drawable)n !is null).map!(n => cast(Drawable)n).array;
        if (targets.length == 0) return;

        // Prepare meshes
        IncMesh[] meshList = targets.map!(t => new IncMesh(t.getMesh())).array;

        // Split by type and lock part textures (match batching window behavior)
        auto parts = targets.filter!(t => cast(Part)t !is null).map!(t => cast(Part)t).array;
        auto nonParts = targets.filter!(t => cast(Part)t is null).array;
        foreach (p; parts) if (p.textures.length > 0 && p.textures[0]) p.textures[0].lock();

        // Shared state for UI/progress
        auto mtx = new Mutex();
        bool canceled = false;
        size_t total = targets.length;
        size_t done = 0;
        string currentName;
        // Human-ish processor name from type
        string procName;
        {
            auto tn = typeid(cast(Object)chosen).toString();
            size_t lastDot = 0; bool hasDot = false;
            foreach (i, ch; tn) if (ch == '.') { lastDot = i; hasDot = true; }
            procName = hasDot ? tn[lastDot + 1 .. $] : tn;
        }

        // Build a scheduler that enqueues a main-thread Fiber task
        bool bgFinished = false;
        bool fgFinished = false;

        auto scheduleTask = delegate(){
            // Show notification popup (infinite until completion/cancel) on main thread
            import bindbc.imgui; // igProgressBar
            import nijigenerate.widgets : incButtonColored; // button helper
            NotificationPopup.instance().popup((ImGuiIO* io){
                float prog = 0;
                string cur; size_t _done, _total; string _proc;
                // No mutex needed on main thread
                _done = done; _total = total; cur = currentName; _proc = procName;
                prog = (_total > 0) ? cast(float)_done / cast(float)_total : 0;
                import std.string : toStringz;
                string title = "AutoMesh: " ~ _proc ~ (cur.length ? (" - " ~ cur) : "");
                igText(title.toStringz);
                igProgressBar(prog, ImVec2(320, 0));
                igSameLine(0, 8);
                if (incButtonColored("Cancel", ImVec2(96, 24))) canceled = true;
            }, -1);

            // Run actual automesh on a background thread (Parts only); apply results on main thread
            Thread th = new Thread({
                IncMesh[uint] results;
                bool cb(Drawable d, IncMesh mesh) {
                    synchronized(mtx){ currentName = d.name; }
                    if (mesh !is null) {
                        synchronized(mtx){ ++done; }
                        results[d.uuid] = mesh;
                    }
                    bool stop;
                    synchronized(mtx){ stop = canceled; }
                    return !stop;
                }
                void work() {
                    // Build background target lists for Parts only
                    Drawable[] bgTargets = parts.map!(p => cast(Drawable)p).array;
                    IncMesh[] bgMeshes = bgTargets.map!(t => new IncMesh(t.getMesh())).array;
                    chosen.autoMesh(bgTargets, bgMeshes, false, 0, false, 0, &cb);
                }
                auto fib = new Fiber(&work);
                while (fib.state != Fiber.State.TERM) fib.call();

                // Unlock textures
                foreach (p; parts) if (p.textures.length > 0 && p.textures[0]) p.textures[0].unlock();

                // Enqueue apply on main thread
                ngMcpEnqueueAction({
                    foreach (t; targets) {
                        if (auto pm = t.uuid in results) {
                            auto mesh = *pm;
                            if (mesh.vertices.length >= 3)
                                applyMeshToTarget(t, mesh.vertices, &mesh);
                        }
                    }
                    bgFinished = true;
                    if (fgFinished) NotificationPopup.instance().close();
                });
            });
            if (parts.length > 0) th.start(); else bgFinished = true;

            // Non-Parts: process sequentially on main thread Fiber (avoid GL/context issues)
            if (nonParts.length > 0) {
                incTaskAdd("Apply AutoMesh (NonParts)", {
                    size_t idx = 0;
                    while (idx < nonParts.length && !canceled) {
                        auto t = nonParts[idx];
                        currentName = t.name;
                        auto cur = new IncMesh(t.getMesh());
                        auto outMesh = chosen.autoMesh(t, cur, false, 0, false, 0);
                        if (outMesh.vertices.length >= 3)
                            applyMeshToTarget(t, outMesh.vertices, &outMesh);
                        ++done;
                        ++idx;
                        incTaskProgress(total > 0 ? cast(float)done / cast(float)total : 0);
                        incTaskYield();
                    }
                    fgFinished = true;
                    if (bgFinished) NotificationPopup.instance().close();
                });
            } else {
                fgFinished = true;
                if (bgFinished) NotificationPopup.instance().close();
            }
        };

        // Dispatch scheduling depending on thread context
        bool onMain = (Thread.getThis is null) ? true : Thread.getThis.isMainThread;
        if (onMain) {
            scheduleTask();
        } else {
            // Enqueue to MCP main-thread queue; assumed to be pumped when running under MCP-triggered context
            ngMcpEnqueueAction(scheduleTask);
        }
    }
}

// Registry of commands per processor
Command[AutoMeshKey] autoMeshApplyCommands;

// Ensure/get command for a processor id
Command ensureApplyAutoMeshCommand(string id)
{
    AutoMeshKey key = AutoMeshKey(id);
    if (auto p = key in autoMeshApplyCommands) return *p;
    auto cmd = cast(Command) new ApplyAutoMeshCommand(id);
    autoMeshApplyCommands[key] = cmd;
    return cmd;
}

// Initialize commands for all available AutoMesh processors
void ngInitCommands(T)() if (is(T == AutoMeshKey))
{
    foreach (p; ngAutoMeshProcessors()) ensureApplyAutoMeshCommand(_procId(p));
}
