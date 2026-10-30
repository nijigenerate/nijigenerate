# Project integration batches

Integrate multiple independent, reviewed implementation leaves before building.
Keep function-level WBS and source comparisons, but do not run a complete runtime
build for every leaf. Each agent groups ready changes in its isolated stage;
the parent reviews and applies a batch, then builds the complete project once.
Run relevant comparisons and the full registered suite against that build.
Individual target builds are reserved for diagnosing a concrete failure.

The parent uses `build/native-integration-parent`, separate from other agents'
build trees. `native/tools/build-integration.ps1` imports only dependency cache
settings from an existing configured tree, configures a fresh output tree,
builds the entire project, and runs CTest only if the build succeeds. It holds
an exclusive lock for the batch. Do not edit integrated runtime headers while
this build/test batch runs; stage the next batch separately.
If CMake regenerates during a Visual Studio build, MSBuild may have already
selected an older target set. The script hashes the CMake input files listed in
the generated dependency lists (not the generated stamp timestamps) and
performs one additional whole-project incremental build in that specific case.
Further regeneration during recovery stops the batch before tests; this is not
permission to continuously change definitions during integration.

Keep configure/build/test logs and JUnit output in that output directory.
`integration-state.json` identifies the batch start time and current phase
(`configure`, `build`, `test`, `passed`, or `failed`). The script resets the text
test log at batch start; a retained JUnit file is not current evidence until
the current batch reaches its test phase and finishes writing that report.
Failed builds must not be followed by tests against stale executables and
reported as current-build validation. Record partial runs and targeted recovery
separately. A passing batch verifies its tested scope, not the whole migration.

On Windows, invoke the script with the installed CMake executable path:

```powershell
./native/tools/build-integration.ps1 -CMake '<absolute path to cmake.exe>'
```

This workflow does not download dependencies, run Git, or change system settings.
