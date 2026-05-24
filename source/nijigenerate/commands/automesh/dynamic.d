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
import core.sync.mutex : Mutex;
import core.sync.condition : Condition;
import vibe.core.task : Task;
import vibe.core.core : vibeYield = yield;
import std.stdio : writefln;
import nijigenerate.api.mcp.task : ngMcpEnqueueAction, ngRunInMainThread; // scheduling helpers
import nijigenerate.utils.crashdump : installNativeCrashDumpThreadHandler;
import nijigenerate.widgets.notification : NotificationPopup;
import bindbc.imgui : ImGuiIO, ImVec2, igText, igProgressBar, igSameLine;

version(CMD_LOG) private void cmdLog(T...)(T args) { writefln(args); }
else             private void cmdLog(T...)(T args) {}

private class AutoMeshApplyResult : CommandResult {
    private Mutex lock;
    private Condition cond;
    private bool done;
    private CommandResult finalResult;

    this() {
        super(true, "AutoMesh queued");
        lock = new Mutex();
        cond = new Condition(lock);
    }

    void complete(CommandResult result) {
        synchronized (lock) {
            if (done) return;
            finalResult = result;
            if (result !is null) {
                succeeded = result.succeeded;
                message = result.message;
            }
            done = true;
            cond.notify();
        }
    }

    override CommandResult waitForCompletion() {
        if (cast(bool)Task.getThis()) {
            for (;;) {
                synchronized (lock) {
                    if (done) break;
                }
                vibeYield();
            }
        } else {
            synchronized (lock) {
                while (!done) cond.wait();
            }
        }
        return finalResult !is null ? finalResult : this;
    }
}

private class AutoMeshProgressState {
    private Mutex lock;
    size_t total;
    private size_t done_;
    private string currentName_;
    private bool canceled_;

    this(size_t total) {
        lock = new Mutex();
        this.total = total;
    }

    void beginTarget(string name) {
        synchronized (lock) {
            currentName_ = name;
        }
    }

    void completeTarget() {
        synchronized (lock) {
            if (done_ < total) ++done_;
        }
    }

    void requestCancel() {
        synchronized (lock) {
            canceled_ = true;
        }
    }

    bool canceled() {
        synchronized (lock) {
            return canceled_;
        }
    }

    void snapshot(out size_t done, out size_t totalOut, out string currentName, out bool canceledOut) {
        synchronized (lock) {
            done = done_;
            totalOut = total;
            currentName = currentName_;
            canceledOut = canceled_;
        }
    }
}

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

            auto asyncResult = new AutoMeshApplyResult();
            auto progress = new AutoMeshProgressState(targets.length);
            string procName = chosen.displayName();
            ulong popupId = NotificationPopup.instance().popup((ImGuiIO* io) {
                import nijigenerate.widgets : incButtonColored;
                import std.string : toStringz;

                size_t done, total;
                string currentName;
                bool canceled;
                progress.snapshot(done, total, currentName, canceled);
                float ratio = total > 0 ? cast(float)done / cast(float)total : 0;
                string title = "AutoMesh: " ~ procName ~ (currentName.length ? (" - " ~ currentName) : "");
                igText(title.toStringz);
                igProgressBar(ratio, ImVec2(320, 0));
                igSameLine(0, 8);
                if (canceled) {
                    igText("Canceling...");
                } else if (incButtonColored("Cancel", ImVec2(96, 24))) {
                    progress.requestCancel();
                }
            }, -1);

            Thread th = new Thread({
                installNativeCrashDumpThreadHandler();
                string workerError;
                auto applyLock = new Mutex();
                size_t applied = 0;
                string applyError;

                void recordApplyError(string message) {
                    synchronized (applyLock) {
                        if (!applyError.length) applyError = message;
                    }
                }

                string currentApplyError() {
                    synchronized (applyLock) {
                        return applyError;
                    }
                }

                void incrementApplied() {
                    synchronized (applyLock) {
                        ++applied;
                    }
                }

                size_t appliedCount() {
                    synchronized (applyLock) {
                        return applied;
                    }
                }

                bool cb(Deformable d, IncMesh mesh) {
                    if (mesh is null) {
                        progress.beginTarget(d.name);
                        return !progress.canceled();
                    }
                    if (mesh !is null) {
                        auto target = d;
                        auto resultMesh = mesh;
                        ngMcpEnqueueAction({
                            if (currentApplyError().length) return;
                            scope(exit) progress.completeTarget();

                            if (resultMesh.vertices.length == 0) {
                                recordApplyError("AutoMesh generated empty mesh for " ~ target.name);
                                return;
                            }
                            if (cast(Drawable)target && resultMesh.vertices.length < 3) {
                                recordApplyError("AutoMesh generated too few drawable vertices for " ~ target.name);
                                return;
                            }

                            string message;
                            if (auto dr = cast(Drawable)target) {
                                if (!ngApplyDrawableMeshFromCommand(dr, resultMesh, message)) {
                                    recordApplyError(message);
                                    return;
                                }
                            } else {
                                if (!ngApplyDeformableVerticesFromCommand(target, ngMeshPositions(resultMesh), message)) {
                                    recordApplyError(message);
                                    return;
                                }
                            }
                            incrementApplied();
                        });
                    }
                    return !progress.canceled();
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

                if (workerError.length) {
                    ngMcpEnqueueAction({
                        NotificationPopup.instance().close(popupId);
                        asyncResult.complete(CommandResult(false, workerError));
                    });
                    return;
                }

                ngMcpEnqueueAction({
                    scope(exit) NotificationPopup.instance().close(popupId);
                    auto error = currentApplyError();
                    auto appliedNow = appliedCount();
                    if (error.length) {
                        asyncResult.complete(CommandResult(false, error));
                    } else if (progress.canceled()) {
                        asyncResult.complete(CommandResult(false, "AutoMesh canceled"));
                    } else if (appliedNow == 0) {
                        asyncResult.complete(CommandResult(false, "AutoMesh generated no applicable meshes"));
                    } else {
                        asyncResult.complete(CommandResult(true));
                    }
                });
            });
            th.start();
            return asyncResult;
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
