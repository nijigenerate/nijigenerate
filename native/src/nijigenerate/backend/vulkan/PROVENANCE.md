# Vulkan renderer provenance

Adapted from `nijiexpose/native/src/render/vulkan` and its GLSL assets,
which translate `nijimi/source/render/vulkan/vulkan_backend.d`.
The source projects use BSD-2-Clause; see their LICENSE files.

The editor integration preserves its pinned ImGui version and nicxlive source
resource contracts. Desktop transparency and Spout presentation are not editor
features and are not included in this renderer.

Editor-specific changes add source texture ownership and readback, mipmaps,
mesh overlays, depth targets, compute deformation/sampling, difference reduction,
the portable advanced blend equations, bump output, and lighting/bloom.
The original GLSL numerical kernels are adapted from the editor's depth shaders.
The UI renderer uses the pinned ImGui Vulkan implementation with a separate
premultiplied pipeline for the viewport image.

## GPU work reduction

Devices with independent attachment blending and without native advanced blend
use one MRT geometry pass for albedo, emission and bump. The separate-pass path
remains available for devices with restricted advanced-blend attachments.
Multiply and Screen reproduce the portable reference equation with fixed
blending when triangles do not overlap. Folded meshes keep the snapshot path;
background copies cover only their conservative pixel bounds. Emission alpha
and bump output remain intact, including when no emission texture is present.

Set `NIJIGENERATE_VULKAN_PROFILE=1` to log GPU timestamp duration for backend
commands, actual draw/pass counts and background-copy bytes every 120 records.
This excludes ImGui/presentation. Timestamp queries are disabled by default.
`NIJIGENERATE_VULKAN_REFERENCE=1` selects the previous separate-pass/full-copy
path for sequential visual and performance comparisons using the same binary.
