#pragma once

#include <limits>

// Match build-aux/osqp/bridge.c, built with OSQP_USE_FLOAT/OSQP_USE_LONG off.
// All pointed-to buffers are borrowed only for this synchronous call.
static_assert(sizeof(int) == 4 && sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
extern "C" int ng_osqp_solve(int n, int m, int* pp, int* pi, double* px, int pn,
    int* ap, int* ai, double* ax, int an, double* q, double* lower, double* upper,
    int iterations, double tolerance, double seconds, int polish,
    double* solution, double* diagnostics, int* status, int* count);
