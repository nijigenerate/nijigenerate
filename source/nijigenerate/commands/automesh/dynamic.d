module nijigenerate.commands.automesh.dynamic;

import nijigenerate.commands.base;
import nijigenerate.viewport.vertex;                // ngAutoMeshProcessors, ngActiveAutoMeshProcessor
import nijigenerate.viewport.common.mesh;          // IncMesh, applyMeshToTarget
import nijigenerate.viewport.vertex.automesh;      // AutoMeshProcessor, AutoMeshProcessorTypes
import nijigenerate.viewport.vertex.automesh.meta : AMProcInfo; // compile-time ids
import nijigenerate.project : incSelectedNodes;    // fallback when ctx has no nodes
import nijigenerate.viewport.vertex.automesh.common : AlphaInput, getAlphaInput, setAutoMeshAlphaInputCache, clearAutoMeshAlphaInputCache;
import nijilive;                                   // Drawable, Node
import i18n;
import std.algorithm : map, filter;
import std.array : array;
import core.thread : Thread;
import core.thread.fiber : Fiber;
import std.stdio : writefln;
import nijigenerate.api.mcp.task : ngRunInMainThread; // scheduling helpers
import nijigenerate.utils.crashdump : installNativeCrashDumpThreadHandler;

version(CMD_LOG) private void cmdLog(T...)(T args) { writefln(args); }
else             private void cmdLog(T...)(T args) {}

// Compile-time presence check for initializer
static if (__traits(compiles, { void _ct_probe(){ ngInitCommands!(AutoMeshKey)(); } })) {
    //pragma(msg, "[CT] automesh.dynamic: ngInitCommands!(AutoMeshKey) present");
} else
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
    @EffectApply
    class ApplyAutoMeshPT : ExCommand!()
    {
        this() {
            super("Apply AutoMesh (" ~ AMProcInfo!(PT).name ~ ")", "Apply AutoMesh to selected nodes");
        }
        override bool runnable(Context ctx) {
            Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
            foreach (n; ns) {
                if (cast(Deformable)n) return true;
            }
            return false;
        }
        override CommandResult run(Context ctx) {
            if (!runnable(ctx)) return CommandResult(false, "No drawable nodes");
            AutoMeshProcessor chosen = null;
            foreach (processor; ngAutoMeshProcessors) {
                if (cast(PT)processor) {
                    chosen = processor; 
                    break;
                }
            }
            if (!chosen) {
                import std.stdio;
                writefln("[BUG] No appropriate AutoMeshProcessor exists!");
                return CommandResult(false, "AutoMesh processor missing");
            }

            Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
            // Apply to explicitly selected deformables (do not include descendants)
            Deformable[] targets;
            IncMesh[] meshList;
            foreach (n; ns) {
                if (auto d = cast(Deformable)n) {
                    IncMesh mesh;
                    if (auto dr = cast(Drawable)d) {
                        mesh = new IncMesh(dr.getMesh());
                    } else {
                        mesh = ngCreateIncMesh(d.vertices); // fallback from vertices only
                    }
                    targets ~= d;
                    meshList ~= mesh;
                }
            }
            if (targets.length == 0) return CommandResult(false, "No deformable targets");

            bool onMain = (Thread.getThis is null) ? true : Thread.getThis.isMainThread;
            if (!onMain) {
                auto self = this;
                return ngRunInMainThread!CommandResult({ return self.run(ctx); });
            }

            // Build all alpha inputs on the main thread. Worker threads must not read GPU textures.
            AlphaInput[uint] alphaInputs;
            foreach (t; targets) {
                alphaInputs[t.uuid] = getAlphaInput(t);
            }

            IncMesh[uint] results;
            string workerError;
            size_t processed = 0;
            Thread th = new Thread({
                installNativeCrashDumpThreadHandler();
                bool cb(Deformable d, IncMesh mesh) {
                    if (mesh !is null) {
                        results[d.uuid] = mesh;
                        ++processed;
                    }
                    return true;
                }
                void work() {
                    Deformable[] bgTargets = targets;
                    IncMesh[] bgMeshes = meshList.dup;
                    chosen.autoMesh(bgTargets, bgMeshes, false, 0, false, 0, &cb);
                }
                try {
                    setAutoMeshAlphaInputCache(&alphaInputs);
                    scope(exit) clearAutoMeshAlphaInputCache(&alphaInputs);
                    auto fib = new Fiber(&work);
                    while (fib.state != Fiber.State.TERM) fib.call();
                } catch (Throwable e) {
                    workerError = e.msg;
                }
            });
            th.start();
            th.join();

            if (workerError.length) {
                writefln("[AutoMesh] worker failed: %s", workerError);
                return CommandResult(false, workerError);
            }

            size_t applied = 0;
            foreach (t; targets) {
                if (auto pm = t.uuid in results) {
                    auto mesh = *pm;
                    if (mesh.vertices.length == 0) continue;
                    if (cast(Drawable)t && mesh.vertices.length < 3) continue;
                    if (auto dr = cast(Drawable)t)
                        applyMeshToTarget(dr, mesh.vertices, &mesh);
                    else
                        applyMeshToTarget(t, mesh.vertices, &mesh);
                    ++applied;
                }
            }

            if (applied == 0) {
                return CommandResult(false, "AutoMesh generated no applicable meshes");
            }
            return CommandResult(true);
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
    size_t before = 0; foreach (_k, _v; autoMeshApplyCommands) ++before;
    static foreach (PT; AutoMeshProcessorTypes) {{
        enum pid = AMProcInfo!(PT).id;
        autoMeshApplyCommands[AutoMeshKey(pid)] = cast(Command) new ApplyAutoMeshPT!PT();
    }}
    size_t after = 0; foreach (_k, _v; autoMeshApplyCommands) ++after;
    cmdLog("[CMD] AutoMeshKey init: before=%s after=%s", before, after);
}
