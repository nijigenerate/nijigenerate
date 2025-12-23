module nijigenerate.commands.vertex.define_mesh;

import nijigenerate.commands.base;
import nijigenerate.project : incSelectedNodes;
import nijigenerate.viewport.common.mesh : IncMesh, applyMeshToTarget;
import nijilive; // Node, Drawable
import nijilive.math : vec2, vec3u;
import std.algorithm : map, filter;
import std.array : array;
import std.conv : to;
import std.exception : enforce;
import i18n;

/**
    Define mesh topology for selected Drawables using provided arrays.

    - vertices: flattened [x0,y0, x1,y1, ...]
    - indices:  triangle indices (triplets) into vertex array
*/
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

    // Do not expose as a usable shortcut (args must be provided programmatically)
    override bool shortcutRunnable() { return false; }

    override CommandResult run(Context ctx) {
        if (!runnable(ctx)) return CommandResult(false, "No drawable nodes available");

        // No-op when verts/inds are not provided (or empty)
        if (vertices is null || indices is null) return CommandResult(false, "Vertices or indices not provided");
        if (vertices.length == 0 || indices.length == 0) return CommandResult(false, "Vertices or indices empty");

        // Basic validation
        enforce(vertices.length % 2 == 0, "vertices length must be even (x,y pairs)");
        enforce(indices.length % 3 == 0, "indices length must be a multiple of 3 (triangles)");

        // Convert flattened float[] -> Vec2Array
        Vec2Array vtx;
        vtx.length = vertices.length / 2;
        foreach (i; 0 .. vtx.length) {
            vtx[i] = vec2(vertices[i*2 + 0], vertices[i*2 + 1]);
        }

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

        // Apply to each target: build IncMesh from arrays, then applyMeshToTarget
        foreach (t; targets) {
            auto base = t.getMesh();
            auto mesh = new IncMesh(base);
            mesh.vertices.length = 0;
            mesh.importVertsAndTris(vtx, tris);
            mesh.refresh();

            // For Drawable targets, applyMeshToTarget will use mesh.export_() for topology
            applyMeshToTarget(t, mesh.vertices, &mesh);
        }
        return CommandResult(true);
    }
}

enum VertexCommand {
    DefineMesh,
}

Command[VertexCommand] commands;

void ngInitCommands(T)() if (is(T == VertexCommand))
{
    // Register with benign defaults; actual args supplied at call-time (e.g., via MCP)
    mixin(registerCommand!(VertexCommand.DefineMesh, null, null));
}
