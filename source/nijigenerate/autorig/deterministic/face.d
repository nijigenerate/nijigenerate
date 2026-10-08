module nijigenerate.autorig.deterministic.face;

import nijigenerate.autorig.solver.quadratic;
import std.exception : enforce;
import std.math : sqrt, abs, isFinite;

alias RigPoint = double[2];

struct FaceProjection {
    RigPoint[] points;
    double rawMinimum = 0;
    double correctedMinimum = 0;
    double maximumCorrection = 0;
    SolverResult solver;
}

private double cross(RigPoint a, RigPoint b) { return a[0] * b[1] - a[1] * b[0]; }
private RigPoint difference(RigPoint a, RigPoint b) { return [a[0] - b[0], a[1] - b[1]]; }

private void checkAxes(double[] xs, double[] ys, size_t count) {
    enforce(xs.length >= 2 && ys.length >= 2 && count == xs.length * ys.length && count <= int.max,
        "Invalid face grid dimensions");
    foreach (axis; [xs, ys]) foreach (i, value; axis)
        enforce(isFinite(value) && (i == 0 || value > axis[i - 1]), "Grid axes must increase strictly");
}

private double constraints(RigPoint[] points, double[] xs, double[] ys, RigPoint direction,
    ref QuadraticProblem problem) {
    double minimum = double.infinity;
    foreach (j; 0 .. ys.length - 1) foreach (i; 0 .. xs.length - 1) {
        int a = cast(int)(j * xs.length + i), b = a + 1, c = a + cast(int)xs.length, d = c + 1;
        double area = (xs[i + 1] - xs[i]) * (ys[j + 1] - ys[j]);
        foreach (edges; [[a,b,a,c], [c,d,b,d], [a,b,b,d], [c,d,a,c]]) {
            auto e = difference(points[edges[1]], points[edges[0]]);
            auto f = difference(points[edges[3]], points[edges[2]]);
            double ratio = cross(e, f) / area;
            if (ratio < minimum) minimum = ratio;
            int row = problem.constraints++;
            double ce = cross(direction, f) / area, cf = cross(e, direction) / area;
            problem.matrix ~= [SparseEntry(row, edges[1], -ce), SparseEntry(row, edges[0], ce),
                SparseEntry(row, edges[3], -cf), SparseEntry(row, edges[2], cf)];
            problem.lower ~= -double.infinity;
            problem.upper ~= ratio - .055;
        }
    }
    return minimum;
}

/** Preserve every bilinear cell's orientation with a bounded, smooth contour correction. */
FaceProjection ngPreserveFaceOrientation(RigPoint[] points, double[] xs, double[] ys, RigPoint[] uv,
    RigPoint direction, double width, bool[] protectedVertices = null, bool delegate() canceled = null) {
    checkAxes(xs, ys, points.length);
    enforce(uv.length == points.length && (!protectedVertices.length || protectedVertices.length == points.length),
        "Face vertex metadata dimensions differ");
    enforce(isFinite(width) && width > 0, "Invalid face width");
    foreach (point; points) foreach (v; point) enforce(isFinite(v), "Nonfinite face point");
    foreach (point; uv) foreach (v; point) enforce(isFinite(v), "Nonfinite face UV");
    foreach (v; direction) enforce(isFinite(v), "Nonfinite depth direction");
    FaceProjection result;
    result.points = points.dup;
    double norm = sqrt(direction[0] * direction[0] + direction[1] * direction[1]);
    if (norm < 1e-9) return result;
    direction[] /= norm;
    QuadraticProblem problem;
    problem.variables = cast(int)points.length;
    problem.linear = new double[points.length];
    problem.linear[] = 0;
    result.rawMinimum = constraints(points, xs, ys, direction, problem);
    result.correctedMinimum = result.rawMinimum;
    if (result.rawMinimum >= .055) return result;
    foreach (a; 0 .. problem.variables) problem.objective ~= SparseEntry(a, a, 1);
    foreach (j; 0 .. ys.length) foreach (i; 0 .. xs.length) {
        int a = cast(int)(j * xs.length + i);
        void edge(int b, double distance) {
            double weight = width * .04 / distance;
            weight *= weight;
            problem.objective ~= [SparseEntry(a, a, weight), SparseEntry(b, b, weight),
                SparseEntry(a, b, -weight)];
        }
        if (i + 1 < xs.length) edge(a + 1, xs[i + 1] - xs[i]);
        if (j + 1 < ys.length) edge(a + cast(int)xs.length, ys[j + 1] - ys[j]);
    }
    foreach (i, p; uv) {
        bool allowed = (p[0] < .3 || p[0] > .7 || p[1] < .2 || p[1] > .8) &&
            (!protectedVertices.length || !protectedVertices[i]);
        double bound = allowed ? width * .04 : 0;
        problem.matrix ~= SparseEntry(problem.constraints++, cast(int)i, 1);
        problem.lower ~= -bound;
        problem.upper ~= bound;
    }
    result.solver = ngSolveQuadratic(problem, SolverOptions.init, canceled);
    enforce(result.solver.solved(), "No bounded contour projection preserving depth");
    enforce(result.solver.constraintViolation <= 1e-5, "Contour solution violates bounds");
    foreach (i, s; result.solver.solution) {
        result.points[i][0] += s * direction[0];
        result.points[i][1] += s * direction[1];
        if (abs(s) > result.maximumCorrection) result.maximumCorrection = abs(s);
    }
    QuadraticProblem verification;
    result.correctedMinimum = constraints(result.points, xs, ys, direction, verification);
    enforce(result.correctedMinimum >= .0549, "Contour projection failed Jacobian constraints");
    return result;
}

/** Fit grid depth with an L1 correction objective, interpolation bounds and exact anchors. */
double[] ngFitGridDepth(double[] xs, double[] ys, double[] base, RigPoint[] query, double[] truth,
    double tolerance, size_t anchorCount, bool delegate() canceled = null) {
    checkAxes(xs, ys, base.length);
    enforce(query.length == truth.length && anchorCount <= query.length && tolerance > 0 && isFinite(tolerance),
        "Invalid depth fitting inputs");
    int n = cast(int)base.length;
    enforce(n <= int.max / 2, "Depth grid too large");
    double limit = tolerance * .98;
    QuadraticProblem problem;
    problem.variables = n * 2;
    problem.linear = new double[n * 2];
    problem.linear[] = 0;
    foreach (k; 0 .. n) {
        enforce(isFinite(base[k]) && base[k] >= 0, "Invalid base depth");
        problem.linear[n + k] = 1;
        int row = problem.constraints++;
        problem.matrix ~= SparseEntry(row, k, 1);
        problem.lower ~= -base[k] > -limit ? -base[k] : -limit;
        problem.upper ~= limit;
        row = problem.constraints++;
        problem.matrix ~= SparseEntry(row, n + k, 1);
        problem.lower ~= 0;
        problem.upper ~= double.infinity;
        foreach (sign; [-1., 1.]) {
            row = problem.constraints++;
            problem.matrix ~= [SparseEntry(row, k, sign), SparseEntry(row, n + k, -1)];
            problem.lower ~= -double.infinity;
            problem.upper ~= 0;
        }
    }
    foreach (k, point; query) {
        enforce(isFinite(point[0]) && isFinite(point[1]) && isFinite(truth[k]), "Nonfinite depth sample");
        size_t i, j;
        while (i + 2 < xs.length && point[0] > xs[i + 1]) i++;
        while (j + 2 < ys.length && point[1] > ys[j + 1]) j++;
        double u = (point[0] - xs[i]) / (xs[i + 1] - xs[i]);
        double v = (point[1] - ys[j]) / (ys[j + 1] - ys[j]);
        int a = cast(int)(j * xs.length + i);
        int[4] indices = [a, a + 1, a + cast(int)xs.length, a + cast(int)xs.length + 1];
        double[4] weights = [(1-u)*(1-v), u*(1-v), (1-u)*v, u*v];
        double residual = truth[k];
        int row = problem.constraints++;
        foreach (h; 0 .. 4) {
            residual -= weights[h] * base[indices[h]];
            problem.matrix ~= SparseEntry(row, indices[h], weights[h]);
        }
        bool anchor = k >= query.length - anchorCount;
        problem.lower ~= residual - (anchor ? 0 : limit);
        problem.upper ~= residual + (anchor ? 0 : limit);
    }
    auto result = ngSolveQuadratic(problem, SolverOptions.init, canceled);
    if (!result.solved() || result.constraintViolation > 1e-5) return null;
    auto fitted = base.dup;
    foreach (k; 0 .. n) fitted[k] += result.solution[k];
    return fitted;
}
