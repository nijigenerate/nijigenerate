module nijigenerate.commands.vertex.define_mesh;

import nijigenerate.commands.base;
import nijigenerate.commands.depth.bone : ngMarkDepthBoneDirtyForTarget;
import nijigenerate.project : incSelectedNodes;
import nijigenerate.viewport.common.mesh : IncMesh, isGrid;
import nijigenerate.viewport.vertex : ngApplyDeformableVerticesFromCommand, ngApplyDrawableMeshFromCommand;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijilive; // Node, Drawable
import nijilive.math : Vec2Array, vec2, vec3u;
import std.algorithm : map, filter;
import std.array : array;
import std.conv : to;
import std.exception : enforce;
import std.math : isFinite;
import i18n;

private Vec2Array parseFlatVertices(float[] vertices) {
    enforce(vertices !is null, "vertices not provided");
    enforce(vertices.length != 0, "vertices empty");
    enforce(vertices.length % 2 == 0, "vertices length must be even (x,y pairs)");

    Vec2Array vtx;
    vtx.length = vertices.length / 2;
    foreach (i; 0 .. vtx.length) {
        auto x = vertices[i * 2 + 0];
        auto y = vertices[i * 2 + 1];
        enforce(x.isFinite && y.isFinite, "vertices must contain finite x,y values");
        vtx[i] = vec2(x, y);
    }
    return vtx;
}

private Vec2Array makeGridVertices(float[] axisX, float[] axisY) {
    enforce(axisX !is null && axisY !is null, "axisX or axisY not provided");
    enforce(axisX.length >= 2, "axisX must contain at least 2 values");
    enforce(axisY.length >= 2, "axisY must contain at least 2 values");

    Vec2Array vtx;
    foreach (y; axisY) {
        enforce(y.isFinite, "axisY must contain finite values");
        foreach (x; axisX) {
            enforce(x.isFinite, "axisX must contain finite values");
            vtx ~= vec2(x, y);
        }
    }
    return vtx;
}

private bool canApplyMeshTopology(Drawable target, out string reason) {
    if (auto part = cast(Part)target) {
        auto tex = part.textures[0];
        if (tex is null) {
            reason = "Cannot define mesh for Part without texture: " ~ part.name;
            return false;
        }
        if (tex.width == 0 || tex.height == 0) {
            reason = "Cannot define mesh for Part with empty texture: " ~ part.name;
            return false;
        }
    }
    return true;
}

/**
    Define mesh topology for selected Drawables using provided arrays.

    - vertices: flattened [x0,y0, x1,y1, ...]
    - indices:  triangle indices (triplets) into vertex array
*/
@ShortcutHidden
@EffectStructuralEdit
class DefineMeshCommand : ExCommand!(
    TW!(float[],  "vertices", "Flattened [x,y]* vertex coordinates"),
    TW!(ushort[], "indices",  "Triangle indices (groups of 3)")
) {
    this(float[] verts, ushort[] inds) {
        super(_("Define Mesh"), _("Apply a mesh defined by vertices and indices to selected nodes."), verts, inds);
    }

    override bool runnable(Context ctx) {
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        foreach (n; ns) if (cast(Drawable)n) return true;
        return false;
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx)) return CommandResult(false, "No drawable nodes available");

        // No-op when verts/inds are not provided (or empty)
        if (vertices is null || indices is null) return CommandResult(false, "Vertices or indices not provided");
        if (vertices.length == 0 || indices.length == 0) return CommandResult(false, "Vertices or indices empty");

        // Basic validation
        enforce(vertices.length % 2 == 0, "vertices length must be even (x,y pairs)");
        enforce(indices.length % 3 == 0, "indices length must be a multiple of 3 (triangles)");

        auto vtx = parseFlatVertices(vertices);

        // Guard indices range
        foreach (idx; indices) {
            enforce(idx < vtx.length, "index out of range in indices array");
        }

        // Convert ushort[] (triplets) -> vec3u[]
        vec3u[] tris;
        tris.length = indices.length / 3;
        foreach (i; 0 .. tris.length) {
            tris[i] = vec3u(indices[i*3 + 0], indices[i*3 + 1], indices[i*3 + 2]);
        }

        // Resolve targets
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        auto targets = ns.filter!(n => cast(Drawable)n !is null).map!(n => cast(Drawable)n).array;
        if (targets.length == 0) return CommandResult(false, "No drawable targets");

        foreach (t; targets) {
            string reason;
            if (!canApplyMeshTopology(t, reason)) return CommandResult(false, reason);
        }

        // Apply to each target: build IncMesh from arrays, then applyMeshToTarget
        foreach (t; targets) {
            auto base = t.getMesh();
            auto mesh = new IncMesh(base);
            mesh.vertices.length = 0;
            mesh.importVertsAndTris(vtx, tris);
            mesh.refresh();

            string message;
            if (!ngApplyDrawableMeshFromCommand(t, mesh, message))
                return CommandResult(false, message);
        }
        return CommandResult(true);
    }
}

/**
    Define control vertices for selected non-Drawable Deformables.

    This is for PathDeformer and other Deformable control-point based nodes.
    GridDeformer is accepted only when the points form a rectangular grid.
*/
@ShortcutHidden
@EffectStructuralEdit
class DefineVerticesCommand : ExCommand!(
    TW!(float[], "vertices", "Flattened [x,y]* control vertex coordinates. For GridDeformer, vertices must form a complete rectangular grid.")
) {
    this(float[] verts) {
        super(_("Define Vertices"), _("Apply control vertices to selected Deformable nodes. Does not define triangle topology."), verts);
    }

    override bool runnable(Context ctx) {
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        foreach (n; ns) {
            if (cast(Deformable)n && cast(Drawable)n is null) return true;
        }
        return false;
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx)) return CommandResult(false, "No non-drawable deformable nodes available");

        auto vtx = parseFlatVertices(vertices);
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        auto targets = ns
            .filter!(n => cast(Deformable)n !is null && cast(Drawable)n is null)
            .map!(n => cast(Deformable)n)
            .array;
        if (targets.length == 0) return CommandResult(false, "No non-drawable deformable targets");

        foreach (t; targets) {
            if (cast(GridDeformer)t) {
                float[][] gridAxes;
                enforce(isGrid(vtx, gridAxes), "GridDeformer vertices must form a complete rectangular grid");
            }
            string message;
            if (!ngApplyDeformableVerticesFromCommand(t, vtx, message))
                return CommandResult(false, message);
            ngMarkDepthBoneDirtyForTarget(cast(Node)t, "Target Vertices");
        }
        return CommandResult(true);
    }
}

/**
    Define GridDeformer vertices from explicit X/Y grid axes.
*/
@ShortcutHidden
@EffectStructuralEdit
class DefineGridCommand : ExCommand!(
    TW!(float[], "axisX", "Grid X axis values"),
    TW!(float[], "axisY", "Grid Y axis values")
) {
    this(float[] xs, float[] ys) {
        super(_("Define Grid"), _("Apply a rectangular grid defined by X/Y axes to selected GridDeformer nodes."), xs, ys);
    }

    override bool runnable(Context ctx) {
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        foreach (n; ns) if (cast(GridDeformer)n) return true;
        return false;
    }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx)) return CommandResult(false, "No GridDeformer nodes available");

        auto vtx = makeGridVertices(axisX, axisY);
        Node[] ns = ctx.hasNodes ? ctx.nodes : incSelectedNodes();
        auto targets = ns.filter!(n => cast(GridDeformer)n !is null).map!(n => cast(GridDeformer)n).array;
        if (targets.length == 0) return CommandResult(false, "No GridDeformer targets");

        foreach (t; targets) {
            string message;
            if (!ngApplyDeformableVerticesFromCommand(t, vtx, message))
                return CommandResult(false, message);
            ngMarkDepthBoneDirtyForTarget(cast(Node)t, "Target Vertices");
        }
        return CommandResult(true);
    }
}

enum VertexCommand {
    DefineMesh,
    DefineVertices,
    DefineGrid,
}

Command[VertexCommand] commands;

void ngInitCommands(T)() if (is(T == VertexCommand))
{
    // Register with benign defaults; actual args supplied at call-time (e.g., via MCP)
    mixin(registerCommand!(VertexCommand.DefineMesh, null, null));
    mixin(registerCommand!(VertexCommand.DefineVertices, null));
    mixin(registerCommand!(VertexCommand.DefineGrid, null, null));
}
