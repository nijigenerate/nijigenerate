module nijigenerate.commands.automesh.dynamic;

import nijigenerate.commands.base;
import nijigenerate.viewport.vertex;                // ngAutoMeshProcessors, ngActiveAutoMeshProcessor
import nijigenerate.viewport.common.mesh;          // IncMesh, applyMeshToTarget
import nijigenerate.viewport.vertex.automesh;      // AutoMeshProcessor, AutoMeshProcessorTypes
import nijigenerate.viewport.vertex.automesh.meta : AMProcInfo; // compile-time ids
import nijigenerate.project : incSelectedNodes;    // fallback when ctx has no nodes
import nijigenerate.viewport.vertex.automesh.alpha_provider : enumerateDrawablesForAutoMesh; // unified target discovery
import nijilive;                                   // Drawable, Node
import i18n;
import std.algorithm : map, filter;
import std.array : array;
import core.thread : Thread;
import core.thread.fiber : Fiber;
import core.sync.mutex : Mutex;
import nijigenerate.api.mcp.task : ngRunInMainThread, ngMcpEnqueueAction; // scheduling helpers
import nijigenerate.widgets.notification : NotificationPopup; // UI progress popup

// Compile-time presence check for initializer
static if (__traits(compiles, { void _ct_probe(){ ngInitCommands!(AutoMeshKey)(); } }))
    pragma(msg, "[CT] automesh.dynamic: ngInitCommands!(AutoMeshKey) present");
else
    pragma(msg, "[CT] automesh.dynamic: ngInitCommands!(AutoMeshKey) MISSING");

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
// Deprecated: prefer p.procId() from processor

// Template: Apply AutoMesh command per processor type
template ApplyAutoMeshPT(alias PT)
{
    class ApplyAutoMeshPT : ExCommand!()
    {
        this() {
            super("Apply AutoMesh (" ~ AMProcInfo!(PT).name ~ ")", "Apply AutoMesh to selected nodes");
        }
        override bool runnable(Context ctx) {
            Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
            foreach (n; ns) if (cast(Drawable)n) return true;
            return false;
        }
        override void run(Context ctx) {
            if (!runnable(ctx)) return;
            auto chosen = cast(AutoMeshProcessor)new PT();

            Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
            Drawable[] targets = enumerateDrawablesForAutoMesh(ns);
            if (targets.length == 0) return;

            IncMesh[] meshList = targets.map!(t => new IncMesh(t.getMesh())).array;

            Texture[] toLock; bool[Texture] locked;
            void collectPartTextures(Node n) {
                if (n is null) return;
                if (auto p = cast(Part)n) {
                    if (p.textures.length > 0 && p.textures[0] !is null) {
                        auto tex = p.textures[0];
                        if (!(tex in locked)) { tex.lock(); locked[tex] = true; toLock ~= tex; }
                    }
                }
                foreach (child; n.children) collectPartTextures(child);
            }
            foreach (t; targets) collectPartTextures(t);

            auto mtx = new Mutex(); bool canceled = false; size_t total = targets.length; size_t done = 0; string currentName;
            string procName = chosen.displayName(); bool workerFinished = false; ulong popupId = 0;
            auto scheduleTask = delegate(){
                import bindbc.imgui; import nijigenerate.widgets : incButtonColored;
                popupId = NotificationPopup.instance().popup((ImGuiIO* io){
                    float prog = 0; string cur; size_t _done, _total; string _proc;
                    _done = done; _total = total; cur = currentName; _proc = procName;
                    prog = (_total > 0) ? cast(float)_done / cast(float)_total : 0;
                    import std.string : toStringz; string title = "AutoMesh: " ~ _proc ~ (cur.length ? (" - " ~ cur) : "");
                    igText(title.toStringz); igProgressBar(prog, ImVec2(320, 0)); igSameLine(0, 8);
                    if (incButtonColored("Cancel", ImVec2(96, 24))) canceled = true;
                }, -1);

                Thread th = new Thread({
                    IncMesh[uint] results;
                    bool cb(Drawable d, IncMesh mesh) {
                        synchronized(mtx){ currentName = d.name; }
                        if (mesh !is null) { synchronized(mtx){ ++done; } results[d.uuid] = mesh; }
                        bool stop; synchronized(mtx){ stop = canceled; } return !stop;
                    }
                    void work() {
                        Drawable[] bgTargets = targets; IncMesh[] bgMeshes = bgTargets.map!(t => new IncMesh(t.getMesh())).array;
                        chosen.autoMesh(bgTargets, bgMeshes, false, 0, false, 0, &cb);
                    }
                    auto fib = new Fiber(&work); while (fib.state != Fiber.State.TERM) fib.call();
                    foreach (tex; toLock) if (tex) tex.unlock();
                    ngMcpEnqueueAction({
                        foreach (t; targets) if (auto pm = t.uuid in results) { auto mesh = *pm; if (mesh.vertices.length >= 3) applyMeshToTarget(t, mesh.vertices, &mesh); }
                        workerFinished = true; NotificationPopup.instance().close(popupId);
                    });
                });
                th.start();
            };

            bool onMain = (Thread.getThis is null) ? true : Thread.getThis.isMainThread;
            if (onMain) scheduleTask(); else ngMcpEnqueueAction(scheduleTask);
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
    // Create per-type command instance when missing (fallback)
    static foreach (i, PT; AutoMeshProcessorTypes) {{
        enum pid_ = AMProcInfo!(PT).id;
        static if (pid_.length) {
            if (pid_ == id) {
                auto cmd = cast(Command) new ApplyAutoMeshPT!PT();
                autoMeshApplyCommands[key] = cmd; return cmd;
            }
        }
    }}
    return null;
}

// Initialize commands for all available AutoMesh processors
void ngInitCommands(T)() if (is(T == AutoMeshKey))
{
    import std.stdio : writefln;
    size_t before = 0; foreach (_k, _v; autoMeshApplyCommands) ++before;
    static foreach (PT; AutoMeshProcessorTypes) {{
        enum pid = AMProcInfo!(PT).id;
        autoMeshApplyCommands[AutoMeshKey(pid)] = cast(Command) new ApplyAutoMeshPT!PT();
    }}
    size_t after = 0; foreach (_k, _v; autoMeshApplyCommands) ++after;
    writefln("[CMD] AutoMeshKey init: before=%s after=%s", before, after);
}
