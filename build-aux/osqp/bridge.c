#include <osqp.h>
#include <string.h>

/* A small fixed ABI keeps upstream settings structures out of D bindings. */
int ng_osqp_solve(int n, int m, int *pp, int *pi, double *px, int pn,
    int *ap, int *ai, double *ax, int an, double *q, double *lower, double *upper,
    int iterations, double tolerance, double seconds, int polish,
    double *solution, double *diagnostics, int *status, int *count) {
    OSQPCscMatrix p = {n, n, pp, pi, px, pn, -1, 0};
    OSQPCscMatrix a = {m, n, ap, ai, ax, an, -1, 0};
    OSQPSettings settings;
    OSQPSolver *solver = NULL;
    osqp_set_default_settings(&settings);
    settings.verbose = 0;
    settings.adaptive_rho = 0;
    settings.max_iter = iterations;
    settings.eps_abs = tolerance;
    settings.eps_rel = tolerance;
    settings.time_limit = seconds;
    settings.polishing = polish;
    int error = osqp_setup(&solver, &p, q, &a, lower, upper, m, n, &settings);
    if (!error) error = osqp_solve(solver);
    if (!error && solver && solver->info) {
        *status = solver->info->status_val;
        *count = solver->info->iter;
        diagnostics[0] = solver->info->prim_res;
        diagnostics[1] = solver->info->dual_res;
        diagnostics[2] = solver->info->obj_val;
        if ((*status == OSQP_SOLVED || *status == OSQP_SOLVED_INACCURATE)
            && solver->solution && solver->solution->x)
            memcpy(solution, solver->solution->x, n * sizeof(double));
    }
    if (solver) osqp_cleanup(solver);
    return error;
}
