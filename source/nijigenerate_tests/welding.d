module nijigenerate_tests.welding;

import nijigenerate.core.math.welding;
import nijilive.core.nodes.drawable;
import nijilive.core.nodes.node;
import nijilive.core.puppet;
import nijilive.fmt.serialize;
import nijilive.math;
import std.stdio : writeln;

private class Surface : Drawable {
    this(Node parent) { super(parent); }
    override void renderMask(bool dodge = false) {}
    void prepare() {
        deformation.length = vertices.length;
        deformation[] = vec2(0, 0);
        postProcessed = 2;
    }
    void weld(Surface other) {
        auto matrix = other.transform.matrix;
        weldingProcessor(other, other.vertices, other.deformation, &matrix);
    }
}

void ngTestWelding() {
    import bindbc.opengl : glGenVertexArrays, GLsizei, GLuint;
    auto previousGen = glGenVertexArrays;
    glGenVertexArrays = (GLsizei count, GLuint* arrays) nothrow @nogc {
        foreach (i; 0 .. count) arrays[i] = 0;
    };
    scope(exit) glGenVertexArrays = previousGen;
    auto points = [vec2(0, 0), vec2(0.1f, 0), vec2(10, 0)];
    auto targets = [vec2(0, 0), vec2(10, 0)];
    assert(ngMatchWeldingVertices(points, targets) == [0, 0, 1]);
    assert(ngMatchWeldingVertices(points, targets, [1]) == [-1, -1, 1]);
    assert(ngMatchWeldingVertices([points[2], points[0]], targets, [0, -1, 1]) == [1, 0]);
    assert(ngMatchWeldingVertices(points, []) == [-1, -1, -1]);
    assert(ngMatchWeldingVertices([vec2(float.nan, 0)], targets) == [-1]);
    // Deleting one endpoint clears its reciprocal group without proximity rematching.
    auto removed = ngRemapWeldingLinks([0, 1, 2], [0, 1, 2], [0, -1, 1], 2);
    assert(removed.forward == [0, 2] && removed.reverse == [0, -1, 1]);
    auto reordered = ngRemapWeldingLinks([0, -1, 1], [0, 2], [2, 1, 0], 4);
    assert(reordered.forward == [1, -1, 0, -1] && reordered.reverse == [2, 0]);
    auto denseRemoved = ngRemapWeldingLinks([0, 0, 1], [0, 2], [-1, 0, 1], 2);
    assert(denseRemoved.forward == [0, 1] && denseRemoved.reverse == [0, 1]);
    auto coarseRemoved = ngRemapWeldingLinks([0, 2], [0, 0, 1], [-1, 0], 1);
    assert(coarseRemoved.forward == [2] && coarseRemoved.reverse == [-1, -1, 0]);
    auto coarse = [vec2(0, 0), vec2(10, 0), vec2(0, 10)];
    auto fine = [vec2(5, 0), vec2(5, -5), vec2(6, -5)];
    auto refined = ngRefineWeldingMesh(fine, [0, 1, 2], coarse, coarse, [0, 1, 2], mat4.identity);
    assert(refined.addedVertices == 1 && refined.vertices[0 .. 3] == coarse);
    assert(refined.vertices[3] == fine[0] && refined.indices.length == 6);
    assert(ngRefineWeldingMesh(fine, [0, 1, 2], refined.vertices, refined.vertices,
        refined.indices, mat4.identity).addedVertices == 0);
    // A curved seam sample lies beyond the point-matching radius but between
    // attached endpoints. An overlapping interior contour must stay untouched.
    auto curved = [vec2(0, 0), vec2(10, -6), vec2(20, 0), vec2(8, 6)];
    auto chord = [vec2(0, 0), vec2(20, 0), vec2(0, 20)];
    auto contour = ngRefineWeldingMesh(curved, [0, 1, 2, 0, 2, 3], chord, chord,
        [0, 1, 2], mat4.identity);
    assert(contour.addedVertices == 1 && contour.vertices[3] == curved[1]);
    assert(contour.vertices[0 .. 3] == chord);
    assert(ngRefineWeldingMesh(curved, [0, 1, 2, 0, 2, 3], contour.vertices,
        contour.vertices, contour.indices, mat4.identity).addedVertices == 0);

    auto puppet = new Puppet;
    auto a = new Surface(puppet.root);
    auto b = new Surface(puppet.root);
    a.vertices.length = points.length;
    foreach (i, point; points) a.vertices[i] = point;
    b.vertices.length = targets.length;
    foreach (i, point; targets) b.vertices[i] = point;
    ptrdiff_t[] indices = [0, -1, 1];
    a.addWeldedTarget(b, indices, 0.25f);
    indices[0] = -1;
    assert(a.welded[0].indices == [0, -1, 1]);
    assert(b.welded[0].indices == [0, 2]);
    a.addWeldedTarget(b, [0, 0, 1], 0.25f);
    assert(a.welded[0].indices == [0, 0, 1]);
    assert(b.welded[0].indices == [0, 2]);
    assert(a.updatedCounterWeldingIndices(b, [-1, 0, 1]) == [1, 2]);
    // An explicitly detached dense vertex must not be restored by a weight edit.
    a.welded[0].indices = [-1, 0, 1];
    b.welded[0].indices = [1, 2];
    assert(b.updatedCounterWeldingIndices(a, [1, 2]) == [-1, 0, 1]);
    a.addWeldedTarget(b, [0, 0, 1], 0.25f);
    bool rejected;
    try { a.addWeldedTarget(b, [0, -1, 2], 0.25f); } catch (Exception) { rejected = true; }
    assert(rejected);
    a.welded[0].indices = [0, 0, 2];
    rejected = false;
    try { inToJson(a); } catch (Exception) { rejected = true; }
    assert(rejected);
    a.finalize();
    b.finalize();
    assert(a.welded[0].indices == [0, 0, -1]);
    assert(b.welded[0].indices == [0, 2]);
    assert(a.welded[0].weight == 0.25f && b.welded[0].weight == 0.75f);
    assert(inLastLoadDiagnostics.repairedWeldingMappings == 1);
    a.addWeldedTarget(b, [0, 0, 1], 0.25f);
    a.prepare();
    b.prepare();
    // A stale opposite link cannot deform a vertex detached from this endpoint.
    a.welded[0].indices = [-1, 0, 1];
    b.deformation[0] = vec2(8, 0);
    a.weld(b);
    assert(a.deformation[0] == vec2(0, 0));
    a.prepare();
    b.prepare();
    a.addWeldedTarget(b, [0, 0, 1], 0.25f);
    a.weldingApplied = null;
    b.weldingApplied = null;
    b.deformation[0] = vec2(8, 0);
    a.weld(b);
    import std.math : abs;
    assert(abs(a.deformation[0].x - 6.0125f) < 0.0001f);
    assert(abs(a.deformation[1].x - 5.9125f) < 0.0001f);
    assert(abs(b.deformation[0].x - 6.0125f) < 0.0001f);
    // Reversing filter order must evaluate every dense-side seam vertex exactly once.
    a.weldingApplied = null;
    b.weldingApplied = null;
    a.deformation[] = vec2(0, 0);
    b.deformation[] = vec2(0, 0);
    b.deformation[0] = vec2(8, 0);
    b.weld(a);
    assert(abs(a.deformation[0].x - 6.0125f) < 0.0001f);
    assert(abs(a.deformation[1].x - 5.9125f) < 0.0001f);
    assert(abs(b.deformation[0].x - 6.0125f) < 0.0001f);
    writeln("PASS: dense Welding, seam restriction, reordered vertices, load/save bounds, averaged anchors and filter-order independence");
}
