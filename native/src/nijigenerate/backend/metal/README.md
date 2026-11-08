# macOS Metal renderer

The native editor defaults to Metal on macOS and Vulkan on other platforms.
Select explicitly when reusing an existing CMake cache:

```sh
cmake --preset macos-local -DNIJIGENERATE_RENDERER=METAL
cmake --build --preset macos-release
```

`NIJIGENERATE_RENDERER=VULKAN` retains the Vulkan implementation. The Metal
configuration does not find, link or package Vulkan/MoltenVK. It uses SDL's Metal
view, a CAMetalLayer, the pinned ImGui Metal renderer, and Metal command buffers.

## Rendering

Model albedo, emission and bump use three color attachments in a single geometry
pass. Apple GPUs supporting Apple GPU family 4 or later use framebuffer fetch
with raster order group 0 for advanced color blending. No background copy is
required for those blend operations, including overlapping triangles. Other
Metal devices use a pre-draw snapshot for the same portable blend equations.
Lighting still takes a separate color snapshot to avoid sampling its own render
target. Texture filtering, mipmaps, stencil masks, DynamicComposites, editor
mesh/point/line overlays, depth previews, texture readback and atlas drawing are
implemented through the same runtime resource and packet contracts as Vulkan.

DepthBone, depth sampling and difference reduction have Metal compute pipelines.
Input and result buffers use shared storage; asynchronous jobs retain their
resources through GPU completion. The editor's texture IDs remain opaque integer
keys and are translated to MTLTexture objects only when submitting ImGui draws.

Set `NIJIGENERATE_METAL_PROFILE=1` to log draw/pass counts and whether ordered
framebuffer fetch is active every 120 backend submissions. A separate completed
command-buffer timestamp reports `frame_gpu_ms`, including the editor UI (unlike
the Vulkan backend-only timestamps). This does not measure whole-device GPU
utilization. Profiling is disabled by default.

## Shader provenance

`render.metal` adapts the editor's Vulkan GLSL shading and portable blend
functions. `depth_bone.metal`, `depth_sample.metal` and `difference.metal` are
translations of the matching Vulkan compute shaders using SPIRV-Cross from
Vulkan SDK 1.4.328.1, with the following command options:

```sh
glslc input.comp -o input.spv
spirv-cross input.spv --msl --msl-version 20300 --msl-decoration-binding --output output.metal
```

The generated dispatch-constant buffer binding is moved to 6 for the depth
kernels and 3 for difference reduction to avoid collision with storage buffers.
Numerical kernels and row-major matrix contracts are preserved. Generated MSL
is checked in: building Metal requires neither glslc nor SPIRV-Cross. The project
and adapted shaders retain their existing BSD-2-Clause licenses.
