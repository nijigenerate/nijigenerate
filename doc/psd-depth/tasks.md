# PSD Depth Map Import Tasks

Status legend: `[ ]` todo, `[>]` in progress, `[x]` done, `[?]` blocked.

This task list implements `doc/psd-depth/design.md`.

## Phase 1: Core Types and Parsing

- [x] PSDDEPTH-01: Add `source/nijigenerate/io/depthmap_psd.d`.
  - Define import settings for invert, depth range, convolution mode, alpha threshold, and missing-data policy.
  - Define result structs for layer mappings, per-grid output, unmatched layers, and summary counts.

- [x] PSDDEPTH-02: Reconstruct PSD layer paths.
  - Follow the same PSD group traversal convention used by PSD import/merge.
  - Output `/group/layer` paths for image layers.
  - Skip PSD group open/close markers.

- [x] PSDDEPTH-03: Extract grayscale depth layer data.
  - Load per-layer image data through `extractLayerImage()`.
  - Preserve alpha for valid-pixel checks.
  - Avoid premultiplying data before depth sampling unless required by the PSD library's output format.

## Phase 2: Mapping

- [x] PSDDEPTH-04: Match PSD layers to existing nodes.
  - Match exact `ExPart.layerPath`.
  - Match `baseName(ExPart.layerPath)` to PSD layer name.
  - Match `ExPart.name` to PSD layer name.
  - Optionally match `GridDeformer.name` when enabled by settings.

- [x] PSDDEPTH-05: Resolve matched nodes to target `GridDeformer`.
  - Use the matched node directly if it is a `GridDeformer`.
  - Otherwise walk parent nodes upward and use the first `GridDeformer`.
  - Mark layers without a target GridDeformer as unmatched.

- [x] PSDDEPTH-06: Detect ambiguous mappings.
  - Report when one PSD layer matches multiple candidate nodes at the same priority.
  - Let UI mark them as ambiguous instead of silently choosing an arbitrary node.
  - For command-only execution, define a deterministic default policy.

## Phase 3: Coordinate Mapping

- [x] PSDDEPTH-07: Convert GridDeformer vertices to PSD layer coordinates.
  - Convert local grid vertex positions into model/world coordinates.
  - Convert model/world coordinates to PSD document coordinates using document center as origin.
  - Convert document coordinates to layer-local coordinates using `layer.left` and `layer.top`.

- [x] PSDDEPTH-08: Verify transform API choice.
  - Compare behavior against existing PSD import/merge placement.
  - Confirm whether to use `transform`, `transformNoLock`, or another established helper.
  - Add a small regression fixture for translated/parented GridDeformer sampling.

## Phase 4: Depth Conversion and Convolution

- [x] PSDDEPTH-09: Implement RGB grayscale to depth conversion.
  - Default: white is front, black is back.
  - Support invert.
  - Use configurable `backDepth` and `frontDepth`.
  - Initially use average RGB.

- [x] PSDDEPTH-10: Implement valid-pixel filtering.
  - Use `alphaThreshold`.
  - Treat transparent pixels as no data, not black/back depth.
  - Return no sampled value when no valid pixels are found.

- [x] PSDDEPTH-11: Implement convolution modes.
  - `Nearest`
  - `Box3x3`
  - `Box5x5`
  - `Gaussian3x3`
  - `Gaussian5x5`
  - `Median3x3`
  - `Frontmost3x3`
  - `Backmost3x3`

- [x] PSDDEPTH-12: Add deterministic convolution tests.
  - Use tiny synthetic layer data.
  - Cover alpha filtering.
  - Cover invert behavior.
  - Cover frontmost/backmost after conversion.

## Phase 5: Layer Compositing

- [x] PSDDEPTH-13: Composite matched layers per GridDeformer vertex.
  - Sample each mapped layer for the vertex.
  - Choose the largest converted depth as the frontmost value.
  - Do not average across layers.

- [x] PSDDEPTH-14: Implement missing-data policies.
  - `KeepExisting`
  - `SetZero`
  - `SetBack`
  - `SkipGrid`
  - Default to `KeepExisting`.

- [x] PSDDEPTH-15: Add multi-layer compositing tests.
  - Multiple layers mapped to one GridDeformer.
  - Multiple valid pixels at the same vertex choose the frontmost depth.
  - Transparent top layer allows deeper valid layer to contribute.

## Phase 6: Command Integration

- [x] PSDDEPTH-16: Add an import command.
  - Prefer `commands.depth.map` for the data-changing command.
  - Expose arguments for path, invert, depth range, convolution, channel, custom radius, alpha threshold, and missing policy.
  - Return summary information for changed grids and unmatched layers.

- [x] PSDDEPTH-17: Apply generated depths with undo support.
  - Group updates across multiple GridDeformers into one action.
  - Use `DepthMappedChangeAction` or the same command path as `SetDepths`.
  - Preserve existing depths according to missing-data policy.

- [x] PSDDEPTH-18: Mark Depth Bone dirty state.
  - Call the existing dirty notification helper for each changed GridDeformer.
  - Verify subsequent Depth Bone refresh behavior remains consistent.

- [x] PSDDEPTH-19: Register command metadata.
  - Add the command enum entry.
  - Register benign default arguments for command browser and MCP compatibility.
  - Update command browser fixture expectations if required.

## Phase 7: UI Integration

- [x] PSDDEPTH-20: Add `source/nijigenerate/windows/psddepthmap.d`.
  - Build a dedicated PSD Depth Map import window.
  - Load PSD once and show mapping results.
  - Dispose preview resources and parsed document data safely on close.

- [x] PSDDEPTH-21: Add mapping review UI.
  - Show PSD layer path.
  - Show matched node.
  - Show target GridDeformer.
  - Show status: matched, unmatched, ambiguous, ignored.
  - Add filters such as "Only show unmatched".

- [x] PSDDEPTH-22: Add conversion and sampling options.
  - `Invert Depth`
  - `Back Depth`
  - `Front Depth`
  - `Channel`
  - `Sampling`
  - `Custom Radius`
  - `Alpha Threshold`
  - `Missing Vertex Pixel`

- [x] PSDDEPTH-23: Add Apply behavior.
  - Apply reviewed mappings through the command or shared apply helper.
  - Show errors through the existing dialog/notification pattern.
  - Close the window only after successful apply.

- [x] PSDDEPTH-24: Add menu entry and dialog command.
  - Add `File > Import > PSD Depth Map...`.
  - Add optional Depth Edit tool settings entry if it fits the existing UI.
  - Wire file dialog filter to `*.psd`.

## Phase 8: Regression and Documentation

- [x] PSDDEPTH-25: Add automated regression coverage.
  - Layer matching.
  - Parent GridDeformer resolution.
  - Depth conversion.
  - Convolution.
  - Layer frontmost compositing.
  - Undo/redo.

- [x] PSDDEPTH-26: Add UI smoke coverage entry.
  - Update regression scenario inventory for the PSD Depth Map window.
  - Mark deeper visual UI testing as computer-use/manual if needed.

- [x] PSDDEPTH-27: Build verification.
  - Run the relevant build command after source changes.
  - Recommended local command depends on platform, for example `dub build --config=win32-full` on Windows.

- [x] PSDDEPTH-28: Update user-facing strings and translation inputs.
  - Add all UI strings through existing i18n helpers.
  - Update translation source files if required by the project workflow.

## Phase 9: Follow-up Enhancements

- [x] PSDDEPTH-29: Add manual remapping support.
  - Allow dragging or selecting a target GridDeformer for a PSD layer.
  - Store only session-local mappings unless a broader persistence design is requested.

- [x] PSDDEPTH-30: Add channel selection.
  - `Average RGB`
  - `R`
  - `G`
  - `B`
  - Optional perceptual luminance.

- [x] PSDDEPTH-31: Add custom convolution radius.
  - Keep named presets for the first release.
  - Add custom radius only if artists need stronger smoothing.

- [x] PSDDEPTH-32: Add preview rendering.
  - Preview generated per-GridDeformer depth values before apply.
  - Show missing vertices and ambiguous mappings clearly.

## Phase 10: Preview and Scale Refinement

- [x] PSDDEPTH-33: Add import-time depth scale.
  - Add `depthScale` to import settings and command arguments.
  - Store imported depths as the converted depth multiplied by `depthScale`.
  - Keep the default scale at `1.0`.

- [x] PSDDEPTH-34: Add layer hover previews.
  - Show the original PSD layer on hover.
  - Show the converted depth mask on hover.
  - Rebuild preview textures when conversion settings change.

- [x] PSDDEPTH-35: Add per-GridDeformer mask review.
  - Show which depth layers contribute to each target `GridDeformer`.
  - Show sampled vertex counts per depth layer.
  - Show final selected vertex counts after frontmost compositing.
