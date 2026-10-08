# OSQP dependency integration

Use unmodified upstream sources as submodules: OSQP v1.0.0 in `vendor/osqp`
and QDLDL v0.1.8 in `vendor/qdldl`. The superproject gitlinks pin the exact
commits. Do not track moving branches. The submodules are registered and pinned;
the following commands document how they were initially added.

```text
git submodule add https://github.com/osqp/osqp.git vendor/osqp
git -C vendor/osqp checkout --detach v1.0.0
git submodule add https://github.com/osqp/qdldl.git vendor/qdldl
git -C vendor/qdldl checkout --detach v0.1.8
git add .gitmodules vendor/osqp vendor/qdldl
```

Existing checkouts initialize dependencies with:

```text
git submodule update --init --recursive
```

## Static build

Require CMake 3.24 or later and a C compiler. From the repository root:

```text
cmake -S build-aux/osqp -B build/osqp -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=out/osqp
cmake --build build/osqp --config Release
cmake --install build/osqp --config Release
```

The wrapper uses the builtin backend, local QDLDL, double values and 32-bit
indices. It disables shared libraries, examples, tests, code generation,
derivatives and process-wide signal handling. Profiling remains available for
time limits. Configuration and compilation must work without network access.
On Windows select a C toolchain and runtime compatible with the application's
D compiler; do not mix MinGW archives with the MSVC linker. Use separate build
directories when changing compiler, architecture, runtime or ABI settings.

## D integration boundary

The future D binding uses `extern(C)`, `double` for OSQPFloat and `int` for
OSQPInt. Match the generated installed headers, including structure alignment.
Own the CSC buffers for the full solver lifetime and always release the solver.
The C bridge in `build-aux/osqp/bridge.c` fixes the D-facing ABI without copying
OSQP settings structures into D. DUB builds and installs the two static archives
before building the app, then links `nijigenerate_osqp` and `osqpstatic`.

Run solves inside the same worker-local Fiber used by AutoRig. The C solve call
does not yield the D Fiber; yield and check cancellation between problems, and
bound each solve's iterations and runtime. Model changes use existing command
execution and the task action boundary rather than NJC subprocesses.

## Redistribution and updates

The CMake install copies upstream LICENSE and NOTICE files into
`share/licenses/osqp` and `share/licenses/qdldl`, retaining nested notices such
as AMD's. Include that tree in binary distributions. OSQP and QDLDL use Apache
2.0; review all bundled notices for the pinned versions before release.

Release source archives must include both populated vendor trees and their
licenses, excluding Git metadata. Ordinary GitHub source archives do not include
submodule contents. Record the two gitlink revisions in the release manifest.
CI must initialize the pinned submodules before configuring, build the wrapper
on Windows/Linux/macOS, and exercise a known QP through the D binding when it is
implemented. Updates require ABI, numerical-result and platform-build checks.

The submodules and standalone static build are configured. The Windows x64
Release build and install were verified with Visual Studio 2022 and CMake 3.31.
The D binding, sparse problem assembly and application link settings are
implemented. Release workflow integration remains to be implemented.

## Implemented deterministic stages

`DeterministicRigProcessor` exposes `preserve-face-orientation` and
`fit-grid-depth` computation presets, plus the editor's `correct-face-key`
workflow. AutoRig executes these tasks in its existing worker-local Fiber.
These are stage-local tools, not a complete PSD-to-rig preset.
Anatomical scaffold fitting, depth sampling and evaluation of 19 embedded
semantic templates are also available; see `autorig-embedded-templates.md`.

The orientation request contains `xs`, `ys`, `points`, `uv`, `depthDirection`,
`width` and optional boolean `protected` values. Points are row-major, with X
varying fastest. Correction preserves the template's depth and moves only along
the supplied projected depth direction. The outer UV ring permits correction
up to 4% of width; protected vertices and the interior remain fixed.

The grid-depth request contains `xs`, `ys`, `base`, `query`, `truth`,
`tolerance` and `anchorCount`. The final `anchorCount` samples are exact anchors.
The former SciPy linear program is represented as a zero-Hessian OSQP problem;
the output reports `passed=false` when a verified solution is unavailable.

For `correct-face-key`, all geometry must use the target's model-local coordinate
frame. The separate `target` input contains `rootId`, `nodeId`, `parameterId`,
`keyPoint` (two indices), and row-major `restPoints`. The apply task dispatches
`SetDeformBindingCommand` directly on the main thread under AutoRig's action
group; it checks the active root, target identities, vertex count and rest
coordinates before applying offsets. Inputs must be regenerated after mesh or
rest-geometry edits.

## Verification

`tests/autorig_solver.d` checks constrained QP, LP, infeasibility, neutral
preservation, repaired orientation, anchored grid fitting, cancellation and
workflow connection/application dispatch. Build its standalone package after
installing the native archives:

```text
dub build --root=tests/autorig-solver --compiler=C:/opt/ldc-1.41/bin/ldc2.exe --skip-registry=all
```

Use Developer PowerShell on Windows. In this environment LDC 1.41's linker
driver terminated with an illegal-instruction exception; compilation followed
by direct MSVC linking succeeded and the numerical/workflow tests passed.
The changed editor modules also compiled with the application's dependency
import paths. The full Windows application build was attempted but stopped in
nijilive's existing version-generation pre-build command because `dub` was not
on that command's PATH. No Git history commands were run to work around it.

PSD preparation, evidence generation, complete rig construction, body/depth
bone application, the remaining facial controls and global review are not yet
ported. Do not present the stage-local presets as a completed automatic rig.
