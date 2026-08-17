# PSD Depth Map Import Design

## Purpose

This feature imports a PSD that stores per-layer depth information as RGB grayscale values and applies the result to the depth map of mapped `GridDeformer` nodes.

The PSD is expected to use layers corresponding to existing model layers. Matching is based on existing layer paths and layer names, then the matched nodes are grouped by the `GridDeformer` that controls them. When multiple matched PSD layers provide depth at the same grid vertex, the frontmost depth is used.

## Existing Model Integration

Depth values are stored on `ExGridDeformer` through `DepthMappedNode`.

Relevant existing code:

- `source/nijigenerate/ext/nodes/exgriddeformer.d`
- `source/nijigenerate/ext/nodes/exdepthmapped.d`
- `source/nijigenerate/actions/depth.d`
- `source/nijigenerate/commands/depth/map.d`
- `source/nijigenerate/io/psd.d`
- `source/nijigenerate/windows/psdmerge.d`

The implementation should reuse the existing undo/redo path used by `DepthMappedChangeAction` and `DepthMapCommand.SetDepths`.

## Proposed Files

- `source/nijigenerate/io/depthmap_psd.d`
  - PSD parsing for depth maps.
  - PSD layer path reconstruction.
  - Layer-to-node and node-to-GridDeformer matching.
  - RGB grayscale to depth conversion.
  - Convolution sampling.
  - Per-GridDeformer frontmost layer compositing.

- `source/nijigenerate/commands/depth/map.d`
  - Add a command such as `ImportPSDDepthsCommand`.
  - Apply generated `float[]` depth arrays through undoable actions.
  - Mark related Depth Bone data dirty.

- `source/nijigenerate/windows/psddepthmap.d`
  - Add a confirmation window for PSD layer mapping, sampling options, and apply action.

- `source/nijigenerate/windows/package.d`
  - Export the new window module.

- `source/nijigenerate/commands/puppet/file.d` or `source/nijigenerate/commands/depth/map.d`
  - Add the user-facing dialog command.
  - Prefer keeping the data-changing command under `commands.depth.map`.

- `source/nijigenerate/widgets/mainmenu.d`
  - Add a menu entry for importing PSD depth maps.

## Data Flow

```text
PSD file
  -> parse PSD layers
  -> reconstruct PSD layer paths
  -> match PSD layers to existing Part/GridDeformer-related nodes
  -> find the containing GridDeformer for each matched node
  -> sample depth around each GridDeformer vertex from each matched layer
  -> composite matched layers by frontmost depth
  -> apply generated depths to each GridDeformer with undo support
```

## Layer Matching

PSD layers should be matched to existing nodes using the same conventions as PSD merge.

For each PSD image layer:

1. Reconstruct its layer path as `/group/layer`.
2. Match against `ExPart.layerPath`.
3. If no exact path match exists, match against the base name of `ExPart.layerPath`.
4. If no layer path match exists, match against `ExPart.name`.
5. Optionally match directly against `GridDeformer.name` for depth-only workflows.

The first implementation should prefer `ExPart` matches because imported and merged PSD assets already maintain `layerPath` metadata there.

## Finding Target GridDeformer

After a PSD layer is matched to a node:

1. If the matched node is a `GridDeformer`, use it directly.
2. Otherwise, walk the node's parents upward.
3. Use the first parent that is a `GridDeformer`.
4. If no containing `GridDeformer` exists, treat the PSD layer as unmatched.

This allows multiple part layers controlled by the same `GridDeformer` to contribute to a single depth map.

## Coordinate Mapping

Sampling should use the same model-to-PSD coordinate convention as existing PSD import and merge logic: the PSD document center corresponds to the model origin.

For each `GridDeformer` vertex:

```text
local vertex -> model/world position
document x = world x + document.width / 2
document y = world y + document.height / 2
layer x = document x - layer.left
layer y = document y - layer.top
```

The exact transform helper should follow the existing node transform APIs used elsewhere in the viewport and command code. The implementation must verify whether the sampled vertex position should use `transform`, `transformNoLock`, or another established model-space helper.

## Depth Value Conversion

The PSD stores depth as RGB grayscale. By default:

```text
white = front
black = back
```

The UI and command should expose an invert option. Internally, conversion should produce depth values where larger values are considered closer/front.

Recommended defaults:

```text
backDepth = -1.0
frontDepth = 1.0
invert = false
```

Conversion:

```text
depth01 = selectedChannel(r, g, b) / 255.0
if invert:
    depth01 = 1.0 - depth01
depth = lerp(backDepth, frontDepth, depth01) * depthScale
```

`depthScale` is applied at import time to the stored depth values. The default is `1.0`.
The scale must be non-negative so that "frontmost" compositing remains stable; use `Invert Depth`
to reverse white/black interpretation.

Supported channel modes:

```text
AverageRGB
R
G
B
Luminance
```

`AverageRGB` is the default. `Luminance` uses perceptual weights for non-neutral source images.

## Valid Pixel Rules

Alpha determines whether a pixel contains depth information.

```text
alpha > alphaThreshold => valid depth pixel
alpha <= alphaThreshold => no depth information
```

Transparent pixels must not be treated as black depth. A fully transparent black pixel means "no data", not "back".

Default `alphaThreshold` should be low, such as `0.01`.

## Convolution Sampling

Depth sampling should support multiple convolution modes. Sampling happens per PSD layer before compositing layers together.

Initial convolution modes:

```text
Nearest
Box3x3
Box5x5
Gaussian3x3
Gaussian5x5
Median3x3
Frontmost3x3
Backmost3x3
BoxCustom
GaussianCustom
MedianCustom
FrontmostCustom
BackmostCustom
```

Recommended default:

```text
Gaussian3x3
```

Rules:

- Only valid pixels participate in convolution.
- If no valid pixels are found in the kernel, the layer contributes no depth at that vertex.
- Average and Gaussian kernels should normalize by the sum of weights for valid pixels only.
- Median should use valid depth samples only.
- Frontmost should choose the largest converted depth value.
- Backmost should choose the smallest converted depth value.
- Custom convolution modes should use `customRadius` clamped to a practical range, currently `1..64`.

The front/back comparison should happen after RGB-to-depth conversion and invert handling. This keeps `Frontmost` consistent even when the user enables invert.

## Layer Compositing

Each matched layer produces zero or one sampled depth value for a GridDeformer vertex. Layer compositing then selects the frontmost contributed value.

```text
bestDepth = none
for each matched layer for this GridDeformer:
    sampled = sampleLayerDepth(layer, vertex)
    if sampled has value and (bestDepth is none or sampled > bestDepth):
        bestDepth = sampled
```

Layers must not be averaged together. Averaging layers would destroy front/back relationships between separate parts.

When no layer contributes depth at a vertex, the UI/command should support a missing-data policy:

```text
KeepExisting
SetZero
SetBack
SkipGrid
```

Recommended default:

```text
KeepExisting
```

If the target GridDeformer has no existing depth array and `KeepExisting` is selected, missing vertices should fall back to `0.0`.

## UI Design

Add a dedicated PSD Depth Map import window instead of a small inspector button. The operation is file-based, affects one or more GridDeformers, and needs mapping review before applying.

Suggested entry points:

- `File > Import > PSD Depth Map...`
- Optionally expose the same action in Depth Edit tool settings.

Window layout:

```text
left pane:  PSD layers and preview
right pane: matched node and target GridDeformer
footer:     conversion/sampling options and Apply button
```

The mapping table should show:

```text
PSD Layer Path
Matched Node
Target GridDeformer
Remap
Status
```

Statuses:

```text
Matched
Unmatched
Ambiguous
Ignored
Manual
```

The `Remap` control is session-local. It should allow `Auto`, `Ignore`, or an explicit `GridDeformer` target for each PSD layer. Applying reviewed mappings should use the same lower-level undoable apply helper as command import, rather than duplicating depth mutation logic in the window.

The window should also show a generated-depth preview per target `GridDeformer` before Apply:

```text
GridDeformer
Sampled vertices
Missing vertices
Min depth
Max depth
Status
```

Footer options:

```text
Invert Depth
Back Depth
Front Depth
Depth Scale
Channel
Sampling
Custom Radius
Alpha Threshold
Missing Vertex Pixel
Only show unmatched
Apply
```

`Invert Depth` should be unchecked by default. The tooltip should state that the default interpretation is white as front and black as back.

Layer preview UI:

- Each mapping row exposes hover previews for both the original PSD layer and the converted depth mask.
- Each target `GridDeformer` exposes a per-layer mask summary showing how many vertices were sampled from the layer and how many vertices finally selected that layer after frontmost compositing.
- The depth mask preview uses the active conversion options, including invert, channel, and alpha threshold. Transparent pixels remain transparent in the preview.

## Command Design

The command should support non-UI execution for regression tests and automation.

Potential command arguments:

```text
path: string
invert: bool
backDepth: float
frontDepth: float
depthScale: float
convolution: string
channel: string
customRadius: int
alphaThreshold: float
missingPolicy: string
matchDirectGridName: bool
```

For UI-reviewed mappings, the window passes session-local ignore/remap overrides into PSD depth building and then applies the resulting `PsdDepthImportResult` through a shared lower-level apply helper.

The command should:

1. Validate that a puppet is active.
2. Parse the PSD.
3. Build automatic mappings.
4. Compute new depth arrays for matched GridDeformers.
5. Apply all depth changes inside one undo group.
6. Mark Depth Bone data dirty for each changed GridDeformer.
7. Return a `CommandResult` or typed result with counts and unmatched layers.

## Undo/Redo

Each changed GridDeformer should be applied through `DepthMappedChangeAction`. Multiple GridDeformer updates should be grouped as one user action.

Expected behavior:

- One Apply action changes all affected GridDeformers.
- Undo restores all previous depth arrays.
- Redo restores imported depth arrays.
- Depth Bone dirty state is marked after apply.

## Testing Strategy

Automated tests should cover the data path independently from the UI.

Recommended coverage:

- PSD layer path reconstruction.
- Layer path match to `ExPart.layerPath`.
- Layer name fallback match.
- Parent traversal to containing `GridDeformer`.
- White-front conversion.
- Invert conversion.
- Alpha threshold excludes transparent pixels.
- Convolution modes produce expected values on small fixtures.
- Multiple matched layers on one GridDeformer choose the frontmost sampled depth.
- Missing-data policies.
- Undo/redo restores depth arrays.

UI testing can be limited to smoke coverage for the window because the critical behavior should be in command and IO-level helpers.

## Resolved Choices

- Vertex-to-document mapping uses the current grid `transform.matrix` and document-center origin convention, with regression coverage for translated GridDeformers.
- Direct `GridDeformer.name` matching is enabled by default and exposed as an option.
- Manual remapping is included in the import window as session-local `Auto` / `Ignore` / explicit `GridDeformer` selection.
- Named convolution presets remain available, and custom-radius variants are available for stronger smoothing or local front/back selection.
