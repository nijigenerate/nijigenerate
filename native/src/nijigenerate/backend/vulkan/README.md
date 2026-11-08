# Vulkan GPU work reduction

Complete recorded scenes propagate texture-output dependencies through their
composites. Targets whose consumers only use albedo render one color attachment
instead of three. When a side plane is needed, the existing MRT or separate-plane
path generates all available planes. Lighting therefore retains emission, and
partial scenes conservatively retain all planes.

The last complete frame retains its CPU geometry and draw packets. Access to an
omitted emission/bump plane replays that frame with all outputs enabled. Texture
mutations first materialize omitted outputs while the old inputs are available.
Replay uses separate descriptor pools and geometry buffers so it cannot reset
resources referenced by already recorded commands. Synchronous materialization
completes offscreen commands before returning; it is not part of normal albedo
display. Replay resources retire after the frame fence completes.

Compatible render-pass variants discard attachment loads when the first packet
clears the target, and omit depth/stencil stores when no later segment needs
them. Formats, subpass references and advanced-blend dependencies remain the
same. Native EXT advanced blending and the portable fallback retain their
existing selection and ordering rules. No Metal-specific memoryless feature is
assumed. Fully transparent source textures and wholly offscreen geometry are
culled conservatively.

`Wrapping::Clamp` uses transparent-black `CLAMP_TO_BORDER`, matching OpenGL's
cropped Part boundary behavior. Repeat and mirrored repeat are unchanged.

## Comparing performance

Build with `-DNIJIGENERATE_RENDERER=VULKAN`. Set
`NIJIGENERATE_VULKAN_PROFILE=1` for backend GPU timestamps and draw/pass counts.
`color_store_MiB` counts configured color attachment store areas, not measured
memory bandwidth. Backend timestamps exclude ImGui and presentation.

`NIJIGENERATE_VULKAN_GPU_REFERENCE=1` disables this output pruning, culling and
load/store optimization for same-binary comparisons. It keeps earlier Vulkan
optimizations enabled; the older `NIJIGENERATE_VULKAN_REFERENCE` switch also
disables those earlier optimizations and is not an isolated baseline for this
change. Transparent-border correctness applies to both paths.

Run each variant sequentially with matching model, viewport, physics and
postprocess settings. Use validation layers for correctness checks, then disable
them for utilization measurements. macOS measurements exercise MoltenVK and do
not establish Windows or native Vulkan driver performance.
