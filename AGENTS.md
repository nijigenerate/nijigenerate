# Repository Guidelines

## Project Structure & Module Organization
- `source/`: D sources. Entry point `app.d`; primary modules under `nijigenerate/*` (core, io, panels, windows, widgets).
  - `actions`: implementation of undo/redo action.
  - `atlas`: Handling texture atlas.
  - `commands`: Handling commands which is invoked in reaction to UI or from API calls.
  - `core`: common implementations.
    - `cv`: lightweight computer vision. Interface is similar with OpenCV. Required to use Fiber framework to work with multi thread programming nicely in D.
    - `math`: commonly used mathmatical functions. Some undo/redo operations are included.
    - `selector`: XPath like object selector implementation.
  - `ext`: Implementation of derived classes for certain `Node` types. `nijilive` implementation is used for `.inp` file, while extended implementation is used for `.inx` files.
  - `io`: actual implementations for file such as load / save / import / export.
  - `panels`: implements dockable windows.
  - `utils`: implements aux functions.
  - `viewport`: implements main view of puppet editing.
    - `common`: common implementation among all edit modes.
      - `mesheditor`: editor for mesh or deformable vertices.
        - `operations`: direct manipulation interface for target object.
        - `tools`: provides purpose oriented manipulation tools such as `brush`, `grid` or `path deformation` for users.
        - `brush`: brush patterns.
    - `model`: implementations specialied for model edit mode.
    - `vertex`: implementations specialized for vertex definition and edit mode.
    - `anim`: implementations for animation edit mode.
  - `wideges`: implementation of GUI parts used in app.
  - `windows`: implementation of modal sub-windows.
- `res/`: assets and string-imported resources (e.g., UI images, i18n files).
- `build-aux/`: platform build resources (e.g., Windows `.rc`).
- `out/`: build artifacts (default binary `nijigenerate`).
- `tl/` and `TRANSLATING.md`: translation data and guidance.
- Root configs: `dub.sdl`, `dub.selections.json`; helper scripts: `genpot.sh`, `gentl.sh`.
- Related projects: `nijilive`. usually located at `../nijilive` in dev environment. Find one from dub configuration if not found.

## Build, Test, and Development Commands
- Build (Linux release): `dub build --config=linux-full`.
- Build (OSX release): `dub build --config=osx-full`.
- Build (Windows release): `dub build --config=win32-full`.
- Run binary: `./out/nijigenerate` in linux.
- Quick local run (debug): `dub` — uses the default config for your OS.

## Coding Style & Naming Conventions
- Indentation: 4 spaces; avoid trailing whitespace; keep lines ≲120 chars.
- Modules: lowercase paths (`nijigenerate.core.*`). Types/enums: `UpperCamelCase`. Functions/properties: `lowerCamelCase`.
- Functions: Add `ng` prefix for public scope functions. do not add any prefix for private scope functions. No prefix is required for methods.
- Compile‑time configuration constants follow `NG_*` (or `INC_*` for older definitions) uppercase style (see `source/nijigenerate/config.d`).
- Imports: group logically; prefer specific imports; minimize `public import` to stable surface areas.
- Formatting: keep consistent with existing code; if available, run `dfmt -i source/...` before pushing.

## Testing Guidelines
- Checks: verfiy implementation by following build instruction when you make any change for source codes; update docs/strings when changing UI; avoid unrelated formatting churn.

## Commit & Pull Request Guidelines
- Do not commit automatically. Commit only if explicitly specified.
- Commits: present tense and concise. Optional scope prefix (e.g., `fix: core: …`, `feature: ui: …`, `refactor: shortcuts: …`).
- For forks/unofficial builds, update links in `source/nijigenerate/config.d` (bug reports, docs, website) before distribution.
- No PR without approval.

## User interaction
- Answer in language which user speaks in request.