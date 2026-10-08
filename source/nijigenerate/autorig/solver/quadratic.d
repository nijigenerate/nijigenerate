module nijigenerate.autorig.solver.quadratic;

import core.thread.fiber : Fiber;
import std.algorithm : sort;
import std.exception : enforce;
import std.math : isFinite, abs;

private extern(C) int ng_osqp_solve(int n, int m, int* pp, int* pi, double* px, int pn,
    int* ap, int* ai, double* ax, int an, double* q, double* lower, double* upper,
    int iterations, double tolerance, double seconds, int polish,
    double* solution, double* diagnostics, int* status, int* count);

struct SparseEntry {
    int row;
    int column;
    double value;
}

struct QuadraticProblem {
    int variables;
    int constraints;
    SparseEntry[] objective;
    SparseEntry[] matrix;
    double[] linear;
    double[] lower;
    double[] upper;
}

struct SolverOptions {
    int iterations = 50_000;
    double tolerance = 1e-7;
    double seconds = 30;
    bool polish = true;
}

struct SolverResult {
    double[] solution;
    int status;
    int iterations;
    double primalResidual = 0;
    double dualResidual = 0;
    double objective = 0;
    double constraintViolation = 0;

    bool solved() const { return status == 1 || status == 2; }
}

private struct Csc {
    int[] pointers;
    int[] rows;
    double[] values;
}

private Csc compress(SparseEntry[] source, int rows, int columns, bool triangular) {
    auto entries = source.dup;
    foreach (entry; entries) {
        enforce(entry.row >= 0 && entry.row < rows && entry.column >= 0 && entry.column < columns,
            "Sparse matrix index out of range");
        enforce(isFinite(entry.value), "Nonfinite sparse matrix coefficient");
        enforce(!triangular || entry.row <= entry.column, "Objective must contain only its upper triangle");
    }
    entries.sort!((a, b) => a.column < b.column || a.column == b.column && a.row < b.row);
    Csc result;
    result.pointers = new int[columns + 1];
    size_t index;
    foreach (column; 0 .. columns) {
        result.pointers[column] = cast(int)result.values.length;
        while (index < entries.length && entries[index].column == column) {
            int row = entries[index].row;
            double value = 0;
            do { value += entries[index++].value; }
            while (index < entries.length && entries[index].column == column && entries[index].row == row);
            enforce(isFinite(value), "Sparse coefficient overflow");
            if (value != 0) { result.rows ~= row; result.values ~= value; }
        }
    }
    enforce(result.values.length <= int.max, "Sparse matrix exceeds index range");
    result.pointers[columns] = cast(int)result.values.length;
    return result;
}

/** Solve one bounded problem. C solving is bounded; cancellation is checked around it. */
SolverResult ngSolveQuadratic(QuadraticProblem problem, SolverOptions options = SolverOptions.init,
    bool delegate() canceled = null) {
    enforce(problem.variables > 0 && problem.constraints >= 0, "Invalid optimization dimensions");
    enforce(problem.linear.length == problem.variables && problem.lower.length == problem.constraints &&
        problem.upper.length == problem.constraints, "Optimization vector dimensions differ");
    enforce(options.iterations > 0 && isFinite(options.tolerance) && options.tolerance > 0 &&
        isFinite(options.seconds) && options.seconds > 0, "Invalid solver options");
    foreach (value; problem.linear) enforce(isFinite(value), "Nonfinite linear objective");
    auto lower = problem.lower.dup;
    auto upper = problem.upper.dup;
    foreach (i; 0 .. lower.length) {
        enforce(lower[i] <= upper[i] && lower[i] != double.infinity && upper[i] != -double.infinity,
            "Invalid optimization bounds");
        if (lower[i] == -double.infinity) lower[i] = -1e30;
        if (upper[i] == double.infinity) upper[i] = 1e30;
    }
    auto p = compress(problem.objective, problem.variables, problem.variables, true);
    auto a = compress(problem.matrix, problem.constraints, problem.variables, false);
    enforce(canceled is null || !canceled(), "Optimization canceled");
    if (Fiber.getThis() !is null) Fiber.yield();
    enforce(canceled is null || !canceled(), "Optimization canceled");
    SolverResult result;
    result.solution = new double[problem.variables];
    double[3] diagnostics;
    auto error = ng_osqp_solve(problem.variables, problem.constraints,
        p.pointers.ptr, p.rows.ptr, p.values.ptr, cast(int)p.values.length,
        a.pointers.ptr, a.rows.ptr, a.values.ptr, cast(int)a.values.length,
        problem.linear.ptr, lower.ptr, upper.ptr, options.iterations, options.tolerance,
        options.seconds, options.polish, result.solution.ptr, diagnostics.ptr, &result.status, &result.iterations);
    enforce(error == 0, "OSQP setup or solve failed");
    enforce(canceled is null || !canceled(), "Optimization canceled");
    result.primalResidual = diagnostics[0];
    result.dualResidual = diagnostics[1];
    result.objective = diagnostics[2];
    if (!result.solved()) { result.solution = null; return result; }
    foreach (value; result.solution) enforce(isFinite(value), "OSQP returned nonfinite solution");
    auto actual = new double[problem.constraints];
    actual[] = 0;
    foreach (entry; problem.matrix) actual[entry.row] += entry.value * result.solution[entry.column];
    foreach (i; 0 .. actual.length) {
        enforce(isFinite(actual[i]), "Constraint evaluation overflow");
        auto violation = problem.lower[i] - actual[i];
        if (actual[i] - problem.upper[i] > violation) violation = actual[i] - problem.upper[i];
        if (violation > result.constraintViolation) result.constraintViolation = violation;
    }
    return result;
}
