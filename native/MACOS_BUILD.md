# macOS native editor build

Use AppleClang with C++20 support, CMake 3.24 or later, Ninja, Python 3,
Boost 1.90 (including JSON and Context), FreeType and GNU gettext (`msgfmt`).
Keep the `nicxlive` checkout on `nijigenerate-compatible` beside this checkout.
The native editor uses SDL3 3.2.28 and Dear ImGui v1.91.9b-docking exactly;
another installed SDL version does not satisfy that dependency.

## Prepare pinned dependencies

Run from the nijigenerate repository root. These downloads are explicit;
CMake configuration does not download dependencies.

```sh
mkdir -p build/macos-deps
curl -fL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b-docking.tar.gz \
  -o build/macos-deps/imgui.tar.gz
tar -xzf build/macos-deps/imgui.tar.gz -C build/macos-deps
curl -fL https://github.com/libsdl-org/SDL/releases/download/release-3.2.28/SDL3-3.2.28.tar.gz \
  -o build/macos-deps/sdl3.tar.gz
tar -xzf build/macos-deps/sdl3.tar.gz -C build/macos-deps
cmake -S build/macos-deps/SDL3-3.2.28 -B build/macos-deps/sdl3-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/build/macos-deps/install" \
  -DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF
cmake --build build/macos-deps/sdl3-build --parallel 4
cmake --install build/macos-deps/sdl3-build
cmake -S build-aux/osqp -B build/macos-deps/osqp-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/build/macos-deps/install"
cmake --build build/macos-deps/osqp-build --parallel 4
cmake --install build/macos-deps/osqp-build
```

## Build

```sh
cmake --preset macos-local
cmake --build --preset macos-release
```

The preset builds the actual editor, its embedded fonts/images, translations,
ImGui, task fibers, OSQP and the local nicxlive runtime. It disables test fixtures,
which are not included in this source branch. The optional inspection-only
asset catalog is disabled because its generator is also absent; application
resources and the renderer's own shader sources are still compiled.

The executable is `build/macos-native/native/nijigenerate.app/Contents/MacOS/nijigenerate`.
The build generates the bundle icon from `build-aux/osx/nijigenerate.iconset` using macOS `iconutil`.
This is a local developer build using local dependency paths, not a redistributable
package. Override cache variables at configure time if dependencies
are installed elsewhere.

The ImGui target retains the original editor's CRC32 hashes and docking host ID
so existing panel layouts remain compatible with the native editor. A layout
already saved with orphaned panels needs those panels docked again; rebuilding
alone cannot recover docking links that were removed from the INI file.

For a standalone nicxlive shared library, run from its own repository:

```sh
cmake --preset macos-release
cmake --build --preset macos-release
```

This produces `build/macos-compatible/libnicxlive.dylib`.
