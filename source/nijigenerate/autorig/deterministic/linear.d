module nijigenerate.autorig.deterministic.linear;

import std.exception : enforce;
import std.math : abs, sqrt, isFinite;

/** Partial-pivoted elimination for small dense landmark and TPS systems. */
double[][] ngRigSolve(double[][] matrix, double[][] right) {
    size_t n = matrix.length;
    enforce(n > 0 && right.length == n, "Invalid linear system dimensions");
    size_t outputs = right[0].length;
    double[][] a, b;
    double scale = 0;
    foreach (row; matrix) {
        enforce(row.length == n, "Expected a square linear system");
        a ~= row.dup;
        foreach (v; row) { enforce(isFinite(v), "Nonfinite linear coefficient"); if (abs(v) > scale) scale = abs(v); }
    }
    foreach (row; right) {
        enforce(row.length == outputs, "Linear right-hand dimensions differ");
        b ~= row.dup;
        foreach (v; row) enforce(isFinite(v), "Nonfinite linear target");
    }
    enforce(scale > 0, "Singular linear system");
    foreach (k; 0 .. n) {
        size_t pivot = k;
        foreach (i; k + 1 .. n) if (abs(a[i][k]) > abs(a[pivot][k])) pivot = i;
        enforce(abs(a[pivot][k]) > scale * 1e-13, "Singular or ill-conditioned linear system");
        auto row = a[k]; a[k] = a[pivot]; a[pivot] = row;
        row = b[k]; b[k] = b[pivot]; b[pivot] = row;
        foreach (i; k + 1 .. n) {
            double factor = a[i][k] / a[k][k];
            foreach (j; k .. n) a[i][j] -= factor * a[k][j];
            foreach (j; 0 .. outputs) b[i][j] -= factor * b[k][j];
        }
    }
    auto result = ngRigZeroMatrix(n, outputs);
    foreach_reverse (i; 0 .. n) foreach (j; 0 .. outputs) {
        double value = b[i][j];
        foreach (k; i + 1 .. n) value -= a[i][k] * result[k][j];
        result[i][j] = value / a[i][i];
        enforce(isFinite(result[i][j]), "Linear solution overflow");
    }
    return result;
}

double[][] ngRigZeroMatrix(size_t rows, size_t columns) {
    double[][] result;
    foreach (i; 0 .. rows) { auto row = new double[columns]; row[] = 0; result ~= row; }
    return result;
}

/** Householder QR avoids squaring the condition number in weighted landmark fitting. */
double[][] ngRigLeastSquares(double[][] matrix, double[][] target) {
    size_t rows = matrix.length, columns = matrix[0].length, outputs = target[0].length;
    enforce(rows >= columns && target.length == rows, "Underdetermined landmark fit");
    double[][] a, b;
    foreach (row; matrix) { enforce(row.length == columns, "Invalid least-squares matrix"); a ~= row.dup; }
    foreach (row; target) { enforce(row.length == outputs, "Invalid least-squares target"); b ~= row.dup; }
    foreach (k; 0 .. columns) {
        double norm = 0;
        foreach (i; k .. rows) norm += a[i][k] * a[i][k];
        norm = sqrt(norm);
        enforce(isFinite(norm) && norm > 1e-12, "Rank-deficient landmark fit");
        double alpha = a[k][k] >= 0 ? -norm : norm;
        auto v = new double[rows - k];
        foreach (i; k .. rows) v[i-k] = a[i][k];
        v[0] -= alpha;
        double denominator = 0;
        foreach (value; v) denominator += value * value;
        foreach (j; k .. columns) {
            double dot = 0;
            foreach (i; k .. rows) dot += v[i-k] * a[i][j];
            foreach (i; k .. rows) a[i][j] -= 2 * v[i-k] * dot / denominator;
        }
        foreach (j; 0 .. outputs) {
            double dot = 0;
            foreach (i; k .. rows) dot += v[i-k] * b[i][j];
            foreach (i; k .. rows) b[i][j] -= 2 * v[i-k] * dot / denominator;
        }
    }
    auto result = ngRigZeroMatrix(columns, outputs);
    foreach_reverse (i; 0 .. columns) foreach (j; 0 .. outputs) {
        double value = b[i][j];
        foreach (k; i + 1 .. columns) value -= a[i][k] * result[k][j];
        result[i][j] = value / a[i][i];
        enforce(isFinite(result[i][j]), "Least-squares solution overflow");
    }
    return result;
}
