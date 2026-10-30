# Native C++ port

This is the in-progress C++20 implementation described in `../cpp_design.md` and
`../tasks.md`. It is not yet an editor executable. The D application remains the
behavioral reference. No configure step downloads packages or invokes Git.

## Build the implemented modules

```text
cmake -S . -B build/native-cpp -DBUILD_TESTING=ON
cmake --build build/native-cpp --config Debug
ctest --test-dir build/native-cpp -C Debug --output-on-failure
```

Run these from the repository root. MSVC, GCC, or Clang with C++20 support is
required. The ordinary native build does not require D or Python.

To compare directly with the current D implementation, also set
`NIJIGENERATE_D_COMPILER` to a local LDC executable and `NIJIGENERATE_PYTHON` to
a Python 3 executable. This compiles the original D IO modules unchanged and
runs the same deterministic inputs through both implementations. Evidence is
written to `build/native-cpp/native/evidence/<config>/`.

The command metadata/scope comparison uses unchanged, dependency-free spans of
`commands/base.d`, with extracted source and evidence in
`build/native-cpp/native/command-evidence/<config>/`. It is not a full-module
or full-command-inventory comparison. CMake also checks the known multiple-scope
compile-fail constraint. See `port/wbs/M066/contract.md` for boundaries.

## Headless logic target

With `BUILD_TESTING=ON`, `nijigenerate_headless_tests` links only the pure native
core and runs without a window, SDL or a GL context. Build and run its labeled
tests together with `cmake --build build/native-cpp --config Debug --target
nijigenerate_headless_check`. When D/Python are configured, this also compiles
the original depthsample module and uses the B005/B006 capture/replay tools.
Immutable per-run snapshots are retained under `native/headless-evidence/Debug`
inside the build directory. This is a logic-fixture target, not a GUI executable.

## Runtime integration probe

Enable `NIJIGENERATE_RUNTIME_PROBE=ON`, provide
`NIJIGENERATE_NICXLIVE_SOURCE_DIR`, and set `SDL3_DIR`/`CMAKE_PREFIX_PATH` to
installed SDL3 and Boost packages. Missing dependencies fail configuration.
SDL2 is not supported. This probe tests native linking and Node parent changes;
it does not claim to test rendering, ImGui, or the complete editor.

For Windows project-wide integration, `windows-local` configures one x64
Debug/Release project using explicit existing local dependencies.
`cmake --build --preset windows-debug` and `windows-release` build each complete
configuration; matching CTest presets run its tests. Provide local Boost/SDL3/
pinned-ImGui paths at configure time. Exceptions, RTTI, UTF-8, DLL CRT and strict
floating point are explicit; global fast-math/AVX overrides fail configuration.

## Embedded resources

Enable `NIJIGENERATE_EMBED_RESOURCES=ON` and provide `NIJIGENERATE_PYTHON` to
build `nijigenerate_resources`. This optional static library embeds every file
under `res/`, including fonts and their notices, as exact bytes. It exposes
`nijigenerate/resources/embedded.hpp`: `ngEmbeddedResource(name)` returns an
optional immutable byte span using the exact forward-slash relative name;
`ngEmbeddedResources()` lists the inventory. Empty files and missing names are
distinct. Byte spans have static lifetime and no added terminator.

Generation records resource size/hash in the build directory's
`native/embedded-resources/manifest.json`. CMake detects input edits and inventory
additions/removals. Unchanged generated sources retain timestamps, avoiding
recompilation of every font for a small resource change. `native_embedded_resources`
checks the compiled inventory and every byte against current inputs. On MSVC,
`native_resource_rebuild` checks actual incremental builds, including binary,
empty and UTF-8 names. This library is ready for module integration; it does not
replace unported D resource consumers by itself.

The asset catalog is enabled with resource embedding unless
`NIJIGENERATE_ASSET_CATALOG=OFF`. `nijigenerate/resources/assets.hpp` resolves
all current application shader literals and startup font/image imports. The
catalog preserves inline ImGui shader bodies separately from the backend's
version prefix. `native_asset_catalog` checks resolved startup bytes and shader
inventory; with D configured, `native_shader_source_differential` compares all
18 shader expressions in one reference compilation. Resource consumers still
need their individual UI/renderer integration.

## Translation catalogs and version metadata

Enable `NIJIGENERATE_TRANSLATION_CATALOGS=ON` to compile `tl/*.po` using an
existing local `NIJIGENERATE_MSGFMT`. The `nijigenerate_translation_catalogs`
target builds all catalogs. Executables can call the CMake helper
`ng_stage_translation_catalogs(target)` to stage them at an existing locale scan
location. The `Translations` install component supports packaging with an
explicit prefix.

`nijigenerate_version_metadata` exposes a generated constexpr metadata header.
Defaults come from current `version.txt`; use `NIJIGENERATE_VERSION_OVERRIDE`,
`NIJIGENERATE_SOURCE_REVISION` and `NIJIGENERATE_BUILD_CHANNEL` for explicit
labels. Native configuration never calls Git or the D version generator. The
JSON companion is `native/generated/version_metadata.json` inside the build.

## Reference integrity

```text
python native/tools/reference_manifest.py verify --manifest native/port/reference.json
```

The initial snapshot contains the current D source/tests and DUB configuration.
External dependencies and assets still need separate provenance locks. Do not
replace this snapshot silently after changing a reference implementation.

On this Windows host, restricted multi-node MSBuild did not progress past the
build-system check. A normal local single-node build completed with
`cmake --build build/native-cpp --config Debug -- /m:1 /nr:false`.
