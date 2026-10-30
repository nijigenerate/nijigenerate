// Private implementation of the original regression module.
#include <nijigenerate/compat/phobos_socket.hpp>
#include <nijigenerate_tests/regression.hpp>
#include <nijigenerate/actions/deformable.hpp>
#include <nijigenerate/actions/node.hpp>
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/actions/binding.hpp>
#include <nijigenerate/actions/depthbone.hpp>
#include <nijigenerate/actions/camera.hpp>
#include <nijigenerate/io/inpexport.hpp>
#include <nijigenerate/io/depthmap_psd.hpp>
#include <nijigenerate/io/psdbudget.hpp>
#include <nijigenerate/io/psd/layer.hpp>
#include <nijigenerate/io/depthsample.hpp>
#include <nijigenerate/commands/depth/map_composition.hpp>
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/panels/inspector/depthbone.hpp>
#include <nijigenerate/core/dpi.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/commands/viewport/control_commands.hpp>
#include <nijigenerate/commands/puppet/view_simple_commands.hpp>
#include <nijigenerate/commands/view/panel.hpp>
#include <nijigenerate/viewport/depth/viewport.hpp>
#include <nijigenerate/windows/base.hpp>
#include <nijigenerate/commands/inspector/drawable_commands.hpp>
#include <nijigenerate/commands/inspector/composite_commands.hpp>
#include <nijigenerate/commands/inspector/camera_command.hpp>
#include <nijigenerate/actions/asyncprogress.hpp>
#include <nijigenerate/commands/depth/bone_gpu_async.hpp>
#include <nijigenerate/commands/depth/bone_packet_build.hpp>
#include <nijigenerate/commands/depth/bone_effective_pivots.hpp>
#include <nijigenerate/commands/depth/bone_gpu_queries.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/ext/nodes/exdepthops.hpp>
#include <atomic>
#include <cstring>
#include "detail/root_existing_depth_gpu.hpp"
#include "detail/root_existing_depthdraw_gpu.hpp"
#include "detail/root_meshgroup_compat.hpp"
#include "detail/root_source_command_inventory.hpp"
#include "detail/root_depthdraw_fixture.hpp"
#include "detail/root_depthdraw_data_model.hpp"
#include "detail/root_depthdraw_artifact.hpp"
#include "detail/root_coincident_welding.hpp"
#include "detail/root_depthbone_composite.hpp"
#include "detail/root_depth_image_facade.hpp"
#include "detail/root_node_move_world.hpp"
#include <runtime_state.hpp>
#include <render/backend_queue.hpp>
#include <render.hpp>
#include <nijigenerate/viewport/vertex/mesheditor/deformable.hpp>
#include <nijigenerate/viewport/vertex/mesheditor/editor.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/grid.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/bezierdeform.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/select.hpp>
#include <nijigenerate/commands/vertex/define_mesh.hpp>
#include <nijigenerate/viewport/vertex/automesh/alpha_provider.hpp>
#include <nijigenerate/viewport/vertex/automesh/common.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/viewport/depth/draw/fitgap.hpp>
#include <nijigenerate/viewport/depth/draw/layer.hpp>
#include <nijigenerate/core/math/triangle.hpp>
#include <nijigenerate/ext/nodes/exgriddeformer_node.hpp>
#include <nijigenerate/commands/depth/map_commands.hpp>
#include <nijigenerate/viewport/common/mesheditor/operations/node.hpp>
#include <nijigenerate/viewport/vertex/mesheditor/drawable.hpp>
#include <nijigenerate/commands/node/simplephysics_parameter_commands.hpp>
#include <nijigenerate/commands/node/simplephysics_config_commands.hpp>
#include <nijigenerate/commands/node/simplephysics_float_commands.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/onetimedeform.hpp>
#include <nijigenerate/windows/command_browser.hpp>
#include <nijigenerate/compat/unicode_strip.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/viewport/depth/common/targetview.hpp>
#include <nijigenerate/viewport/depth/common/session.hpp>
#include <nijigenerate/viewport/depth/mesheditor/editor.hpp>
#include <nijigenerate/viewport/depth/renderer.hpp>
#include <nijigenerate/compat/source_image_slice_view.hpp>
#include <nijigenerate/core/math/path.hpp>
#include <nijigenerate/core/math/skeletonize.hpp>
#include <nijigenerate/core/cv/image.hpp>
#include <nijigenerate/core/cv/distancetransform.hpp>
#include <nijigenerate/core/cv/contours.hpp>
#include <nijigenerate/commands/node/base.hpp>
#include <nijigenerate/commands/depth/psd_dialog_commands.hpp>
#include <nijigenerate/commands/node/dynamic.hpp>
#include <nijigenerate/commands/depth/map.hpp>
#include <nijigenerate/viewport/depth/draw/session.hpp>
#include <nijigenerate/viewport/depth/draw/composer.hpp>
#include <nijigenerate/viewport/depth/draw/coordinate.hpp>
#include <nijigenerate/viewport/depth/draw/package.hpp>
#include <nijigenerate/commands/inspector/meshgroup_commands.hpp>
#include <nijigenerate/commands/inspector/griddeform_commands.hpp>
#include <nijigenerate/commands/inspector/path_commands.hpp>
#include <nodes/deformer/drivers/phys.hpp>
#include <nijigenerate/commands/puppet/edit.hpp>
#include <nijigenerate/windows/psddepthmap.hpp>
#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <nijigenerate/autorig/deterministic/evidence.hpp>
#include <nodes/composite.hpp>
#include <nodes/dynamic_composite.hpp>
#include <nijigenerate/commands/package.hpp>
#include <nijigenerate/commands/binding/binding_commands.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/platform/detail/path_allocation.hpp>
#include <nijigenerate/commands/node/clipboard_state.hpp>
#include <nijigenerate/windows/paramsplit.hpp>
#include <nijigenerate/compat/phobos_relative_path.hpp>
#include <timing.hpp>
#include "nijigenerate/compat/phobos_base64.hpp"
#include "detail/report_regex_unicode.hpp"
#include "detail/stdio_text_writer.hpp"
#include "detail/uint_associative_array.hpp"
#include "detail/regression_commands.hpp"
#include "detail/regression_internal.hpp"
#include "detail/root_simplephysics_composite.hpp"
#include "detail/root_viewport_settings_composite.hpp"
#include "detail/root_node_composite.hpp"
#include "detail/root_parameter_composite.hpp"
#include "detail/root_render_composite.hpp"
#include "detail/root_deform_composite.hpp"
#include "detail/root_image_merge_composite.hpp"
#include "detail/root_part_composite.hpp"
#include "detail/root_mesh_composite.hpp"
#include "detail/root_undo_composite.hpp"
#include "detail/root_atlas_texture_slots.hpp"
#include "detail/root_texture_reload.hpp"
#include "detail/root_prepared_welding.hpp"
#include "detail/source_strings.hpp"
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/api/mcp/task/task.hpp>
#include <nijigenerate/api/mcp/auth.hpp>
#include <nijigenerate/api/mcp/http_transport.hpp>
#include <nijigenerate/api/mcp/resource_listing.hpp>
#include <nijigenerate/api/mcp/helpers.hpp>
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/core/asyncderivedupdate.hpp>
#include <nijigenerate/core/shortcut/base.hpp>
#include <nijigenerate/core/shortcut/defaults.hpp>
#include <nijigenerate/core/input_shortcut_template.hpp>
#include <nijigenerate/commands/puppet/edit_table.hpp>
#include <nijigenerate/commands/viewport/palette_table.hpp>
#include <nijigenerate/commands/viewport/palette.hpp>
#include <nijigenerate/core/selector/query.hpp>
#include <nijigenerate/core/selector/treestore.hpp>
#include <nijigenerate/commands/node/simplephysics_complete.hpp>
#include <nijigenerate/commands/viewport/control_pair_commands.hpp>
#include <nijigenerate/commands/automesh/config.hpp>
#include <nijigenerate/commands/automesh/dynamic.hpp>
#include <nijigenerate/commands/depth/psd_dialog_scope.hpp>
#include <nijigenerate/viewport/vertex/automesh/meta_reflect.hpp>
#include <nijigenerate/commands/puppet/file_commands.hpp>
#include <nijigenerate/io/project_reader.hpp>
#include <nijigenerate/io/project_writer.hpp>
#include <nijigenerate/io/save.hpp>
#include <nijigenerate/io/autosave.hpp>
#include <nijigenerate/commands/puppet/tool.hpp>
#include <nijigenerate/compat/phobos_set_extension.hpp>
#include <puppet_ext.hpp>
#include <nijigenerate/commands/parameter/animedit_commands.hpp>
#include <nijigenerate/commands/parameter/prop_commands.hpp>
#include <nijigenerate/commands/parameter/group_commands.hpp>
#include <nijigenerate/commands/binding/base.hpp>
#include <nijigenerate/panels/parameters.hpp>
#include <nijigenerate/commands/depth/bone_commands.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/ext/nodes/exdepthrig_create.hpp>
#include <nijigenerate/viewport/model/onionslice.hpp>
#include <nijigenerate/viewport/model/depthboneoverlay.hpp>
#include <nijigenerate/viewport/common/transformhandle.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/ext/nodes/excamera_math.hpp>
#include <nijigenerate/commands/inspector/part_commands.hpp>
#include <nijigenerate/commands/inspector/node_commands.hpp>
#include <nijigenerate/commands/node/conversion_map.hpp>
#include <nijigenerate/panels/inspector/common.hpp>
#include <nijigenerate/panels/inspector/part.hpp>
#include <nijigenerate/ext/nodes/excamera.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/commands/model/set_deform_binding_commands.hpp>
#include <animation/animation.hpp>
#include <animation/player.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/core/selector/phobos_sort.hpp>
#include <nijigenerate/viewport/vertex/package.hpp>
#include <nijigenerate/viewport/vertex/automesh/automesh.hpp>
#include <nijigenerate/utils/crashdump.hpp>
#include <nijigenerate/compat/phobos_file_rmdir.hpp>
#include <nijigenerate/compat/phobos_file_directory_range.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/platform/autosave_files.hpp>
#include <nijigenerate_tests/welding.hpp>
#include <chrono>
#include <new>
#include <tuple>
#include <string_view>
#include <array>
#include <concepts>
#include <functional>
#include <type_traits>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/retained_string_map.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/actions/depthbone_mutation_hook.hpp>
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/commands/depth/detail/gpu_simple_hook_types.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/math/mesh_vertices.hpp>
#include <nijigenerate/viewport/common/spline.hpp>
#include <nijigenerate/core/colorbleed.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/atlas/packer.hpp>
#include <nijigenerate/viewport/depth/camera.hpp>
#include <nijigenerate/viewport/depth/tools/operation.hpp>
#include <nodes/mask.hpp>
#include <nodes/composite.hpp>
#include <nodes/simple_physics_driver.hpp>
#include <nodes/mesh_group.hpp>
#include <nodes/dynamic_composite.hpp>
#include <nodes/path_deformer.hpp>
#include <nijigenerate/ver.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/parameter/param_commands.hpp>
#include <nijigenerate/commands/parameter/paramedit_commands.hpp>
#include <nijigenerate/api/mcp/server.hpp>
#include <nijigenerate/api/acp/protocol.hpp>
#include <nijigenerate/api/acp/types.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/commands/depth/bone_status.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/io/depthimage.hpp>
#include <nijigenerate/viewport/depth/draw/binding.hpp>
#include <nijigenerate/compat/utf8.hpp>
#include <nijigenerate/compat/phobos_file_write.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <nijigenerate/compat/phobos_temp_directory.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_execute_shell.hpp>
#include <nijigenerate/platform/environment.hpp>
#include <nijigenerate/core/path.hpp>
#include <math/mat4.hpp>
#include <math/veca.hpp>
#include <span>
#include <bit>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <limits>
#include <nijigenerate/compat/unicode_strip.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/ext/nodes/exgriddeformer_node.hpp>
#include <common/shared_array.hpp>
#include <param/binding.hpp>

namespace nijigenerate_tests::regression {
namespace detail {
struct Scenario final {
    nijigenerate::compat::RetainedString id;
    nijigenerate::compat::RetainedString category;
    nijigenerate::compat::RetainedString title;
    nijigenerate::compat::RetainedString status;
    nijigenerate::compat::RetainedString note;
    friend bool operator==(const Scenario&, const Scenario&) = default;
};
struct CompositeGridCase final {
    nijigenerate::compat::RetainedString label;
    nijigenerate::compat::Slice<float> axisX;
    nijigenerate::compat::Slice<float> axisY;
};
constexpr std::string_view automated = "automated";
constexpr std::string_view computerUse = "computer-use";
constexpr std::string_view pending = "pending";

using StaticLiteral = nijigenerate::compat::RetainedString::StaticLiteralTag;

constinit const std::array<Scenario, 328> scenarios{{
    {
        {StaticLiteral{}, "coverage.source-command-inventory"},
        {StaticLiteral{}, "Coverage Audit"},
        {StaticLiteral{}, "Source modules, major subsystems, and command entrypoints are represented in the regression catalog"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "This is an accounting guard, not behavioral coverage; it fails when a source subsystem or command family has no catalog owner."},
    },
    {
        {StaticLiteral{}, "coverage.command-base"},
        {StaticLiteral{}, "Coverage Audit"},
        {StaticLiteral{}, "Base command contracts, command metadata, categories, shortcut labels, and command result behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers base CommandResult payloads, Context masks, ExCommand labels/args, and command metadata reflection."},
    },
    {
        {StaticLiteral{}, "coverage.full-feature-scenario-inventory"},
        {StaticLiteral{}, "Coverage Audit"},
        {StaticLiteral{}, "Every command, window, panel, inspector, mesh tool, and depth tool class has a regression scenario owner"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Builds a source-derived feature inventory so newly added user-facing feature entrypoints cannot remain uncataloged."},
    },
    {
        {StaticLiteral{}, "coverage.source-module-scenario-inventory"},
        {StaticLiteral{}, "Coverage Audit"},
        {StaticLiteral{}, "Every source module is assigned to a regression scenario family"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Builds a module-to-scenario map for the whole source tree; this is the baseline scenario catalog before individual behavioral tests are filled in."},
    },
    {
        {StaticLiteral{}, "coverage.composite-workflow-inventory"},
        {StaticLiteral{}, "Coverage Audit"},
        {StaticLiteral{}, "Major feature families have cross-boundary regression scenarios for command, mode, history, serialization, and MCP interactions"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Requires representative composite workflow owners so command-only tests cannot be mistaken for end-to-end coverage."},
    },
    {
        {StaticLiteral{}, "coverage.composite-case-matrix"},
        {StaticLiteral{}, "Coverage Audit"},
        {StaticLiteral{}, "Composite workflows execute a broad table-driven matrix instead of only hand-picked examples"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Runs mode, node type, parameter, depth, physics, and AutoMesh combinations and fails when the executed matrix is too small."},
    },
    {
        {StaticLiteral{}, "mesh.composite-grid-mode-matrix"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "DefineGrid runs across ModelEdit, AnimEdit, VertexEdit, DepthEdit and multiple grid shapes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Expands the matrix into a direct scenario for mode-specific grid/history behavior."},
    },
    {
        {StaticLiteral{}, "node.composite-type-matrix"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Every registered node menu type can be created, renamed, undone, and redone"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Expands the matrix into a direct scenario for node type command coverage."},
    },
    {
        {StaticLiteral{}, "parameter.composite-preset-matrix"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "1D and 2D parameter presets across ranges preserve bindings through rename undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Expands the matrix into a direct scenario for parameter preset and binding combinations."},
    },
    {
        {StaticLiteral{}, "depth.composite-operation-matrix"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Attached-point, ring, and plane depth operations are undoable after depth initialization"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Expands the matrix into a direct scenario for depth operation combinations."},
    },
    {
        {StaticLiteral{}, "depth.common-target-view"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Shared depth target view preserves GridDeformer working-depth contracts"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers the non-rendering common DepthTargetView foundation used by DepthEdit and DepthDraw."},
    },
    {
        {StaticLiteral{}, "depth.edit-live-ui-smoke"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "DepthEdit live UI smoke for direct, attached point, ring, and plane tools"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs live render/input verification for direct manipulation tools after common depth view extraction."},
    },
    {
        {StaticLiteral{}, "automesh.composite-processor-matrix-scenario"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "Grid, contour, skeleton, and optimum AutoMesh processors produce valid meshes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Expands the matrix into a direct scenario for all standard AutoMesh processors."},
    },
    {
        {StaticLiteral{}, "simplephysics.composite-settings-matrix"},
        {StaticLiteral{}, "SimplePhysics"},
        {StaticLiteral{}, "SimplePhysics model, map, local, gravity, length, frequency, and damping settings undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Expands the matrix into a direct scenario for SimplePhysics setting combinations."},
    },
    {
        {StaticLiteral{}, "project.new-open-save"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "New, open, save, save-as, close, dirty state, and recent-file behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers headless-safe save/open command paths, file creation, dirty-state clearing, and model round-trip."},
    },
    {
        {StaticLiteral{}, "project.memory-restore"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Native memory snapshot rollback preserves dirty state and project identity"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers dirty and clean memory snapshot restores without changing the project path."},
    },
    {
        {StaticLiteral{}, "project.composite-command-roundtrip"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Command-created nodes, parameters, bindings, mesh/deformer state, save, reopen, and resource-visible type preservation"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers a cross-feature command workflow persisted through native INX and reloaded through OpenFileCommand."},
    },
    {
        {StaticLiteral{}, "project.composite-image-merge-export"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Generated image import, merge, INP export/import, and native reopen preserve parts and parameters"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers image-backed project creation followed by merge/export/reimport transitions."},
    },
    {
        {StaticLiteral{}, "project.file-dialogs"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Open, save, save-as, import, merge, and export dialog command entrypoints"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI dialog smoke with cancellable and accepted paths."},
    },
    {
        {StaticLiteral{}, "project.close-dirty-prompts"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Close project, unsaved-change prompts, cancel, save, and discard paths"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI prompt smoke."},
    },
    {
        {StaticLiteral{}, "project.recent-files"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Recent-file insertion, duplicate removal, missing-file cleanup, and menu display"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers settings-backed recent project insertion, duplicate promotion, and list length pruning."},
    },
    {
        {StaticLiteral{}, "project.autosave-recovery"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Autosave, recovery, lockfile, recovery rejection, and stale-record cleanup paths"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers autosave file creation, lockfile state, recovery record creation, and stale-record pruning; restart dialog rejection uses computer-use."},
    },
    {
        {StaticLiteral{}, "project.import-psd"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "PSD import, layer grouping, node-type mapping, clipping, opacity, blend, and texture placement"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated PSD fixture reader and import command path; rich layered PSD golden coverage remains asset-dependent."},
    },
    {
        {StaticLiteral{}, "project.import-psd-depth-map"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "PSD depth map import maps grayscale layer depth to GridDeformer depth arrays"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers PSD depth sampling helpers, target GridDeformer resolution, and command registration; full layered PSD golden coverage remains asset-dependent."},
    },
    {
        {StaticLiteral{}, "project.depthdraw-compatibility"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "DepthDraw calculation entrypoints remain compatible with depth-draw fixture expectations"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers the neutral depth image sampling facade and the future depth-draw golden fixture runner."},
    },
    {
        {StaticLiteral{}, "project.depthdraw-data-model"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "DepthDraw layer, binding, and session state preserve PSD/PNG source metadata and target links"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers the initial DepthDraw data model before compositor and viewport integration."},
    },
    {
        {StaticLiteral{}, "project.depthdraw-composer"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "DepthDraw compositor, GPU preview boundary, and viewport depth-space geometry map image layers to depth-mapped deformer arrays"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers GridDeformer and PathDeformer target vertex sampling, layer ordering, merge policy, missing samples, preview working-depth updates, GPU-style readback, depth-space summary, and viewport render geometry."},
    },
    {
        {StaticLiteral{}, "project.depthdraw-fit-z"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "DepthDraw Fit Z to Gap computes layer z scale and offset without changing source pixels"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers raw range measurement, manual gap fitting, margins, and invalid range rejection."},
    },
    {
        {StaticLiteral{}, "project.psd-depth-map-import-ui-smoke"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "PSD Depth Map Import live UI smoke still opens and previews existing import behavior"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs live dialog verification for the legacy PSD depth map importer after DepthDraw refactoring."},
    },
    {
        {StaticLiteral{}, "project.import-kra"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "KRA import, layer grouping, node-type mapping, clipping, opacity, blend, and texture placement"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated KRA fixture reader and import command path; rich layered KRA golden coverage remains asset-dependent."},
    },
    {
        {StaticLiteral{}, "project.import-inp"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "INP import and compatibility with exported files"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers command-level import and merge of generated INP fixtures with nodes, parameters, and bindings."},
    },
    {
        {StaticLiteral{}, "project.import-images"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Image and image-folder import into Parts"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated PNG fixtures through image-folder import and merge-image-files command paths."},
    },
    {
        {StaticLiteral{}, "project.merge-psd"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Merge PSD into existing model while preserving existing nodes and bindings"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated PSD fixture merge command preserving existing model state."},
    },
    {
        {StaticLiteral{}, "project.merge-kra"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Merge KRA into existing model while preserving existing nodes and bindings"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated KRA fixture merge command preserving existing model state."},
    },
    {
        {StaticLiteral{}, "project.merge-inp"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Merge INP into an existing model"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers INP merge command with generated fixture."},
    },
    {
        {StaticLiteral{}, "project.merge-images"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Merge individual image files into an existing model"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered with generated PNG fixtures."},
    },
    {
        {StaticLiteral{}, "project.export-inp"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "INP export including exclusion of editor-only nodes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers export pruning of DepthRigRoot/DepthBone plus their bindings; full file serialization still needs a stable fixture."},
    },
    {
        {StaticLiteral{}, "project.export-png"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "PNG export with transparency, bounds, scale, and selected output path"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs render verification."},
    },
    {
        {StaticLiteral{}, "project.export-jpeg"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "JPEG export with background handling and selected output path"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs render verification."},
    },
    {
        {StaticLiteral{}, "project.export-tga"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "TGA export with alpha and selected output path"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs render verification."},
    },
    {
        {StaticLiteral{}, "project.export-screenshot"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Viewport screenshot save and capture-live screenshot command paths"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs viewport render verification."},
    },
    {
        {StaticLiteral{}, "project.export-video"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Video export dialog, encoder settings, frame range, and output file"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs render/video fixture."},
    },
    {
        {StaticLiteral{}, "project.session-import"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Import session data into the current project"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated INP session extData import and missing-session rejection."},
    },
    {
        {StaticLiteral{}, "project.texture-maintenance"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Premultiply texture, rebleed texture, regenerate mipmaps, and atlas maintenance commands"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers texture maintenance commands on a generated texture-backed part and drains rebleed task queue."},
    },
    {
        {StaticLiteral{}, "project.repair-maintenance"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Attempt repair, regenerate node IDs, fake layer names, and corrupted-project recovery commands"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers repair command entrypoints, fake layer-path generation, and UUID regeneration on a generated puppet."},
    },
    {
        {StaticLiteral{}, "io.serialization-inx"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Native INX save/load serialization of nodes, parameters, bindings, mesh, and metadata"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated native INX round-trip for node transforms, mesh data, parameters, and value bindings."},
    },
    {
        {StaticLiteral{}, "io.camera-viewport-roundtrip"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Camera viewport survives native INX save/load"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers Camera viewport persistence through child-node save hooks."},
    },
    {
        {StaticLiteral{}, "io.composite-import-export-roundtrip"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Image import, native save, INP export/import, and binding reconnection in one workflow"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers cross-format IO workflow instead of isolated codec checks."},
    },
    {
        {StaticLiteral{}, "io.serialization-textures"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Native project texture file paths, texture slots, atlas references, and missing-texture recovery"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated texture-backed INX save/load and restored Part texture slot dimensions."},
    },
    {
        {StaticLiteral{}, "io.serialization-inp"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "INP import/export serialization including external compatibility and editor-only pruning"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered in part by project.import-inp and project.export-inp."},
    },
    {
        {StaticLiteral{}, "io.save-native"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Native save path, backup overwrite, dirty-state reset, and format compatibility"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers native save extension handling, overwrite, swap cleanup, project path state, dirty reset, and reload."},
    },
    {
        {StaticLiteral{}, "io.inimport-model"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "INP importer model merge, UUID remap, texture remap, and binding remap"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated INP import/merge with nodes, textures, parameters, and binding remap."},
    },
    {
        {StaticLiteral{}, "io.inpexport-model"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "INP exporter node filtering, texture packaging, binding export, and external compatibility"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated INP export plus reimport and editor-only node pruning."},
    },
    {
        {StaticLiteral{}, "io.psd-reader"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "PSD reader layer tree, masks, blend modes, opacity, clipping, and image extraction"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated PSD fixture parse and import handoff; layered mask golden coverage remains asset-dependent."},
    },
    {
        {StaticLiteral{}, "io.kra-reader"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "KRA reader layer tree, masks, blend modes, opacity, clipping, and image extraction"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated KRA fixture parse and import handoff; layered mask golden coverage remains asset-dependent."},
    },
    {
        {StaticLiteral{}, "io.image-export"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Image export renderer path for PNG, JPEG, TGA, alpha, scale, and bounds"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs render snapshot fixture."},
    },
    {
        {StaticLiteral{}, "io.video-export"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Video export frame stepping, encoder invocation, cancellation, and output validation"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs video export fixture."},
    },
    {
        {StaticLiteral{}, "io.image-codecs"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Image loading/saving through PNG, JPEG, TGA, PSD, and KRA code paths"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated PNG/JPEG/TGA texture codec round-trips and keeps PSD/KRA reader coverage in import scenarios."},
    },
    {
        {StaticLiteral{}, "io.video-codecs"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Video export frame generation, encoder invocation, cancellation, and error paths"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs video export fixture."},
    },
    {
        {StaticLiteral{}, "atlas.pack"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Atlas packer rectangle placement, texture upload metadata, invalidation, and repack stability"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers headless max-rect texture packer placement, non-overlap, removal, and clear behavior; GL atlas rendering remains render.atlas-packer."},
    },
    {
        {StaticLiteral{}, "atlas.color-bleed"},
        {StaticLiteral{}, "Project/File"},
        {StaticLiteral{}, "Color bleed and rebleed operations preserve alpha edges and atlas consistency"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated transparent-edge texture color bleeding and alpha preservation."},
    },
    {
        {StaticLiteral{}, "node.create-delete-undo"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Create, delete, undo, redo, copy, cut, paste, duplicate"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Create/delete/toggle through commands are covered; copy/cut/paste/duplicate still need deeper fixture coverage."},
    },
    {
        {StaticLiteral{}, "node.add-types"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Add Node, Part, Composite, DynamicComposite, MeshGroup, GridDeformer, PathDeformer, Camera, SimplePhysics, DepthRigRoot, and DepthBone nodes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers dynamic AddNode command creation for every registered node-menu type with undo/redo."},
    },
    {
        {StaticLiteral{}, "node.insert-types"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Insert supported node types before, after, and inside selected nodes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers dynamic InsertNode command creation for every registered node-menu type with undo/redo."},
    },
    {
        {StaticLiteral{}, "node.cut-copy-paste-duplicate"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Cut, copy, paste, duplicate, UUID regeneration, texture references, and binding references"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers node clipboard copy/paste, duplicate UUID regeneration, undo/redo, and Cut remaining disabled until implemented."},
    },
    {
        {StaticLiteral{}, "node.reparent-order"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Move, reorder, reparent, and preserve transforms"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Move/reparent through command is covered; transform preservation needs deeper fixture coverage."},
    },
    {
        {StaticLiteral{}, "node.rename-undo-merge"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Node name edit merges per text-edit session and remains undoable"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered by node-name-undo-merge."},
    },
    {
        {StaticLiteral{}, "node.transform-inspector"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Translation, rotation, scale, sort, lock-to-root, pin-to-parent, snapping"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers inspector apply commands for translation, rotation, scale, z-sort, and lock-to-root with undo/redo."},
    },
    {
        {StaticLiteral{}, "node.visibility-lock"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Visibility, lock, selection, multi-selection, and tree filtering"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI interaction smoke."},
    },
    {
        {StaticLiteral{}, "node.convert-reload"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Reload texture and convert node operations"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "ConvertToCommand is covered with undo/redo; reload side effects still need deeper fixture coverage."},
    },
    {
        {StaticLiteral{}, "node.centralize"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Centralize selected node pivots and undo/redo the resulting transform changes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers CentralizeNodeCommand with undo/redo for parent and child local transforms."},
    },
    {
        {StaticLiteral{}, "node.type-conversion"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Convert node type between compatible registered node classes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers every conversionMap pair through ConvertToCommand with undo/redo and type preservation."},
    },
    {
        {StaticLiteral{}, "node.composite-hierarchy-binding-roundtrip"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Create, rename, move, bind, undo/redo, save, reopen, and keep hierarchy/binding identity"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers node hierarchy edits chained with parameter binding and persistence."},
    },
    {
        {StaticLiteral{}, "node.resource-selector"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Selector parser, query, tree store, and resource selector integration"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers selector query evaluation for nodes, parameters, bindings, direct children, and tree-store hierarchy."},
    },
    {
        {StaticLiteral{}, "core.selector-parser"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Selector tokenizer, parser, query evaluation, and tree-store resource lookup"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers selector grammar through query fixtures and Resource TreeStore construction."},
    },
    {
        {StaticLiteral{}, "core.node-registry"},
        {StaticLiteral{}, "Node Hierarchy"},
        {StaticLiteral{}, "Node class registration, menu descriptors, icons, and dynamic construction"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers menu-visible node type registration, dynamic command creation, stable command ids, and construction."},
    },
    {
        {StaticLiteral{}, "inspectors.node-types"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "Puppet, Node, Part, Drawable, Composite, DynamicComposite, Camera inspectors"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers Node, Part, Drawable, Composite, and Camera inspector command properties with undo/redo; Puppet and DynamicComposite render/UI smoke still need deeper coverage."},
    },
    {
        {StaticLiteral{}, "inspectors.composite-node-part-mesh"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "Node, Part, and mesh deformer inspector changes interleaved with undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers inspector command composition across node, drawable, and deformer targets."},
    },
    {
        {StaticLiteral{}, "inspectors.puppet"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "Puppet metadata, canvas, texture atlas, and project-level inspector controls"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers puppet metadata, physics globals, preserve-pixels render setting, texture slot population, and native save/load round-trip."},
    },
    {
        {StaticLiteral{}, "inspectors.node"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "Node transform, visibility, lock, lockToRoot, pinToParent, and sort controls"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered in part by node.transform-inspector."},
    },
    {
        {StaticLiteral{}, "inspectors.part"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "Part texture, tint, screen tint, emission, opacity, blend, clipping, masks, and welding controls"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered in part by part.texture, part.mask-*, and part.welding."},
    },
    {
        {StaticLiteral{}, "inspectors.drawable"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "Drawable offset, bounds, drawing behavior, and shared drawable controls"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered in part by inspectors.node-types."},
    },
    {
        {StaticLiteral{}, "inspectors.composite"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "Composite and DynamicComposite blending, masks, thresholds, tint, and draw settings"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered in part by inspectors.node-types; dynamic runtime needs render fixture."},
    },
    {
        {StaticLiteral{}, "inspectors.camera"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "Camera viewport and projection inspector controls"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered in part by inspectors.node-types."},
    },
    {
        {StaticLiteral{}, "inspectors.simplephysics"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "SimplePhysics mapping, curve type, parameter assignment, runtime settings, and reset controls"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered in part by simplephysics.*."},
    },
    {
        {StaticLiteral{}, "inspectors.mesh-deformers"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "MeshGroup, GridDeformer, PathDeformer inspector controls"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers MeshGroup, GridDeformer, and PathDeformer inspector command properties with undo/redo."},
    },
    {
        {StaticLiteral{}, "inspectors.depth-bone"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "DepthRigRoot and DepthBone inspector controls"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers DepthBone inspector command paths for rest pose and constraints with undo/redo."},
    },
    {
        {StaticLiteral{}, "inspectors.format-strings"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "Formatted labels substitute values and do not show raw %.0f%% tokens"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers source-level ImGui format string mistakes that render raw percent patterns such as %%0.2f."},
    },
    {
        {StaticLiteral{}, "inspectors.commit-boundaries"},
        {StaticLiteral{}, "Inspectors"},
        {StaticLiteral{}, "Inspector text inputs, drags, toggles, color edits, and drag-drop commit undo actions once per edit"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use UI commit-boundary fixture."},
    },
    {
        {StaticLiteral{}, "part.texture"},
        {StaticLiteral{}, "Part Properties"},
        {StaticLiteral{}, "Texture load, reload, UV, opacity, tint multiply/screen, emission, blend mode"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers Part inspector command state changes for tint, screen tint, emission, opacity, and blend mode with undo/redo; texture reload and UV still need fixture coverage."},
    },
    {
        {StaticLiteral{}, "part.composite-inspector-mesh-roundtrip"},
        {StaticLiteral{}, "Part Properties"},
        {StaticLiteral{}, "Part inspector edits, mesh topology, deformation binding, save, and reopen"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers part property and topology edits chained with binding persistence."},
    },
    {
        {StaticLiteral{}, "part.texture-reload"},
        {StaticLiteral{}, "Part Properties"},
        {StaticLiteral{}, "Reload texture from source path, texture slot update, and missing-source handling"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers file-backed texture replacement on Part texture slots, post-load texture slot repopulation, and missing file rejection with generated PNG fixtures."},
    },
    {
        {StaticLiteral{}, "part.uv-mesh-coherence"},
        {StaticLiteral{}, "Part Properties"},
        {StaticLiteral{}, "Texture UVs, mesh vertices, indices, and deformation array lengths stay coherent"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers DefineMesh/DefineVertices keeping mesh, UV, and deformation array lengths coherent through undo/redo."},
    },
    {
        {StaticLiteral{}, "part.clipping-mask"},
        {StaticLiteral{}, "Part Properties"},
        {StaticLiteral{}, "Clipping and mask threshold behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers Clip/Slice blend modes and mask threshold state changes with undo/redo; pixel render output still needs snapshot coverage."},
    },
    {
        {StaticLiteral{}, "part.mask-add-remove"},
        {StaticLiteral{}, "Part Properties"},
        {StaticLiteral{}, "Mask Source add/remove undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered by mask-source-add-undo-redo."},
    },
    {
        {StaticLiteral{}, "part.mask-reorder"},
        {StaticLiteral{}, "Part Properties"},
        {StaticLiteral{}, "Mask Source reorder preserves exact bindings through undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered by mask-source-reorder-undo-redo."},
    },
    {
        {StaticLiteral{}, "part.mask-mode"},
        {StaticLiteral{}, "Part Properties"},
        {StaticLiteral{}, "Mask Source mode changes are undoable"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered by mask-source-mode-undo-redo."},
    },
    {
        {StaticLiteral{}, "part.welding"},
        {StaticLiteral{}, "Part Properties"},
        {StaticLiteral{}, "Welding add/remove/edit undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered by welding-undo-redo."},
    },
    {
        {StaticLiteral{}, "part.welding-runtime"},
        {StaticLiteral{}, "Part Properties"},
        {StaticLiteral{}, "Welding deformation follows source part and preserves inverse/counter weights"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers welded Drawable post-process deformation blending, counter-link index generation, weights, and filter removal."},
    },
    {
        {StaticLiteral{}, "parameter.lifecycle"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Create, delete, duplicate, rename, move, group, split, range, defaults"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Create/duplicate/delete/rename/group/move/color/delete through commands are covered; split/range/defaults still need deeper fixture coverage."},
    },
    {
        {StaticLiteral{}, "parameter.create-presets"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Create 1D, 2D, mouth, and template parameters with correct ranges and default keypoints"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers command-created 1D, 2D, and mouth parameter presets with ranges and axis points."},
    },
    {
        {StaticLiteral{}, "parameter.groups"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Create, delete, move, recolor, and reorder parameter groups"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered in part by parameter.lifecycle."},
    },
    {
        {StaticLiteral{}, "parameter.split-window"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Parameter split dialog, axis split, and binding migration"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers the shared split implementation used by the dialog, including axis-copy, binding migration, and undo/redo."},
    },
    {
        {StaticLiteral{}, "parameter.copy-paste"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Copy, paste, duplicate, duplicate with flip, and paste with flip parameters"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers parameter clipboard paste, paste-with-flip command path, duplicate, duplicate-with-flip, binding copy, and undo/redo."},
    },
    {
        {StaticLiteral{}, "parameter.link"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Link one parameter axis to another and preserve axis mapping"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers LinkToCommand creating ParameterParameterBinding with min/max axis mapping and undo/redo."},
    },
    {
        {StaticLiteral{}, "parameter.arm-select"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Arm/disarm/select parameter and controller value changes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers arm/disarm plus command-driven keypoint and armed keypoint selection."},
    },
    {
        {StaticLiteral{}, "parameter.starting-keyframe"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Set starting keyframe for parameter preview and editing"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers SetStartingKeyFrameCommand defaults update and undo/redo."},
    },
    {
        {StaticLiteral{}, "parameter.keyframe-basic"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Set, unset, reset, invert, mirror, flip, paste keyframes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Set/unset/reset/invert/mirror are covered on ValueParameterBinding; flip/paste still need deformation/golden fixtures."},
    },
    {
        {StaticLiteral{}, "parameter.keyframe-mirror-fill"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Horizontal, vertical, diagonal, and 1D mirrored auto-fill commands"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers mirror-fill commands for value bindings with source extrapolation, sign flip, undo, and redo."},
    },
    {
        {StaticLiteral{}, "parameter.keyframe-copy-paste"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Copy, paste, remove binding, and interpolation mode commands"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers binding copy/paste, remove binding, interpolation mode, and undo/redo."},
    },
    {
        {StaticLiteral{}, "parameter.keyframe-2d"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "2D keypoint X/Y frame add/remove pair behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers grouped X/Y add and remove undo/redo as one action."},
    },
    {
        {StaticLiteral{}, "parameter.binding-interp"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Binding interpolation and reInterpolate stability"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers 1D and 2D ValueParameterBinding interpolation and reInterpolate recovery after unset."},
    },
    {
        {StaticLiteral{}, "parameter.binding-deform"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "DeformationParameterBinding value length, interpolation, mirror, flip, and symmetrize behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers SetDeformBinding command creation, value length, undo/redo, and binding restoration."},
    },
    {
        {StaticLiteral{}, "parameter.binding-trs"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "TRS binding creation, value setting, interpolation, and target cleanup"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers SetTRSBinding command creation, translation/scale/rotation values, and undo/redo."},
    },
    {
        {StaticLiteral{}, "parameter.binding-model"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "SetDeformBinding and SetTRSBinding model commands wire targets and undo/redo correctly"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers model binding commands against generated Part fixtures."},
    },
    {
        {StaticLiteral{}, "parameter.mesh-binding-composite"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Mesh topology commands and deformation bindings remain coherent across undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers DefineMesh plus SetDeformBinding on the same target, with topology and binding length restored across history operations."},
    },
    {
        {StaticLiteral{}, "parameter.axes-props"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Parameter min/max, axis breakpoint, and binding remap undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers ApplyParameterPropsAxesCommand with bound values."},
    },
    {
        {StaticLiteral{}, "parameter.template-depth-bone"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "DepthBone standard parameter template creation and key values"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers standard DepthBone face/body parameter template creation, binding values, and undo/redo."},
    },
    {
        {StaticLiteral{}, "parameter.controller-widget"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Parameter controller widget drag, snap, armed keypoint update, and view synchronization"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs parameter panel input smoke."},
    },
    {
        {StaticLiteral{}, "parameter.binding-cleanup"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Deleting parameters, nodes, and targets removes stale bindings without dangling references"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers command-level binding removal and parameter deletion undo/redo without dangling active bindings."},
    },
    {
        {StaticLiteral{}, "parameter.composite-lifecycle-binding"},
        {StaticLiteral{}, "Parameters"},
        {StaticLiteral{}, "Create, rename, group, bind, resize axes, duplicate, delete, undo, and redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers parameter lifecycle and binding migration across multiple command families."},
    },
    {
        {StaticLiteral{}, "animation.lifecycle"},
        {StaticLiteral{}, "Animation"},
        {StaticLiteral{}, "Create, rename, delete animations and tracks"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers animation create/update/rename/delete action paths with undo/redo and lane preservation."},
    },
    {
        {StaticLiteral{}, "animation.properties"},
        {StaticLiteral{}, "Animation"},
        {StaticLiteral{}, "Animation name, length, lead-in/out, additive, weight, fps/timestep, and metadata editing"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers animation property create/update/rename undo/redo through animation action paths."},
    },
    {
        {StaticLiteral{}, "animation.keyframes"},
        {StaticLiteral{}, "Animation"},
        {StaticLiteral{}, "Add, edit, remove, copy, paste animation keyframes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers 1D and 2D add/edit/remove keyframe action paths with undo/redo; copy/paste still needs timeline UI/input coverage."},
    },
    {
        {StaticLiteral{}, "animation.timeline-ui"},
        {StaticLiteral{}, "Animation"},
        {StaticLiteral{}, "Timeline track selection, lane expansion, scrub, drag, and keyframe selection"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs timeline input smoke."},
    },
    {
        {StaticLiteral{}, "animation.playback"},
        {StaticLiteral{}, "Animation"},
        {StaticLiteral{}, "Playback, scrubbing, loop, fps, and preview state"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI timing smoke."},
    },
    {
        {StaticLiteral{}, "animation.keyframe-copy-paste"},
        {StaticLiteral{}, "Animation"},
        {StaticLiteral{}, "Animation keyframe clipboard copy, paste, replace, and undo grouping"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use timeline fixture."},
    },
    {
        {StaticLiteral{}, "animation.track-binding-cleanup"},
        {StaticLiteral{}, "Animation"},
        {StaticLiteral{}, "Animation tracks survive node/parameter rename and clean up deleted targets"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers parameter rename stability and deleted target behavior across native save/load."},
    },
    {
        {StaticLiteral{}, "animation.composite-keyframe-history"},
        {StaticLiteral{}, "Animation"},
        {StaticLiteral{}, "Animation creation, 2D keyframes, parameter rename, undo/redo order, and lane references"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers a chained animation workflow so keyframe groups, parameter edits, and redo order stay coherent."},
    },
    {
        {StaticLiteral{}, "api.external-control"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "External API, MCP task queue, command execution, and agent panel integration"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers command result JSON, context payload overrides, resource listing, and queued main-thread dispatch; agent panel UI remains computer-use."},
    },
    {
        {StaticLiteral{}, "api.acp-protocol"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "ACP protocol transport, echo agent, message parsing, cancellation, and error reporting"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers ACP protocol constants, typed payload structs, and JSON-RPC error serialization; process transport remains in ACP stdio/client scenarios."},
    },
    {
        {StaticLiteral{}, "api.mcp-mode-history"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "MCP command execution across ModelEdit, VertexEdit, DepthEdit, and undo/redo history boundaries"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers MCP prepare/finish boundaries with pre-existing root history, active edit scopes, and type-preserving undo/redo."},
    },
    {
        {StaticLiteral{}, "api.command-scope"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "Current command scope, allowed MCP tools, scope stack, and finish/cancel transitions"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers normal and PSD dialog scope discovery plus metadata-driven lifecycle command reporting."},
    },
    {
        {StaticLiteral{}, "api.mcp-server"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "MCP auth, HTTP transport, resource listing, command execution, and queued task dispatch"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers disabled settings application, auth toggle behavior, resource listing, and queued command dispatch; live HTTP request smoke remains separate."},
    },
    {
        {StaticLiteral{}, "api.mcp-auth"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "MCP auth token generation, validation, rejection, and settings persistence"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers approval request data contract and auth UI source contract without blocking headless tests."},
    },
    {
        {StaticLiteral{}, "api.mcp-http-transport"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "MCP HTTP transport request parsing, response streaming, errors, and shutdown"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers transport construction, auth toggles, handler dispatch, close path, and registered route/source contract."},
    },
    {
        {StaticLiteral{}, "api.mcp-resources"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "MCP resource listing, selectors, node resources, parameter resources, and serialization"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers model-derived MCP resource entries, binding resource URIs, resources/list response rewriting, and context override parsing."},
    },
    {
        {StaticLiteral{}, "api.mcp-task-queue"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "MCP queued task dispatch, cancellation, result propagation, and main-thread safety"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers queued action processing, cross-thread ngRunInMainThread result propagation, and exception propagation."},
    },
    {
        {StaticLiteral{}, "api.acp-client"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "ACP client start, request, response, cancellation, stderr, and shutdown behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers ACPClient source contract for process launch, reader threads, cancellation, permission responses, stderr capture, polling, and shutdown paths."},
    },
    {
        {StaticLiteral{}, "api.acp-stdio"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "ACP stdio transport framing, partial reads, malformed input, and process cleanup"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers ACP stdio adapter contract and its delegated transport factory."},
    },
    {
        {StaticLiteral{}, "api.acp-echo-agent"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "ACP echo agent round-trip and error behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers echo-agent build guard, Content-Length framing, initialize handling, and protocol-version response contract."},
    },
    {
        {StaticLiteral{}, "api.agent-panel"},
        {StaticLiteral{}, "API/Agent"},
        {StaticLiteral{}, "Agent panel opt-in state, disabled state, connection errors, and tool output display"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "mesh.vertex-scope"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "VertexEdit enter/exit scope and undo/redo scope guard"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers common ActionStackScope guard semantics for VertexEdit nesting."},
    },
    {
        {StaticLiteral{}, "mesh.select-tool"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Select tool single select, marquee select, lasso select, additive select, and deselect"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.point-tool"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Point tool create, select, drag, delete, snap, and constrained movement"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.connect-tool"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Connect tool edge creation, deletion, duplicate prevention, and topology validity"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.line-tool"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Line tool stroke creation, continuation, cancellation, and topology editing"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.path-tool"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Path tool curve/path creation, point movement, and topology editing"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.edge-cutter"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Edge cutter split, intersection, undo/redo, and invalid-cut rejection"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.lasso-tool"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Lasso selection and lasso-based topology operations"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.brush-tools"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Circle, rectangle, double-threshold, and brush-state behavior"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.grid-tool"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Grid creation, resize, drag, point move, and bake"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.bezier-deform-tool"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "BezierDeform tool creation, editing, apply, cancel, and undo"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.define-grid-command"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Define Grid command updates GridDeformer vertices and undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers headless GridDeformer topology changes through command path."},
    },
    {
        {StaticLiteral{}, "mesh.define-mesh-command"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Define Mesh and Define Vertices commands update mesh topology and undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers DefineMeshCommand and DefineVerticesCommand on drawable and deformable targets with undo/redo."},
    },
    {
        {StaticLiteral{}, "mesh.operations-node"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Node, Drawable, and Deformable mesh editor operations update the correct targets"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers operation-level fixtures for Node hit testing plus Drawable and Deformable target updates through applyToTarget."},
    },
    {
        {StaticLiteral{}, "mesh.multi-object"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Edit multiple objects at once"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers multi-target mesh editor apply grouping and one-step undo/redo."},
    },
    {
        {StaticLiteral{}, "mesh.composite-topology-scope"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Topology edits across Part, GridDeformer, PathDeformer, scopes, undo, and redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers mixed target topology edits under scoped history boundaries."},
    },
    {
        {StaticLiteral{}, "mesh.mirror-symmetry"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Mirror, symmetry, snap, apply, cancel"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers mirror counterpart lookup, mirrored delta application, reset-before-apply cancellation, and undo/redo."},
    },
    {
        {StaticLiteral{}, "core.math-mesh"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Mesh math, triangulation, vertex operations, path math, and spline helpers"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers mesh vertex connection invariants, triangulation invariants, and path extraction fixtures."},
    },
    {
        {StaticLiteral{}, "core.math-triangle"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Triangle predicates, triangulation invariants, winding, and degenerate geometry handling"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers deterministic triangulation bounds, triangle index invariants, and fillPoly rasterization."},
    },
    {
        {StaticLiteral{}, "core.math-path"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Path sampling, closest-point, tangent, subdivision, and spline helper behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers skeleton path extraction for empty, connected, and multi-component fixtures."},
    },
    {
        {StaticLiteral{}, "core.math-skeletonize"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Skeletonization graph construction, pruning, branch selection, and deterministic output"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated bitmap skeletonization and extracted path invariants."},
    },
    {
        {StaticLiteral{}, "mesh.common-operations"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Common mesh helpers update vertices, indices, selection, and deformation arrays consistently"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers MeshVertex connection, duplicate prevention, disconnect, and disconnectAll symmetry."},
    },
    {
        {StaticLiteral{}, "mesh.spline"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Viewport spline editing helpers for handles, tangents, and sampling"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers CatmullSpline interpolation, closest-point lookup, point insertion/removal, and remapped mesh target updates."},
    },
    {
        {StaticLiteral{}, "mesh.operation-node"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Mesh editor node operation hit testing, transform handles, and scoped actions"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers Node operation target setup, hit testing, rectangle filtering, and selection lookup."},
    },
    {
        {StaticLiteral{}, "mesh.operation-drawable"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Mesh editor drawable operation edits mesh vertices and drawable deformation arrays"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers Drawable operation mesh vertex mutation, applyToTarget, undo, and redo."},
    },
    {
        {StaticLiteral{}, "mesh.operation-deformable"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Mesh editor deformable operation edits deformer vertices and offsets"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers Deformable operation vertex mutation, applyToTarget, undo, and redo."},
    },
    {
        {StaticLiteral{}, "mesh.tool-select"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Vertex select tool hit testing, rectangle selection, additive selection, and deselection"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.tool-point"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Vertex point tool drag, move, undo, and initial mode activation"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.tool-connect"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Vertex connect tool creates, removes, and validates mesh edges/triangles"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.tool-lasso"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Lasso selection tool selects, deselects, and handles closed/freeform strokes"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.tool-edge-cutter"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Edge cutter tool splits edges/faces and keeps mesh arrays coherent"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "mesh.tool-brush"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "Brush tool with circle, rectangle, and double-threshold brush state edits vertices predictably"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "core.cv-image-contours"},
        {StaticLiteral{}, "Mesh/Vertex Editor"},
        {StaticLiteral{}, "CV image, contour extraction, distance transform, and skeletonize helpers"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated image buffers, distance transform, contour extraction, hierarchy validation, polygon tests, and contour simplification."},
    },
    {
        {StaticLiteral{}, "deform.path-tool"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "PathDeformer based deformation tool creation, edit, apply, undo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers BezierDeformTool point insertion, point movement, apply to PathDeformer, and undo/redo."},
    },
    {
        {StaticLiteral{}, "deform.grid-tool"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "GridDeformer based deformation tool creation, edit, apply, undo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers GridTool virtual mesh creation, apply to GridDeformer, and undo/redo."},
    },
    {
        {StaticLiteral{}, "deform.pathdeformer-runtime"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "PathDeformer runtime deformation, handles, smoothing, and binding interaction"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers PathDeformer child deformation sampling, length invariants, and unchanged baseline behavior."},
    },
    {
        {StaticLiteral{}, "deform.griddeformer-runtime"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "GridDeformer runtime deformation, grid interpolation, binding interaction, and mesh length invariants"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers GridDeformer child deformation interpolation, length invariants, and unchanged baseline behavior."},
    },
    {
        {StaticLiteral{}, "deform.meshgroup-compat"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "Legacy MeshGroup compatibility and migration behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers GridDeformer-to-MeshGroup migration copy, grid mesh reconstruction, deformation preservation, and conversion map ownership."},
    },
    {
        {StaticLiteral{}, "deform.bezier-tool"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "Bezier deformation tool handles, interpolation, apply, and undo/redo"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "deform.onetime-scope"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "OneTimeDeform Vertex/Deform subtool scope and escape prevention"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers common ActionStackScope guard semantics used by tool subscopes; input transition smoke remains manual."},
    },
    {
        {StaticLiteral{}, "deform.composite-grid-path-binding"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "Grid and Path deformer topology changes with parameter bindings and runtime update"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers deformer command composition beyond a single grid tool path."},
    },
    {
        {StaticLiteral{}, "deform.onetime-vertex-subtool"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "OneTimeDeform Vertex subtool with PathDeformer and GridDeformer filter creation/editing"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "deform.onetime-deform-subtool"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "OneTimeDeform Deform subtool point/grid movement and visual preview"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs input simulation."},
    },
    {
        {StaticLiteral{}, "deform.onetime-apply"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "OneTimeDeform Apply preserves final visual result as much as possible"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use golden deformation comparison."},
    },
    {
        {StaticLiteral{}, "deform.onetime-save"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "Saving while OneTimeDeform subtool is active applies virtual deformation to copied model"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use save fixture."},
    },
    {
        {StaticLiteral{}, "deform.undo-redo-group"},
        {StaticLiteral{}, "Deformation Tools"},
        {StaticLiteral{}, "Nested undo/redo for VertexEdit, DepthEdit, and OneTimeDeform"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers nested scope close behavior and level restoration."},
    },
    {
        {StaticLiteral{}, "depth.edit-scope"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "DepthEdit enter/exit scope and undo/redo scope guard"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers common ActionStackScope guard semantics for DepthEdit."},
    },
    {
        {StaticLiteral{}, "depth.select-tool"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Depth selection tool hit testing, select, deselect, and box selection"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "depth.point-tool"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Depth point tool edits depth values and supports undo/redo"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "depth.attached-point-tool"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Attached depth point tool follows mesh vertices and updates bound depth values"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "depth.ring-tool"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Depth ring tool displays front-side rings and edits radial depth"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "depth.plane-tool"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Depth plane tool fits and applies planar depth gradients"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "depth.landmark-tool"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Depth landmark tool creates, selects, moves, and deletes landmarks"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI input simulation."},
    },
    {
        {StaticLiteral{}, "depth.renderer"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Depth renderer draws signs, colors, rings, lines, and hidden/back-side cues correctly"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use render snapshot fixture."},
    },
    {
        {StaticLiteral{}, "depth.sign-colors"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Positive and negative depth display colors and front-side markers"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers positive, negative, zero, selected, and rounding color contracts; rendered marker geometry remains in depth.renderer."},
    },
    {
        {StaticLiteral{}, "depth.persistence"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Depth map edit apply, cancel, interpolation, and save/load"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers depth arrays and depth operations through copy/replace/rebuffer helpers plus native INX save/load round-trip."},
    },
    {
        {StaticLiteral{}, "depth.exdepthmapped"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "DepthMapped node serialization, depth array resize, and depth operation helpers"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers ExGridDeformer and ExPathDeformer depth array copying, resize-on-rebuffer, depth operation copy, and native INX round-trip."},
    },
    {
        {StaticLiteral{}, "depth.camera"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Depth camera projection, viewport transform, hit testing, and depth edit view state"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers depth camera projection/unprojection math and pan/zoom/yaw/pitch/depth effects; UI viewport smoke remains in viewport.depth-mode."},
    },
    {
        {StaticLiteral{}, "depth.operation-helpers"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Depth operation helpers apply, cancel, copy, resize, and interpolate depth arrays"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers DepthMapped/DepthOperation copy, replace, resize, clone, and basic geometry helper contracts."},
    },
    {
        {StaticLiteral{}, "depth.commands"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Depth map and individual depth operation commands"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers Set/List/Clear Depths and Add/Update/Move/Remove/Clear/Apply depth-ops commands with undo/redo."},
    },
    {
        {StaticLiteral{}, "depth.psd-map-import"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "PSD depth map import conversion, convolution, frontmost compositing, and target resolution"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers deterministic PSD depth helper behavior used by the import command."},
    },
    {
        {StaticLiteral{}, "depth.composite-map-ops"},
        {StaticLiteral{}, "Depth Edit"},
        {StaticLiteral{}, "Define grid, edit depths, add operations, apply, undo, redo, and preserve vertex/depth lengths"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers chained topology and depth-operation edits on the same target."},
    },
    {
        {StaticLiteral{}, "depthbone.template-bones"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "Standard DepthBone skeleton template creation"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers standard skeleton creation, hierarchy, Head parent-to-target default, and Foot lock-to-root defaults."},
    },
    {
        {StaticLiteral{}, "depthbone.template-parameters"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "Standard DepthBone parameter template creation from param values"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers standard DepthBone parameter and binding template command on a generated skeleton."},
    },
    {
        {StaticLiteral{}, "depthbone.root-node"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "DepthRigRoot creation, icon/type registration, serialization, and export exclusion"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers node registration, DepthRigRoot round-trip, and INP export pruning."},
    },
    {
        {StaticLiteral{}, "depthbone.bone-node"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "DepthBone creation, parent/child hierarchy, rest transforms, constraints, and serialization"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers DepthBone node round-trip of rest pose and constraints."},
    },
    {
        {StaticLiteral{}, "depthbone.binding-create"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "Binding creation, update, removal, and target validation"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers command-level DepthBone source binding creation, settings update, removal, and undo/redo."},
    },
    {
        {StaticLiteral{}, "depthbone.sources"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "Bone Source add/remove/reorder/offset/scale/rotation/weight undo and refresh"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers source list actions and command-level add/remove/settings undo/redo, including rotation normalization and JSON state."},
    },
    {
        {StaticLiteral{}, "depthbone.fit-z"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "Fit Z to Depth updates descendant DepthBone Z from mapped GridDeformer depth"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers DepthRigRoot Fit Z to Depth using per-DepthBone depth samples and rotated BoneSource depth vectors from DepthMapped GridDeformer targets."},
    },
    {
        {StaticLiteral{}, "depthbone.influence-rule"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "Influence rule get/set, terminal bone selection, max influence, and radius behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers command-level influence rule set/get with undo/redo and serialization."},
    },
    {
        {StaticLiteral{}, "depthbone.preview-commands"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "List, preview influence, preview deform, and apply deform commands"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers reduced command fixture for listing bones/sources, influence preview deformation, posed deform preview, apply-to-binding, undo, and redo."},
    },
    {
        {StaticLiteral{}, "depthbone.gpu-packet"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "GPU offset packet construction for GridDeformer and PathDeformer"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers packet construction, BoneSource rotation records, shader source contract, and rotated deformation before OpenGL transform feedback dispatch."},
    },
    {
        {StaticLiteral{}, "depthbone.gpu-all-keypoints"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "GPU all-keypoints refresh dispatch and readback"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers GPU dispatch/readback through all-keypoints refresh."},
    },
    {
        {StaticLiteral{}, "depthbone.overlay-selection"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "Selected DepthBone point and parent/child link classification"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers selected-node ownership and distinct incoming/outgoing overlay geometry."},
    },
    {
        {StaticLiteral{}, "depthbone.update-visualization"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "Event-driven GridDeformer and PathDeformer update status visualization"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers detected, queued, processing, applied, stale, and failed target states plus cached target outlines."},
    },
    {
        {StaticLiteral{}, "async.derived-update"},
        {StaticLiteral{}, "Async Infrastructure"},
        {StaticLiteral{}, "Generic progress registry for asynchronous updates derived from edits"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers scoped target registration, explicit work handles, duplicate completion, immutable visuals, supersession, and AutoMesh UI isolation."},
    },
    {
        {StaticLiteral{}, "depthbone.refresh-queue"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "All-keypoint refresh queue slices across frames and prioritizes current keypoints"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use scheduler/frame fixture."},
    },
    {
        {StaticLiteral{}, "depthbone.cleanup"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "Deleting bones or target structures cleans stale source/binding references"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers DeleteNodeCommand cleanup of DepthBone source references with undo/redo."},
    },
    {
        {StaticLiteral{}, "depthbone.skinning"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "Skinning influence, terminal bone rule, lockToRoot, and parent-to-target options"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers a golden two-bone fixture where terminal lockToRoot prevents parent translation from moving vertices beyond the locked terminal bone."},
    },
    {
        {StaticLiteral{}, "depthbone.serialization"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "DepthRigRoot, DepthBone, binding, source, influence, and template serialization round-trip"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers native INX round-trip of root, bones, constraints, bindings, source settings, and influence rule."},
    },
    {
        {StaticLiteral{}, "depthbone.composite-source-preview"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "DepthBone source binding, influence settings, preview apply, undo, redo, and binding cleanup"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers chained DepthBone source/rule/apply workflow against a GridDeformer target."},
    },
    {
        {StaticLiteral{}, "depthbone.export-pruning"},
        {StaticLiteral{}, "Depth Bone"},
        {StaticLiteral{}, "INP export removes editor-only DepthRigRoot and DepthBone nodes and related bindings"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered by project.export-inp."},
    },
    {
        {StaticLiteral{}, "automesh.grid-processor"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "Grid AutoMesh processor for Part and GridDeformer targets"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers deterministic alpha fixtures for Part targets and cached non-Part input."},
    },
    {
        {StaticLiteral{}, "automesh.skeleton-processor"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "Skeleton AutoMesh processor for Part and PathDeformer targets"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers deterministic alpha fixture path extraction and control point output."},
    },
    {
        {StaticLiteral{}, "automesh.optimum-processor"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "Optimum AutoMesh processor for Part targets"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers deterministic alpha fixture contour/skeleton triangulation output."},
    },
    {
        {StaticLiteral{}, "automesh.contour-processor"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "Contour AutoMesh processor and contour extraction pipeline"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers deterministic alpha fixture contour sampling and mesh output."},
    },
    {
        {StaticLiteral{}, "automesh.alpha-provider"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "Alpha provider, contour thresholds, and teacher image auto-fit options"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers PartAlphaProvider and cached AlphaInput conversion used by AutoMesh processors."},
    },
    {
        {StaticLiteral{}, "automesh.non-part-targets"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "AutoMesh for GridDeformer, PathDeformer, MeshGroup, and non-Part nodes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers GridDeformer AutoMesh through cached alpha input without render backend dependency."},
    },
    {
        {StaticLiteral{}, "automesh.schema-values"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "AutoMesh schema, get values, set values, presets, active processor, and derived config"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers reflected AutoMesh schemas, values, preset application, active processor switching, and config undo/redo."},
    },
    {
        {StaticLiteral{}, "automesh.batch-undo"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "Batch AutoMesh settings apply as one undo/redo operation"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers reflectable AutoMesh config changes, derived advanced values, and single-entry undo/redo."},
    },
    {
        {StaticLiteral{}, "automesh.composite-processor-matrix"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "AutoMesh processor configuration and generated mesh outputs across standard processors"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers processor config changes plus generated mesh sanity for grid, contour, skeleton, and optimum processors in one matrix."},
    },
    {
        {StaticLiteral{}, "automesh.async-shortcut"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "Async AutoMesh calls and shortcut-triggered execution"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI/async smoke."},
    },
    {
        {StaticLiteral{}, "automesh.processor-common"},
        {StaticLiteral{}, "AutoMesh"},
        {StaticLiteral{}, "AutoMesh common helpers, processor registration, reflected options, and error reporting"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers processor registry, schema/value reflection, preset application, active processor switching, and config undo/redo."},
    },
    {
        {StaticLiteral{}, "simplephysics.parameter"},
        {StaticLiteral{}, "SimplePhysics"},
        {StaticLiteral{}, "Parameter set, replace, clear, and drag-and-drop set undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered for command path by simplephysics-parameter-undo-redo."},
    },
    {
        {StaticLiteral{}, "simplephysics.settings"},
        {StaticLiteral{}, "SimplePhysics"},
        {StaticLiteral{}, "Gravity, damping, frequency, output scale, length, and curve type undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered by simplephysics-settings-undo-redo."},
    },
    {
        {StaticLiteral{}, "simplephysics.mapping"},
        {StaticLiteral{}, "SimplePhysics"},
        {StaticLiteral{}, "Model type, map mode, local-only, curve type, and parameter unmap behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers model type, map mode, local-only, parameter assignment, and parameter clear through SimplePhysics commands."},
    },
    {
        {StaticLiteral{}, "simplephysics.runtime"},
        {StaticLiteral{}, "SimplePhysics"},
        {StaticLiteral{}, "Physics preview, reset, and runtime parameter output"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs simulation fixture."},
    },
    {
        {StaticLiteral{}, "simplephysics.serialization"},
        {StaticLiteral{}, "SimplePhysics"},
        {StaticLiteral{}, "SimplePhysics node serialization of mappings, curve type, parameter, and runtime settings"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers native INX round-trip of parameter reference, model type, map mode, local-only, and numeric runtime settings."},
    },
    {
        {StaticLiteral{}, "simplephysics.composite-roundtrip"},
        {StaticLiteral{}, "SimplePhysics"},
        {StaticLiteral{}, "SimplePhysics parameter assignment, settings edits, undo/redo, save, reopen, and parameter reference restoration"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers a chained SimplePhysics command workflow persisted through native INX."},
    },
    {
        {StaticLiteral{}, "viewport.navigation"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "Zoom, pan, focus, reset, fit model, reset position, reset zoom, mirror, and background controls"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "viewport.autorig-presentation"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "AutoRig preserves presentation commands while excluding model edits"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers camera/layout command admission and model-edit exclusion during AutoRig ownership."},
    },
    {
        {StaticLiteral{}, "viewport.model-mode"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "Model viewport layout/deform mode switching and selected editor delegation"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "viewport.animation-mode"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "Animation viewport mode, playback preview, and animation editor delegation"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "viewport.depth-mode"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "Depth viewport mode, camera, editor delegation, and depth renderer integration"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "viewport.driver-postprocess"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "Driver, onion/slice, physics, postprocess, and rendering toggles"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs render smoke."},
    },
    {
        {StaticLiteral{}, "viewport.flip-pairs"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "List, add, auto-add, remove flip pairs, and flip-pair window command paths"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers list/add/auto-add/remove command paths; window opening remains computer-use."},
    },
    {
        {StaticLiteral{}, "viewport.palette-command-list"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "Palette command list and command palette discovery"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers command collection, category mapping, token derivation, and filtering."},
    },
    {
        {StaticLiteral{}, "viewport.panels"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "Panel show/hide, docking layout, tree panel, parameter panel, inspector panel"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "viewport.main-menu-toolbar-status"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "Main menu, toolbar, status bar, and viewport bottom controls"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "viewport.action-history"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "Action history panel undo/redo index and clear behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers ActionStack index traversal, saved-state tracking, and clear behavior behind the history panel."},
    },
    {
        {StaticLiteral{}, "viewport.settings-command-composite"},
        {StaticLiteral{}, "Viewport/UI"},
        {StaticLiteral{}, "Shortcut settings and flip-pair viewport commands remain independent across undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers settings-backed shortcut persistence interleaved with viewport flip-pair commands and history operations."},
    },
    {
        {StaticLiteral{}, "render.backend-gl-sdl"},
        {StaticLiteral{}, "Rendering"},
        {StaticLiteral{}, "OpenGL and SDL backend initialization, resize, frame lifecycle, and shutdown"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use app/backend smoke."},
    },
    {
        {StaticLiteral{}, "render.atlas-packer"},
        {StaticLiteral{}, "Rendering"},
        {StaticLiteral{}, "Atlas packing, texture upload, texture slot population, and atlas invalidation"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers packer allocation/reuse plus Puppet texture slot population, dedupe, replacement, and invalidation; GL upload smoke remains in render.backend-gl-sdl."},
    },
    {
        {StaticLiteral{}, "render.blend-modes"},
        {StaticLiteral{}, "Rendering"},
        {StaticLiteral{}, "Normal, clip, slice, masks, advanced blend modes, opacity, tint, screen tint, and emission"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use render snapshot fixtures."},
    },
    {
        {StaticLiteral{}, "render.postprocess"},
        {StaticLiteral{}, "Rendering"},
        {StaticLiteral{}, "Postprocess toggle, difference aggregation, onion/slice, and live screenshot rendering"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use render snapshot fixtures."},
    },
    {
        {StaticLiteral{}, "render.camera"},
        {StaticLiteral{}, "Rendering"},
        {StaticLiteral{}, "Camera projection, viewport crop, and camera-driven export"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers camera node projection plus PNG/TGA export command paths and output dimensions; JPEG and pixel golden coverage remain in project.export-jpeg/render.blend-modes/postprocess."},
    },
    {
        {StaticLiteral{}, "render.composite-camera-texture-export"},
        {StaticLiteral{}, "Rendering"},
        {StaticLiteral{}, "Texture-backed part, camera export, texture slot rebuild, and native save/reopen"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers render-adjacent asset lifecycle composed with camera export."},
    },
    {
        {StaticLiteral{}, "render.onion-slice"},
        {StaticLiteral{}, "Rendering"},
        {StaticLiteral{}, "Onion skin, slice visualization, and model viewport postprocess flags"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use render snapshot fixture."},
    },
    {
        {StaticLiteral{}, "render.onion-slice-geometry"},
        {StaticLiteral{}, "Rendering"},
        {StaticLiteral{}, "Captured onion geometry retains model size and position across viewport changes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers capture zoom, pan, mirror, rotation, and independent history geometry."},
    },
    {
        {StaticLiteral{}, "render.texture-lifecycle"},
        {StaticLiteral{}, "Rendering"},
        {StaticLiteral{}, "Texture upload, reload, disposal, atlas slot reuse, and missing texture fallbacks"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers headless texture replacement, slot rebuild, missing-source rejection, and explicit disposal; backend upload smoke remains in render.backend-gl-sdl."},
    },
    {
        {StaticLiteral{}, "settings.shortcuts"},
        {StaticLiteral{}, "Settings/Shortcuts"},
        {StaticLiteral{}, "Shortcut create, update, clear, conflict, save/load"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers command registry shortcut conflict handling and settings-backed save/load."},
    },
    {
        {StaticLiteral{}, "settings.composite-shortcuts-recent"},
        {StaticLiteral{}, "Settings/Shortcuts"},
        {StaticLiteral{}, "Shortcut conflict, save/load, recent files, and project open state remain independent"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers settings interactions chained with project-level state."},
    },
    {
        {StaticLiteral{}, "settings.default-shortcuts"},
        {StaticLiteral{}, "Settings/Shortcuts"},
        {StaticLiteral{}, "Default shortcut registration, command metadata, and reset-to-default behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers default shortcut registration against command instances."},
    },
    {
        {StaticLiteral{}, "settings.ui"},
        {StaticLiteral{}, "Settings/Shortcuts"},
        {StaticLiteral{}, "Language, UI scale, theme, file handling, autosave, viewport settings"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers typed settings store for UI/file/viewport settings; visual application still needs UI smoke."},
    },
    {
        {StaticLiteral{}, "settings.window"},
        {StaticLiteral{}, "Settings/Shortcuts"},
        {StaticLiteral{}, "Settings window tabs, editing, validation, persistence, and cancel/apply behavior"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "settings.ai-mcp"},
        {StaticLiteral{}, "Settings/Shortcuts"},
        {StaticLiteral{}, "AI agent and MCP settings are opt-in and persist correctly"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers AI/MCP opt-in settings persistence without starting services."},
    },
    {
        {StaticLiteral{}, "settings.paths"},
        {StaticLiteral{}, "Settings/Shortcuts"},
        {StaticLiteral{}, "Config path resolution, user data path, recent file path, and portable path behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers config, imgui, font, locale, and settings path resolution."},
    },
    {
        {StaticLiteral{}, "panels.node-tree"},
        {StaticLiteral{}, "Panels"},
        {StaticLiteral{}, "Node tree display, search/filter, selection, context actions, drag reorder, and scroll state"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "panels.parameter-list"},
        {StaticLiteral{}, "Panels"},
        {StaticLiteral{}, "Parameter list display, search/filter, group display, armed parameter controls, and keypoint grid"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "panels.inspector"},
        {StaticLiteral{}, "Panels"},
        {StaticLiteral{}, "Inspector routing, multi-selection inspector selection, and property commit behavior"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "panels.timeline"},
        {StaticLiteral{}, "Panels"},
        {StaticLiteral{}, "Timeline display, keyframe rows, playback controls, and animation editing panel"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "panels.scene-resource"},
        {StaticLiteral{}, "Panels"},
        {StaticLiteral{}, "Scene panel, resource panel, logger panel, and tool-settings panel behavior"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "panels.shell"},
        {StaticLiteral{}, "Panels"},
        {StaticLiteral{}, "Shell command input, output, error handling, and history"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "panels.logger"},
        {StaticLiteral{}, "Panels"},
        {StaticLiteral{}, "Logger panel filtering, message display, warning/error visibility, and clear behavior"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "panels.armed-parameter"},
        {StaticLiteral{}, "Panels"},
        {StaticLiteral{}, "Armed parameter panel shows armed state, selected keypoint, and current values"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "panels.action-history"},
        {StaticLiteral{}, "Panels"},
        {StaticLiteral{}, "Action history panel displays groups, undo/redo cursor, saved state, and clear behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered by viewport.action-history."},
    },
    {
        {StaticLiteral{}, "tools.command-browser"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Command browser search, execution, shortcut display"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "tools.command-browser-resource-pickers"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Command browser resource-array arguments initialize picker selections before drawing"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers #173-style Node[]/Parameter[]/ParameterBinding[] picker AA initialization before render."},
    },
    {
        {StaticLiteral{}, "tools.shell"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Shell panel execution and output behavior"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "tools.texture-viewer"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Texture viewer and texture operations"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "tools.export-dialogs"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Image, video, INP, and merge/export dialog workflows"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "tools.ai-agent"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "AI agent panel permission, disabled state, and tool output states"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "windows.welcome-about"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Welcome window, about window, nag screen, and default layout behavior"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "windows.automesh-batch"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "AutoMesh batching window selection, per-target config, apply, and undo grouping"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "windows.export-import"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "PSD/KRA merge, image export, video export, INP export, and texture viewer windows"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "windows.psd-depth-map"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "PSD depth map import window mapping review and options"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke; command and IO helper behavior are automated."},
    },
    {
        {StaticLiteral{}, "windows.parameter-editors"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Parameter editor, split parameter, rename, flip config, and edit animation windows"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "windows.settings"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Settings window tabs, validation, apply/cancel, and persistent values"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "windows.autosave"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Autosave recovery window accepts, rejects, deletes, and opens records"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "windows.rename"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Rename window commit, cancel, merge undo, and validation behavior"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "windows.flip-config"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Flip configuration window add, auto-add, remove, apply, and persistence"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "windows.parameter-split"},
        {StaticLiteral{}, "Tools/Windows"},
        {StaticLiteral{}, "Parameter split window migrates axes, bindings, and undo grouping"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs UI smoke."},
    },
    {
        {StaticLiteral{}, "widgets.buttons-tooltips"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Button, toolbar, tooltip, label, category, lock, and markdown widgets"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.inputs"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "InputText, drag, drag-drop, toggle, controller, timeline, progress, and texture widgets"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.dialogs-modals"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Dialog, modal, notification, output, shadow, viewport, and dummy widgets"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.button"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Button variants, icon buttons, disabled state, active state, and click result"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.toolbar"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Toolbar layout, icon spacing, selection, disabled state, and tooltip integration"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.tooltip"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Tooltip delay, markdown text, wrapped labels, and disabled widget hints"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.label-category"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Label, category header, collapsible state, and nested layout behavior"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.inputtext"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "InputText editing, focus, commit, cancel, and IME-safe behavior"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.drag"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Drag numeric controls, slider stepping, commit boundaries, and formatting"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.dragdrop"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Drag-drop source, target, payload typing, cancellation, and hover feedback"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.toggle-lock"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Toggle and lock widgets show boolean state and produce one commit per change"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.controller"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "2D controller widget hit testing, snapping, current value display, and keypoint selection"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.timeline"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Timeline widget track rows, key markers, scrub position, zoom, and selection"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.progress-output-notification"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Progress, output, notification, and status widgets render and clear state correctly"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "widgets.texture-viewport-shadow"},
        {StaticLiteral{}, "Widgets"},
        {StaticLiteral{}, "Texture preview, viewport widget, shadow, dummy, and modal helper drawing"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use widget-level render fixture."},
    },
    {
        {StaticLiteral{}, "i18n.wrapped-strings"},
        {StaticLiteral{}, "I18N"},
        {StaticLiteral{}, "UI strings are wrapped by _() or __() where appropriate"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers common visible UI text call sites with a source scan for direct English string literals."},
    },
    {
        {StaticLiteral{}, "i18n.pot"},
        {StaticLiteral{}, "I18N"},
        {StaticLiteral{}, "POT update includes new UI text"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers template.pot presence and simple single-line _()/__() source msgid coverage."},
    },
    {
        {StaticLiteral{}, "platform.windows-write"},
        {StaticLiteral{}, "Platform/Crash"},
        {StaticLiteral{}, "Windows release build does not crash on console write paths"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers release-sensitive console write calls with a source scan; file/socket writes and debug/version/unittest writes are allowed."},
    },
    {
        {StaticLiteral{}, "platform.paths-dpi-fonts"},
        {StaticLiteral{}, "Platform/Crash"},
        {StaticLiteral{}, "Platform paths, DPI scaling, font loading, logo resources, and config paths"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers config/font/locale path creation and DPI scale persistence in a headless platform fixture."},
    },
    {
        {StaticLiteral{}, "platform.crashdump"},
        {StaticLiteral{}, "Platform/Crash"},
        {StaticLiteral{}, "Crash dump setup, exception logging, and release crash paths"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers crash dump text generation, path generation, and file writing without triggering a native crash."},
    },
    {
        {StaticLiteral{}, "platform.startup-shutdown"},
        {StaticLiteral{}, "Platform/Crash"},
        {StaticLiteral{}, "Startup, shutdown, background service cleanup, and no module-constructor cycles"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers a source-level module-constructor inventory guard and test-binary startup without constructor cycles."},
    },
    {
        {StaticLiteral{}, "platform.version"},
        {StaticLiteral{}, "Platform/Crash"},
        {StaticLiteral{}, "Version metadata is available to UI, export, diagnostics, and release builds"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers generated version metadata availability."},
    },
    {
        {StaticLiteral{}, "platform.input-window"},
        {StaticLiteral{}, "Platform/Crash"},
        {StaticLiteral{}, "Window lifecycle, input event mapping, timing callback, DPI changes, and focus state"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use backend/window fixture."},
    },
    {
        {StaticLiteral{}, "platform.tasks"},
        {StaticLiteral{}, "Platform/Crash"},
        {StaticLiteral{}, "Background task queue runs, cancels, reports errors, and shuts down cleanly"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers task queue add, yield, update, completion, status, and progress reset."},
    },
    {
        {StaticLiteral{}, "platform.debug-logging"},
        {StaticLiteral{}, "Platform/Crash"},
        {StaticLiteral{}, "Debug logging is gated by build mode and does not crash release builds"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers source-level console/debug output guard shared with Windows console write safety."},
    },
    {
        {StaticLiteral{}, "undo.grouped-actions"},
        {StaticLiteral{}, "Undo/Redo"},
        {StaticLiteral{}, "Action groups undo/redo as one unit"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered by action-group-undo-redo."},
    },
    {
        {StaticLiteral{}, "undo.async-grouped-actions"},
        {StaticLiteral{}, "Undo/Redo"},
        {StaticLiteral{}, "Asynchronous derived actions stay attached to their owner and are canceled and rescheduled across undo/redo"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers AsyncGroupAction pending, completed, stale-generation, undo, and redo behavior."},
    },
    {
        {StaticLiteral{}, "undo.command-actions"},
        {StaticLiteral{}, "Undo/Redo"},
        {StaticLiteral{}, "All mutating commands push undoable actions or explicitly declare non-mutating behavior"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers source-level command audit for Action pushes, grouped action pushes, history helpers, and explicitly non-undoable command classes."},
    },
    {
        {StaticLiteral{}, "undo.ui-commit-boundaries"},
        {StaticLiteral{}, "Undo/Redo"},
        {StaticLiteral{}, "Text edits, drags, sliders, toggles, and dialogs merge or split undo entries at correct commit boundaries"},
        {StaticLiteral{}, "computer-use"},
        {StaticLiteral{}, "Needs computer-use UI interaction smoke."},
    },
    {
        {StaticLiteral{}, "undo.direct-mutation-audit"},
        {StaticLiteral{}, "Undo/Redo"},
        {StaticLiteral{}, "Model, parameter, deformer, mesh, and inspector mutations go through actions"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers a source-level guard for direct model/parameter/deformation writes in command, panel, window, and viewport layers; targeted fixtures remain in related scenarios."},
    },
    {
        {StaticLiteral{}, "undo.scope-guards"},
        {StaticLiteral{}, "Undo/Redo"},
        {StaticLiteral{}, "Mode, editor, subtool, and nested scopes close automatically at defined guard boundaries"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covered by mesh/depth/onetime scope guard fixtures."},
    },
    {
        {StaticLiteral{}, "undo.action-merge"},
        {StaticLiteral{}, "Undo/Redo"},
        {StaticLiteral{}, "Text edit and drag actions merge within one edit session and split across sessions"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers core action-stack merge semantics for same-target edits, non-merge splits, undo truncation, and redo of merged final values; UI commit boundaries remain in undo.ui-commit-boundaries."},
    },
    {
        {StaticLiteral{}, "undo.composite-workflows"},
        {StaticLiteral{}, "Undo/Redo"},
        {StaticLiteral{}, "Composite command workflows do not consume unrelated root history or corrupt node types across modes"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers MCP-like mode/history sequences with root actions present, edit scopes active, and resource-visible type invariants after undo/redo."},
    },
    {
        {StaticLiteral{}, "undo.composite-cross-family-stack"},
        {StaticLiteral{}, "Undo/Redo"},
        {StaticLiteral{}, "Node, parameter, mesh, inspector, and binding commands undo/redo in exact stack order"},
        {StaticLiteral{}, "automated"},
        {StaticLiteral{}, "Covers a deliberately mixed command stack across major mutable domains."},
    },
}};

constinit const std::array<nijigenerate::compat::RetainedString, 57> requiredSourceScenarioIds{{
    {StaticLiteral{}, "api.acp-client"},
    {StaticLiteral{}, "api.acp-echo-agent"},
    {StaticLiteral{}, "api.acp-stdio"},
    {StaticLiteral{}, "api.mcp-auth"},
    {StaticLiteral{}, "api.mcp-http-transport"},
    {StaticLiteral{}, "api.mcp-resources"},
    {StaticLiteral{}, "api.mcp-task-queue"},
    {StaticLiteral{}, "atlas.color-bleed"},
    {StaticLiteral{}, "atlas.pack"},
    {StaticLiteral{}, "automesh.processor-common"},
    {StaticLiteral{}, "core.math-path"},
    {StaticLiteral{}, "core.math-skeletonize"},
    {StaticLiteral{}, "core.math-triangle"},
    {StaticLiteral{}, "core.node-registry"},
    {StaticLiteral{}, "depth.camera"},
    {StaticLiteral{}, "depth.operation-helpers"},
    {StaticLiteral{}, "deform.bezier-tool"},
    {StaticLiteral{}, "depthbone.serialization"},
    {StaticLiteral{}, "io.image-export"},
    {StaticLiteral{}, "io.inimport-model"},
    {StaticLiteral{}, "io.inpexport-model"},
    {StaticLiteral{}, "io.kra-reader"},
    {StaticLiteral{}, "io.psd-reader"},
    {StaticLiteral{}, "io.save-native"},
    {StaticLiteral{}, "io.video-export"},
    {StaticLiteral{}, "mesh.operation-deformable"},
    {StaticLiteral{}, "mesh.operation-drawable"},
    {StaticLiteral{}, "mesh.operation-node"},
    {StaticLiteral{}, "mesh.spline"},
    {StaticLiteral{}, "mesh.tool-brush"},
    {StaticLiteral{}, "mesh.tool-connect"},
    {StaticLiteral{}, "mesh.tool-edge-cutter"},
    {StaticLiteral{}, "mesh.tool-lasso"},
    {StaticLiteral{}, "mesh.tool-point"},
    {StaticLiteral{}, "mesh.tool-select"},
    {StaticLiteral{}, "panels.armed-parameter"},
    {StaticLiteral{}, "panels.logger"},
    {StaticLiteral{}, "platform.input-window"},
    {StaticLiteral{}, "platform.tasks"},
    {StaticLiteral{}, "settings.paths"},
    {StaticLiteral{}, "widgets.button"},
    {StaticLiteral{}, "widgets.controller"},
    {StaticLiteral{}, "widgets.drag"},
    {StaticLiteral{}, "widgets.dragdrop"},
    {StaticLiteral{}, "widgets.inputtext"},
    {StaticLiteral{}, "widgets.label-category"},
    {StaticLiteral{}, "widgets.progress-output-notification"},
    {StaticLiteral{}, "widgets.texture-viewport-shadow"},
    {StaticLiteral{}, "widgets.timeline"},
    {StaticLiteral{}, "widgets.toggle-lock"},
    {StaticLiteral{}, "widgets.toolbar"},
    {StaticLiteral{}, "widgets.tooltip"},
    {StaticLiteral{}, "windows.autosave"},
    {StaticLiteral{}, "windows.flip-config"},
    {StaticLiteral{}, "windows.parameter-split"},
    {StaticLiteral{}, "windows.rename"},
    {StaticLiteral{}, "windows.settings"},
}};

constinit const std::array<nijigenerate::compat::RetainedString, 11> regressionNodeMenuTypes{{
    {nijigenerate::compat::RetainedString::StaticLiteralTag{}, "Node"},
    {nijigenerate::compat::RetainedString::StaticLiteralTag{}, "Mask"},
    {nijigenerate::compat::RetainedString::StaticLiteralTag{}, "Composite"},
    {nijigenerate::compat::RetainedString::StaticLiteralTag{}, "SimplePhysics"},
    {nijigenerate::compat::RetainedString::StaticLiteralTag{}, "MeshGroup"},
    {nijigenerate::compat::RetainedString::StaticLiteralTag{}, "DynamicComposite"},
    {nijigenerate::compat::RetainedString::StaticLiteralTag{}, "PathDeformer"},
    {nijigenerate::compat::RetainedString::StaticLiteralTag{}, "GridDeformer"},
    {nijigenerate::compat::RetainedString::StaticLiteralTag{}, "DepthRigRoot"},
    {nijigenerate::compat::RetainedString::StaticLiteralTag{}, "DepthBone"},
    {nijigenerate::compat::RetainedString::StaticLiteralTag{}, "Camera"},
}};

double regressionNow() {
    return 0.0;
}

nijigenerate::compat::RetainedString scenarioSlug(std::string_view value) {
    std::string result;
    bool lastDash = false;
    for (const auto scalar : nijigenerate::compat::ngUtf8Scalars(value)) {
        const auto ch = scalar.value;
        char outc;
        if (ch >= U'A' && ch <= U'Z') outc = static_cast<char>(ch + (U'a' - U'A'));
        else if ((ch >= U'a' && ch <= U'z') || (ch >= U'0' && ch <= U'9')) outc = static_cast<char>(ch);
        else outc = '-';
        if (outc == '-') {
            if (!lastDash && !result.empty()) result += outc;
            lastDash = true;
        } else {
            result += outc;
            lastDash = false;
        }
    }
    if (!result.empty() && result.back() == '-') result.pop_back();
    if (result.empty()) return nijigenerate::compat::RetainedString::fromStaticLiteral("case");
    return nijigenerate::compat::RetainedString(std::move(result));
}

bool near(float a, float b) {
    auto d = a - b;
    if (d < 0) d = -d;
    return d < 0.0001f;
}

nijigenerate::compat::Slice<Scenario> generatedCompositeCaseScenarios() {
    using nijigenerate::compat::RetainedString;
    nijigenerate::compat::Slice<Scenario> result;
    const auto append = [&]<std::size_t C, std::size_t N>(std::string id, const char (&category)[C],
        std::string title, const char (&note)[N]) {
        result.append(Scenario{RetainedString(std::move(id)), RetainedString::fromStaticLiteral(category),
            RetainedString(std::move(title)), RetainedString::fromStaticLiteral("automated"),
            RetainedString::fromStaticLiteral(note)});
    };
    for (const std::string_view mode : {"Model Edit", "Anim Edit", "Vertex Edit", "Depth Edit"}) {
        for (const std::string_view shape : {"2x2 cells", "3x2 cells", "4x4 cells", "wide face", "5x5 cells", "tall body"}) {
            std::string id = "mesh.composite-grid-mode-matrix.";
            id += scenarioSlug(mode).view();
            id += ".";
            id += scenarioSlug(shape).view();
            auto title = std::string("DefineGrid ") + std::string(mode) + " " + std::string(shape);
            append(std::move(id), "Mesh/Vertex Editor", std::move(title),
                "Generated case-level scenario for DefineGrid mode/grid matrix.");
        }
    }
    for (const auto& className : regressionNodeMenuTypes) {
        std::string id = "node.composite-type-matrix.";
        id += scenarioSlug(className.view()).view();
        auto title = std::string("Create/rename/undo/redo ") + std::string(className.view());
        append(std::move(id), "Node Hierarchy", std::move(title),
            "Generated case-level scenario for registered node type command coverage.");
    }
    for (const std::string_view dim : {"1d", "2d"}) {
        for (const std::string_view range : {"0-1", "minus-1-1", "minus-2-2", "0-2", "minus-5-5"}) {
            auto id = std::string("parameter.composite-preset-matrix.") + std::string(dim) + "." + std::string(range);
            auto title = std::string("Parameter preset ") + std::string(dim) + " " + std::string(range);
            append(std::move(id), "Parameters", std::move(title),
                "Generated case-level scenario for parameter preset and binding matrix.");
        }
    }
    for (const std::string_view type : {"attached-point", "ring", "plane"}) {
        auto id = std::string("depth.composite-operation-matrix.") + std::string(type);
        auto title = std::string("Depth operation ") + std::string(type);
        append(std::move(id), "Depth Edit", std::move(title),
            "Generated case-level scenario for depth operation undo/redo matrix.");
    }
    for (const std::string_view processor : {"grid", "contour", "skeleton", "optimum"}) {
        auto id = std::string("automesh.composite-processor-matrix-scenario.") + std::string(processor);
        auto title = std::string("AutoMesh processor ") + std::string(processor);
        append(std::move(id), "AutoMesh", std::move(title),
            "Generated case-level scenario for AutoMesh processor matrix.");
    }
    for (const std::string_view setting : {"gravity-0-5", "gravity-2-0", "gravity-4-0",
        "length-50", "length-125", "length-300", "frequency-0-25", "frequency-1-5", "frequency-5-0",
        "angle-damping-0-1", "angle-damping-0-5", "angle-damping-0-9",
        "length-damping-0-1", "length-damping-0-5", "length-damping-0-9"}) {
        auto id = std::string("simplephysics.composite-settings-matrix.") + std::string(setting);
        auto title = std::string("SimplePhysics setting ") + std::string(setting);
        append(std::move(id), "SimplePhysics", std::move(title),
            "Generated case-level scenario for SimplePhysics setting matrix.");
    }
    return result;
}

nijigenerate::compat::Slice<Scenario> allScenarios() {
    nijigenerate::compat::Slice<Scenario> result(scenarios.begin(), scenarios.end());
    result.append(generatedCompositeCaseScenarios());
    return result;
}

bool hasScenarioPrefix(nijigenerate::compat::RetainedString prefix) {
    const auto expected = concatenateSourceStrings({prefix,
        nijigenerate::compat::RetainedString::fromStaticLiteral(".")});
    const auto values = allScenarios();
    for (const auto scenario : values)
        if (scenario.id.view() == prefix.view() || scenario.id.view().starts_with(expected.view())) return true;
    return false;
}

bool hasScenarioId(nijigenerate::compat::RetainedString id) {
    const auto values = allScenarios();
    for (const auto& scenario : values) if (scenario.id.view() == id.view()) return true;
    return false;
}

bool hasScenarioStatus(nijigenerate::compat::RetainedString id, nijigenerate::compat::RetainedString status) {
    const auto values = allScenarios();
    for (const auto& scenario : values)
        if (scenario.id.view() == id.view()) return scenario.status.view() == status.view();
    return false;
}

void require(bool condition, nijigenerate::compat::RetainedString message) {
    if (condition) return;
    auto failure = message.empty()
        ? nijigenerate::compat::RetainedString::fromStaticLiteral("Enforcement failed")
        : nijigenerate::compat::RetainedString(message.view());
    // Preserve the logical original enforce site; diagnostic-path parity is unverified.
    throw nijigenerate::compat::ThrowableException(std::move(failure),
        nijigenerate::compat::RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), 735);
}

nijigenerate::compat::RetainedString regressionSourceRoot(nijigenerate::compat::RetainedString subdir) {
    using nijigenerate::compat::RetainedString;
    const std::array directSegments{RetainedString::fromStaticLiteral("source"),
        RetainedString::fromStaticLiteral("nijigenerate"), subdir};
    const auto direct = nijigenerate::compat::phobos::ngBuildPath(directSegments);
    if (nijigenerate::compat::phobos::ngFileExists(direct.view())) return direct;
    const std::array parentSegments{RetainedString::fromStaticLiteral(".."),
        RetainedString::fromStaticLiteral("source"), RetainedString::fromStaticLiteral("nijigenerate"), subdir};
    const auto parent = nijigenerate::compat::phobos::ngBuildPath(parentSegments);
    if (nijigenerate::compat::phobos::ngFileExists(parent.view())) return parent;
    return nijigenerate::compat::phobos::ngBuildPath(directSegments);
}

nijigenerate::compat::RetainedString regressionRepoRoot() {
    using nijigenerate::compat::RetainedString;
    const std::array direct{RetainedString::fromStaticLiteral("tl"),
        RetainedString::fromStaticLiteral("template.pot")};
    if (nijigenerate::compat::phobos::ngFileExists(nijigenerate::compat::phobos::ngBuildPath(direct).view()))
        return RetainedString::fromStaticLiteral(".");
    const std::array parent{RetainedString::fromStaticLiteral(".."),
        RetainedString::fromStaticLiteral("tl"), RetainedString::fromStaticLiteral("template.pot")};
    if (nijigenerate::compat::phobos::ngFileExists(nijigenerate::compat::phobos::ngBuildPath(parent).view()))
        return RetainedString::fromStaticLiteral("..");
    return RetainedString::fromStaticLiteral(".");
}

void configureRegressionConfigDir() {
    using nijigenerate::compat::RetainedString;
    const std::array segments{
        nijigenerate::compat::phobos::ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-config")};
    const auto configDir = nijigenerate::compat::phobos::ngBuildPath(segments);
    if (!nijigenerate::compat::phobos::ngFileExists(configDir.view()))
        nijigenerate::compat::phobos::ngMkdirRecurse(configDir.view());
    nijigenerate::platform::ngSetEnvironmentVariableRetained(configDir,
        RetainedString::fromBorrowedView(nijigenerate::core::path::ENV_CONFIG_PATH));
}

void resetCase() {
    nijigenerate::core::actionstack::incActionClearHistory();
    nijigenerate::commands::depth::bone_status::ngClearDepthBoneUpdateStatuses();
    nijigenerate::project::ngCreateHeadlessRegressionProject();
    nijigenerate::core::actionstack::incActionClearHistory();
}

void drainRegressionTasks() {
    std::size_t guard = 0;
    while (nijigenerate::core::tasks::ngTaskLength() > 0 && guard++ < 1000)
        nijigenerate::core::tasks::ngTaskUpdate();
    require(nijigenerate::core::tasks::ngTaskLength() == 0,
        nijigenerate::compat::RetainedString::fromStaticLiteral("task queue should drain within guard"));
}

bool rectsOverlap(nicxlive::core::math::Vec4i a, nicxlive::core::math::Vec4i b) {
    // D int addition wraps; retain that behavior before each signed comparison.
    const auto add = [](std::int32_t left, std::int32_t right) {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(left)
            + static_cast<std::uint32_t>(right));
    };
    return a.x < add(b.x, b.z) && add(a.x, a.z) > b.x
        && a.y < add(b.y, b.w) && add(a.y, a.w) > b.y;
}

// Existing original regression.d2265-2285; no additional case is introduced.
void testAtlasPackerRectLifecycle() {
    using nicxlive::core::math::Vec2i;
    using nicxlive::core::math::Vec4i;
    using nijigenerate::atlas::packer::TexturePacker;
    using nijigenerate::compat::RetainedString;
    auto packer = std::allocate_shared<TexturePacker>(
        nicxlive::core::d_runtime::SourceAllocator<TexturePacker>{}, Vec2i{32, 32});
    const auto a = packer->packTexture(Vec2i{10, 10});
    const auto b = packer->packTexture(Vec2i{8, 12});
    const auto c = packer->packTexture(Vec2i{6, 6});
    require(a.z == 10 && a.w == 10,
        RetainedString::fromStaticLiteral("atlas packer should preserve first rectangle size"));
    require(b.z == 8 && b.w == 12,
        RetainedString::fromStaticLiteral("atlas packer should preserve second rectangle size"));
    require(c.z == 6 && c.w == 6,
        RetainedString::fromStaticLiteral("atlas packer should preserve third rectangle size"));
    require(!rectsOverlap(a, b),
        RetainedString::fromStaticLiteral("atlas packer should not overlap first and second rectangles"));
    require(!rectsOverlap(a, c),
        RetainedString::fromStaticLiteral("atlas packer should not overlap first and third rectangles"));
    require(!rectsOverlap(b, c),
        RetainedString::fromStaticLiteral("atlas packer should not overlap second and third rectangles"));
    packer->remove(b);
    const auto reused = packer->packTexture(Vec2i{8, 12});
    require(reused.z == 8 && reused.w == 12,
        RetainedString::fromStaticLiteral("atlas packer should reuse a removed rectangle-sized area"));
    packer->clear();
    const auto afterClear = packer->packTexture(Vec2i{32, 32});
    require(afterClear == Vec4i{0, 0, 32, 32},
        RetainedString::fromStaticLiteral("atlas packer clear should restore the full free area"));
}

nicxlive::core::math::Mat4 emulateDepthBoneShaderMat4FromRowMajorPacket(std::span<const float> values) {
    require(values.size() >= 16,
        nijigenerate::compat::RetainedString::fromStaticLiteral("matrix packet needs 16 floats"));
    const std::array<float, 16> glslColumnMajorArgs{
        values[0], values[4], values[8], values[12],
        values[1], values[5], values[9], values[13],
        values[2], values[6], values[10], values[14],
        values[3], values[7], values[11], values[15],
    };
    const std::array<float, 16> arguments{
        glslColumnMajorArgs[0], glslColumnMajorArgs[4], glslColumnMajorArgs[8], glslColumnMajorArgs[12],
        glslColumnMajorArgs[1], glslColumnMajorArgs[5], glslColumnMajorArgs[9], glslColumnMajorArgs[13],
        glslColumnMajorArgs[2], glslColumnMajorArgs[6], glslColumnMajorArgs[10], glslColumnMajorArgs[14],
        glslColumnMajorArgs[3], glslColumnMajorArgs[7], glslColumnMajorArgs[11], glslColumnMajorArgs[15],
    };
    nicxlive::core::math::Mat4 result;
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column) result[row][column] = arguments[row * 4 + column];
    return result;
}

float maximumDepthBoneOffsetDifference(nicxlive::core::math::Vec2Array first,
    nicxlive::core::math::Vec2Array second) {
    require(first.size() == second.size(), nijigenerate::compat::RetainedString::fromStaticLiteral(
        "DepthBone offset difference requires equal vertex counts"));
    float result = 0.0f;
    for (std::size_t i = 0; i < first.size(); ++i) {
        const bool finite = std::isfinite(first.get(i).x) && std::isfinite(first.get(i).y) &&
            std::isfinite(second.get(i).x) && std::isfinite(second.get(i).y);
        require(finite, nijigenerate::compat::RetainedString::fromStaticLiteral(
            "DepthBone offset difference requires finite values"));
        const float x = std::fabs(first.get(i).x - second.get(i).x);
        const float y = std::fabs(first.get(i).y - second.get(i).y);
        result = std::max(result, std::max(x, y));
    }
    return result;
}

double fixtureNumber(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name) {
    if (value.is_double()) return value.as_double();
    if (value.is_int64()) return static_cast<double>(value.as_int64());
    if (value.is_uint64()) return static_cast<double>(value.as_uint64());
    require(false, concatenateSourceStrings({name,
        nijigenerate::compat::RetainedString::fromStaticLiteral(" must be a number")}));
    nijigenerate::compat::ngAssertUnreachable("Assertion failure",
        "source/nijigenerate_tests/regression.d", 9730);
}

nijigenerate::compat::RetainedString fixtureString(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name) {
    require(value.is_string(),
        concatenateSourceStrings({name, nijigenerate::compat::RetainedString::fromStaticLiteral(" must be a string")}));
    return value.as_string().retainedString();
}

bool fixtureBool(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name) {
    if (value.is_bool()) return value.as_bool();
    require(false, concatenateSourceStrings({name, nijigenerate::compat::RetainedString::fromStaticLiteral(" must be a boolean")}));
    nijigenerate::compat::ngAssertUnreachable("Assertion failure",
        "source/nijigenerate_tests/regression.d", 9753);
}

nijigenerate::core::settings::SettingsJsonValue fixtureRequired(
    nijigenerate::core::settings::SettingsJsonObject object, nijigenerate::compat::RetainedString key,
    nijigenerate::compat::RetainedString owner) {
    auto* value = object.if_contains(key.view());
    require(value != nullptr, concatenateSourceStrings({owner,
        nijigenerate::compat::RetainedString::fromStaticLiteral(" missing required key: "), key}));
    return *value;
}

nijigenerate::compat::RetainedString fixtureIndexedName(
    nijigenerate::compat::RetainedString name, std::size_t index, std::string_view suffix = {}) {
    using namespace nijigenerate::core::translation;
    const std::array<TypedFormatArgument, 2> arguments{ngFormatArgument(name), ngFormatArgument(index)};
    const auto pattern = suffix.empty() ? std::string_view("%s[%s]") :
        suffix == " must be an integer byte" ? std::string_view("%s[%s] must be an integer byte") :
        std::string_view("%s[%s] must be an integer");
    return nijigenerate::compat::RetainedString(ngFormatTyped(pattern, arguments));
}

nijigenerate::compat::Slice<float> fixtureFloatArray(nijigenerate::core::settings::SettingsJsonValue value,
    nijigenerate::compat::RetainedString name) {
    require(value.is_array(),
        concatenateSourceStrings({name, nijigenerate::compat::RetainedString::fromStaticLiteral(" must be an array")}));
    nijigenerate::compat::Slice<float> result;
    const auto entries = value.as_array();
    for (std::size_t i = 0; i < entries.size(); ++i)
        result.append(static_cast<float>(fixtureNumber(entries[i], fixtureIndexedName(name, i))));
    return result;
}

nijigenerate::compat::Slice<std::uint8_t> fixtureByteArray(
    nijigenerate::core::settings::SettingsJsonValue value, nijigenerate::compat::RetainedString name) {
    require(value.is_array(),
        concatenateSourceStrings({name, nijigenerate::compat::RetainedString::fromStaticLiteral(" must be an array")}));
    nijigenerate::compat::Slice<std::uint8_t> result;
    const auto entries = value.as_array();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto number = fixtureNumber(entries[i], fixtureIndexedName(name, i));
        const bool valid = number >= 0 && number <= 255 && number == nijigenerate::compat::ngFloatToInt32(number);
        const auto message = fixtureIndexedName(name, i, " must be an integer byte");
        require(valid, message);
        result.append(static_cast<std::uint8_t>(number));
    }
    return result;
}

nijigenerate::compat::Slice<std::int32_t> fixtureIntArray(
    nijigenerate::core::settings::SettingsJsonValue value, nijigenerate::compat::RetainedString name) {
    require(value.is_array(),
        concatenateSourceStrings({name, nijigenerate::compat::RetainedString::fromStaticLiteral(" must be an array")}));
    nijigenerate::compat::Slice<std::int32_t> result;
    const auto entries = value.as_array();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto number = fixtureNumber(entries[i], fixtureIndexedName(name, i));
        const bool valid = number == nijigenerate::compat::ngFloatToInt32(number);
        const auto message = fixtureIndexedName(name, i, " must be an integer");
        require(valid, message);
        result.append(nijigenerate::compat::ngFloatToInt32(number));
    }
    return result;
}

nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> fixtureStringArray(
    nijigenerate::core::settings::SettingsJsonValue value, nijigenerate::compat::RetainedString name) {
    require(value.is_array(),
        concatenateSourceStrings({name, nijigenerate::compat::RetainedString::fromStaticLiteral(" must be an array")}));
    nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> result;
    const auto entries = value.as_array();
    for (std::size_t i = 0; i < entries.size(); ++i)
        result.append(fixtureString(entries[i], fixtureIndexedName(name, i)));
    return result;
}

nijigenerate::viewport::depth::draw::DepthMergePolicy fixtureMergePolicy(
    nijigenerate::compat::RetainedString value) {
    using nijigenerate::viewport::depth::draw::DepthMergePolicy;
    if (value.view() == "Replace") return DepthMergePolicy::Replace;
    if (value.view() == "Frontmost") return DepthMergePolicy::Frontmost;
    if (value.view() == "Backmost") return DepthMergePolicy::Backmost;
    if (value.view() == "Add") return DepthMergePolicy::Add;
    if (value.view() == "KeepExistingWhereMissing") return DepthMergePolicy::KeepExistingWhereMissing;
    require(false, concatenateSourceStrings({
        nijigenerate::compat::RetainedString::fromStaticLiteral("unknown depthdraw fixture merge policy: "), value}));
    nijigenerate::compat::ngAssertUnreachable("Assertion failure",
        "source/nijigenerate_tests/regression.d", 9811);
}

bool fixtureStringArrayContains(nijigenerate::core::settings::SettingsJsonObject object,
    nijigenerate::compat::RetainedString key, nijigenerate::compat::RetainedString expected,
    nijigenerate::compat::RetainedString owner) {
    const auto required = fixtureRequired(object, key, owner);
    const auto name = concatenateSourceStrings({owner,
        nijigenerate::compat::RetainedString::fromStaticLiteral("."), key});
    const auto values = fixtureStringArray(required, name);
    for (const auto& value : values) if (value.view() == expected.view()) return true;
    return false;
}

nijigenerate::compat::RetainedString idOrEmpty(nijigenerate::core::settings::SettingsJsonObject object) {
    if (const auto* id = object.if_contains("id"))
        return fixtureString(*id, nijigenerate::compat::RetainedString::fromStaticLiteral("id"));
    return nijigenerate::compat::RetainedString::emptyLiteral();
}

std::size_t runDepthDrawApiCapabilitiesFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    using Strings = nijigenerate::compat::Slice<RetainedString>;
    const auto idField = fixtureRequired(object, RetainedString::fromStaticLiteral("id"), path);
    const auto id = fixtureString(idField, RetainedString(std::string(path.view()) + ".id"));
    const auto checkString = [&]<std::size_t K, std::size_t E, std::size_t M>(
        const char (&key)[K], const char (&expected)[E], const char (&message)[M]) {
        const auto field = fixtureRequired(object, RetainedString::fromStaticLiteral(key), id);
        const auto actual = fixtureString(field, RetainedString(std::string(id.view()) + "." + key));
        const bool valid = actual.view() == std::string_view(expected, E - 1);
        auto diagnostic = std::string(id.view());
        diagnostic.append(message, M - 1);
        require(valid, RetainedString(std::move(diagnostic)));
    };
    const auto checkBool = [&]<std::size_t K, std::size_t M>(
        const char (&key)[K], bool expected, const char (&message)[M]) {
        const auto field = fixtureRequired(object, RetainedString::fromStaticLiteral(key), id);
        const auto actual = fixtureBool(field, RetainedString(std::string(id.view()) + "." + key));
        auto diagnostic = std::string(id.view());
        diagnostic.append(message, M - 1);
        require(actual == expected, RetainedString(std::move(diagnostic)));
    };
    const auto checkMembers = [&](RetainedString key, const Strings& entries, std::string_view message,
        bool present = true) {
        for (const auto& entry : entries) {
            const bool found = fixtureStringArrayContains(object, key, entry, id);
            auto diagnostic = std::string(id.view());
            diagnostic += message;
            diagnostic += entry.view();
            require(found == present, RetainedString(std::move(diagnostic)));
        }
    };
    checkString("referenceRepository", "C:/Users/siget/src/depth-draw",
        " must record the hard depth-draw repository path");
    checkMembers(RetainedString::fromStaticLiteral("referenceFiles"), Strings{
        {StaticLiteral{}, "scripts/depth-draw-cli.js"}, {StaticLiteral{}, "doc/api.md"},
        {StaticLiteral{}, "src/app/externalApi.js"}}, " must record searched depth-draw reference file: ");
    checkMembers(RetainedString::fromStaticLiteral("sourceSearchRoots"), Strings{
        {StaticLiteral{}, "src"}, {StaticLiteral{}, "doc"}, {StaticLiteral{}, "scripts"}},
        " must record searched depth-draw source root: ");
    checkMembers(RetainedString::fromStaticLiteral("blockedCapabilitySearchTerms"), Strings{
        {StaticLiteral{}, "Fit Z"}, {StaticLiteral{}, "fitZ"}, {StaticLiteral{}, "fit-z"}, {StaticLiteral{}, "gap"},
        {StaticLiteral{}, "depthOffset"}, {StaticLiteral{}, "depthScale"}, {StaticLiteral{}, "setLayerDepthTransform"},
        {StaticLiteral{}, "target-depth"}, {StaticLiteral{}, "targetDepth"}, {StaticLiteral{}, "winner"},
        {StaticLiteral{}, "winning"}}, " must record blocked capability search term: ");
    checkString("lastCapabilitySearchDate", "2026-07-05",
        " must record the latest hard depth-draw capability search date");
    checkMembers(RetainedString::fromStaticLiteral("supportedCliCommands"), Strings{
        {StaticLiteral{}, "composite"}, {StaticLiteral{}, "layer-depth"}, {StaticLiteral{}, "layer-mesh"},
        {StaticLiteral{}, "meshes"}, {StaticLiteral{}, "set-layer-depth-mesh"}}, " must record depth-draw CLI command: ");
    checkMembers(RetainedString::fromStaticLiteral("supportedApiActions"), Strings{
        {StaticLiteral{}, "getCompositeSource"}, {StaticLiteral{}, "getLayerMesh"},
        {StaticLiteral{}, "getAllLayerMeshes"}, {StaticLiteral{}, "setLayerDepthMesh"}},
        " must record depth-draw API action: ");
    const auto unsupportedCommandsField = fixtureRequired(object,
        RetainedString::fromStaticLiteral("unsupportedCliCommands"), id);
    const auto unsupportedCommands = fixtureStringArray(unsupportedCommandsField,
        RetainedString(std::string(id.view()) + ".unsupportedCliCommands"));
    checkMembers(RetainedString::fromStaticLiteral("supportedCliCommands"), unsupportedCommands,
        " must not list unsupported depth-draw CLI command as supported: ", false);
    const auto unsupportedActionsField = fixtureRequired(object,
        RetainedString::fromStaticLiteral("unsupportedApiActions"), id);
    const auto unsupportedActions = fixtureStringArray(unsupportedActionsField,
        RetainedString(std::string(id.view()) + ".unsupportedApiActions"));
    checkMembers(RetainedString::fromStaticLiteral("supportedApiActions"), unsupportedActions,
        " must not list unsupported depth-draw API action as supported: ", false);
    checkString("targetGeometryFixtureRoute", "blocked",
        " must record that target-geometry fixture generation is blocked");
    checkString("fitZFixtureRoute", "blocked", " must record that Fit Z fixture generation is blocked");
    checkMembers(RetainedString::fromStaticLiteral("unsupportedCliCommands"), Strings{
        {StaticLiteral{}, "target-depths"}, {StaticLiteral{}, "final-target-depths"},
        {StaticLiteral{}, "per-vertex-layer-winners"}, {StaticLiteral{}, "fit-z"}, {StaticLiteral{}, "fit-z-to-gap"}},
        " must record unsupported depth-draw CLI command: ");
    checkMembers(RetainedString::fromStaticLiteral("unsupportedApiActions"), Strings{
        {StaticLiteral{}, "getTargetDepthArray"}, {StaticLiteral{}, "getFinalTargetDepths"},
        {StaticLiteral{}, "getPerVertexLayerWinners"}, {StaticLiteral{}, "fitZ"}, {StaticLiteral{}, "fitZToGap"},
        {StaticLiteral{}, "fitLayerDepthToGap"}}, " must record unsupported depth-draw API action: ");
    checkBool("meshExportAppliesShaderDepth", true,
        " must record that exported layer mesh positions include shader depth");
    checkBool("supportsLayerMeshes", true, " must record layer mesh export support");
    checkBool("supportsAllLayerMeshes", true, " must record all-layer mesh export support");
    checkBool("supportsLayerDepthMeshWriteback", true, " must record per-layer depth mesh writeback support");
    checkString("psdWritebackRoute", "browser-js-ag-psd-only",
        " must record that layered PSD writeback is browser-side ag-psd only");
    checkBool("supportsBrowserSideLayeredPsdWriteback", true,
        " must record depth-draw browser-side layered PSD writeback support");
    checkBool("supportsNativeDLayeredPsdWriteback", false,
        " must not claim native D layered PSD writeback support until nijigenerate exposes a verified writer");
    checkBool("supportsFinalTargetDepthArrays", false,
        " must not claim final target depth array support until depth-draw exposes it");
    checkBool("supportsPerVertexLayerWinners", false,
        " must not claim per-vertex layer winner support until depth-draw exposes it");
    checkBool("supportsFitZToGap", false,
        " must not claim Fit Z to Gap support until depth-draw exposes it");
    return 1;
}

std::size_t runDepthDrawDepthConversionFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using nijigenerate::compat::RetainedString;
    const auto idField = fixtureRequired(object, RetainedString::fromStaticLiteral("id"), path);
    const auto id = fixtureString(idField, concatenateSourceStrings({path, RetainedString::fromStaticLiteral(".id")}));
    const auto rgbaField = fixtureRequired(object, RetainedString::fromStaticLiteral("rgba"), id);
    const auto rgba = fixtureByteArray(rgbaField, concatenateSourceStrings({id, RetainedString::fromStaticLiteral(".rgba")}));
    const auto expectedValue = fixtureRequired(object, RetainedString::fromStaticLiteral("expected"), id);
    require(expectedValue.is_object(), concatenateSourceStrings({id,
        RetainedString::fromStaticLiteral(".expected must be an object")}));
    const auto expected = expectedValue.sourceObjectRef();
    const auto equalBytes = [](const nijigenerate::compat::Slice<std::uint8_t>& actual,
        const nijigenerate::compat::Slice<std::uint8_t>& required) {
        if (actual.size() != required.size()) return false;
        for (std::size_t i = 0; i < actual.size(); ++i) if (actual[i] != required[i]) return false;
        return true;
    };
    const auto red = nijigenerate::io::depthimage::ngDepthDrawDepthPixelsFromRgbaRedSource(rgba);
    const auto expectedRedField = fixtureRequired(expected,
        RetainedString::fromStaticLiteral("depthPixelsFromImageData"),
        concatenateSourceStrings({id, RetainedString::fromStaticLiteral(".expected")}));
    const auto expectedRed = fixtureByteArray(expectedRedField,
        concatenateSourceStrings({id, RetainedString::fromStaticLiteral(".expected.depthPixelsFromImageData")}));
    require(equalBytes(red, expectedRed),
        concatenateSourceStrings({id, RetainedString::fromStaticLiteral(" depthPixelsFromImageData must use the red channel")}));
    const auto grayscale = nijigenerate::io::depthimage::ngDepthDrawDecodeGrayscaleDepthPixelsFromRgbaSource(rgba);
    const auto expectedGrayscaleField = fixtureRequired(expected,
        RetainedString::fromStaticLiteral("decodeGrayscaleDepthPixels"),
        concatenateSourceStrings({id, RetainedString::fromStaticLiteral(".expected")}));
    const auto expectedGrayscale = fixtureByteArray(expectedGrayscaleField,
        concatenateSourceStrings({id, RetainedString::fromStaticLiteral(".expected.decodeGrayscaleDepthPixels")}));
    require(equalBytes(grayscale, expectedGrayscale), concatenateSourceStrings({id,
        RetainedString::fromStaticLiteral(" decodeGrayscaleDepthPixels must average RGB and zero transparent pixels")}));
    return 1;
}

bool hasParameter(std::shared_ptr<nicxlive::core::param::Parameter> param) {
    const auto parameters = nijigenerate::project::incActivePuppet()->parameters;
    for (const auto& candidate : parameters) {
        if (candidate == param) return true;
    }
    return false;
}

nicxlive::core::common::SharedArray<std::uint32_t> maskOrder(
    std::shared_ptr<nicxlive::core::nodes::Part> part) {
    nicxlive::core::common::SharedArray<std::uint32_t> result;
    const auto masks = part->masks;
    for (const auto& mask : masks) result.push_back(mask.maskSrcUUID);
    return result;
}

void assertMaskOrder(std::shared_ptr<nicxlive::core::nodes::Part> part,
    std::span<const std::uint32_t> expected, nijigenerate::compat::RetainedString label) {
    const auto actual = maskOrder(part);
    bool same = actual.size() == expected.size();
    for (std::size_t i = 0; same && i < actual.size(); ++i) same = actual[i] == expected[i];
    require(same, label);
}

// Original regression.d19046-19116. The authored queue and real Thread are used.
void testMcpTaskQueueMainThreadDispatch() {
    using namespace nijigenerate::api::mcp::task;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ThrowableException;
    using nijigenerate::core::thread_context::Thread;
    ngMcpInitTask();
    auto value = std::allocate_shared<std::int32_t>(
        nicxlive::core::d_runtime::SourceAllocator<std::int32_t>{}, 0);
    ngMcpEnqueueAction(Action([value] { *value = 7; }));
    ngMcpProcessQueue();
    require(*value == 7, RetainedString::fromStaticLiteral("MCP task queue should execute queued actions"));
    bool directThrown = false;
    ngMcpEnqueueAction(Action([] {
        throw ThrowableException(RetainedString::fromStaticLiteral("direct queued failure"),
            RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), 19058);
    }));
    try { ngMcpProcessQueue(); }
    catch (const ThrowableException& error) {
        directThrown = error.msg.view().find("direct queued failure") != std::string_view::npos;
    }
    require(directThrown, RetainedString::fromStaticLiteral(
        "MCP task queue should not swallow direct queued action exceptions"));
    static bool done;
    static bool thrown;
    static std::int32_t result;
    done = false;
    thrown = false;
    result = 0;
    auto worker = Thread::createSourceDeferred([value] {
        return nijigenerate::compat::SharedDelegate<void()>([value] {
            try {
                result = ngRunInMainThread<std::int32_t>(MainThreadAction<std::int32_t>([value] {
                    *value = 11;
                    return 42;
                }));
            } catch (const ThrowableException&) { thrown = true; }
            done = true;
        });
    });
    worker->start();
    for (int index = 0; index < 1000; ++index) {
        ngMcpProcessQueue();
        if (done) break;
        Thread::sleep(std::chrono::milliseconds{1});
    }
    worker->join();
    require(done, RetainedString::fromStaticLiteral(
        "ngRunInMainThread should complete when the main queue is processed"));
    require(!thrown, RetainedString::fromStaticLiteral(
        "ngRunInMainThread should propagate successful result without exception"));
    require(result == 42 && *value == 11, RetainedString::fromStaticLiteral(
        "ngRunInMainThread should run the delegate on the queued main-thread path"));
    done = false;
    thrown = false;
    auto failingWorker = Thread::createSourceDeferred([] {
        return nijigenerate::compat::SharedDelegate<void()>([] {
            try {
                ngRunInMainThread<void>(MainThreadAction<void>([] {
                    throw ThrowableException(RetainedString::fromStaticLiteral("expected failure"),
                        RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), 19101);
                }));
            } catch (const ThrowableException&) { thrown = true; }
            done = true;
        });
    });
    failingWorker->start();
    for (int index = 0; index < 1000; ++index) {
        ngMcpProcessQueue();
        if (done) break;
        Thread::sleep(std::chrono::milliseconds{1});
    }
    failingWorker->join();
    require(done && thrown, RetainedString::fromStaticLiteral(
        "ngRunInMainThread should propagate queued delegate exceptions"));
}

void requireSourceContains(nijigenerate::compat::RetainedString path,
    nijigenerate::compat::RetainedString needle, nijigenerate::compat::RetainedString message) {
    const std::array segments{regressionRepoRoot(), path};
    const auto text = nijigenerate::compat::phobos::ngReadUtf8Text(
        nijigenerate::compat::phobos::ngBuildPath(segments));
    require(!text.empty() && text.view().find(needle.view()) != std::string_view::npos, message);
}

void requireSourceNotContains(nijigenerate::compat::RetainedString path,
    nijigenerate::compat::RetainedString needle, nijigenerate::compat::RetainedString message) {
    const std::array segments{regressionRepoRoot(), path};
    const auto text = nijigenerate::compat::phobos::ngReadUtf8Text(
        nijigenerate::compat::phobos::ngBuildPath(segments));
    require(text.empty() || text.view().find(needle.view()) == std::string_view::npos, message);
}

void requireSourceSectionNotContains(nijigenerate::compat::RetainedString path,
    nijigenerate::compat::RetainedString sectionStart, nijigenerate::compat::RetainedString sectionEnd,
    nijigenerate::compat::RetainedString needle, nijigenerate::compat::RetainedString message) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    const std::array segments{regressionRepoRoot(), path};
    const auto text = nijigenerate::compat::phobos::ngReadUtf8Text(
        nijigenerate::compat::phobos::ngBuildPath(segments));
    const auto start = countUntilSourceString(text, sectionStart);
    require(start >= 0, concatenateSourceStrings({
        RetainedString::fromStaticLiteral("Source section start is missing: "), sectionStart}));
    const auto tail = text.subslice(static_cast<std::size_t>(start), text.size());
    const auto end = countUntilSourceString(tail, sectionEnd);
    require(end > 0, concatenateSourceStrings({
        RetainedString::fromStaticLiteral("Source section end is missing after "), sectionStart,
        RetainedString::fromStaticLiteral(": "), sectionEnd}));
    const auto section = tail.subslice(0, static_cast<std::size_t>(end));
    require(section.empty() || section.view().find(needle.view()) == std::string_view::npos, message);
}

std::size_t runDepthDrawFixtureFile(nijigenerate::compat::RetainedString path);

void testDepthDrawFixtureRunner() {
    using nijigenerate::compat::RetainedString;
    using namespace nijigenerate::compat::phobos;
    const std::array segments{regressionRepoRoot(), RetainedString::fromStaticLiteral("source"),
        RetainedString::fromStaticLiteral("nijigenerate_tests"), RetainedString::fromStaticLiteral("fixtures"),
        RetainedString::fromStaticLiteral("depthdraw")};
    const auto fixtureDir = ngBuildPath(segments);
    require(ngFileExists(fixtureDir.view()), RetainedString::fromStaticLiteral("DepthDraw fixture directory should exist"));
    std::size_t fixtureCount = 0;
    std::size_t depthDrawFixtureCount = 0;
    for (auto entry : ngDirEntries(fixtureDir, SpanMode::shallow)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".json")) continue;
        const auto text = ngReadUtf8Text(entry.name().retainedString());
        const bool generatedBy = text.view().find("\"generatedBy\"") != std::string_view::npos;
        if (!generatedBy) continue;
        ++fixtureCount;
        depthDrawFixtureCount += runDepthDrawFixtureFile(entry.name().retainedString());
    }
    require(fixtureCount > 0, RetainedString::fromStaticLiteral("DepthDraw fixture runner should have at least one fixture"));
    if (depthDrawFixtureCount == 0)
        detail::writeLine(detail::standardError(), {"DepthDraw fixture runner: no generatedBy=depth-draw fixtures are present yet"});
}

bool sourceTreeContainsAny(nijigenerate::compat::RetainedString rootPath,
    nijigenerate::compat::ConstSlice<nijigenerate::compat::RetainedString> needles) {
    using namespace nijigenerate::compat::phobos;
    const std::array segments{regressionRepoRoot(), rootPath};
    const auto root = ngBuildPath(segments);
    for (auto entry : ngDirEntries(root, SpanMode::depth)) {
        if (!entry.isFile()) continue;
        const auto text = ngReadUtf8Text(entry.name().retainedString());
        for (const auto needle : needles) {
            if (!text.empty() && text.view().find(needle.view()) != std::string_view::npos) return true;
        }
    }
    return false;
}

// Original regression.d19118-19150. Module initialization belongs to the entry.
void testMcpFailedStartupRetry() {
    using namespace nijigenerate::api::mcp::server;
    using namespace nijigenerate::compat::phobos::socket;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::core::thread_context::Thread;
    ngMcpStop();
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto occupied = TcpSocket::createSource();
        nijigenerate::compat::ngRunWithScopeExit([&] {
            occupied->bind(InternetAddress::createSourceDeferred(
                [] { return RetainedString::fromStaticLiteral("127.0.0.1"); },
                [] { return std::uint16_t{0}; }));
            occupied->listen(1);
            const auto address = std::dynamic_pointer_cast<InternetAddress>(occupied->localAddress());
            const auto port = nijigenerate::compat::ngRequireObject(address)->port();
            ngMcpApplySettings(true, RetainedString::fromStaticLiteral("127.0.0.1"), port);
            for (int index = 0; index < 500; ++index) {
                if (!ngMcpIsRunning()) break;
                Thread::sleep(std::chrono::milliseconds{10});
            }
            require(!ngMcpIsRunning(), RetainedString::fromStaticLiteral(
                "An occupied port must leave MCP stopped after the failed thread exits"));
            occupied->close();
            ngMcpApplySettings(true, RetainedString::fromStaticLiteral("127.0.0.1"), port);
            bool connected = false;
            for (int index = 0; index < 500; ++index) {
                auto client = TcpSocket::createSource();
                try {
                    client->connect(InternetAddress::createSourceDeferred(
                        [] { return RetainedString::fromStaticLiteral("127.0.0.1"); },
                        [&] { return port; }));
                    connected = true;
                } catch (const nijigenerate::compat::ThrowableException&) {}
                client->close();
                if (connected || !ngMcpIsRunning()) break;
                Thread::sleep(std::chrono::milliseconds{10});
            }
            require(connected && ngMcpIsRunning(), RetainedString::fromStaticLiteral(
                "Applying the same settings must retry successfully once the port is free"));
        }, [&] { occupied->close(); });
    }, [] { ngMcpStop(); });
}

// Original regression.d20484-20515. This body is never invoked during porting.
void testPlatformCrashDumpGeneration() {
    using namespace nijigenerate::utils::crashdump;
    using namespace nijigenerate::compat::phobos;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-crashdump")};
    const auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    ngSetCrashDumpDirOverrideForTests(RetainedStringSlice(fixtureDir));
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto ex = std::allocate_shared<nijigenerate::compat::ThrowableException>(
            nicxlive::core::d_runtime::SourceAllocator<nijigenerate::compat::ThrowableException>{},
            RetainedString::fromStaticLiteral("regression crash"),
            RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), 20499);
        const auto dump = genCrashDump(ex, RetainedString::fromStaticLiteral("state"), std::int32_t{12});
        require(dump.view().find("=== Args State ===") != std::string_view::npos,
            RetainedString::fromStaticLiteral("crash dump should include state header"));
        require(dump.view().find("=== Exception ===") != std::string_view::npos,
            RetainedString::fromStaticLiteral("crash dump should include exception header"));
        require(dump.view().find("regression crash") != std::string_view::npos,
            RetainedString::fromStaticLiteral("crash dump should include exception message"));
        require(dump.view().find("\"state\"") != std::string_view::npos && dump.view().find("12") != std::string_view::npos,
            RetainedString::fromStaticLiteral("crash dump should serialize state arguments"));
        const auto dir = getCrashDumpDir();
        require(!dir.empty(), RetainedString::fromStaticLiteral("crash dump directory should resolve"));
        const auto path = genCrashDumpPath(RetainedStringSlice(
            RetainedString::fromStaticLiteral("nijigenerate-regression-crashdump")));
        require(path.view().find("nijigenerate-regression-crashdump") != std::string_view::npos &&
            path.view().ends_with(".txt"), RetainedString::fromStaticLiteral(
                "crash dump path should include filename and txt extension"));
        const auto written = writeCrashDump(RetainedStringSlice(
            RetainedString::fromStaticLiteral("nijigenerate-regression-crashdump")), ex,
            RetainedString::fromStaticLiteral("written"));
        require(ngFileExists(written.view()), RetainedString::fromStaticLiteral("writeCrashDump should write a file"));
        const auto text = ngReadUtf8Text(written.retainedString());
        require(text.view().find("written") != std::string_view::npos &&
            text.view().find("regression crash") != std::string_view::npos,
            RetainedString::fromStaticLiteral("written crash dump should contain state and exception"));
        nijigenerate::platform::ngAutosaveRemove(written);
    }, [&] {
        ngSetCrashDumpDirOverrideForTests({});
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}

void testColorBleedPreservesAlphaAndExtendsColor() {
    nicxlive::core::SourceTextureBytes pixels;
    pixels.resize(3 * 3 * 4);
    const auto center = (1 + 1 * 3) * 4;
    pixels[center + 0] = 200;
    pixels[center + 1] = 50;
    pixels[center + 2] = 25;
    pixels[center + 3] = 255;
    auto shallow = nicxlive::core::ShallowTexture(pixels, 3, 3, 4);
    nijigenerate::core::colorbleed::incColorBleedPixels(&shallow, 4);
    const auto top = (1 + 0 * 3) * 4;
    require(shallow.data[center + 0] == 200 && shallow.data[center + 3] == 255,
        nijigenerate::compat::RetainedString::fromStaticLiteral("color bleed should preserve opaque source pixel"));
    require(shallow.data[top + 0] == 200,
        nijigenerate::compat::RetainedString::fromStaticLiteral("color bleed should copy red channel into transparent neighbor"));
    require(shallow.data[top + 1] == 50,
        nijigenerate::compat::RetainedString::fromStaticLiteral("color bleed should copy green channel into transparent neighbor"));
    require(shallow.data[top + 2] == 25,
        nijigenerate::compat::RetainedString::fromStaticLiteral("color bleed should copy blue channel into transparent neighbor"));
    require(shallow.data[top + 3] == 0,
        nijigenerate::compat::RetainedString::fromStaticLiteral("color bleed should preserve transparent neighbor alpha"));
}

// Original regression.d1373-1376; both parses use the canonical std.json body.
bool jsonEquivalent(nijigenerate::compat::RetainedString a, nijigenerate::compat::RetainedString b) {
    const auto first = nijigenerate::autorig::json::detail::parseJSON(
        nijigenerate::compat::RetainedStringSlice(a));
    const auto second = nijigenerate::autorig::json::detail::parseJSON(
        nijigenerate::compat::RetainedStringSlice(b));
    return first == second;
}

std::shared_ptr<nicxlive::core::nodes::Part> newPart(nijigenerate::compat::RetainedString name) {
    auto part = nicxlive::core::nodes::Part::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    part->setName(name.toNullableString());
    return part;
}

std::shared_ptr<nicxlive::core::nodes::Part> newMeshPart(nijigenerate::compat::RetainedString name) {
    using namespace nicxlive::core;
    using namespace nicxlive::core::nodes;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    MeshData data;
    data.vertices = Vec2Array{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}};
    data.uvs = Vec2Array{Vec2{0, 0}, Vec2{1, 0}, Vec2{0, 1}};
    data.indices = {0, 1, 2};
    auto texture = Texture::createDataSourceDeferred(
        [] { return SourceTextureBytes{255, 255, 255, 255}; },
        [] { return 1; }, [] { return 1; }, [] { return 4; }, [] { return 4; },
        [] { return false; }, [] { return false; });
    auto part = Part::createDataSourceDeferred([&] { return data; },
        [&] { return nicxlive::common::SharedArray<std::shared_ptr<Texture>>{texture}; },
        [] { return std::uint32_t{0}; }, [] {
            return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
    part->setName(name.toNullableString());
    return part;
}

std::shared_ptr<nijigenerate::viewport::vertex::automesh::AutoMeshProcessor> findAutoMeshProcessor(
    nijigenerate::compat::RetainedString id) {
    const auto processors = nijigenerate::viewport::vertex::ngAutoMeshProcessors();
    for (const auto processor : processors) {
        if (nijigenerate::compat::ngRequireObject(processor)->procId() == id) return processor;
    }
    return {};
}

// Original regression.d18547-18584, including only the authored alpha pattern.
std::shared_ptr<nicxlive::core::nodes::Part> newAutoMeshAlphaPart(nijigenerate::compat::RetainedString name) {
    using namespace nicxlive::core;
    using namespace nicxlive::core::nodes;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    constexpr int width = 48;
    constexpr int height = 48;
    SourceTextureBytes pixels;
    pixels.resize(width * height * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto offset = (y * width + x) * 4;
            pixels[offset] = 255;
            pixels[offset + 1] = 255;
            pixels[offset + 2] = 255;
            const bool insideRect = x >= 10 && x <= 37 && y >= 8 && y <= 39;
            const bool insideBar = x >= 22 && x <= 26 && y >= 4 && y <= 43;
            pixels[offset + 3] = insideRect || insideBar ? 255 : 0;
        }
    }
    MeshData data;
    data.vertices = Vec2Array{Vec2{-24, -24}, Vec2{24, -24}, Vec2{24, 24}, Vec2{-24, 24}};
    data.uvs = Vec2Array{Vec2{0, 0}, Vec2{1, 0}, Vec2{1, 1}, Vec2{0, 1}};
    data.indices = {0, 1, 2, 0, 2, 3};
    auto texture = Texture::createDataSourceDeferred([&] { return pixels; },
        [] { return width; }, [] { return height; }, [] { return 4; }, [] { return 4; },
        [] { return false; }, [] { return false; });
    auto part = Part::createDataSourceDeferred([&] { return data; },
        [&] { return nicxlive::common::SharedArray<std::shared_ptr<Texture>>{texture}; },
        [] { return std::uint32_t{0}; }, [] {
            return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
    part->setName(name.toNullableString());
    return part;
}

// Original regression.d812-824; saving remains a call to the real texture codec.
void writeRegressionPng(nijigenerate::compat::RetainedString path, std::uint8_t r,
    std::uint8_t g, std::uint8_t b, std::int32_t width = 2, std::int32_t height = 2,
    std::uint8_t a = 255) {
    const auto area = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height));
    const auto length = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(area) * 4u);
    nicxlive::core::SourceTextureBytes pixels;
    pixels.resize(static_cast<std::size_t>(length));
    for (std::int32_t i = 0; i < area; ++i) {
        const auto offset = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i) * 4u);
        pixels[static_cast<std::size_t>(offset)] = r;
        pixels[static_cast<std::size_t>(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(offset) + 1u))] = g;
        pixels[static_cast<std::size_t>(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(offset) + 2u))] = b;
        pixels[static_cast<std::size_t>(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(offset) + 3u))] = a;
    }
    auto texture = nicxlive::core::ShallowTexture(pixels, width, height, 4);
    texture.save(path);
}

// Original regression.d826-828, using the canonical decoder and real file writer.
void writeRegressionFixtureBase64(nijigenerate::compat::RetainedString path,
    nijigenerate::compat::RetainedString encoded) {
    const auto actualPath = path;
    const auto bytes = nijigenerate::compat::phobos::ngDecodeBase64(
        nijigenerate::compat::RetainedStringSlice(encoded));
    nijigenerate::compat::phobos::ngFileWrite(nijigenerate::compat::RetainedStringSlice(actualPath),
        std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

// Original regression.d830-833, retaining the authored fixture literally.
void writeRegressionPsdFixture(nijigenerate::compat::RetainedString path) {
    writeRegressionFixtureBase64(path, nijigenerate::compat::RetainedString::fromStaticLiteral(
        "OEJQUwABAAAAAAAAAAMAAAABAAAAAQAIAAMAAAAAAAAAAAAAAAQAAAAAAAAAAAA="));
}

void testImageCodecRoundTrips() {
    using namespace nijigenerate::compat::phobos;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nicxlive::core::ShallowTexture;
    using nicxlive::core::Texture;
    const std::array directorySegments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-image-codecs")};
    const auto fixtureDir = ngBuildPath(directorySegments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const std::array sourceSegments{fixtureDir, RetainedString::fromStaticLiteral("source.png")};
        const auto sourcePng = ngBuildPath(sourceSegments);
        writeRegressionPng(sourcePng, 24, 128, 240, 5, 3);
        auto png = ShallowTexture(sourcePng, 4);
        require(png.width == 5 && png.height == 3 && png.convChannels == 4,
            RetainedString::fromStaticLiteral("PNG codec should load generated RGBA dimensions"));
        auto texture = Texture::createSourceDeferred([&] { return png; });
        const nijigenerate::compat::Slice<RetainedString> extensions{
            RetainedString::fromStaticLiteral("png"), RetainedString::fromStaticLiteral("tga")};
        for (const auto extension : extensions) {
            const std::array pathSegments{fixtureDir,
                concatenateSourceStrings({RetainedString::fromStaticLiteral("roundtrip."), extension})};
            const auto path = ngBuildPath(pathSegments);
            texture->save(path);
            const bool isTextureFile = ngFileExists(path.view()) && ngIsFile(path.view());
            require(isTextureFile, concatenateSourceStrings({extension,
                RetainedString::fromStaticLiteral(" codec should write a texture file")}));
            auto loaded = ShallowTexture(path, 4);
            const bool correctSize = loaded.width == 5 && loaded.height == 3;
            require(correctSize, concatenateSourceStrings({extension,
                RetainedString::fromStaticLiteral(" codec should preserve texture dimensions")}));
            const bool rgba = loaded.convChannels == 4;
            require(rgba, concatenateSourceStrings({extension,
                RetainedString::fromStaticLiteral(" codec should convert loaded texture to RGBA")}));
        }
        texture->dispose();
        const std::array psdSegments{regressionSourceRoot(RetainedString::fromStaticLiteral("io")),
            RetainedString::fromStaticLiteral("psd.d")};
        const auto psdReader = ngBuildPath(psdSegments);
        const std::array kraSegments{regressionSourceRoot(RetainedString::fromStaticLiteral("io")),
            RetainedString::fromStaticLiteral("kra.d")};
        const auto kraReader = ngBuildPath(kraSegments);
        require(ngFileExists(psdReader.view()) &&
            ngReadUtf8Text(psdReader).view().find("module nijigenerate.io.psd") != std::string_view::npos,
            RetainedString::fromStaticLiteral("PSD reader module should remain present for fixture-backed import scenarios"));
        require(ngFileExists(kraReader.view()) &&
            ngReadUtf8Text(kraReader).view().find("module nijigenerate.io.kra") != std::string_view::npos,
            RetainedString::fromStaticLiteral("KRA reader module should remain present for fixture-backed import scenarios"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}

// Original regression.d835-844.
nijigenerate::compat::RetainedString localDepthDrawDataPath(nijigenerate::compat::RetainedString filename) {
    using nijigenerate::compat::RetainedString;
    const auto userProfile = nijigenerate::platform::ngGetEnvironmentVariableRetained("USERPROFILE");
    if (!userProfile.view().empty()) {
        const std::array segments{userProfile, RetainedString::fromStaticLiteral("src"),
            RetainedString::fromStaticLiteral("depth-draw"), RetainedString::fromStaticLiteral("data"), filename};
        auto candidate = nijigenerate::compat::phobos::ngBuildPath(segments);
        if (nijigenerate::compat::phobos::ngFileExists(candidate.view())) return candidate;
    }
    return {};
}

// Original regression.d846-849, retaining the authored fixture literally.
void writeRegressionKraFixture(nijigenerate::compat::RetainedString path) {
    writeRegressionFixtureBase64(path, nijigenerate::compat::RetainedString::fromStaticLiteral(
        "UEsDBBQAAAAAALmEulyh8AOnEwAAABMAAAAIAAAAbWltZXR5cGVhcHBsaWNhdGlvbi94LWtyaXRhUEsDBBQAAAAAALmEulyi9NGxYAAAAGAAAAALAAAAbWFpbmRvYy54bWw8RE9DPjxJTUFHRSBuYW1lPSJyZWdyZXNzaW9uIiB3aWR0aD0iMSIgaGVpZ2h0PSIxIiBjb2xvcnNwYWNlbmFtZT0iUkdCQSI+PGxheWVycy8+PC9JTUFHRT48L0RPQz5QSwECFAMUAAAAAAC5hLpcofADpxMAAAATAAAACAAAAAAAAAAAAAAAgAEAAAAAbWltZXR5cGVQSwECFAMUAAAAAAC5hLpcovTRsWAAAABgAAAACwAAAAAAAAAAAAAAgAE5AAAAbWFpbmRvYy54bWxQSwUGAAAAAAIAAgBvAAAAwgAAAAAA"));
}

nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> directPartNames(
    std::shared_ptr<nicxlive::core::Puppet> puppet) {
    nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> result;
    const auto children = nijigenerate::compat::ngRequireObject(
        nijigenerate::compat::ngRequireObject(puppet)->root)->children;
    for (const auto child : children) {
        if (auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(child))
            result.append(nijigenerate::compat::RetainedString(part->getName()));
    }
    return result;
}

std::shared_ptr<nicxlive::core::nodes::Part> findDirectPart(
    std::shared_ptr<nicxlive::core::Puppet> puppet, std::string_view name) {
    const auto children = nijigenerate::compat::ngRequireObject(
        nijigenerate::compat::ngRequireObject(puppet)->root)->children;
    for (const auto child : children) {
        if (auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(child)) {
            if (part->getName() == name) return part;
        }
    }
    return nullptr;
}

std::shared_ptr<nicxlive::core::param::Parameter> findParameter(
    std::shared_ptr<nicxlive::core::Puppet> puppet, std::string_view name) {
    const auto parameters = nijigenerate::compat::ngRequireObject(puppet)->parameters;
    for (const auto param : parameters) {
        if (nijigenerate::compat::ngRequireObject(param)->getName() == name) return param;
    }
    return nullptr;
}

std::shared_ptr<nicxlive::core::nodes::Node> findDirectNode(
    std::shared_ptr<nicxlive::core::Puppet> puppet, std::string_view name) {
    const auto children = nijigenerate::compat::ngRequireObject(
        nijigenerate::compat::ngRequireObject(puppet)->root)->children;
    for (const auto child : children) {
        if (nijigenerate::compat::ngRequireObject(child)->getName() == name) return child;
    }
    return nullptr;
}

std::shared_ptr<nicxlive::core::nodes::Node> findNodeRecursive(
    std::shared_ptr<nicxlive::core::nodes::Node> root, std::string_view name) {
    if (nijigenerate::compat::ngRequireObject(root)->getName() == name) return root;
    const auto children = nijigenerate::compat::ngRequireObject(root)->children;
    for (const auto child : children) {
        auto found = findNodeRecursive(child, name);
        if (found) return found;
    }
    return nullptr;
}

bool isChildOf(std::shared_ptr<nicxlive::core::nodes::Node> parent,
    std::shared_ptr<nicxlive::core::nodes::Node> child) {
    for (const auto& candidate : parent->children) {
        if (candidate == child) return true;
    }
    return false;
}

void appendNodeSummary(std::shared_ptr<nicxlive::core::nodes::Node> root,
    nijigenerate::compat::Slice<nijigenerate::compat::RetainedString>& lines,
    nijigenerate::compat::RetainedString indent = nijigenerate::compat::RetainedString::emptyLiteral()) {
    using nijigenerate::compat::RetainedString;
    const auto receiver = nijigenerate::compat::ngRequireObject(root);
    const auto name = RetainedString(receiver->getName());
    const auto type = RetainedString(receiver->typeId());
    lines.append(concatenateSourceStrings({indent, name, RetainedString::fromStaticLiteral(":"), type}));
    const auto children = receiver->children;
    for (const auto child : children)
        appendNodeSummary(child, lines, concatenateSourceStrings({indent, RetainedString::fromStaticLiteral("  ")}));
}

nijigenerate::compat::RetainedString nodeTreeSummary(std::shared_ptr<nicxlive::core::nodes::Node> root) {
    nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> lines;
    appendNodeSummary(root, lines);
    return joinSourceStrings(std::span<const nijigenerate::compat::RetainedString>(lines.data(), lines.size()),
        nijigenerate::compat::RetainedString::fromStaticLiteral("\n"));
}

template<class T> bool treeContainsType(std::shared_ptr<nicxlive::core::nodes::Node> root) {
    if (std::dynamic_pointer_cast<T>(root)) return true;
    const auto children = nijigenerate::compat::ngRequireObject(root)->children;
    for (const auto child : children) {
        if (treeContainsType<T>(child)) return true;
    }
    return false;
}

nicxlive::core::math::Vec3 nodeWorldTranslation(std::shared_ptr<nicxlive::core::nodes::Node> node) {
    const auto mat = nijigenerate::compat::ngRequireObject(node)->transformNoLock().matrix();
    return {mat[0][3], mat[1][3], mat[2][3]};
}

nicxlive::core::math::Vec3 nodeRenderedWorldTranslation(
    std::shared_ptr<nicxlive::core::nodes::Node> node) {
    const auto mat = node->transform().matrix();
    return {mat[0][3], mat[1][3], mat[2][3]};
}

nijigenerate::core::selector::ResourceArray runSelector(nijigenerate::compat::RetainedString query) {
    using namespace nijigenerate::core::selector;
    auto selector = std::allocate_shared<Selector>(nicxlive::core::d_runtime::SourceAllocator<Selector>{});
    selector->build(SelectorString(query));
    return selector->run();
}

bool resourceNamed(nijigenerate::core::selector::ResourceArray resources,
    nijigenerate::compat::RetainedString name, nijigenerate::core::selector::ResourceType type) {
    for (const auto resource : resources) {
        const auto receiver = nijigenerate::compat::ngRequireObject(resource);
        if (receiver->type() == type && receiver->name() == name.toNullableString()) return true;
    }
    return false;
}

bool hasGroup(std::shared_ptr<nijigenerate::ext::ExParameterGroup> group) {
    const auto puppet = std::dynamic_pointer_cast<nijigenerate::ext::ExPuppet>(
        nijigenerate::project::incActivePuppet());
    const auto groups = puppet->groups;
    for (const auto& candidate : groups) {
        if (candidate == group) return true;
    }
    return false;
}

bool containsBinding(
    nicxlive::core::common::SharedArray<std::shared_ptr<nicxlive::core::param::ParameterBinding>> bindings,
    std::shared_ptr<nicxlive::core::param::ParameterBinding> target) {
    for (const auto& binding : bindings) {
        if (binding == target) return true;
    }
    return false;
}

nijigenerate::core::settings::SettingsJsonValue jsonVec2Value(float x, float y) {
    nijigenerate::core::settings::SettingsJsonValue value(
        nijigenerate::core::settings::SettingsJsonArray{});
    value.as_array().append(nijigenerate::core::settings::SettingsJsonValue(static_cast<double>(x)));
    value.as_array().append(nijigenerate::core::settings::SettingsJsonValue(static_cast<double>(y)));
    return value;
}

void runCase(nijigenerate::compat::RetainedString name, void (*test)()) {
    detail::writeLine(detail::standardError(), {"running ", name.view()});
    test();
}

template<class F>
requires std::is_class_v<std::remove_cvref_t<F>> && std::invocable<F&>
    && std::same_as<std::invoke_result_t<F&>, void>
    && (!std::same_as<std::remove_cvref_t<F>, std::function<void()>>)
void runCase(nijigenerate::compat::RetainedString name, F&& test) {
    detail::writeLine(detail::standardError(), {"running ", name.view()});
    std::invoke(test);
}

void printScenario(Scenario scenario) {
    detail::writeLine(detail::standardOutput(), {scenario.status.view(), "\t", scenario.id.view(), "\t",
        scenario.category.view(), "\t", scenario.title.view()});
    if (!scenario.note.empty()) detail::writeLine(detail::standardOutput(), {"\t", scenario.note.view()});
}

void printList() {
    const auto values = allScenarios();
    for (auto scenario : values) printScenario(scenario);
}

int printCoverage(bool requireAll) {
    std::size_t automatedCount = 0;
    std::size_t computerUseCount = 0;
    std::size_t pendingCount = 0;
    for (const auto& scenario : allScenarios()) {
        if (scenario.status.view() == automated) ++automatedCount;
        else if (scenario.status.view() == computerUse) ++computerUseCount;
        else ++pendingCount;
    }
    detail::writeLine(detail::standardOutput(), {"coverage:"});
    detail::writeUnsignedLine(detail::standardOutput(), "  total: ", allScenarios().size());
    detail::writeUnsignedLine(detail::standardOutput(), "  automated: ", automatedCount);
    detail::writeUnsignedLine(detail::standardOutput(), "  computer-use: ", computerUseCount);
    detail::writeUnsignedLine(detail::standardOutput(), "  pending: ", pendingCount);
    if (requireAll && pendingCount != 0) {
        detail::writeUnsignedLine(detail::standardError(), "regression catalog is incomplete: ", pendingCount,
            " pending scenarios are not assigned to an automated or computer-use runner");
        return 1;
    }
    return 0;
}

int usage() {
    detail::writeLine(detail::standardOutput(), {"usage:"});
    detail::writeLine(detail::standardOutput(), {"  nijigenerate-regression-tests"});
    detail::writeLine(detail::standardOutput(), {"  nijigenerate-regression-tests --list"});
    detail::writeLine(detail::standardOutput(), {"  nijigenerate-regression-tests --feature-inventory"});
    detail::writeLine(detail::standardOutput(), {"  nijigenerate-regression-tests --module-inventory"});
    detail::writeLine(detail::standardOutput(), {"  nijigenerate-regression-tests --computer-use-manifest"});
    detail::writeLine(detail::standardOutput(), {"  nijigenerate-regression-tests --coverage"});
    detail::writeLine(detail::standardOutput(), {"  nijigenerate-regression-tests --require-all"});
    detail::writeLine(detail::standardOutput(), {"  nijigenerate-regression-tests --only <scenario-id>"});
    detail::writeLine(detail::standardOutput(), {
        "  nijigenerate-regression-tests --computer-use-only <scenario-id> <driver-command> [args...]"});
    detail::writeLine(detail::standardOutput(), {"  nijigenerate-regression-tests --computer-use-driver <driver-command> [args...]"});
    return 2;
}

nijigenerate::core::settings::SettingsJsonValue scenarioToJson(Scenario scenario) {
    nijigenerate::core::settings::SettingsJsonObject object;
    object["id"] = nijigenerate::core::settings::SettingsString(scenario.id);
    object["category"] = nijigenerate::core::settings::SettingsString(scenario.category);
    object["title"] = nijigenerate::core::settings::SettingsString(scenario.title);
    object["status"] = nijigenerate::core::settings::SettingsString(scenario.status);
    object["note"] = nijigenerate::core::settings::SettingsString(scenario.note);
    return nijigenerate::core::settings::SettingsJsonValue(object);
}

nijigenerate::core::settings::SettingsJsonValue depthCommandOp(nijigenerate::compat::RetainedString type,
    nijigenerate::core::settings::SettingsJsonObject fields) {
    fields["type"] = nijigenerate::core::settings::SettingsJsonValue(
        nijigenerate::core::settings::SettingsString(type));
    return nijigenerate::core::settings::SettingsJsonValue(fields);
}

bool scopeCommandListContains(nijigenerate::core::settings::SettingsJsonArray commands, std::string_view tool) {
    for (auto command : commands) {
        auto& object = command.as_object();
        if (object.isOrdered()) {
            throw nijigenerate::core::settings::SettingsJsonError(
                "JSONValue object is ordered, cannot return by ref");
        }
        const auto* item = object.if_contains("tool");
        if (!item) throw nijigenerate::compat::RangeError();
        if (item->as_string().view() == tool) return true;
    }
    return false;
}

bool nearVec3(nicxlive::core::math::Vec3 a, nicxlive::core::math::Vec3 b) {
    return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
}

bool nearVec4(nicxlive::core::math::Vec4 a, nicxlive::core::math::Vec4 b) {
    return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z) && near(a.w, b.w);
}

bool nearVec2(nicxlive::core::math::Vec2 a, nicxlive::core::math::Vec2 b) {
    return near(a.x, b.x) && near(a.y, b.y);
}

bool nearFloatArray(std::span<const float> a, std::span<const float> b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!near(a[i], b[i])) return false;
    }
    return true;
}

bool nearMat4(nicxlive::core::math::Mat4 a, nicxlive::core::math::Mat4 b) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            if (!near(a[r][c], b[r][c])) return false;
        }
    }
    return true;
}

bool nearVec2Array(nicxlive::core::math::Vec2Array a, nicxlive::core::math::Vec2Array b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!nearVec2(a.get(i), b.get(i))) return false;
    }
    return true;
}

bool containsVec2(nicxlive::core::math::Vec2Array values, nicxlive::core::math::Vec2 expected) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (nearVec2(values.get(i), expected)) return true;
    }
    return false;
}

bool isRegressionIdentChar(std::uint8_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_';
}

bool containsConsoleWriteCall(std::string_view line) {
    for (const std::string_view needle : {"writefln(", "writeln(", "writef("}) {
        if (line.find(needle) != std::string_view::npos) return true;
    }
    return false;
}

std::int32_t braceDelta(std::string_view line) {
    std::uint32_t result = 0;
    for (const char ch : line) {
        if (ch == '{') ++result;
        else if (ch == '}') --result;
    }
    return std::bit_cast<std::int32_t>(result);
}
bool commandClassShouldBeUndoable(std::string_view className) {
    static constexpr std::string_view nonUndoableExact[] = {
        "AnimEditModeCommand",
        "AutoMeshApplyActiveCommand",
        "AutoMeshGetActiveCommand",
        "AutoMeshGetSchemaCommand",
        "AutoMeshGetValuesCommand",
        "AutoMeshListProcessorsCommand",
        "AttemptRepairPuppetCommand",
        "CaptureLiveScreenshotCommand",
        "CloseProjectCommand",
        "CopyBindingCommand",
        "CopyNodeCommand",
        "CopyParameterCommand",
        "CutNodeCommand",
        "ExportINPCommand",
        "ExportJPEGCommand",
        "ExportPNGCommand",
        "ExportTGACommand",
        "ExportVideoCommand",
        "FitViewportToModelCommand",
        "GenerateFakeLayerNameCommand",
        "GetDepthBoneInfluenceRuleCommand",
        "ImportINPCommand",
        "ImportImageFolderCommand",
        "ImportKRACommand",
        "ImportPSDCommand",
        "ImportSessionDataCommand",
        "ListCommandCommand",
        "ListDepthBoneSourcesCommand",
        "ListDepthBonesCommand",
        "ListFlipPairsCommand",
        "MergeINPCommand",
        "MergeImageFilesCommand",
        "MergeKRACommand",
        "MergePSDCommand",
        "ModelEditModeCommand",
        "NewFileCommand",
        "OpenAutomeshBatchingCommand",
        "OpenFileCommand",
        "OpenFlipPairWindowCommand",
        "PremultTextureCommand",
        "PreviewDepthBoneDeformCommand",
        "PreviewDepthBoneInfluenceCommand",
        "RebleedTextureCommand",
        "RedoCommand",
        "RegenerateMipmapsCommand",
        "RegenerateNodeIDsCommand",
        "ReloadNodeCommand",
        "ResetParametersCommand",
        "ResetPhysicsCommand",
        "ResetViewportPositionCommand",
        "ResetViewportZoomCommand",
        "SaveFileCommand",
        "SaveScreenshotCommand",
        "SelectToolModeCommand",
        "SetArmedParameterAndKeypointCommand",
        "SetDefaultLayoutCommand",
        "ShowCommandBrowserWindowCommand",
        "ShowExportToINPDialogCommand",
        "ShowExportToJpegDialogCommand",
        "ShowExportToPNGDialogCommand",
        "ShowExportToTGADialogCommand",
        "ShowExportToVideoDialogCommand",
        "ShowImportINPDialogCommand",
        "ShowImportImageFolderDialogCommand",
        "ShowImportKRADialogCommand",
        "ShowImportPSDDialogCommand",
        "ShowImportSessionDataDialogCommand",
        "ShowMergeINPDialogCommand",
        "ShowMergeImageFileDialogCommand",
        "ShowMergeKRADialogCommand",
        "ShowMergePSDDialogCommand",
        "ShowOpenFileDialogCommand",
        "ShowSaveFileAsDialogCommand",
        "ShowSaveFileDialogCommand",
        "ShowSaveScreenshotDialogCommand",
        "ShowSettingsWindowCommand",
        "ShowStatusForNerdsCommand",
        "ToggleDifferenceAggregationCommand",
        "ToggleMirrorViewCommand",
        "ToggleOnionSliceCommand",
        "TogglePanelVisibilityCommand",
        "ToggleParameterArmCommand",
        "TogglePhysicsCommand",
        "TogglePostProcessCommand",
        "UndoCommand",
        "VertexModeCommand",
    };
    for (const auto excluded : nonUndoableExact) {
        if (className == excluded) return false;
    }
    for (const std::string_view prefix : {"Show", "List", "Get", "Open", "Export", "Import", "Merge", "Save", "Capture"}) {
        if (className.starts_with(prefix)) return false;
    }
    return true;
}

bool commandModuleHasUndoPath(std::string_view text) {
    static constexpr std::string_view needles[] = {
        "incActionPush(",
        "incActionPushGroup(",
        "incActionPopGroup(",
        ".addAction(",
        ".redo();",
        "ngAddNodes(",
        "ngInsertNodes(",
        "ngConvertTo(",
        "incMoveChildrenWithHistory(",
        "incDeleteChildrenWithHistory(",
        "incDeleteChildWithHistory(",
        "pasteFromClipboard(",
        "ngApplyDrawableMeshFromCommand(",
        "ngApplyDeformableVerticesFromCommand(",
        "ensureApplyAutoMeshCommand(",
        "ngAnimationCreateOrUpdate(",
        "ngAnimationDelete(",
        "incAnimationKeyframeAdd(",
        "incAnimationKeyframeRemove(",
    };
    for (const auto needle : needles) {
        if (text.find(needle) != std::string_view::npos) return true;
    }
    return false;
}

nijigenerate::compat::RetainedString decodePoQuoted(std::string_view quoted) {
    if (quoted.size() < 2 || quoted.front() != '"' || quoted.back() != '"') return {};
    std::string result;
    for (std::size_t i = 1; i + 1 < quoted.size(); ++i) {
        const auto c = quoted[i];
        if (c == '\\' && i + 1 < quoted.size() - 1) {
            const auto next = quoted[++i];
            switch (next) {
            case 'n': result += '\n'; break;
            case 't': result += '\t'; break;
            case 'r': result += '\r'; break;
            case '"': result += '"'; break;
            case '\\': result += '\\'; break;
            default: result += next; break;
            }
        } else result += c;
    }
    if (result.empty()) return {};
    return nijigenerate::compat::RetainedString(std::move(result));
}

nijigenerate::compat::RetainedString parseDStringLiteral(std::string_view line, std::size_t start) {
    if (start >= line.size() || line[start] != '"') return {};
    std::string result;
    for (std::size_t i = start + 1; i < line.size(); ++i) {
        const auto c = line[i];
        if (c == '\\' && i + 1 < line.size()) {
            const auto next = line[++i];
            switch (next) {
            case 'n': result += '\n'; break;
            case 't': result += '\t'; break;
            case 'r': result += '\r'; break;
            case '"': result += '"'; break;
            case '\\': result += '\\'; break;
            default: result += next; break;
            }
        } else if (c == '"') {
            if (result.empty()) return {};
            return nijigenerate::compat::RetainedString(std::move(result));
        } else result += c;
    }
    return {};
}
nijigenerate::compat::RetainedString scenarioPrefixForCommandModule(std::string_view rel) {
    if (rel == "commands/command_scope.d")
        return nijigenerate::compat::RetainedString::fromStaticLiteral("api");
    if (rel.starts_with("commands/automesh/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("automesh");
    if (rel.starts_with("commands/binding/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("parameter");
    if (rel.starts_with("commands/depth/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone");
    if (rel.starts_with("commands/inspector/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("inspectors");
    if (rel.starts_with("commands/mesheditor/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("mesh");
    if (rel.starts_with("commands/model/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("parameter");
    if (rel.starts_with("commands/node/mask.d") || rel.starts_with("commands/node/welding.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("part");
    if (rel.starts_with("commands/node/simplephysics.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("simplephysics");
    if (rel.starts_with("commands/node/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("node");
    if (rel.starts_with("commands/parameter/animedit.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("animation");
    if (rel.starts_with("commands/parameter/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("parameter");
    if (rel.starts_with("commands/puppet/file.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("project");
    if (rel.starts_with("commands/puppet/tool.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("project");
    if (rel.starts_with("commands/puppet/view.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("viewport");
    if (rel.starts_with("commands/puppet/edit.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("undo");
    if (rel.starts_with("commands/vertex/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("mesh");
    if (rel.starts_with("commands/view/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("panels");
    if (rel.starts_with("commands/viewport/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("viewport");
    return nijigenerate::compat::RetainedString::fromStaticLiteral("");
}

bool isAllowedDirectMutation(std::string_view rel, std::string_view line) {
    for (const std::string_view allowed : {
"commands/depth/bone.d|deformable.deformation = offsets",
        "commands/depth/bone.d|target.deformation = offsets",
        "commands/depth/bone.d|target.deformation = job.offsets",
        "commands/depth/bone.d|job.keypoints = keypoints",
        "commands/depth/bone.d|packet.vertices = vertices",
        "commands/depth/bone.d|packet.vertices = target.vertices.dup",
        "commands/depth/bone.d|root.name = name.length",
        "commands/depth/bone.d|deformable.deformation = generateInfluencePreviewOffsets",
        "commands/depth/bone.d|pivot.rotation = setting.rotation",
        "commands/depth/bone.d|setting.rotation = strictJsonNumber",
        "commands/depth/bone.d|setting.rotation = normalizeDepthBoneSourceRotation",
        "commands/node/base.d|newChild.localTransform.translation =",
        "commands/parameter/base.d|parent.children = parent.children.remove",
        "commands/parameter/base.d|incActivePuppet().parameters = incActivePuppet().parameters.remove",
        "commands/parameter/prop.d|param.name = newName",
        "commands/puppet/file.d|binding.node).name = binding.layer.name",
        "commands/puppet/file.d|binding.node.localTransform.translation =",
        "commands/puppet/file.d|part.localTransform.translation = localPosition",
        "commands/puppet/file.d|settings.scale = scale",
        "commands/viewport/control.d|camera.scale = vec2(zoom)",
        "commands/viewport/control.d|camera.rotation = 0",
        "panels/nodes.d|pctx.parameters = [param]",
        "panels/parameters.d|ctx.parameters =",
        "panels/inspector/griddeform.d|setting.rotation = normalizeDepthBoneSourceRotation",
        "panels/inspector/pathdeform.d|setting.rotation = normalizeDepthBoneSourceRotation",
        "panels/viewport.d|camera.scale =",
        "viewport/base.d|camera.scale = vec2(incViewportZoom)",
        "viewport/common/transformhandle.d|node.localTransform.scale = scale",
        "viewport/common/transformhandle.d|camera.localTransform.scale = vec2",
        "viewport/common/transformhandle.d|child.localTransform.translation =",
        "viewport/common/transformhandle.d|child.localTransform.rotation =",
        "viewport/common/transformhandle.d|child.localTransform.scale =",
        "viewport/common/mesh.d|data.vertices = positions.dup",
        "viewport/common/mesheditor/operations/impl.d|deform.vertices = value",
        "viewport/common/mesheditor/operations/impl.d|node.transform.translation =",
        "viewport/common/mesheditor/tools/grid.d|deformable.vertices =",
        "viewport/common/mesheditor/tools/onetimedeform.d|backup.binding.values = backup.values",
        "viewport/depth/draw/gpu.d|clone.vertices = packet.vertices.dup",
        "viewport/depth/draw/gpu.d|packet.vertices = target.getVertices()",
        "viewport/depth/mesheditor/node.d|offscreenCamera.scale =",
        "viewport/depth/mesheditor/node.d|offscreenCamera.rotation = 0",
        "viewport/depth/renderer.d|offscreenCamera.scale =",
        "viewport/depth/renderer.d|offscreenCamera.rotation = 0",
        "viewport/vertex/mesheditor/deformable.d|this.vertices = getTarget().getVertices().toMVertices",
        "viewport/vertex/mesheditor/drawable.d|mesh.vertices = indexedVerts",
        "viewport/vertex/package.d|part.deformation = originalDeform",
        "viewport/vertex/package.d|deformEditor.vertices = ngMeshVerticesFromPositions",
        "viewport/vertex/package.d|targetEditor.vertices = ngMeshVerticesFromPositions",
        "viewport/vertex/package.d|mesh.vertices = source.vertices",
        "windows/command_browser.d|info.name =",
        "windows/command_browser.d|ctx.parameters = paramsOverride",
        "windows/editanim.d|this.name =",
        "windows/flipconfig.d|this.name = name",
        "windows/inpexport.d|settings.scale =",
        "windows/kramerge.d|binding.node).name = binding.layer.name",
        "windows/kramerge.d|binding.node.localTransform.translation =",
        "windows/kramerge.d|part.localTransform.translation = localPosition",
        "windows/parameditor.d|ctx.parameters = [param]",
        "windows/psdmerge.d|binding.node).name = binding.layer.name",
        "windows/psdmerge.d|binding.node.localTransform.translation =",
        "windows/psdmerge.d|part.localTransform.translation = localPosition",
    }) {
        std::vector<std::string_view> parts;
        std::size_t first = 0;
        for (;;) {
            const auto next = allowed.find('|', first);
            if (next == std::string_view::npos) { parts.push_back(allowed.substr(first)); break; }
            parts.push_back(allowed.substr(first, next - first));
            first = next + 1;
        }
        if (parts.size() == 2 && rel == parts[0] && line.find(parts[1]) != std::string_view::npos)
            return true;
    }
    return false;
}

nijigenerate::compat::RetainedString scenarioPrefixForSourceModule(std::string_view rel) {
    if (rel == "package.d" || rel.ends_with("/package.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("coverage");
    if (rel == "ver.d")
        return nijigenerate::compat::RetainedString::fromStaticLiteral("platform");
    if (rel == "project.d")
        return nijigenerate::compat::RetainedString::fromStaticLiteral("project");
    if (rel == "config.d")
        return nijigenerate::compat::RetainedString::fromStaticLiteral("settings");
    if (rel == "regression_smoke.d")
        return nijigenerate::compat::RetainedString::fromStaticLiteral("platform");
    if (rel.starts_with("actions/depthbone.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone");
    if (rel.starts_with("actions/depth.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("depth");
    if (rel.starts_with("actions/mesh.d") || rel.starts_with("actions/mesheditor.d") || rel.starts_with("actions/vertex.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("mesh");
    if (rel.starts_with("actions/binding.d") || rel.starts_with("actions/parameter.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("parameter");
    if (rel.starts_with("actions/deformable.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("deform");
    if (rel.starts_with("actions/drawable.d") || rel.starts_with("actions/node.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("node");
    if (rel.starts_with("actions/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("undo");
    if (rel.starts_with("api/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("api");
    if (rel.starts_with("atlas/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("atlas");
    if (rel.starts_with("backend/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("render");
    if (rel == "commands/base.d" || (rel.starts_with("commands/") && rel.ends_with("/base.d")))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("coverage");
    if (rel.starts_with("commands/"))
        return scenarioPrefixForCommandModule(rel);
    if (rel.starts_with("core/actionstack.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("undo");
    if (rel.starts_with("core/asyncderivedupdate.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("async");
    if (rel.starts_with("core/cv/") || rel.starts_with("core/math/") || rel.starts_with("core/selector/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("core");
    if (rel.starts_with("core/shortcut/") || rel.starts_with("core/settings.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("settings");
    if (rel.starts_with("core/i18n.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("i18n");
    if (rel.starts_with("core/input.d") || rel.starts_with("core/window.d") || rel.starts_with("core/tasks.d") ||
        rel.starts_with("core/dpi.d") || rel.starts_with("core/font.d") || rel.starts_with("core/logo.d") ||
        rel.starts_with("core/path.d") || rel.starts_with("core/dbg.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("platform");
    if (rel.starts_with("core/colorbleed.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("atlas");
    if (rel.starts_with("core/fmt.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("widgets");
    if (rel.starts_with("ext/nodes/exdepthbone.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone");
    if (rel.starts_with("ext/nodes/exdepthmapped.d") || rel.starts_with("ext/nodes/exdepthops.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("depth");
    if (rel.starts_with("ext/nodes/exgriddeformer.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("deform");
    if (rel.starts_with("ext/nodes/expart.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("part");
    if (rel.starts_with("ext/nodes/excamera.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("render");
    if (rel.starts_with("ext/param.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("parameter");
    if (rel.starts_with("ext/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("node");
    if (rel.starts_with("io/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("io");
    if (rel.starts_with("panels/inspector/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("inspectors");
    if (rel.starts_with("panels/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("panels");
    if (rel.starts_with("utils/crashdump.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("platform");
    if (rel.starts_with("utils/repair.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("project");
    if (rel.starts_with("utils/transform.d") || rel.starts_with("utils/link.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("core");
    if (rel.starts_with("viewport/vertex/automesh/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("automesh");
    if (rel.starts_with("viewport/asyncderivedupdateoverlay.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("async");
    if (rel.starts_with("viewport/depth/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("depth");
    if (rel.starts_with("viewport/common/mesheditor/brushes/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("mesh");
    if (rel.starts_with("viewport/common/mesheditor/tools/onetimedeform.d") ||
        rel.starts_with("viewport/common/mesheditor/tools/pathdeform.d") ||
        rel.starts_with("viewport/common/mesheditor/tools/grid.d") ||
        rel.starts_with("viewport/common/mesheditor/tools/bezierdeform.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("deform");
    if (rel.starts_with("viewport/common/mesheditor/") || rel.starts_with("viewport/vertex/mesheditor/") ||
        rel.starts_with("viewport/model/mesheditor/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("mesh");
    if (rel.starts_with("viewport/common/mesh.d") || rel.starts_with("viewport/common/spline.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("mesh");
    if (rel.starts_with("viewport/common/transformhandle.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("viewport");
    if (rel.starts_with("viewport/model/onionslice.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("render");
    if (rel.starts_with("viewport/model/") || rel.starts_with("viewport/anim/") || rel.starts_with("viewport/base.d") ||
        rel.starts_with("viewport/vertex/package.d"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("viewport");
    if (rel.starts_with("widgets/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("widgets");
    if (rel.starts_with("windows/"))
        return nijigenerate::compat::RetainedString::fromStaticLiteral("windows");
    return nijigenerate::compat::RetainedString::fromStaticLiteral("");
}

nijigenerate::compat::RetainedString normalizeRegressionSourcePath(
    nijigenerate::compat::RetainedString path, nijigenerate::compat::RetainedString root) {
    using nijigenerate::compat::RetainedString;
    auto relative = nijigenerate::compat::phobos::ngRelativePath(path, [&] { return root; });
    relative = replaceSourcePathSeparators(relative);
    const auto prefix = RetainedString::fromStaticLiteral("source/nijigenerate/");
    const auto prefixIndex = countUntilSourceString(relative, prefix);
    if (prefixIndex >= 0)
        relative = relative.subslice(static_cast<std::size_t>(prefixIndex) + prefix.size(), relative.size());
    while (relative.view().starts_with("../")) relative = relative.subslice(3, relative.size());
    return relative;
}

// Validation remains canonical and bounded to visited first scalars; the
// remaining narrow-string startsWith comparison is byte-wise in Phobos.
std::ptrdiff_t countUntilRegressionNeedle(std::string_view line, std::string_view needle) {
    return countUntilSourceString(nijigenerate::compat::RetainedString::fromBorrowedView(line),
        nijigenerate::compat::RetainedString::fromBorrowedView(needle));
}

nicxlive::core::common::SharedArray<nijigenerate::compat::RetainedString>
extractSimpleI18nLiterals(std::string_view line) {
    nicxlive::core::common::SharedArray<nijigenerate::compat::RetainedString> result;
    for (const std::string_view marker : {"_(", "__("}) {
        std::size_t cursor = 0;
        while (cursor < line.size()) {
            const auto index = countUntilRegressionNeedle(line.substr(cursor), marker);
            if (index < 0) break;
            auto start = cursor + static_cast<std::size_t>(index) + marker.size();
            while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
            auto literal = parseDStringLiteral(line, start);
            if (!literal.isNull() && !literal.empty()) result.push_back(std::move(literal));
            cursor = start + 1;
        }
    }
    return result;
}

bool containsDirectMutationNeedle(std::string_view line) {
    for (const std::string_view needle : {".name =", ".translation =", ".rotation =", ".scale =",
            ".vertices =", ".deformation =", ".parameters =", ".children =", ".values =", ".keypoints ="}) {
        const auto index = countUntilRegressionNeedle(line, needle);
        if (index >= 0 && (static_cast<std::size_t>(index) + needle.size() >= line.size() ||
                          line[static_cast<std::size_t>(index) + needle.size()] != '='))
            return true;
    }
    return false;
}

constinit thread_local std::uint32_t fakeDepthBoneGpuNextJobId = 0;
thread_local detail::UIntAssociativeArray<std::size_t> fakeDepthBoneGpuJobVertexCounts;
constinit thread_local std::uint32_t fakeDepthBoneGpuSubmitCount = 0;
constinit thread_local std::uint32_t fakeDepthBoneGpuPollCount = 0;
constinit thread_local std::uint32_t fakeDepthBoneGpuCancelCount = 0;

void fakeDepthBoneGpuCancel(std::uint32_t jobId) {
    fakeDepthBoneGpuJobVertexCounts.remove(jobId);
    ++fakeDepthBoneGpuCancelCount;
}
constinit thread_local std::uint32_t fakeDepthBoneGpuSubmitFailAfter = 0;
constinit thread_local std::uint32_t fakeDepthBoneGpuNotReadyPolls = 0;
constinit thread_local float fakeDepthBoneGpuOutputX = 2.0f;
constinit thread_local float fakeDepthBoneGpuOutputY = -1.0f;
constinit thread_local float fakeDepthBoneGpuLastSourceDepthOffset = std::numeric_limits<float>::quiet_NaN();
constinit thread_local float fakeDepthBoneGpuLastSourceSinRotation = std::numeric_limits<float>::quiet_NaN();
constinit thread_local float fakeDepthBoneGpuLastSourceCosRotation = std::numeric_limits<float>::quiet_NaN();
constinit thread_local bool fakeDepthBoneGpuVaryOutputByVertex = false;
constinit thread_local bool fakeDepthBoneGpuMutateDepthOnReady = false;
thread_local nicxlive::core::common::SharedArray<float> fakeDepthBoneGpuLastBones;
thread_local std::shared_ptr<nijigenerate::ext::nodes::ExGridDeformer> fakeDepthBoneGpuMutatingTarget;
constinit thread_local std::uint32_t fakeDepthBoneTransformNextJobId = 1;
constinit thread_local std::uint32_t fakeDepthBoneTransformSubmitCount = 0;
constinit thread_local std::uint32_t fakeDepthBoneTransformNonIdentitySourceCount = 0;
thread_local detail::UIntAssociativeArray<
    nijigenerate::commands::depth::bone_gpu_async::NgDepthBoneGpuAsyncResult> fakeDepthBoneTransformResults;
thread_local nicxlive::core::common::SharedArray<float> fakeDepthBoneTransformLastZOffsets;
thread_local nicxlive::core::common::SharedArray<nicxlive::core::math::Vec3>
    fakeDepthBoneTransformLastDeformedPoints;
constinit thread_local std::uint32_t fakeDepthDrawGpuNextJobId = 0;
constinit thread_local std::uint32_t fakeDepthDrawGpuSubmitCount = 0;
constinit thread_local std::uint32_t fakeDepthDrawGpuPollCount = 0;
constinit thread_local std::uint32_t fakeDepthDrawGpuNotReadyPolls = 0;

nijigenerate::compat::RetainedString featureScenarioOwner(std::string_view kind, std::string_view rel) {
    if (kind == "command") return scenarioPrefixForCommandModule(rel);
    if (kind == "window") return nijigenerate::compat::RetainedString::fromStaticLiteral("windows");
    if (kind == "panel") {
        if (rel.starts_with("panels/inspector/"))
            return nijigenerate::compat::RetainedString::fromStaticLiteral("inspectors");
        return nijigenerate::compat::RetainedString::fromStaticLiteral("panels");
    }
    if (kind == "inspector") return nijigenerate::compat::RetainedString::fromStaticLiteral("inspectors");
    if (kind == "tool") return scenarioPrefixForSourceModule(rel);
    return nijigenerate::compat::RetainedString::fromStaticLiteral("");
}

// Reuse the canonical whitespace set without copying its table. Only the
// returned start pointer matters; restore the right end to obtain stripLeft.
std::string_view stripRegressionLeft(std::string_view input) {
    if (input.empty()) return input;
    const auto stripped = nijigenerate::compat::ngStripUnicode(input);
    return input.substr(static_cast<std::size_t>(stripped.data() - input.data()));
}

nijigenerate::compat::RetainedString extractRegressionClassName(
        nijigenerate::compat::RetainedString line) {
    const auto pos = countUntilRegressionNeedle(line.view(), "class ");
    if (pos < 0) return nijigenerate::compat::RetainedString::fromStaticLiteral("");
    auto rest = line.subslice(static_cast<std::size_t>(pos) + 6, line.size());
    const auto stripped = stripRegressionLeft(rest.view());
    const auto first = static_cast<std::size_t>(stripped.data() - rest.data());
    rest = rest.subslice(first, rest.size());
    std::size_t end = 0;
    while (end < rest.size() && isRegressionIdentChar(static_cast<std::uint8_t>(rest.view()[end]))) ++end;
    if (end == 0) return nijigenerate::compat::RetainedString::fromStaticLiteral("");
    return rest.subslice(0, end);
}

bool isRegressionCommandClassLine(std::string_view line) {
    const auto stripped = stripRegressionLeft(line);
    return stripped.starts_with("class ") && stripped.find(": ExCommand") != std::string_view::npos;
}

bool containsDirectEnglishLiteralAfter(std::string_view line, std::string_view marker) {
    const auto index = countUntilRegressionNeedle(line, marker);
    if (index < 0) return false;
    const auto rest = stripRegressionLeft(line.substr(static_cast<std::size_t>(index) + marker.size()));
    if (!rest.starts_with("\"")) return false;
    if (rest.size() < 2) return false;
    if (rest.starts_with("\"nijigenerate\"")) return false;
    const auto first = static_cast<std::uint8_t>(rest[1]);
    if (rest.size() > 2 && rest[2] == '"') return false;
    return (first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z');
}

constinit thread_local std::size_t fitZMutationCount = 0;
constinit thread_local nijigenerate::actions::depthboneinvalidation::DepthBoneMutationHook
    fitZForwardMutationHook = nullptr;
bool containsUnwrappedVisibleUiLiteral(std::string_view line) {
    for (const std::string_view call : {"incTooltip(", "incText(", "incTextDisabled(", "incTextWrapped(",
            "incTextLabel(", "incTextShadowed(", "incTextBordered(", "incBeginCategory(",
            "igMenuItem(", "igBeginMenu("}) {
        if (containsDirectEnglishLiteralAfter(line, call)) return true;
    }
    for (const std::string_view call : {"incTextColored(", "igTextColored(", "incDialog("}) {
        const auto index = countUntilRegressionNeedle(line, call);
        if (index < 0) continue;
        const auto rest = line.substr(static_cast<std::size_t>(index) + call.size());
        const auto comma = countUntilRegressionNeedle(rest, ",");
        if (comma < 0) continue;
        if (containsDirectEnglishLiteralAfter(rest.substr(static_cast<std::size_t>(comma) + 1), "")) return true;
    }
    return false;
}

void sortRegressionStrings(nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> values) {
    const auto less = [](const nijigenerate::compat::RetainedString& left,
        const nijigenerate::compat::RetainedString& right) { return left.view() < right.view(); };
    // Source string is a pointer/length descriptor, independent of this C++ owner.
    constexpr auto shortThreshold = std::max(std::size_t{32}, 1024 / (2 * sizeof(std::size_t)));
    nijigenerate::core::selector::detail::phobosQuickSort(
        std::span<nijigenerate::compat::RetainedString>(values.data(), values.size()), less, values.size(), shortThreshold);
}

nijigenerate::compat::RetainedStringMap<nijigenerate::compat::RetainedString> readPotMsgids(
    nijigenerate::compat::RetainedString path) {
    using nijigenerate::compat::RetainedString;
    const auto lines = splitSourceLines(nijigenerate::compat::phobos::ngReadUtf8Text(path));
    nijigenerate::compat::RetainedStringMap<RetainedString> result;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto stripped = stripRegressionLeft(lines[index].view());
        if (!stripped.starts_with("msgid ")) continue;
        auto value = decodePoQuoted(stripped.substr(6));
        auto next = index + 1;
        while (next < lines.size()) {
            const auto continuation = stripRegressionLeft(lines[next].view());
            if (!continuation.starts_with("\"")) break;
            const auto decoded = decodePoQuoted(continuation);
            value = concatenateSourceStrings({value, decoded});
            ++next;
        }
        result.setRetained(value, value);
        index = next > index ? next - 1 : index;
    }
    return result;
}

nijigenerate::compat::RetainedStringMap<nijigenerate::compat::Slice<nijigenerate::compat::RetainedString>>
collectFullFeatureScenarioInventory(nijigenerate::compat::Slice<nijigenerate::compat::RetainedString>& failures) {
    using nijigenerate::compat::RetainedString;
    using Strings = nijigenerate::compat::Slice<RetainedString>;
    using namespace nijigenerate::compat::phobos;
    failures = {};
    const auto root = regressionSourceRoot(RetainedString::emptyLiteral());
    nijigenerate::compat::RetainedStringMap<Strings> inventory;
    const auto add = [&](RetainedString kind, RetainedString relative, RetainedString name) {
        if (!inventory.contains(kind.view())) inventory.setRetained(kind, {});
        auto items = inventory.get(kind.view());
        items.append(concatenateSourceStrings({relative, RetainedString::fromStaticLiteral(":"), name}));
        inventory.setRetained(kind, items);
        const auto prefix = featureScenarioOwner(kind.view(), relative.view());
        if (prefix.empty()) {
            failures.append(concatenateSourceStrings({RetainedString::fromStaticLiteral("feature has no scenario owner: "),
                kind, RetainedString::fromStaticLiteral(" "), relative, RetainedString::fromStaticLiteral(":"), name}));
        } else if (!hasScenarioPrefix(prefix)) {
            failures.append(concatenateSourceStrings({RetainedString::fromStaticLiteral("feature owner has no scenario family: "),
                kind, RetainedString::fromStaticLiteral(" "), relative, RetainedString::fromStaticLiteral(":"), name,
                RetainedString::fromStaticLiteral(" -> "), prefix}));
        }
    };
    for (auto entry : ngDirEntries(root, SpanMode::depth)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".d")) continue;
        const auto relative = normalizeRegressionSourcePath(entry.name().retainedString(), root);
        for (const auto line : splitSourceLines(ngReadUtf8Text(entry.name().retainedString()))) {
            const auto name = extractRegressionClassName(line);
            if (name.empty()) continue;
            if (relative.view().starts_with("commands/") && name.view().ends_with("Command") && name.view() != "ExCommand") {
                add(RetainedString::fromStaticLiteral("command"), relative, name);
            } else if (relative.view().starts_with("windows/") && name.view() != "Window" &&
                (name.view().ends_with("Window") || name.view().ends_with("Modal"))) {
                add(RetainedString::fromStaticLiteral("window"), relative, name);
            } else if (relative.view().starts_with("panels/") && name.view() != "Panel" && name.view().ends_with("Panel")) {
                add(RetainedString::fromStaticLiteral("panel"), relative, name);
            } else if (relative.view().starts_with("panels/inspector/") &&
                (name.view() == "NodeInspector" || name.view() == "PuppetInspector" || name.view() == "BaseInspector" ||
                 name.view() == "InspectorHolder" || name.view() == "TypedInspector")) {
                add(RetainedString::fromStaticLiteral("inspector"), relative, name);
            } else if (relative.view().starts_with("viewport/") && name.view() != "Tool" && name.view().ends_with("Tool")) {
                add(RetainedString::fromStaticLiteral("tool"), relative, name);
            }
        }
    }
    return inventory;
}

nijigenerate::compat::RetainedStringMap<nijigenerate::compat::Slice<nijigenerate::compat::RetainedString>>
collectSourceModuleScenarioInventory(nijigenerate::compat::Slice<nijigenerate::compat::RetainedString>& failures) {
    using nijigenerate::compat::RetainedString;
    using Strings = nijigenerate::compat::Slice<RetainedString>;
    using namespace nijigenerate::compat::phobos;
    failures = {};
    const auto root = regressionSourceRoot(RetainedString::emptyLiteral());
    nijigenerate::compat::RetainedStringMap<Strings> inventory;
    for (auto entry : ngDirEntries(root, SpanMode::depth)) {
        if (!entry.isFile() || !entry.name().view().ends_with(".d")) continue;
        const auto relative = normalizeRegressionSourcePath(entry.name().retainedString(), root);
        const auto prefix = scenarioPrefixForSourceModule(relative.view());
        if (prefix.empty()) {
            failures.append(concatenateSourceStrings({RetainedString::fromStaticLiteral("source module has no scenario owner: "), relative}));
            continue;
        }
        if (!hasScenarioPrefix(prefix))
            failures.append(concatenateSourceStrings({RetainedString::fromStaticLiteral("source module owner has no scenario family: "),
                relative, RetainedString::fromStaticLiteral(" -> "), prefix}));
        if (!inventory.contains(prefix.view())) inventory.setRetained(prefix, {});
        auto modules = inventory.get(prefix.view());
        modules.append(relative);
        inventory.setRetained(prefix, modules);
    }
    inventory.forEachRetained([](RetainedString, Strings modules) { sortRegressionStrings(modules); });
    return inventory;
}

bool fakeDepthBoneGpuSupported() { return true; }
nicxlive::core::math::Vec2Array gridVerticesFromAxes(
        std::span<const float> axisX, std::span<const float> axisY) {
    nicxlive::core::math::Vec2Array result;
    for (const float y : axisY)
        for (const float x : axisX)
            result.push_back(nicxlive::core::math::Vec2{x, y});
    return result;
}

bool fakeDepthDrawGpuSupported() { return true; }
void testCoverageCompositeWorkflowInventory() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    Slice<RetainedString> failures;
    const Slice<RetainedString> requiredCompositeScenarios{
        {StaticLiteral{}, "coverage.composite-case-matrix"},
        {StaticLiteral{}, "mesh.composite-grid-mode-matrix"},
        {StaticLiteral{}, "node.composite-type-matrix"},
        {StaticLiteral{}, "parameter.composite-preset-matrix"},
        {StaticLiteral{}, "depth.composite-operation-matrix"},
        {StaticLiteral{}, "automesh.composite-processor-matrix-scenario"},
        {StaticLiteral{}, "simplephysics.composite-settings-matrix"},
        {StaticLiteral{}, "project.new-open-save"},
        {StaticLiteral{}, "project.composite-command-roundtrip"},
        {StaticLiteral{}, "project.composite-image-merge-export"},
        {StaticLiteral{}, "project.autosave-recovery"},
        {StaticLiteral{}, "project.import-inp"},
        {StaticLiteral{}, "project.merge-psd"},
        {StaticLiteral{}, "io.serialization-inx"},
        {StaticLiteral{}, "io.composite-import-export-roundtrip"},
        {StaticLiteral{}, "io.save-native"},
        {StaticLiteral{}, "node.composite-hierarchy-binding-roundtrip"},
        {StaticLiteral{}, "node.composite-type-matrix"},
        {StaticLiteral{}, "node.type-conversion"},
        {StaticLiteral{}, "node.cut-copy-paste-duplicate"},
        {StaticLiteral{}, "inspectors.composite-node-part-mesh"},
        {StaticLiteral{}, "part.composite-inspector-mesh-roundtrip"},
        {StaticLiteral{}, "parameter.composite-lifecycle-binding"},
        {StaticLiteral{}, "parameter.composite-preset-matrix"},
        {StaticLiteral{}, "parameter.binding-cleanup"},
        {StaticLiteral{}, "parameter.mesh-binding-composite"},
        {StaticLiteral{}, "parameter.keyframe-basic"},
        {StaticLiteral{}, "animation.composite-keyframe-history"},
        {StaticLiteral{}, "animation.track-binding-cleanup"},
        {StaticLiteral{}, "api.external-control"},
        {StaticLiteral{}, "api.mcp-mode-history"},
        {StaticLiteral{}, "api.mcp-resources"},
        {StaticLiteral{}, "mesh.define-grid-command"},
        {StaticLiteral{}, "mesh.define-mesh-command"},
        {StaticLiteral{}, "mesh.composite-grid-mode-matrix"},
        {StaticLiteral{}, "mesh.composite-topology-scope"},
        {StaticLiteral{}, "mesh.multi-object"},
        {StaticLiteral{}, "deform.composite-grid-path-binding"},
        {StaticLiteral{}, "deform.grid-tool"},
        {StaticLiteral{}, "deform.onetime-scope"},
        {StaticLiteral{}, "depth.composite-map-ops"},
        {StaticLiteral{}, "depth.composite-operation-matrix"},
        {StaticLiteral{}, "depth.commands"},
        {StaticLiteral{}, "depth.persistence"},
        {StaticLiteral{}, "depthbone.composite-source-preview"},
        {StaticLiteral{}, "depthbone.preview-commands"},
        {StaticLiteral{}, "depthbone.gpu-packet"},
        {StaticLiteral{}, "depthbone.gpu-all-keypoints"},
        {StaticLiteral{}, "depthbone.serialization"},
        {StaticLiteral{}, "automesh.composite-processor-matrix"},
        {StaticLiteral{}, "automesh.composite-processor-matrix-scenario"},
        {StaticLiteral{}, "automesh.non-part-targets"},
        {StaticLiteral{}, "automesh.batch-undo"},
        {StaticLiteral{}, "simplephysics.composite-roundtrip"},
        {StaticLiteral{}, "simplephysics.composite-settings-matrix"},
        {StaticLiteral{}, "simplephysics.serialization"},
        {StaticLiteral{}, "render.composite-camera-texture-export"},
        {StaticLiteral{}, "viewport.settings-command-composite"},
        {StaticLiteral{}, "viewport.action-history"},
        {StaticLiteral{}, "settings.composite-shortcuts-recent"},
        {StaticLiteral{}, "undo.composite-workflows"},
        {StaticLiteral{}, "undo.composite-cross-family-stack"},
        {StaticLiteral{}, "undo.scope-guards"},
        {StaticLiteral{}, "render.texture-lifecycle"},
        {StaticLiteral{}, "settings.shortcuts"},
    };
    for (const auto& id : requiredCompositeScenarios) {
        if (!hasScenarioId(id))
            failures.append(concatenateSourceStrings({
                RetainedString::fromStaticLiteral("missing composite workflow scenario: "), id}));
    }
    nijigenerate::compat::RetainedStringMap<Slice<RetainedString>> familyHits;
    for (const auto& id : requiredCompositeScenarios) {
        const auto dot = countUntilRegressionNeedle(id.view(), ".");
        if (dot > 0) {
            const auto family = id.subslice(0, static_cast<std::size_t>(dot));
            auto values = familyHits.contains(family.view()) ? familyHits.get(family.view()) : Slice<RetainedString>{};
            values.append(id);
            familyHits.set(family.view(), values);
        }
    }
    const Slice<RetainedString> requiredFamilies{
        {StaticLiteral{}, "api"}, {StaticLiteral{}, "automesh"}, {StaticLiteral{}, "deform"},
        {StaticLiteral{}, "depth"}, {StaticLiteral{}, "depthbone"}, {StaticLiteral{}, "io"},
        {StaticLiteral{}, "mesh"}, {StaticLiteral{}, "node"}, {StaticLiteral{}, "parameter"},
        {StaticLiteral{}, "project"}, {StaticLiteral{}, "render"}, {StaticLiteral{}, "settings"},
        {StaticLiteral{}, "simplephysics"}, {StaticLiteral{}, "undo"}, {StaticLiteral{}, "viewport"},
    };
    for (const auto& family : requiredFamilies) {
        if (!familyHits.contains(family.view()) || familyHits.get(family.view()).empty())
            failures.append(concatenateSourceStrings({
                RetainedString::fromStaticLiteral("composite workflow inventory missing family: "), family}));
    }
    const bool noFailures = failures.empty();
    auto joined = joinSourceStrings({failures.data(), failures.size()}, RetainedString::fromStaticLiteral("\n"));
    require(noFailures, concatenateSourceStrings({
        RetainedString::fromStaticLiteral("composite workflow inventory failures:\n"), joined}));
}

void ensureRegressionNodeTypesRegistered() {
    using namespace nicxlive::core::nodes;
    using nijigenerate::compat::RetainedString;
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("Node"),
        [](const std::shared_ptr<Node>& parent) { return Node::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("Mask"),
        [](const std::shared_ptr<Node>& parent) { return Mask::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("Composite"),
        [](const std::shared_ptr<Node>& parent) { return Composite::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("SimplePhysics"),
        [](const std::shared_ptr<Node>& parent) { return SimplePhysicsDriver::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("MeshGroup"),
        [](const std::shared_ptr<Node>& parent) { return MeshGroup::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("DynamicComposite"),
        [](const std::shared_ptr<Node>& parent) { return DynamicComposite::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("PathDeformer"),
        [](const std::shared_ptr<Node>& parent) { return PathDeformer::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("GridDeformer"),
        [](const std::shared_ptr<Node>& parent) { return GridDeformer::create(parent); });
    nijigenerate::ext::nodes::incInitExtNodes();
}

void testNodeDynamicAddTypesUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::commands::Context;
    using namespace nijigenerate::core::actionstack;
    ensureRegressionNodeTypesRegistered();
    for (const auto className : regressionNodeMenuTypes) {
        const auto check = [&](bool condition, std::string_view prefix, std::string_view suffix = {}) {
            require(condition, concatenateSourceStrings({RetainedString::fromBorrowedView(prefix), className,
                RetainedString::fromBorrowedView(suffix)}));
        };
        resetCase();
        auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
        ctx->puppet(nijigenerate::project::incActivePuppet());
        auto result = nijigenerate::commands::node::dynamic::ensureAddNodeCommand(className)->run(ctx);
        check(nijigenerate::compat::ngRequireObject(result)->succeeded, "dynamic AddNode should succeed for ");
        check(result->created.size() == 1, "dynamic AddNode should create exactly one node for ");
        auto created = result->created[0];
        check(created != nullptr, "dynamic AddNode should return a created node for ");
        const bool correctType = nijigenerate::compat::ngRequireObject(created)->typeId() == className.toNullableString();
        const auto typeMessage = concatenateSourceStrings({RetainedString::fromStaticLiteral(
            "dynamic AddNode should create requested type "), className, RetainedString::fromStaticLiteral(", got "),
            RetainedString(created->typeId())});
        require(correctType, typeMessage);
        check(created->parent.lock() == nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root,
            "dynamic AddNode should parent ", " under root when no selection exists");
        check(isChildOf(nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root, created),
            "dynamic AddNode should insert ", " into root children");
        incActionUndo();
        check(!isChildOf(nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root, created),
            "undo dynamic AddNode should remove ");
        incActionRedo();
        check(isChildOf(nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root, created),
            "redo dynamic AddNode should restore ");
    }
}

void testNodeDynamicInsertTypesUndoRedo() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::commands::Context;
    using nicxlive::core::nodes::Node;
    using namespace nijigenerate::core::actionstack;
    ensureRegressionNodeTypesRegistered();
    for (const auto className : regressionNodeMenuTypes) {
        const auto check = [&](bool condition, std::string_view prefix, std::string_view suffix = {}) {
            require(condition, concatenateSourceStrings({RetainedString::fromBorrowedView(prefix), className,
                RetainedString::fromBorrowedView(suffix)}));
        };
        resetCase();
        auto parent = nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        auto child = Node::createSourceDeferred([&] { return parent; });
        child->setName(concatenateSourceStrings({RetainedString::fromStaticLiteral("insert-child-"), className}).toNullableString());
        auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
        ctx->puppet(nijigenerate::project::incActivePuppet());
        ctx->nodes(Context::Nodes{child});
        auto result = nijigenerate::commands::node::dynamic::ensureInsertNodeCommand(className)->run(ctx);
        check(nijigenerate::compat::ngRequireObject(result)->succeeded, "dynamic InsertNode should succeed for ");
        check(result->created.size() == 1, "dynamic InsertNode should create exactly one node for ");
        auto inserted = result->created[0];
        check(inserted != nullptr, "dynamic InsertNode should return a created node for ");
        const bool correctType = nijigenerate::compat::ngRequireObject(inserted)->typeId() == className.toNullableString();
        require(correctType, concatenateSourceStrings({RetainedString::fromStaticLiteral(
            "dynamic InsertNode should create requested type "), className, RetainedString::fromStaticLiteral(", got "),
            RetainedString(inserted->typeId())}));
        check(inserted->parent.lock() == parent, "dynamic InsertNode should place ", " under the original parent");
        check(child->parent.lock() == inserted, "dynamic InsertNode should move the selected child under inserted ");
        check(isChildOf(inserted, child), "dynamic InsertNode should keep child in inserted node children for ");
        incActionUndo();
        check(child->parent.lock() == parent, "undo dynamic InsertNode should restore selected child parent for ");
        check(isChildOf(parent, child), "undo dynamic InsertNode should restore selected child in original parent for ");
        check(!isChildOf(parent, inserted), "undo dynamic InsertNode should remove inserted ");
        incActionRedo();
        check(inserted->parent.lock() == parent, "redo dynamic InsertNode should restore inserted ");
        check(child->parent.lock() == inserted, "redo dynamic InsertNode should move selected child under inserted ");
        check(isChildOf(inserted, child), "redo dynamic InsertNode should restore child list for ");
    }
}

void testNodeRegistryDynamicCommandCoverage() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::commands::Context;
    using namespace nijigenerate::commands::node::dynamic;
    ensureRegressionNodeTypesRegistered();
    for (const auto className : regressionNodeMenuTypes) {
        const auto check = [&](bool condition, std::string_view prefix) {
            require(condition, concatenateSourceStrings({RetainedString::fromBorrowedView(prefix), className}));
        };
        resetCase();
        auto addCommand = ensureAddNodeCommand(className);
        auto insertCommand = ensureInsertNodeCommand(className);
        check(addCommand != nullptr, "node registry should expose AddNode command for ");
        check(insertCommand != nullptr, "node registry should expose InsertNode command for ");
        check(!nijigenerate::compat::ngRequireObject(addCommand)->label().empty(), "AddNode command should have a label for ");
        check(!addCommand->description().empty(), "AddNode command should have a description for ");
        check(!nijigenerate::compat::ngRequireObject(insertCommand)->label().empty(), "InsertNode command should have a label for ");
        check(!insertCommand->description().empty(), "InsertNode command should have a description for ");
        check(addCommand->shortcutRunnable(), "dynamic AddNode command should be shortcut-visible for ");
        check(insertCommand->shortcutRunnable(), "dynamic InsertNode command should be shortcut-visible for ");
        check(addCommand->mcpExposed(), "dynamic AddNode command should be MCP-visible for ");
        check(insertCommand->mcpExposed(), "dynamic InsertNode command should be MCP-visible for ");
        const auto addId = nijigenerate::commands::ngCommandIdFromKey(AddNodeKey(className));
        const auto insertId = nijigenerate::commands::ngCommandIdFromKey(InsertNodeKey(className));
        const auto expectedAdd = concatenateSourceStrings({RetainedString::fromStaticLiteral("Node.Add."), className});
        const bool addMatches = std::string_view(addId) == expectedAdd.view();
        require(addMatches, concatenateSourceStrings({RetainedString::fromStaticLiteral(
            "AddNode key should produce a stable command id for "), className,
            RetainedString::fromStaticLiteral(": "), RetainedString::fromBorrowedView(addId)}));
        const auto expectedInsert = concatenateSourceStrings({RetainedString::fromStaticLiteral("Node.Insert."), className});
        const bool insertMatches = std::string_view(insertId) == expectedInsert.view();
        require(insertMatches, concatenateSourceStrings({RetainedString::fromStaticLiteral(
            "InsertNode key should produce a stable command id for "), className,
            RetainedString::fromStaticLiteral(": "), RetainedString::fromBorrowedView(insertId)}));
        auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
        ctx->puppet(nijigenerate::project::incActivePuppet());
        auto added = addCommand->run(ctx);
        check(nijigenerate::compat::ngRequireObject(added)->succeeded, "node registry AddNode construction should succeed for ");
        check(added->created.size() == 1 && added->created[0] != nullptr,
            "node registry AddNode should return a constructed node for ");
        check(nijigenerate::compat::ngRequireObject(added->created[0])->typeId() == className.toNullableString(),
            "node registry AddNode should construct requested type ");
    }
}

void testNodeCommandCreateDeleteToggleUndoRedo() {
    using namespace nijigenerate::commands::node::node;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::commands::Context;
    using nijigenerate::compat::RetainedString;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto root = [] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    };
    resetCase();
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    auto addResult = nijigenerate::commands::detail::constructSourceCommand<AddNodeCommand>(
        RetainedString::fromStaticLiteral("Node"))->run(ctx);
    check(nijigenerate::compat::ngRequireObject(addResult)->succeeded, "AddNodeCommand should succeed");
    auto node = addResult->created[0];
    check(nijigenerate::compat::ngRequireObject(node)->parent.lock() == root(), "AddNodeCommand should parent node under puppet root");
    check(isChildOf(root(), node), "AddNodeCommand should insert node into root children");
    incActionUndo();
    check(!isChildOf(root(), node), "undo AddNodeCommand should remove created node");
    incActionRedo();
    check(isChildOf(root(), node), "redo AddNodeCommand should restore created node");
    ctx->nodes(Context::Nodes{node});
    check(nijigenerate::compat::ngRequireObject(
        nijigenerate::commands::detail::constructSourceCommand<ToggleVisibilityCommand>()->run(ctx))->succeeded,
        "ToggleVisibilityCommand should succeed");
    check(!node->getEnabled(), "ToggleVisibilityCommand should disable enabled node");
    incActionUndo();
    check(node->getEnabled(), "undo ToggleVisibilityCommand should restore enabled state");
    incActionRedo();
    check(!node->getEnabled(), "redo ToggleVisibilityCommand should restore disabled state");
    check(nijigenerate::compat::ngRequireObject(
        nijigenerate::commands::detail::constructSourceCommand<DeleteNodeCommand>()->run(ctx))->succeeded,
        "DeleteNodeCommand should succeed");
    check(!isChildOf(root(), node), "DeleteNodeCommand should remove node from parent");
    incActionUndo();
    check(isChildOf(root(), node), "undo DeleteNodeCommand should restore node under parent");
    incActionRedo();
    check(!isChildOf(root(), node), "redo DeleteNodeCommand should remove node again");
}

void testNodeClipboardCopyPasteUndoRedo() {
    using namespace nijigenerate::commands::node::node;
    using nijigenerate::commands::node::base::clipboardNodes;
    using nijigenerate::commands::Context;
    using nicxlive::core::nodes::Node;
    using nijigenerate::compat::RetainedString;
    using namespace nijigenerate::core::actionstack;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto parent = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    parent->setName(RetainedString::fromStaticLiteral("ClipboardParent").toNullableString());
    auto child = Node::createSourceDeferred([&] { return parent; });
    child->setName(RetainedString::fromStaticLiteral("ClipboardChild").toNullableString());
    auto grandchild = Node::createSourceDeferred([&] { return child; });
    grandchild->setName(RetainedString::fromStaticLiteral("ClipboardGrandchild").toNullableString());
    auto ctxChild = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctxChild->nodes(Context::Nodes{child});
    auto ctxParent = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctxParent->nodes(Context::Nodes{parent});
    clipboardNodes.resize(0);
    auto cut = nijigenerate::commands::detail::constructSourceCommand<CutNodeCommand>();
    check(!cut->runnable(ctxChild), "CutNodeCommand should remain disabled until cut semantics are implemented");
    auto copy = nijigenerate::commands::detail::constructSourceCommand<CopyNodeCommand>();
    check(copy->runnable(ctxChild), "CopyNodeCommand should be runnable for selected nodes");
    check(nijigenerate::compat::ngRequireObject(copy->run(ctxChild))->succeeded, "CopyNodeCommand should copy selected node");
    check(clipboardNodes.size() == 1, "CopyNodeCommand should populate node clipboard");
    check(clipboardNodes[0] != child, "copied node should be a duplicate object");
    check(nijigenerate::compat::ngRequireObject(clipboardNodes[0])->getUUID() != child->getUUID(),
        "copied node should regenerate UUID");
    check(clipboardNodes[0]->children.size() == 1, "copied node should preserve children");
    check(nijigenerate::compat::ngRequireObject(clipboardNodes[0]->children[0])->getUUID() != grandchild->getUUID(),
        "copied child should regenerate UUID");
    const auto originalChildren = parent->children.size();
    auto paste = nijigenerate::commands::detail::constructSourceCommand<PasteNodeCommand>();
    check(paste->runnable(ctxParent), "PasteNodeCommand should be runnable when clipboard has nodes");
    check(nijigenerate::compat::ngRequireObject(paste->run(ctxParent))->succeeded, "PasteNodeCommand should paste clipboard nodes");
    check(clipboardNodes.size() == 0, "PasteNodeCommand should consume clipboard nodes");
    check(parent->children.size() == originalChildren + 1, "paste should add one child under target parent");
    std::shared_ptr<Node> pasted;
    const auto children = parent->children;
    for (const auto candidate : children) {
        if (candidate != child && nijigenerate::compat::ngRequireObject(candidate)->getUUID() != child->getUUID()) {
            pasted = candidate;
            break;
        }
    }
    check(pasted != nullptr, "paste should add a duplicated child");
    check(pasted != child && nijigenerate::compat::ngRequireObject(pasted)->getUUID() != child->getUUID(),
        "pasted node should be a distinct duplicate");
    check(pasted->children.size() == 1 &&
        nijigenerate::compat::ngRequireObject(pasted->children[0])->getUUID() != grandchild->getUUID(),
        "pasted node should preserve descendant structure");
    incActionUndo();
    check(parent->children.size() == originalChildren, "undo paste should remove pasted duplicate");
    incActionRedo();
    check(parent->children.size() == originalChildren + 1, "redo paste should restore pasted duplicate");
    clipboardNodes.resize(0);
}

void testDepthSignColorContracts() {
    using namespace nijigenerate::viewport::depth::tools::operation;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, nijigenerate::compat::RetainedString::fromStaticLiteral(message));
    };
    check(nearVec4(depthOperationColor(0.25f), DepthOperationPositiveColor),
        "positive depth should use positive operation color");
    check(nearVec4(depthOperationColor(0.25f, true), DepthOperationPositiveSelectedColor),
        "selected positive depth should use selected positive color");
    check(nearVec4(depthOperationColor(-0.25f), DepthOperationNegativeColor),
        "negative depth should use negative operation color");
    check(nearVec4(depthOperationColor(-0.25f, true), DepthOperationNegativeSelectedColor),
        "selected negative depth should use selected negative color");
    check(nearVec4(depthOperationColor(0.0f), DepthOperationColor), "zero depth should use neutral operation color");
    check(nearVec4(depthOperationColor(0.0f, true), DepthOperationSelectedColor),
        "selected zero depth should use selected neutral color");
    check(near(depthToolRound(0.12345f), 0.123f), "depth values should round to 0.001 precision for UI editing");
    check(near(depthToolRound(-0.12355f), -0.124f),
        "negative depth values should round symmetrically to 0.001 precision");
}

void testDepthOperationHelperContracts() {
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nijigenerate::compat::Slice;
    using namespace nijigenerate::ext::nodes;
    using namespace nijigenerate::viewport::depth::tools::operation;
    using nijigenerate::viewport::depth::DepthBrushSettings;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, nijigenerate::compat::RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    ensureRegressionNodeTypesRegistered();
    auto grid = ExGridDeformer::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    grid->setName(nicxlive::core::common::NullableString::fromStaticLiteral("depth-helper-grid"));
    grid->rebuffer(Vec2Array{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}});
    grid->replaceDepths(Slice<float>{0.25f, -0.5f, 1.0f});
    auto copiedDepths = grid->copyDepths();
    copiedDepths[0] = 99;
    check(grid->copyDepths()[0] == 0.25f, "copyDepths should return a defensive copy");
    auto copied = ExGridDeformer::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    copied->copyDepthsFrom(grid);
    {
        const auto actual = copied->copyDepths();
        const Slice<float> expected{0.25f, -0.5f, 1.0f};
        bool equal = actual.size() == expected.size();
        for (std::size_t i = 0; equal && i < actual.size(); ++i) equal = actual[i] == expected[i];
        check(equal, "copyDepthsFrom should copy depths from depth-mapped source");
    }
    copied->replaceDepths({});
    check(copied->copyDepths().isNull(), "replaceDepths(null) should clear depth array");
    grid->rebuffer(Vec2Array{Vec2{0, 0}, Vec2{10, 0}, Vec2{0, 10}, Vec2{10, 10}});
    const auto copiedCount = grid->copyDepths().size();
    check(copiedCount == grid->vertices.size(), "rebuffer should resize existing depth array");
    {
        const auto values = grid->copyDepths();
        check(values[values.size() - 1] == 0.0f, "rebuffer should initialize added depth entries to zero");
    }
    ExDepthOp attached;
    attached.type = ExDepthOpType::AttachedPoint;
    attached.index = 2;
    attached.amount = 0.5f;
    ExDepthOp plane;
    plane.type = ExDepthOpType::Plane;
    plane.center = {1, 2};
    plane.radiusX = 3;
    plane.radiusY = 4;
    plane.angle = 15;
    plane.targetDepth = -0.25f;
    plane.flattenStrength = 0.75f;
    grid->replaceDepthOps(Slice<ExDepthOp>{attached, plane});
    auto copiedOps = grid->copyDepthOps();
    copiedOps[0].amount = 99;
    check(near(grid->copyDepthOps()[0].amount, 0.5f), "copyDepthOps should return a defensive copy");
    copied->copyDepthOpsFrom(grid);
    check(copied->copyDepthOps().size() == 2, "copyDepthOpsFrom should copy operation count");
    check(copied->copyDepthOps()[1].type == ExDepthOpType::Plane,
        "copyDepthOpsFrom should preserve operation type");
    auto attachedOp = DepthAttachedPointOperation::createSource([] { return std::tuple{std::size_t{1}, 0.4f}; });
    auto attachedClone = std::dynamic_pointer_cast<DepthAttachedPointOperation>(attachedOp->clone());
    check(attachedClone != attachedOp && nijigenerate::compat::ngRequireObject(attachedClone)->index == 1
            && near(attachedClone->amount, 0.4f), "attached depth operation clone should preserve index and amount");
    DepthBrushSettings settings;
    settings.amount = -0.6f;
    settings.radiusY = 12.0f;
    settings.hardness = 0.35f;
    auto ringOp = DepthRingOperation::createSource([&] { return std::tuple{Vec2{0, 0}, Vec2{10, 0}, settings}; });
    ringOp->p0Angle = 45;
    ringOp->p1Angle = 135;
    auto ringClone = std::dynamic_pointer_cast<DepthRingOperation>(ringOp->clone());
    check(ringClone != ringOp && nearVec2(nijigenerate::compat::ngRequireObject(ringClone)->p0, ringOp->p0)
            && nearVec2(ringClone->p1, ringOp->p1), "ring depth operation clone should preserve endpoints");
    check(near(nijigenerate::compat::ngRequireObject(ringClone)->amount, -0.6f)
            && near(ringClone->width, 12.0f) && near(ringClone->hardness, 0.35f),
        "ring depth operation clone should preserve brush settings");
    check(near(nijigenerate::compat::ngRequireObject(ringClone)->p0Angle, 45) && near(ringClone->p1Angle, 135),
        "ring depth operation clone should preserve endpoint angles");
    settings.amount = 0.75f;
    settings.flattenStrength = 0.4f;
    settings.angle = 25.0f;
    auto planeOp = DepthPlaneOperation::createSource([&] { return std::tuple{Vec2{5, 6}, 7.0f, 8.0f, settings}; });
    auto planeClone = std::dynamic_pointer_cast<DepthPlaneOperation>(planeOp->clone());
    check(planeClone != planeOp && nearVec2(nijigenerate::compat::ngRequireObject(planeClone)->center, planeOp->center),
        "plane depth operation clone should preserve center");
    check(near(nijigenerate::compat::ngRequireObject(planeClone)->radiusX, 7) && near(planeClone->radiusY, 8),
        "plane depth operation clone should preserve radii");
    check(near(distanceToSegment({5, 5}, {0, 0}, {10, 0}), 5.0f),
        "distanceToSegment should measure perpendicular distance within segment");
    check(near(distanceToSegment({15, 0}, {0, 0}, {10, 0}), 5.0f),
        "distanceToSegment should clamp beyond segment endpoint");
}

// Existing original regression.d4204-4239, retaining all authored sample values.
void testDepthCameraProjectionContracts() {
    using nicxlive::core::math::Vec2;
    using namespace nijigenerate::viewport::depth;
    using nijigenerate::compat::RetainedString;
    DepthCamera3D camera;
    camera.yaw = 0.45f;
    camera.pitch = -0.32f;
    camera.zoom = 1.75f;
    camera.pan = {23.0f, -17.0f};
    const std::array points{Vec2{-120.0f, -40.0f}, Vec2{0.0f, 0.0f}, Vec2{75.0f, 95.0f}};
    const std::array depths{-85.0f, 0.0f, 140.0f};
    for (const auto point : points) {
        for (const auto depth : depths) {
            const auto projected = projectDepthPoint(point, depth, camera);
            const auto unprojected = unprojectDepthPoint(projected, depth, camera);
            require(nearVec2(unprojected, point),
                RetainedString::fromStaticLiteral("depth camera project/unproject should round-trip"));
        }
    }
    const auto originProjection = projectDepthPoint({0, 0}, 0.0f, camera);
    require(nearVec2(originProjection, camera.pan),
        RetainedString::fromStaticLiteral("zero-depth origin should project to camera pan"));
    const auto shallow = projectDepthPoint({30, -20}, 0.0f, camera);
    const auto deep = projectDepthPoint({30, -20}, 100.0f, camera);
    require(!nearVec2(shallow, deep), RetainedString::fromStaticLiteral(
        "non-zero yaw/pitch should make depth visible in projected position"));
    DepthCamera3D flatCamera;
    flatCamera.zoom = 2.0f;
    flatCamera.pan = {-10.0f, 5.0f};
    const Vec2 flatPoint{12.5f, -7.25f};
    const auto flatProjected = projectDepthPoint(flatPoint, 500.0f, flatCamera);
    const Vec2 expected{flatPoint.x * flatCamera.zoom + flatCamera.pan.x,
        flatPoint.y * flatCamera.zoom + flatCamera.pan.y};
    require(nearVec2(flatProjected, expected), RetainedString::fromStaticLiteral(
        "flat camera projection should be ordinary pan/zoom regardless of depth"));
    require(nearVec2(unprojectDepthPoint(flatProjected, 500.0f, flatCamera), flatPoint),
        RetainedString::fromStaticLiteral("flat camera unprojection should invert pan/zoom"));
}

// Existing original regression.d19419-19489, with the same real command dispatcher.
void testCommandBaseContracts() {
    using namespace nijigenerate::commands;
    using nijigenerate::compat::RetainedString;
    using Node = nicxlive::core::nodes::Node;
    using NodeRef = std::shared_ptr<Node>;
    using Parameter = nicxlive::core::param::Parameter;
    using nijigenerate::api::mcp::server::ngMcpJsonArgumentSchemaForRegression;
    using nijigenerate::api::mcp::server::ngMcpValidateJsonArgumentForRegression;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    check(toCodeString("a\"b").view() == "\"a\\\"b\"", "toCodeString should quote and escape strings");
    check(toCodeString(nullptr).view() == "null", "toCodeString should encode null");
    check(!toCodeString(CommandGuiDisplay::dialog).empty(), "toCodeString should encode enum values");
    auto ok = CommandResult::opCall(true, RetainedString::fromStaticLiteral("done"));
    check(ok->succeeded && ok->message.view() == "done", "CommandResult should store success and message");
    check(ok->waitForCompletion() == ok,
        "CommandResult.waitForCompletion should return itself for synchronous commands");
    auto typed = ExCommandResult<std::int32_t>::opCall(true, 42, RetainedString::fromStaticLiteral("answer"));
    check(typed->succeeded && typed->result == 42 && typed->message.view() == "answer",
        "ExCommandResult should preserve typed payload");
    auto created = CreateResult<NodeRef>::opCall(true, nijigenerate::compat::Slice<NodeRef>{Node::createSourceDeferred([] { return NodeRef{}; })},
        RetainedString::fromStaticLiteral("created"));
    check(created->created.size() == 1 && created->message.view() == "created",
        "CreateResult should preserve created objects");
    auto deleted = DeleteResult<NodeRef>::opCall(true, nijigenerate::compat::Slice<NodeRef>{created->created[0]},
        RetainedString::fromStaticLiteral("deleted"));
    check(deleted->deleted.size() == 1 && deleted->message.view() == "deleted",
        "DeleteResult should preserve deleted objects");
    auto loaded = LoadResult<NodeRef>::opCall(true, nijigenerate::compat::Slice<NodeRef>{created->created[0]},
        RetainedString::fromStaticLiteral("loaded"));
    check(loaded->loaded.size() == 1 && loaded->message.view() == "loaded",
        "LoadResult should preserve loaded objects");
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    check(!ctx->hasPuppet() && !ctx->hasNodes() && !ctx->hasParameters() && !ctx->hasKeyPoint(),
        "Context should start without explicit values");
    ctx->puppet(nijigenerate::project::incActivePuppet());
    ctx->nodes(created->created);
    ctx->parameters(Context::Parameters{Parameter::createSourceDeferred(
        [] { return nicxlive::core::common::NullableString("Param"); }, [] { return false; })});
    ctx->armedParameters(ctx->parameters());
    ctx->bindings({});
    ctx->activeBindings({});
    ctx->keyPoint({1, 2});
    ctx->parameterValue({0.25f, -0.5f});
    check(ctx->hasPuppet() && ctx->hasNodes() && ctx->hasParameters() && ctx->hasArmedParameters(),
        "Context setters should mark object masks");
    check(ctx->hasBindings() && ctx->hasActiveBindings() && ctx->hasKeyPoint() && ctx->hasParameterValue(),
        "Context setters should mark binding/key/value masks");
    const auto key = ctx->keyPoint();
    const bool equalKey = key.x == 1 && key.y == 2;
    // Keep the right side of the original && lazy.
    check(equalKey && [&] {
        const auto value = ctx->parameterValue();
        return value.x == 0.25f && value.y == -0.5f;
    }(), "Context should store keypoint and parameter value");
    ctx->hasNodes(false);
    check(!ctx->hasNodes(), "Context mask setters should clear flags");
    auto meta = std::allocate_shared<detail::RegressionMetaCommand>(
        nicxlive::core::d_runtime::SourceAllocator<detail::RegressionMetaCommand>{});
    check(meta->label().view() == "Meta Label" && meta->description().view() == "Meta Description",
        "ExCommand should expose label and description");
    check(!meta->shortcutRunnable(), "ShortcutHidden metadata should hide command from shortcuts");
    check(!meta->mcpExposed(), "McpHidden metadata should hide command from MCP");
    check(meta->guiDisplay() == CommandGuiDisplay::dialog, "GuiDisplay metadata should be reflected");
    check(meta->irreversibleEffect() == CommandIrreversibleEffect::configEdit,
        "IrreversibleEffect metadata should be reflected");
    auto arg = std::allocate_shared<detail::RegressionArgCommand>(
        nicxlive::core::d_runtime::SourceAllocator<detail::RegressionArgCommand>{});
    check(arg->label().view() == "Arg Label" && arg->description().view() == "Arg Description",
        "ExCommand with args should expose label and description");
    check(arg->name.view() == "Alice" && arg->arg1 == 7, "ExCommand should assign TW and positional argument fields");
    const auto metas = detail::RegressionArgCommand::reflectArgMeta();
    check(metas.size() == 1 && metas[0].fieldName.view() == "name" && metas[0].fieldDesc.view() == "Name",
        "ExCommand should reflect TW argument metadata");
    auto nonRunnable = std::allocate_shared<detail::RegressionNonRunnableCommand>(
        nicxlive::core::d_runtime::SourceAllocator<detail::RegressionNonRunnableCommand>{});
    const auto rejected = ngRunCommand(nonRunnable, ctx);
    check(!rejected->succeeded && !nonRunnable->didRun,
        "the central command dispatcher must reject a command whose current context is not runnable");
    auto depthOperationSchema = ngMcpJsonArgumentSchemaForRegression(CommandJsonSchema::depthOperation);
    check(depthOperationSchema.sourceIndexValue("type").as_string() == "object" &&
        depthOperationSchema.sourceIndexValue("properties").sourceIndexValue("type").sourceIndexValue("type").as_string() == "string" &&
        depthOperationSchema.sourceIndexValue("properties").sourceIndexValue("p0").sourceIndexValue("type").as_string() == "array",
        "MCP depth-operation arguments must publish an object schema instead of the overlay array schema");
    auto depthOperationsSchema = ngMcpJsonArgumentSchemaForRegression(CommandJsonSchema::depthOperations);
    check(depthOperationsSchema.sourceIndexValue("type").as_string() == "array" &&
        depthOperationsSchema.sourceIndexValue("items").sourceIndexValue("type").as_string() == "object",
        "MCP depth-operation list arguments must publish an array of depth-operation objects");
    auto overlaySchema = ngMcpJsonArgumentSchemaForRegression(CommandJsonSchema::overlayObjects);
    check(overlaySchema.sourceIndexValue("type").as_string() == "array" &&
        !overlaySchema.sourceIndexValue("items").as_object().if_contains("required"),
        "MCP overlay arguments must admit the documented keyed-object form without requiring uuid");
    ngMcpValidateJsonArgumentForRegression(CommandJsonSchema::overlayObjects,
        nijigenerate::autorig::json::detail::parseJSON("[{\"123\":\"bounds\"}]"));
    ngMcpValidateJsonArgumentForRegression(CommandJsonSchema::overlayObjects,
        nijigenerate::autorig::json::detail::parseJSON("[{\"uuid\":123,\"overlay\":\"mesh\"}]"));
}

// Original regression.d13147-13178 helper bodies.
std::shared_ptr<nicxlive::core::param::ValueParameterBinding> newValueBinding(
    std::shared_ptr<nicxlive::core::param::Parameter> param,
    std::shared_ptr<nicxlive::core::nodes::Node> node,
    nijigenerate::compat::RetainedString name, bool setZero = true) {
    auto binding = std::dynamic_pointer_cast<nicxlive::core::param::ValueParameterBinding>(
        nijigenerate::compat::ngRequireObject(param)->createBinding(node, name.toNullableString(), setZero));
    require(binding != nullptr, nijigenerate::compat::RetainedString::fromStaticLiteral(
        "ValueParameterBinding fixture should be created"));
    nijigenerate::compat::ngRequireObject(param)->addBinding(binding);
    return binding;
}

bool containsBinding(
    nijigenerate::compat::Slice<std::shared_ptr<nicxlive::core::param::ParameterBinding>> bindings,
    std::shared_ptr<nicxlive::core::param::ParameterBinding> target) {
    for (const auto binding : bindings) if (binding == target) return true;
    return false;
}

std::shared_ptr<nijigenerate::commands::Context> bindingContext(
    std::shared_ptr<nicxlive::core::param::Parameter> param,
    std::shared_ptr<nicxlive::core::param::ParameterBinding> binding,
    nijigenerate::compat::Vec2u keyPoint) {
    using nijigenerate::commands::Context;
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->parameters(Context::Parameters{param});
    ctx->bindings(Context::Bindings{binding});
    ctx->keyPoint(keyPoint);
    return ctx;
}

std::shared_ptr<nijigenerate::ext::ExParameter> new2DParameter(nijigenerate::compat::RetainedString name) {
    auto param = nijigenerate::ext::ExParameter::createSourceDeferred(
        [&] { return name.toNullableString(); }, [] { return true; });
    param->min = {-1, -1};
    param->max = {1, 1};
    param->insertAxisPoint(0, 0.5f);
    param->insertAxisPoint(1, 0.5f);
    nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param);
    return param;
}

void testParameterBindingInterpolation() {
    using nicxlive::core::nodes::Node;
    using nijigenerate::ext::ExParameter;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Vec2u;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto node1D = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto param1D = ExParameter::createSourceDeferred(
        [] { return RetainedString::fromStaticLiteral("Interp1D").toNullableString(); }, [] { return false; });
    param1D->min = {-1, 0};
    param1D->max = {1, 0};
    param1D->insertAxisPoint(0, 0.5f);
    nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param1D);
    auto binding1D = newValueBinding(param1D, node1D, RetainedString::fromStaticLiteral("transform.t.x"), false);
    binding1D->setValue(Vec2u{0, 0}, 0.0f);
    binding1D->setValue(Vec2u{2, 0}, 20.0f);
    check(binding1D->isSet(Vec2u{0, 0}) && binding1D->isSet(Vec2u{2, 0}),
        "1D interpolation endpoints should remain set");
    check(near(binding1D->getValue(Vec2u{1, 0}), 10.0f), "1D interpolation should fill midpoint");
    binding1D->unset(Vec2u{1, 0});
    check(near(binding1D->getValue(Vec2u{1, 0}), 10.0f), "1D reInterpolate should recover midpoint after unset");
    auto node2D = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto param2D = ExParameter::createSourceDeferred(
        [] { return RetainedString::fromStaticLiteral("Interp2D").toNullableString(); }, [] { return true; });
    param2D->min = {-1, -1};
    param2D->max = {1, 1};
    param2D->insertAxisPoint(0, 0.5f);
    param2D->insertAxisPoint(1, 0.5f);
    nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param2D);
    auto binding2D = newValueBinding(param2D, node2D, RetainedString::fromStaticLiteral("transform.t.x"), false);
    binding2D->setValue(Vec2u{0, 0}, 0.0f);
    binding2D->setValue(Vec2u{2, 0}, 10.0f);
    binding2D->setValue(Vec2u{0, 2}, 20.0f);
    binding2D->setValue(Vec2u{2, 2}, 30.0f);
    check(binding2D->isSet(Vec2u{0, 0}) && binding2D->isSet(Vec2u{2, 0}) &&
        binding2D->isSet(Vec2u{0, 2}) && binding2D->isSet(Vec2u{2, 2}), "2D interpolation corners should remain set");
    check(near(binding2D->getValue(Vec2u{1, 0}), 5.0f), "2D interpolation should fill top edge");
    check(near(binding2D->getValue(Vec2u{0, 1}), 10.0f), "2D interpolation should fill left edge");
    check(near(binding2D->getValue(Vec2u{2, 1}), 20.0f), "2D interpolation should fill right edge");
    check(near(binding2D->getValue(Vec2u{1, 2}), 25.0f), "2D interpolation should fill bottom edge");
    check(near(binding2D->getValue(Vec2u{1, 1}), 15.0f), "2D interpolation should fill center from both axes");
    binding2D->unset(Vec2u{1, 1});
    check(near(binding2D->getValue(Vec2u{1, 1}), 15.0f), "2D reInterpolate should recover center after unset");
}

void testParameterArmSelectAndKeypointCommands() {
    using namespace nijigenerate::project;
    using namespace nijigenerate::commands::parameter::paramedit;
    using nijigenerate::commands::Context;
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::math::Vec2;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto first = new2DParameter(RetainedString::fromStaticLiteral("First"));
    auto second = new2DParameter(RetainedString::fromStaticLiteral("Second"));
    incSelectParam(first);
    check(incSelectedParam() == first, "incSelectParam should set the primary selected parameter");
    incAddSelectParam(second);
    check(incSelectedParams().size() == 2, "incAddSelectParam should add to parameter selection");
    incRemoveSelectParam(first);
    check(incSelectedParams().size() == 1 && incSelectedParam() == second,
        "incRemoveSelectParam should update parameter selection");
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->puppet(incActivePuppet());
    ctx->parameters(Context::Parameters{second});
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        ToggleParameterArmCommand>(std::int32_t{1})->run(ctx))->succeeded,
        "ToggleParameterArmCommand should arm the selected parameter");
    check(incArmedParameter() == second, "ToggleParameterArmCommand should set armed parameter");
    check(incArmedParameterIdx() == 1, "ToggleParameterArmCommand should preserve armed parameter index");
    check(!nijigenerate::compat::ngRequireObject(incActivePuppet())->enableDrivers,
        "arming a parameter should disable drivers");
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        ToggleParameterArmCommand>(std::int32_t{1})->run(ctx))->succeeded,
        "ToggleParameterArmCommand should disarm the armed parameter");
    check(!incArmedParameter(), "ToggleParameterArmCommand should disarm when the same parameter is armed");
    check(nijigenerate::compat::ngRequireObject(incActivePuppet())->enableDrivers, "disarming should re-enable drivers");
    ctx->parameterValue(Vec2{-1, 1});
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        SetParameterKeypointCommand>()->run(ctx))->succeeded,
        "SetParameterKeypointCommand should set a keypoint value");
    check(second->value == Vec2{-1, 1}, "SetParameterKeypointCommand should update parameter value");
    check(!incArmedParameter(), "SetParameterKeypointCommand should not arm the parameter");
    incSetEditMode(EditMode::VertexEdit, false);
    auto armedCtx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    armedCtx->puppet(incActivePuppet());
    armedCtx->armedParameters(Context::Parameters{second});
    armedCtx->parameterValue(Vec2{0, 0});
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        SetArmedParameterAndKeypointCommand>()->run(armedCtx))->succeeded,
        "SetArmedParameterAndKeypointCommand should set and arm a keypoint");
    check(incEditMode() == EditMode::ModelEdit, "SetArmedParameterAndKeypointCommand should return to model edit mode");
    check(incArmedParameter() == second, "SetArmedParameterAndKeypointCommand should arm the target parameter");
    check(incArmedParameterIdx() == 1, "SetArmedParameterAndKeypointCommand should resolve the parameter index");
    check(second->value == Vec2{0, 0}, "SetArmedParameterAndKeypointCommand should update parameter value");
}

void testParameterStartingKeyframeCommandUndoRedo() {
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::parameter::paramedit::SetStartingKeyFrameCommand;
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::math::Vec2;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto param = new2DParameter(RetainedString::fromStaticLiteral("StartingKey"));
    param->defaults = Vec2{0, 0};
    param->value = Vec2{0.5f, -0.25f};
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->parameters(Context::Parameters{param});
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        SetStartingKeyFrameCommand>()->run(ctx))->succeeded, "SetStartingKeyFrameCommand should succeed");
    check(param->defaults == Vec2{0.5f, -0.25f}, "SetStartingKeyFrameCommand should copy current value to defaults");
    incActionUndo();
    check(param->defaults == Vec2{0, 0}, "undo SetStartingKeyFrameCommand should restore previous defaults");
    incActionRedo();
    check(param->defaults == Vec2{0.5f, -0.25f}, "redo SetStartingKeyFrameCommand should restore new defaults");
}

void testParameterKeyframeCommandUndoRedo() {
    using namespace nijigenerate::commands::binding;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Vec2u;
    using nicxlive::core::nodes::Node;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto node = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto param = nijigenerate::ext::ExParameter::createSourceDeferred(
        [] { return RetainedString::fromStaticLiteral("Param").toNullableString(); }, [] { return true; });
    param->min = {-1, -1};
    param->max = {1, 1};
    param->insertAxisPoint(0, 0.5f);
    param->insertAxisPoint(1, 0.5f);
    nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param);
    auto binding = newValueBinding(param, node, RetainedString::fromStaticLiteral("transform.t.x"));
    const auto kp = Vec2u{2, 1};
    auto ctx = bindingContext(param, binding, kp);
    node->setValueSource(RetainedString::fromStaticLiteral("transform.t.x"), 4.0f);
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        ResetKeyFrameCommand>()->run(ctx))->succeeded, "ResetKeyFrameCommand should succeed");
    check(binding->isSet(kp), "reset keyframe should mark keypoint as set");
    check(near(binding->getValue(kp), 0.0f), "reset keyframe should use default value");
    incActionUndo();
    check(!binding->isSet(kp), "undo reset keyframe should unset keypoint");
    incActionRedo();
    check(binding->isSet(kp) && near(binding->getValue(kp), 0.0f), "redo reset keyframe should restore reset value");
    binding->setValue(kp, 3.0f);
    incActionClearHistory();
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        InvertKeyFrameCommand>()->run(ctx))->succeeded, "InvertKeyFrameCommand should succeed");
    check(near(binding->getValue(kp), -3.0f), "invert keyframe should negate value");
    incActionUndo();
    check(near(binding->getValue(kp), 3.0f), "undo invert keyframe should restore old value");
    incActionRedo();
    check(near(binding->getValue(kp), -3.0f), "redo invert keyframe should restore inverted value");
    binding->setValue(kp, 5.0f);
    incActionClearHistory();
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        MirrorKeyFrameHorizontallyCommand>()->run(ctx))->succeeded, "MirrorKeyFrameHorizontallyCommand should succeed");
    check(near(binding->getValue(kp), -5.0f), "horizontal mirror should flip transform.t.x");
    incActionUndo();
    check(near(binding->getValue(kp), 5.0f), "undo horizontal mirror should restore old value");
    binding->setValue(kp, 7.0f);
    incActionClearHistory();
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        UnsetKeyFrameCommand>()->run(ctx))->succeeded, "UnsetKeyFrameCommand should succeed");
    check(!binding->isSet(kp), "unset keyframe should clear keypoint");
    incActionUndo();
    check(binding->isSet(kp) && near(binding->getValue(kp), 7.0f), "undo unset keyframe should restore old value");
    incActionRedo();
    check(!binding->isSet(kp), "redo unset keyframe should clear keypoint again");
    binding->unset(kp);
    node->setValueSource(RetainedString::fromStaticLiteral("transform.t.x"), 9.0f);
    incActionClearHistory();
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        SetKeyFrameCommand>()->run(ctx))->succeeded, "SetKeyFrameCommand should succeed");
    check(binding->isSet(kp), "set keyframe should mark keypoint as set");
    incActionUndo();
    check(!binding->isSet(kp), "undo set keyframe should restore unset state");
    incActionRedo();
    check(binding->isSet(kp), "redo set keyframe should mark keypoint as set again");
}

void testParameterBindingClipboardAndCleanupCommands() {
    using namespace nijigenerate::commands::binding;
    using namespace nijigenerate::core::actionstack;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::param::InterpolateMode;
    using nijigenerate::commands::Context;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Vec2u;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto sourceNode = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    sourceNode->setName(RetainedString::fromStaticLiteral("source-binding-node").toNullableString());
    auto destNode = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    destNode->setName(RetainedString::fromStaticLiteral("dest-binding-node").toNullableString());
    auto param = new2DParameter(RetainedString::fromStaticLiteral("BindingClipboard"));
    const auto key = Vec2u{2, 1};
    auto sourceBinding = newValueBinding(param, sourceNode, RetainedString::fromStaticLiteral("transform.t.x"), false);
    auto destBinding = newValueBinding(param, destNode, RetainedString::fromStaticLiteral("transform.t.x"), false);
    sourceBinding->setValue(key, 12.5f);
    destBinding->setValue(key, -1.0f);
    auto copyCtx = bindingContext(param, sourceBinding, key);
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        CopyBindingCommand>()->run(copyCtx))->succeeded, "CopyBindingCommand should succeed");
    auto pasteCtx = bindingContext(param, destBinding, key);
    pasteCtx->activeBindings(Context::Bindings{destBinding});
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        PasteBindingCommand>()->run(pasteCtx))->succeeded, "PasteBindingCommand should succeed");
    check(near(destBinding->getValue(key), 12.5f), "PasteBindingCommand should copy the keypoint value");
    incActionUndo();
    check(near(destBinding->getValue(key), -1.0f), "undo PasteBindingCommand should restore destination value");
    incActionRedo();
    check(near(destBinding->getValue(key), 12.5f), "redo PasteBindingCommand should restore pasted value");
    auto interpCtx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    interpCtx->parameters(Context::Parameters{param});
    interpCtx->activeBindings(Context::Bindings{destBinding});
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        SetInterpolationCommand>(InterpolateMode::Nearest)->run(interpCtx))->succeeded,
        "SetInterpolationCommand should succeed");
    check(destBinding->interpolateMode() == InterpolateMode::Nearest, "SetInterpolationCommand should change interpolation mode");
    incActionUndo();
    check(destBinding->interpolateMode() != InterpolateMode::Nearest, "undo SetInterpolationCommand should restore interpolation mode");
    incActionRedo();
    check(destBinding->interpolateMode() == InterpolateMode::Nearest, "redo SetInterpolationCommand should restore interpolation mode");
    auto removeCtx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    removeCtx->parameters(Context::Parameters{param});
    removeCtx->activeBindings(Context::Bindings{destBinding});
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        RemoveBindingCommand>()->run(removeCtx))->succeeded, "RemoveBindingCommand should succeed");
    check(!param->getBinding(destNode, RetainedString::fromStaticLiteral("transform.t.x").toNullableString()),
        "RemoveBindingCommand should remove destination binding");
    incActionUndo();
    check(param->getBinding(destNode, RetainedString::fromStaticLiteral("transform.t.x").toNullableString()) == destBinding,
        "undo RemoveBindingCommand should restore binding");
    incActionRedo();
    check(!param->getBinding(destNode, RetainedString::fromStaticLiteral("transform.t.x").toNullableString()),
        "redo RemoveBindingCommand should remove binding again");
}

void testParameterBindingCleanupOnDeleteUndoRedo() {
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::commands::binding::RemoveBindingCommand;
    using nijigenerate::commands::parameter::paramedit::DeleteParameterCommand;
    using nijigenerate::commands::Context;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Vec2u;
    using nicxlive::core::nodes::Node;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto node = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    node->setName(RetainedString::fromStaticLiteral("binding-cleanup-node").toNullableString());
    auto param = new2DParameter(RetainedString::fromStaticLiteral("BindingCleanup"));
    auto binding = newValueBinding(param, node, RetainedString::fromStaticLiteral("transform.t.x"), false);
    binding->setValue(Vec2u{2, 2}, 7.0f);
    auto removeCtx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    removeCtx->parameters(Context::Parameters{param});
    removeCtx->activeBindings(Context::Bindings{binding});
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        RemoveBindingCommand>()->run(removeCtx))->succeeded, "RemoveBindingCommand should remove active binding");
    check(param->bindings.size() == 0, "binding cleanup should leave no binding in parameter");
    incActionUndo();
    check(param->bindings.size() == 1 && param->bindings[0] == binding, "undo binding cleanup should restore binding");
    auto deleteCtx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    deleteCtx->puppet(nijigenerate::project::incActivePuppet());
    deleteCtx->parameters(Context::Parameters{param});
    incActionClearHistory();
    check(nijigenerate::compat::ngRequireObject(nijigenerate::commands::detail::constructSourceCommand<
        DeleteParameterCommand>()->run(deleteCtx))->succeeded, "DeleteParameterCommand should delete parameter with bindings");
    check(!findParameter(nijigenerate::project::incActivePuppet(), "BindingCleanup"),
        "DeleteParameterCommand should remove parameter from puppet");
    incActionUndo();
    auto restored = findParameter(nijigenerate::project::incActivePuppet(), "BindingCleanup");
    check(restored == param, "undo DeleteParameterCommand should restore parameter object");
    check(param->bindings.size() == 1 && param->bindings[0] == binding,
        "undo DeleteParameterCommand should preserve bindings on restored parameter");
    incActionRedo();
    check(!findParameter(nijigenerate::project::incActivePuppet(), "BindingCleanup"),
        "redo DeleteParameterCommand should remove parameter again");
}

void testParameterSplitBindingMigrationUndoRedo() {
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::windows::paramsplit::ngSplitParameterBindings;
    using nicxlive::core::nodes::Node;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Vec2u;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto param = new2DParameter(RetainedString::fromStaticLiteral("Split Source"));
    param->insertAxisPoint(0, 0.25f);
    param->insertAxisPoint(1, 0.75f);
    auto keepA = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    keepA->setName(RetainedString::fromStaticLiteral("split-keep-a").toNullableString());
    auto move = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    move->setName(RetainedString::fromStaticLiteral("split-move").toNullableString());
    auto keepB = Node::createSourceDeferred([] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    keepB->setName(RetainedString::fromStaticLiteral("split-keep-b").toNullableString());
    auto keepBindingA = newValueBinding(param, keepA, RetainedString::fromStaticLiteral("transform.t.x"));
    auto movedBinding = newValueBinding(param, move, RetainedString::fromStaticLiteral("transform.t.y"));
    auto keepBindingB = newValueBinding(param, keepB, RetainedString::fromStaticLiteral("transform.r.z"));
    keepBindingA->setValue(Vec2u{0, 0}, -1.0f);
    movedBinding->setValue(Vec2u{1, 1}, 2.0f);
    keepBindingB->setValue(Vec2u{2, 2}, 3.0f);
    auto originalBindings = param->bindings.duplicate();
    auto newParam = ngSplitParameterBindings(0, param, nijigenerate::compat::Slice<std::uint32_t>{move->getUUID()});
    check(newParam != nullptr, "parameter split should create a new parameter when a node is selected");
    check(nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.size() == 2,
        "parameter split should insert the new parameter");
    check(nijigenerate::project::incActivePuppet()->parameters[1] == newParam,
        "parameter split should insert immediately after the source parameter");
    check(nijigenerate::compat::ngRequireObject(newParam)->getName() == "Split Source (Split)",
        "split parameter should use the dialog suffix");
    check(newParam->isVec2 == param->isVec2, "split parameter should preserve dimensionality");
    const auto equalAxis = [&](std::size_t axis) {
        const auto left = param->axisPoints[axis];
        const auto right = newParam->axisPoints[axis];
        return (left.data() == right.data() && left.size() == right.size()) ||
            std::equal(left.begin(), left.end(), right.begin(), right.end());
    };
    check(equalAxis(0) && equalAxis(1), "split parameter should copy all axis points");
    check(param->bindings.size() == 2, "source parameter should retain unselected-node bindings");
    check(containsBinding(param->bindings, keepBindingA) && containsBinding(param->bindings, keepBindingB),
        "source parameter should keep both non-moved bindings");
    check(!containsBinding(param->bindings, movedBinding), "source parameter should not retain moved binding");
    check(newParam->bindings.size() == 1 && newParam->bindings[0] == movedBinding,
        "new parameter should receive selected-node bindings");
    incActionUndo();
    check(nijigenerate::project::incActivePuppet()->parameters.size() == 1 &&
        nijigenerate::project::incActivePuppet()->parameters[0] == param, "undo split should remove the created parameter");
    check(param->bindings.size() == originalBindings.size(), "undo split should restore all source bindings");
    for (const auto binding : originalBindings)
        check(containsBinding(param->bindings, binding), "undo split should restore binding identity");
    check(newParam->bindings.size() == 0, "undo split should clear bindings from the removed split parameter");
    incActionRedo();
    check(nijigenerate::project::incActivePuppet()->parameters.size() == 2 &&
        nijigenerate::project::incActivePuppet()->parameters[1] == newParam, "redo split should restore the split parameter");
    check(param->bindings.size() == 2 && !containsBinding(param->bindings, movedBinding),
        "redo split should move selected binding again");
    check(newParam->bindings.size() == 1 && newParam->bindings[0] == movedBinding,
        "redo split should restore moved binding on split parameter");
    auto noOp = ngSplitParameterBindings(0, param, {});
    check(!noOp, "parameter split should not create an empty split parameter");
    check(nijigenerate::project::incActivePuppet()->parameters.size() == 2, "empty split should not change parameter list");
}

void assertAxisPoints(std::shared_ptr<nicxlive::core::param::Parameter> param, std::int32_t axis,
    nijigenerate::compat::ConstSlice<float> expected, nijigenerate::compat::RetainedString label) {
    using nijigenerate::compat::RetainedString;
    const auto row = [&](std::size_t line) -> const nicxlive::core::common::SharedArray<float>& {
        const auto receiver = nijigenerate::compat::ngRequireObject(param);
        const auto index = static_cast<std::size_t>(axis);
        if (index >= receiver->axisPoints.size())
            throw nijigenerate::compat::ngStaticError<nijigenerate::compat::ArrayIndexError>(index,
                receiver->axisPoints.size(), RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), line);
        return receiver->axisPoints[index];
    };
    const bool correctCount = row(12940).size() == expected.size();
    require(correctCount, concatenateSourceStrings({label, RetainedString::fromStaticLiteral(" axis point count")}));
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const float value = expected[index];
        const bool matches = near(row(12942)[index], value);
        const std::array arguments{nijigenerate::core::translation::ngFormatArgument(label),
            nijigenerate::core::translation::ngFormatArgument(index)};
        const auto message = RetainedString(nijigenerate::core::translation::ngFormatTyped("%s axis point %s", arguments));
        require(matches, message);
    }
}

void testParameterCreatePresets() {
    using namespace nijigenerate::commands::parameter::param;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::commands::Context;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::NotifyReason;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    const auto resourcePanelSource = nijigenerate::compat::phobos::ngReadUtf8Text(
        regressionSourceRoot(RetainedString::fromStaticLiteral("panels/resource.d")));
    check(resourcePanelSource.view().find("incRunPendingParameterUiCommand();") != std::string_view::npos,
        "ResourcePanel should execute queued parameter menu commands from its Add Resource and right-click menus");
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    auto resourceStructureNotified = std::allocate_shared<bool>(nicxlive::core::d_runtime::SourceAllocator<bool>{}, false);
    auto root = nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    const auto listener = [&] {
        try {
            return nicxlive::core::nodes::NodeNotifyListener(
                [resourceStructureNotified](std::shared_ptr<Node> target, NotifyReason reason) {
                    if (target == nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->root &&
                        reason == NotifyReason::StructureChanged) *resourceStructureNotified = true;
                });
        } catch (const std::bad_alloc&) {
            throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    nijigenerate::compat::ngRequireObject(root)->addNotifyListener(listener);
    auto onePositiveResult = nijigenerate::commands::detail::constructSourceCommand<Add1DParameterCommand>(
        std::int32_t{0}, std::int32_t{1})->run(ctx);
    check(nijigenerate::compat::ngRequireObject(onePositiveResult)->succeeded, "Add1DParameterCommand 0..1 should succeed");
    check(*resourceStructureNotified, "Add1DParameterCommand should notify resource views after creating a parameter");
    *resourceStructureNotified = false;
    incActionUndo();
    check(*resourceStructureNotified, "undo Add1DParameterCommand should notify resource views after removing a parameter");
    *resourceStructureNotified = false;
    incActionRedo();
    check(*resourceStructureNotified, "redo Add1DParameterCommand should notify resource views after restoring a parameter");
    auto onePositive = onePositiveResult->created[0];
    check(!nijigenerate::compat::ngRequireObject(onePositive)->isVec2, "0..1 preset should create 1D parameter");
    check(near(onePositive->min.x, 0) && near(onePositive->max.x, 1), "0..1 preset should set range");
    assertAxisPoints(onePositive, 0, Slice<float>{0.0f, 1.0f}, RetainedString::fromStaticLiteral("0..1 1D"));
    auto oneSymResult = nijigenerate::commands::detail::constructSourceCommand<Add1DParameterCommand>(
        std::int32_t{-1}, std::int32_t{1})->run(ctx);
    check(nijigenerate::compat::ngRequireObject(oneSymResult)->succeeded, "Add1DParameterCommand -1..1 should succeed");
    auto oneSym = oneSymResult->created[0];
    check(!nijigenerate::compat::ngRequireObject(oneSym)->isVec2, "-1..1 preset should create 1D parameter");
    check(near(oneSym->min.x, -1) && near(oneSym->max.x, 1), "-1..1 preset should set range");
    assertAxisPoints(oneSym, 0, Slice<float>{0.0f, 0.5f, 1.0f}, RetainedString::fromStaticLiteral("-1..1 1D"));
    auto twoPositiveResult = nijigenerate::commands::detail::constructSourceCommand<Add2DParameterCommand>(
        std::int32_t{0}, std::int32_t{1})->run(ctx);
    check(nijigenerate::compat::ngRequireObject(twoPositiveResult)->succeeded, "Add2DParameterCommand 0..1 should succeed");
    auto twoPositive = twoPositiveResult->created[0];
    check(nijigenerate::compat::ngRequireObject(twoPositive)->isVec2, "0..1 preset should create 2D parameter");
    check(nearVec2(twoPositive->min, Vec2{0, 0}) && nearVec2(twoPositive->max, Vec2{1, 1}), "0..1 2D preset should set range");
    assertAxisPoints(twoPositive, 0, Slice<float>{0.0f, 1.0f}, RetainedString::fromStaticLiteral("0..1 2D X"));
    assertAxisPoints(twoPositive, 1, Slice<float>{0.0f, 1.0f}, RetainedString::fromStaticLiteral("0..1 2D Y"));
    auto twoSymResult = nijigenerate::commands::detail::constructSourceCommand<Add2DParameterCommand>(
        std::int32_t{-1}, std::int32_t{1})->run(ctx);
    check(nijigenerate::compat::ngRequireObject(twoSymResult)->succeeded, "Add2DParameterCommand -1..1 should succeed");
    auto twoSym = twoSymResult->created[0];
    check(nijigenerate::compat::ngRequireObject(twoSym)->isVec2, "-1..1 preset should create 2D parameter");
    check(nearVec2(twoSym->min, Vec2{-1, -1}) && nearVec2(twoSym->max, Vec2{1, 1}), "-1..1 2D preset should set range");
    assertAxisPoints(twoSym, 0, Slice<float>{0.0f, 0.5f, 1.0f}, RetainedString::fromStaticLiteral("-1..1 2D X"));
    assertAxisPoints(twoSym, 1, Slice<float>{0.0f, 0.5f, 1.0f}, RetainedString::fromStaticLiteral("-1..1 2D Y"));
    auto mouthResult = nijigenerate::commands::detail::constructSourceCommand<AddMouthParameterCommand>()->run(ctx);
    check(nijigenerate::compat::ngRequireObject(mouthResult)->succeeded, "AddMouthParameterCommand should succeed");
    auto mouth = mouthResult->created[0];
    check(nijigenerate::compat::ngRequireObject(mouth)->isVec2, "mouth preset should create 2D parameter");
    check(nearVec2(mouth->min, Vec2{-1, 0}) && nearVec2(mouth->max, Vec2{1, 1}), "mouth preset should set range");
    assertAxisPoints(mouth, 0, Slice<float>{0.0f, 0.25f, 0.5f, 0.6f, 1.0f}, RetainedString::fromStaticLiteral("mouth X"));
    assertAxisPoints(mouth, 1, Slice<float>{0.0f, 0.3f, 0.5f, 0.6f, 1.0f}, RetainedString::fromStaticLiteral("mouth Y"));
    check(nijigenerate::project::incActivePuppet()->parameters.size() == 5, "all parameter presets should be added");
    incActionUndo();
    check(nijigenerate::project::incActivePuppet()->parameters.size() == 4, "undo should remove latest preset");
    incActionRedo();
    check(nijigenerate::project::incActivePuppet()->parameters.size() == 5, "redo should restore latest preset");
}

void testParameterLifecycleUndoRedo() {
    using nicxlive::core::param::Parameter;
    using nicxlive::core::common::NullableString;
    using namespace nijigenerate::actions::parameter;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::compat::RetainedString;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto param = Parameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("Param"); }, [] { return true; });
    param->min = {-1, -1};
    param->max = {1, 1};
    nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param);
    incActionPush(ParameterAddAction::createSourceDeferred([&] { return param; }));
    check(hasParameter(param), "parameter add should apply");
    incActionUndo();
    check(!hasParameter(param), "parameter add undo should remove parameter");
    incActionRedo();
    check(hasParameter(param), "parameter add redo should restore parameter");
    const auto oldName = param->getName();
    param->setName(NullableString::fromStaticLiteral("Renamed"));
    incActionPush(ngCreateParameterValueActionDeferred<ParameterValueChangeAction<NullableString>>(
        [] { return RetainedString::fromStaticLiteral("name"); },
        [&] { return param; }, [&] { return oldName; }, [&] { return param->getName(); },
        [&] { return std::shared_ptr<NullableString>(param, &param->name); }));
    check(param->getName().view() == "Renamed", "parameter rename should apply");
    incActionUndo();
    check(param->getName().view() == "Param", "parameter rename undo should restore old name");
    incActionRedo();
    check(param->getName().view() == "Renamed", "parameter rename redo should restore new name");
    incActionPush(ParameterRemoveAction::createSourceDeferred([&] { return param; }));
    nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet())->removeParameter(param);
    check(!hasParameter(param), "parameter remove should apply");
    incActionUndo();
    check(hasParameter(param), "parameter remove undo should restore parameter");
    incActionRedo();
    check(!hasParameter(param), "parameter remove redo should remove parameter again");
}

void testParameterCommandLifecycleUndoRedo() {
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::parameter::param::Add2DParameterCommand;
    using nijigenerate::commands::parameter::paramedit::DuplicateParameterCommand;
    using nijigenerate::commands::parameter::paramedit::DeleteParameterCommand;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, nijigenerate::compat::RetainedString::fromStaticLiteral(message));
    };
    const auto active = [] {
        return nijigenerate::compat::ngRequireObject(nijigenerate::project::incActivePuppet());
    };
    resetCase();
    auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    auto add2d = nijigenerate::commands::detail::constructSourceCommand<Add2DParameterCommand>(
        std::int32_t{-1}, std::int32_t{1});
    const auto addResult = add2d->run(ctx);
    check(nijigenerate::compat::ngRequireObject(addResult)->succeeded, "Add2DParameterCommand should succeed");
    check(active()->parameters.size() == 1, "Add2DParameterCommand should add one parameter");
    auto param = active()->parameters[0];
    check(nijigenerate::compat::ngRequireObject(param)->isVec2, "Add2DParameterCommand should create a 2D parameter");
    check(nijigenerate::compat::ngRequireObject(param)->axisPoints[0].size() == 3 && param->axisPoints[1].size() == 3,
        "Add2DParameterCommand should create centered axis points for symmetric range");
    incActionUndo();
    check(active()->parameters.size() == 0, "undo Add2DParameterCommand should remove the parameter");
    incActionRedo();
    check(active()->parameters.size() == 1, "redo Add2DParameterCommand should restore the parameter");
    param = active()->parameters[0];
    ctx->parameters(Context::Parameters{param});
    const auto duplicateResult = nijigenerate::commands::detail::constructSourceCommand<DuplicateParameterCommand>()->run(ctx);
    check(nijigenerate::compat::ngRequireObject(duplicateResult)->succeeded, "DuplicateParameterCommand should succeed");
    check(active()->parameters.size() == 2, "DuplicateParameterCommand should add a second parameter");
    auto duplicated = active()->parameters[1];
    check(duplicated != param, "DuplicateParameterCommand should create a new parameter instance");
    const auto dimensionality = nijigenerate::compat::ngRequireObject(duplicated)->isVec2;
    check(dimensionality == nijigenerate::compat::ngRequireObject(param)->isVec2,
        "DuplicateParameterCommand should preserve dimensionality");
    incActionUndo();
    check(active()->parameters.size() == 1, "undo DuplicateParameterCommand should remove duplicated parameter");
    incActionRedo();
    check(active()->parameters.size() == 2, "redo DuplicateParameterCommand should restore duplicated parameter");
    duplicated = active()->parameters[1];
    ctx->parameters(Context::Parameters{duplicated});
    const auto deleteResult = nijigenerate::commands::detail::constructSourceCommand<DeleteParameterCommand>()->run(ctx);
    check(nijigenerate::compat::ngRequireObject(deleteResult)->succeeded, "DeleteParameterCommand should succeed");
    check(active()->parameters.size() == 1, "DeleteParameterCommand should remove selected parameter");
    incActionUndo();
    check(active()->parameters.size() == 2, "undo DeleteParameterCommand should restore selected parameter");
    incActionRedo();
    check(active()->parameters.size() == 1, "redo DeleteParameterCommand should remove selected parameter again");
}

void testTypedSettingsStore() {
    using namespace nijigenerate::core::settings;
    using nijigenerate::compat::RetainedString;
    const auto literal = []<std::size_t N>(const char (&text)[N]) {
        return SettingsString(RetainedString::fromStaticLiteral(text));
    };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    incSettingsSet(literal("Language"), literal("ja_JP"));
    incSettingsSet(literal("UiScale"), 1.25f);
    incSettingsSet(literal("ShowDebugOverlay"), true);
    incSettingsSet(literal("MaxUndoHistory"), std::size_t{42});
    incSettingsSet(literal("RecentFiles"), SettingsStringArray{literal("a.inx"), literal("b.inx")});
    check(incSettingsGet<SettingsString>(literal("Language")).view() == "ja_JP", "string setting should round-trip");
    check(near(incSettingsGet<float>(literal("UiScale")), 1.25f), "float setting should round-trip");
    check(incSettingsGet<bool>(literal("ShowDebugOverlay")), "bool setting should round-trip");
    check(incSettingsGet<std::size_t>(literal("MaxUndoHistory")) == 42, "size_t setting should round-trip");
    const auto actual = incSettingsGet<SettingsStringArray>(literal("RecentFiles"));
    const SettingsStringArray expected{literal("a.inx"), literal("b.inx")};
    bool equal = actual.size() == expected.size();
    for (std::size_t i = 0; equal && i < actual.size(); ++i) equal = actual[i].view() == expected[i].view();
    check(equal, "string[] setting should round-trip");
    check(incSettingsCanGet(literal("ViewportZoomMode")), "default settings should be loaded");
}

void testAiMcpSettingsOptInPersistence() {
    using namespace nijigenerate::core::settings;
    using nijigenerate::compat::RetainedString;
    const auto literal = []<std::size_t N>(const char (&text)[N]) {
        return SettingsString(RetainedString::fromStaticLiteral(text));
    };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    SettingsStringMap servers;
    servers[literal("local")] = literal("127.0.0.1:39200");
    incSettingsSet(literal("MCP.Enabled"), false);
    incSettingsSet(literal("MCP.Servers"), servers);
    incSettingsSet(literal("ACP.Enabled"), false);
    incSettingsSet(literal("ACP.Command"), literal("codex"));
    incSettingsSet(literal("ACP.Workdir"), literal("/tmp/nijigenerate-acp"));
    check(!incSettingsGet<bool>(literal("MCP.Enabled")), "MCP should remain opt-in false in settings store");
    check(incSettingsGet<SettingsStringMap>(literal("MCP.Servers"))[literal("local")].view() == "127.0.0.1:39200",
        "MCP server map should round-trip");
    check(!incSettingsGet<bool>(literal("ACP.Enabled")), "ACP should remain opt-in false in settings store");
    check(incSettingsGet<SettingsString>(literal("ACP.Command")).view() == "codex", "ACP command should round-trip");
    check(incSettingsGet<SettingsString>(literal("ACP.Workdir")).view() == "/tmp/nijigenerate-acp",
        "ACP workdir should round-trip");
}

void testAcpProtocolTypesAndErrorJson() {
    using namespace nijigenerate::api::acp::protocol;
    using namespace nijigenerate::api::acp::types;
    using nijigenerate::compat::RetainedString;
    const auto literal = []<std::size_t N>(const char (&text)[N]) {
        return RetainedString::fromStaticLiteral(text);
    };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    check(ACP_PROTOCOL_VERSION == 1, "ACP protocol version should remain stable");
    check(JSONRPC_VERSION.view() == "2.0", "ACP should use JSON-RPC 2.0");
    check(ACP_METHOD_INITIALIZE.view() == "initialize", "ACP initialize method constant should be stable");
    check(ACP_METHOD_PING.view() == "ping", "ACP ping method constant should be stable");
    auto err = std::allocate_shared<ACPError>(nicxlive::core::d_runtime::SourceAllocator<ACPError>{},
        static_cast<std::int32_t>(ErrorCode::invalidParams), literal("bad params"), literal("missing field"),
        literal("source/nijigenerate_tests/regression.d"), std::size_t{18904});
    auto json = err->toJSON();
    check(json.is_object(), "ACP error should serialize to an object");
    check(json.sourceIndexValue("jsonrpc").as_string().view() == JSONRPC_VERSION.view(),
        "ACP error should include jsonrpc version");
    check(json.sourceIndexValue("error").sourceIndexValue("code").as_int64()
            == static_cast<std::int32_t>(ErrorCode::invalidParams), "ACP error should include numeric code");
    check(json.sourceIndexValue("error").sourceIndexValue("message").as_string().view() == "bad params",
        "ACP error should include message");
    check(json.sourceIndexValue("error").sourceIndexValue("data").as_string().view() == "missing field",
        "ACP error should include optional details");
    Document doc;
    doc.uri = literal("file:///tmp/model.inx");
    doc.languageId = literal("json");
    doc.text = literal("{}");
    doc.version_ = 7;
    check(doc.version_ == 7 && doc.uri.view().ends_with("model.inx"),
        "ACP document payload should store uri and version");
    TextEdit edit;
    edit.range.start = Position{1, 2};
    edit.range.end = Position{3, 4};
    edit.newText = literal("replacement");
    WorkspaceEdit workspaceEdit;
    workspaceEdit.uri = doc.uri;
    workspaceEdit.edits = nijigenerate::compat::Slice<TextEdit>{edit};
    check(workspaceEdit.edits.size() == 1, "ACP workspace edit should store text edits");
    check(workspaceEdit.edits[0].range.start.line == 1, "ACP range start should be preserved");
    StatusNotification status;
    status.title = literal("Running");
    status.message = literal("Testing");
    status.level = StatusLevel::progress;
    check(status.level == StatusLevel::progress, "ACP status notification level should be preserved");
}

void testMeshSplineContracts() {
    using nijigenerate::viewport::common::spline::CatmullSpline;
    using nijigenerate::viewport::common::spline::SplinePoint;
    using nijigenerate::viewport::common::mesh::IncMesh;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::math::Mat4;
    using nicxlive::core::nodes::MeshData;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, nijigenerate::compat::RetainedString::fromStaticLiteral(message));
    };
    auto spline = std::allocate_shared<CatmullSpline>(nicxlive::core::d_runtime::SourceAllocator<CatmullSpline>{});
    spline->resolution = 10;
    spline->points = {SplinePoint{{0, 0}, 1, 1}, SplinePoint{{10, 0}, 1, 1}};
    spline->interpolate();
    check(nearVec2(spline->eval(0), {0, 0}), "spline eval should clamp to the first point");
    check(nearVec2(spline->eval(1), {10, 0}), "spline eval should clamp to the last point");
    check(nearVec2(spline->eval(0.5f), {5, 0}), "two-point spline should interpolate linearly");
    Vec2 tangent{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    const auto off = spline->findClosestPointOffset({4.25f, 2}, tangent);
    check(near(off, 0.425f), "closest-point offset should project onto a linear segment");
    check(nearVec2(tangent, {1, 0}), "closest-point tangent should follow the segment direction");
    const auto inserted = spline->addPoint({5, 0.2f}, 1.0f);
    check(inserted == 1 && spline->points.size() == 3, "addPoint near a segment should split the spline");
    check(nearVec2(spline->points[1].position, {5, 0}), "split point should be placed on the curve");
    spline->removePoint(1);
    check(spline->points.size() == 2, "removePoint should remove the inserted point");
    spline->prependPoint({-10, 0});
    spline->appendPoint({20, 0});
    check(spline->points.size() == 4, "prependPoint and appendPoint should mutate endpoints");
    check(spline->findPoint({-10, 0}, 1.0f) == 0, "findPoint should find an endpoint within radius");
    check(spline->findPoint({1000, 1000}, 1.0f) == -1, "findPoint should reject distant points");
    MeshData data;
    data.vertices = Vec2Array{Vec2{10, 0}, Vec2{11, 0}, Vec2{10, 1}};
    data.uvs = Vec2Array{Vec2{0, 0}, Vec2{1, 0}, Vec2{0, 1}};
    data.indices = {0, 1, 2};
    auto mesh = IncMesh::create(data);
    auto deformPath = std::allocate_shared<CatmullSpline>(nicxlive::core::d_runtime::SourceAllocator<CatmullSpline>{});
    deformPath->resolution = 10;
    deformPath->points = {SplinePoint{{0, 0}, 1, 1}, SplinePoint{{10, 0}, 1, 1}, SplinePoint{{20, 0}, 1, 1}};
    deformPath->interpolate();
    deformPath->createTarget(mesh, Mat4::identity());
    const auto offsetCount = deformPath->refOffsets.size();
    check(offsetCount == nijigenerate::compat::ngRequireObject(mesh)->vertices.size(),
        "createTarget should map every mesh vertex to the spline");
    deformPath->splitAt(0.5f);
    const auto targetPointCount = nijigenerate::compat::ngRequireObject(deformPath->target)->points.size();
    check(targetPointCount == deformPath->points.size(),
        "splitAt should propagate point insertion to the mapped target spline");
}

// Original regression.d19591-19610.
void testMeshCommonVertexConnections() {
    using namespace nijigenerate::core::math::mesh;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, nijigenerate::compat::RetainedString::fromStaticLiteral(message));
    };
    // The original uses three local value records and passes their real addresses.
    MeshVertex a;
    a.position = {0, 0};
    MeshVertex b;
    b.position = {1, 0};
    MeshVertex c;
    c.position = {2, 0};
    check(!isConnectedTo(&a, &b), "vertices should start disconnected");
    connect(&a, &b);
    check(isConnectedTo(&a, &b) && isConnectedTo(&b, &a), "connect should be symmetric");
    connect(&a, &b);
    check(a.connections.size() == 1 && b.connections.size() == 1, "connect should not duplicate existing edges");
    connect(&a, &c);
    check(a.connections.size() == 2 && isConnectedTo(&c, &a), "connect should support multiple neighbors");
    disconnect(&a, &b);
    check(!isConnectedTo(&a, &b) && !isConnectedTo(&b, &a), "disconnect should be symmetric");
    check(isConnectedTo(&a, &c), "disconnect should leave other edges intact");
    disconnectAll(&a);
    check(a.connections.size() == 0 && !isConnectedTo(&a, &c), "disconnectAll should remove every edge symmetrically");
}

// Original regression.d20455-20482, using the real editor task queue.
void testPlatformTaskQueue() {
    using namespace nijigenerate::core::tasks;
    using nijigenerate::compat::RetainedString;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    while (ngTaskLength() > 0) ngTaskUpdate();
    // The queued Source delegate retains its captured variable even if the
    // enclosing case exits through require. This is that real captured cell.
    auto step = std::allocate_shared<std::int32_t>(nicxlive::core::d_runtime::SourceAllocator<std::int32_t>{}, 0);
    ngTaskAddSource(RetainedString::fromStaticLiteral("regression task"), TaskWorker([step] {
        *step = 1;
        ngTaskStatusSource(RetainedString::fromStaticLiteral("half"));
        ngTaskProgress(0.5f);
        ngTaskYield();
        *step = 2;
    }));
    check(ngTaskLength() == 1, "adding a task should enqueue it");
    ngTaskUpdate();
    check(*step == 1, "first task update should run until yield");
    check(ngTaskGetStatusSource().view() == "half", "task status should be set by worker");
    check(ngTaskGetProgress() == 0.5f, "task progress should be set by worker");
    check(ngTaskLength() == 1, "yielded task should remain queued");
    ngTaskUpdate();
    check(*step == 2, "second task update should complete worker");
    ngTaskUpdate();
    check(ngTaskLength() == 0, "completed task should be removed on update");
    check(ngTaskGetProgress() == -1, "completed queue should reset progress");
    check(ngTaskGetStatusSource().view() == "No pending tasks...", "empty task queue should reset status");
}

void testMaskSourceAddUndoRedo() {
    using nijigenerate::actions::node::PartAddMaskAction;
    using nicxlive::core::nodes::MaskingMode;
    using nijigenerate::compat::RetainedString;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto target = newPart(RetainedString::fromStaticLiteral("target"));
    auto mask = newPart(RetainedString::fromStaticLiteral("mask"));
    incActionPush(std::allocate_shared<PartAddMaskAction>(
        nicxlive::core::d_runtime::SourceAllocator<PartAddMaskAction>{}, mask, target, MaskingMode::Mask));
    {
        const nicxlive::core::common::SharedArray<std::uint32_t> expected{mask->getUUID()};
        assertMaskOrder(target, std::span<const std::uint32_t>(expected.data(), expected.size()),
            RetainedString::fromStaticLiteral("mask add should apply immediately"));
    }
    incActionUndo();
    assertMaskOrder(target, {}, RetainedString::fromStaticLiteral("mask add undo should remove the mask"));
    incActionRedo();
    {
        const nicxlive::core::common::SharedArray<std::uint32_t> expected{mask->getUUID()};
        assertMaskOrder(target, std::span<const std::uint32_t>(expected.data(), expected.size()),
            RetainedString::fromStaticLiteral("mask add redo should restore the mask"));
    }
}

void testMaskSourceReorderUndoRedo() {
    using nijigenerate::actions::node::PartMaskListChangeAction;
    using nicxlive::core::nodes::MaskBinding;
    using nicxlive::core::nodes::MaskingMode;
    using nijigenerate::compat::RetainedString;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    auto target = newPart(RetainedString::fromStaticLiteral("target"));
    auto maskA = newPart(RetainedString::fromStaticLiteral("maskA"));
    auto maskB = newPart(RetainedString::fromStaticLiteral("maskB"));
    auto maskC = newPart(RetainedString::fromStaticLiteral("maskC"));
    target->masks = {MaskBinding{maskA->getUUID(), maskA, MaskingMode::Mask},
        MaskBinding{maskB->getUUID(), maskB, MaskingMode::Mask},
        MaskBinding{maskC->getUUID(), maskC, MaskingMode::DodgeMask}};
    auto oldMasks = target->masks.duplicate();
    auto moving = oldMasks[1];
    auto newMasks = oldMasks.slice(0, 1).duplicate();
    newMasks.appendRange(oldMasks.slice(2, oldMasks.size()));
    auto reordered = newMasks.slice(0, 0).duplicate();
    reordered.push_back(moving);
    reordered.appendRange(newMasks.slice(0, newMasks.size()));
    newMasks = reordered;
    incActionPush(PartMaskListChangeAction::createSourceDeferred(
        [] { return RetainedString::fromStaticLiteral("Reorder Mask Source"); },
        [&] { return target; }, [&] { return oldMasks; }, [&] { return newMasks; }));
    const std::array after{maskB->getUUID(), maskA->getUUID(), maskC->getUUID()};
    assertMaskOrder(target, after, RetainedString::fromStaticLiteral("mask reorder should apply immediately"));
    incActionUndo();
    const std::array before{maskA->getUUID(), maskB->getUUID(), maskC->getUUID()};
    assertMaskOrder(target, before, RetainedString::fromStaticLiteral("mask reorder undo should restore exact old order"));
    incActionRedo();
    const std::array redone{maskB->getUUID(), maskA->getUUID(), maskC->getUUID()};
    assertMaskOrder(target, redone, RetainedString::fromStaticLiteral("mask reorder redo should restore exact new order"));
}

void testMaskSourceModeUndoRedo() {
    using nijigenerate::actions::node::PartChangeMaskModeAction;
    using nicxlive::core::nodes::MaskingMode;
    using nicxlive::core::nodes::MaskBinding;
    using nijigenerate::compat::RetainedString;
    using namespace nijigenerate::core::actionstack;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto target = newPart(RetainedString::fromStaticLiteral("target"));
    auto mask = newPart(RetainedString::fromStaticLiteral("mask"));
    target->masks = {MaskBinding{mask->getUUID(), mask, MaskingMode::Mask}};
    incActionPush(std::allocate_shared<PartChangeMaskModeAction>(
        nicxlive::core::d_runtime::SourceAllocator<PartChangeMaskModeAction>{}, target, mask, MaskingMode::DodgeMask));
    check(target->masks[0].mode == MaskingMode::DodgeMask, "mask mode change should apply immediately");
    incActionUndo();
    check(target->masks[0].mode == MaskingMode::Mask, "mask mode undo should restore old mode");
    incActionRedo();
    check(target->masks[0].mode == MaskingMode::DodgeMask, "mask mode redo should restore new mode");
}

void printComputerUseManifest() {
    const auto scenarios = allScenarios();
    for (const auto scenario : scenarios) {
        if (scenario.status.view() == computerUse) {
            const auto line = nijigenerate::compat::ngJsonStringify(scenarioToJson(scenario));
            detail::writeLine(detail::standardOutput(), {line.view()});
        }
    }
}

// Original regression.d22043-22692; every branch below comes from that dispatcher.
std::size_t runCompositeMatrixGridModes(nijigenerate::compat::RetainedString onlyId = {});
std::size_t runCompositeMatrixNodeTypes(nijigenerate::compat::RetainedString onlyId = {});
std::size_t runCompositeMatrixParameterPresets(nijigenerate::compat::RetainedString onlyId = {});
std::size_t runCompositeMatrixDepthOps(nijigenerate::compat::RetainedString onlyId = {});
std::size_t runCompositeMatrixAutoMeshProcessors(nijigenerate::compat::RetainedString onlyId = {});
std::size_t runCompositeMatrixSimplePhysicsSettings(nijigenerate::compat::RetainedString onlyId = {});

bool runGeneratedCompositeCaseScenario(nijigenerate::compat::RetainedString id) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::SharedDelegate;
    if (id.view().starts_with("mesh.composite-grid-mode-matrix.")) {
        runCase(id, SharedDelegate<void()>([id] {
            const bool condition = runCompositeMatrixGridModes(id) == 1;
            auto message = concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "generated grid/mode scenario should execute exactly one case: "), id});
            require(condition, message);
        }));
        return true;
    }
    if (id.view().starts_with("node.composite-type-matrix.")) {
        runCase(id, SharedDelegate<void()>([id] {
            const bool condition = runCompositeMatrixNodeTypes(id) == 1;
            auto message = concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "generated node-type scenario should execute exactly one case: "), id});
            require(condition, message);
        }));
        return true;
    }
    if (id.view().starts_with("parameter.composite-preset-matrix.")) {
        runCase(id, SharedDelegate<void()>([id] {
            const bool condition = runCompositeMatrixParameterPresets(id) == 1;
            auto message = concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "generated parameter preset scenario should execute exactly one case: "), id});
            require(condition, message);
        }));
        return true;
    }
    if (id.view().starts_with("depth.composite-operation-matrix.")) {
        runCase(id, SharedDelegate<void()>([id] {
            const bool condition = runCompositeMatrixDepthOps(id) == 1;
            auto message = concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "generated depth operation scenario should execute exactly one case: "), id});
            require(condition, message);
        }));
        return true;
    }
    if (id.view().starts_with("automesh.composite-processor-matrix-scenario.")) {
        runCase(id, SharedDelegate<void()>([id] {
            const bool condition = runCompositeMatrixAutoMeshProcessors(id) == 1;
            auto message = concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "generated automesh processor scenario should execute exactly one case: "), id});
            require(condition, message);
        }));
        return true;
    }
    if (id.view().starts_with("simplephysics.composite-settings-matrix.")) {
        runCase(id, SharedDelegate<void()>([id] {
            const bool condition = runCompositeMatrixSimplePhysicsSettings(id) == 1;
            auto message = concatenateSourceStrings({RetainedString::fromStaticLiteral(
                "generated simplephysics setting scenario should execute exactly one case: "), id});
            require(condition, message);
        }));
        return true;
    }
    return false;
}
void testCoverageSourceCommandInventory();
void testCommandBaseContracts();
void testCoverageFullFeatureScenarioInventory();
void testCoverageCompositeWorkflowInventory();
void testCompositeCaseMatrixBreadth();
void testCompositeMatrixGridModesScenario();
void testCompositeMatrixNodeTypesScenario();
void testCompositeMatrixParameterPresetsScenario();
void testCompositeMatrixDepthOpsScenario();
void testDepthTargetViewContracts();
void testCompositeMatrixAutoMeshProcessorsScenario();
void testCompositeMatrixSimplePhysicsSettingsScenario();
void testCoverageSourceModuleScenarioInventory();
void testProjectNewSaveOpenCommandPaths();
void testPuppetMemoryRestoreDirtyState();
void testProjectCompositeCommandRoundTrip();
void testProjectImageMergeExportCompositeWorkflow();
void testProjectINXSerializationRoundTrip();
void testProjectCameraViewportRoundTrip();
void testNativeSavePathOverwriteAndReload();
void testImageCodecRoundTrips();
void testProjectAutosaveRecoveryRecords();
void testProjectSessionImportCommandPath();
void testProjectRecentFilesSettings();
void testProjectINPImportMergeRoundTripCommandPaths();
void testProjectINPExportPrunesDepthRigNodes();
void testProjectImportImagesCommandPaths();
void testPsdDepthMapImportHelpers();
void testPsdDepthAdjustedValuesAreNotRenormalized();
void testDepthImageFacadeMatchesPsdDepthSampling();
void testDepthDrawCalculationGateContracts();
void testDepthDrawFixtureRunner();
void testDepthDrawDataModelContracts();
void testDepthDrawComposerContracts();
void testDepthDrawFitZContracts();
void testPSDAndKRAReaderImportMergeFixtures();
void testRigClippingVisibility();
void testAutoMeshParentCancellation();
void testProjectTextureMaintenanceCommands();
void testProjectRepairMaintenanceCommands();
void testAtlasPackerRectLifecycle();
void testRenderAtlasPackerTextureSlots();
void testColorBleedPreservesAlphaAndExtendsColor();
void testNodeCommandCreateDeleteToggleUndoRedo();
void testNodeClipboardCopyPasteUndoRedo();
void testNodeDynamicAddTypesUndoRedo();
void testNodeDynamicInsertTypesUndoRedo();
void testNodeRegistryDynamicCommandCoverage();
void testSelectorQueryAndTreeStore();
void testNodeCommandMoveUndoRedo();
void testNodeCommandMovePreservesWorldTransform();
void testNodeCentralizeCommandUndoRedo();
void testNodeInspectorTransformUndoRedo();
void testNodeTypeInspectorCommandsUndoRedo();
void testPuppetInspectorStateRoundTrip();
void testNodeConvertCommandUndoRedo();
void testNodeTypeConversionMapUndoRedo();
void testNodeTypeConversionPreservesChildWorldTransform();
void testNodeTypeConversionPreservesLockedAndOffsetChildBaseTransform();
void testNodeCompositeHierarchyBindingRoundTrip();
void testDefineGridCommandUndoRedo();
void testMcpModeHistoryCompositeWorkflows();
void testPsdDepthDialogCommandsUndoRedo();
void testDefineMeshAndVerticesCommandsUndoRedo();
void testMeshCompositeTopologyScopeWorkflow();
void testMeshCommonVertexConnections();
void testMeshEditorOperationTargets();
void testMeshEditorMultiObjectApplyUndoRedo();
void testMeshEditorMirrorSymmetryContracts();
void testOnionSliceCaptureGeometry();
void testOneTimeDeformerSchedulesCapturedTargetsOnce();
void testOneTimeDeformerPreservesUpdatePhasesAndDeformation();
void testOneTimeDeformerSelectedVertices();
void testGridDeformerToolVirtualMeshApplyUndoRedo();
void testPathDeformerToolApplyUndoRedo();
void testMeshGroupGridDeformerCompatibility();
void testDeformCompositeGridPathBindingWorkflow();
void testGridDeformerRuntimeInterpolationContracts();
void testPathDeformerRuntimeContracts();
void testMeshSplineContracts();
void testCoreMathTriangleInvariants();
void testCoreMathPathExtraction();
void testRejectedMeshResamplingGroupCleanup();
void testCoreMathSkeletonizeInvariants();
void testCoreCvImageContoursDistanceTransform();
void testAutoMeshBatchConfigUndoRedo();
void testAutoMeshCompositeProcessorMatrix();
void testAutoMeshGridProcessorDeterministicOutput();
void testAutoMeshContourProcessorDeterministicOutput();
void testAutoMeshSkeletonProcessorDeterministicOutput();
void testAutoMeshOptimumProcessorDeterministicOutput();
void testAutoMeshAlphaProviderAndNonPartCachedInput();
void testAutoMeshSchemaValuesPresetsAndActiveProcessor();
void testPartInspectorPropertiesUndoRedo();
void testPartCompositeInspectorMeshRoundTrip();
void testPartTextureReloadFixture();
void testPartClippingMaskPropertiesUndoRedo();
void testMaskSourceAddUndoRedo();
void testMaskSourceReorderUndoRedo();
void testDepthBoneActionsUndoRedo();
void testDepthBoneSourceCommandsUndoRedo();
void testDepthBoneSourceSettingsActionMerge();
void testDepthBoneSourceRefreshIgnoresPhysicsParameter();
void testDepthRigRootFitZToDepth();
void testDepthBonePlanarRestAxisIgnoresChildZ();
void testDepthBoneInspectorCommandsUndoRedo();
void testMeshDeformerInspectorCommandsUndoRedo();
void testInspectorCompositeNodePartMeshWorkflow();
void testDepthBoneStandardSkeletonTemplate();
void testDepthBoneCreationNotifiesResourceViews();
void testDepthBoneStandardParameterTemplate();
void testDepthBoneSerializationRoundTrip();
void testDepthBoneInfluenceRuleCommandUndoRedo();
void testDepthBonePreviewApplyCommands();
void testDepthBoneGpuOffsetPacketConstruction();
void testDepthBoneSourceRotationDeform();
void testDepthBoneGpuAllKeypointsDispatch();
void testDepthBoneFrameFailureRecovery();
void testDepthBoneOverlaySelectionGeometry();
void testDepthBoneUpdateVisualizationState();
void testAsyncDerivedUpdateRegistry();
void testDepthBoneSkinningLockToRootTerminal();
void testDepthBoneDeleteCleanupUndoRedo();
void testDepthBoneConversionCleanupUndoRedo();
void testMaskSourceModeUndoRedo();
void testCoincidentWeldingVertexSamples();
void testWeldingUndoRedo();
void testPreparedWeldingMesh();
void testWeldingRuntimeDeformation();
void testParameterLifecycleUndoRedo();
void testParameterCommandLifecycleUndoRedo();
void testParameterCommandRenameAndGroupUndoRedo();
void testParameterCreatePresets();
void testParameterSplitBindingMigrationUndoRedo();
void testParameterCopyPasteDuplicateAndLinkCommands();
void testParameterArmSelectAndKeypointCommands();
void testParameterStartingKeyframeCommandUndoRedo();
void testParameterKeyframeCommandUndoRedo();
void testParameterKeyframeMirrorFillCommands();
void testParameterBindingClipboardAndCleanupCommands();
void testParameter2DAnimationKeyframeGroupUndoRedo();
void testParameterBindingInterpolation();
void testParameterTRSBindingModelCommandUndoRedo();
void testParameterRotationXBindingPartUndoRedo();
void testParameterDeformBindingModelCommandUndoRedo();
void testParameterMeshBindingCompositeWorkflow();
void testParameterCompositeLifecycleBindingWorkflow();
void testParameterAxesPropsCommandUndoRedo();
void testParameterBindingCleanupOnDeleteUndoRedo();
void testAnimationLifecycleUndoRedo();
void testAnimationTrackBindingCleanup();
void testAnimationCompositeKeyframeHistory();
void testAnimationKeyframesUndoRedo();
void testNodeNameUndoMergesPerNode();
void testSimplePhysicsParameterUndoRedo();
void testSimplePhysicsSettingsUndoRedo();
void testSimplePhysicsSerializationRoundTrip();
void testSimplePhysicsCompositeRoundTripWorkflow();
void testActionGroupUndoRedo();
void testAsyncActionGroupUndoRedo();
void testActionMergeSemantics();
void testActionHistoryIndexAndModifiedState();
void testAutoRigPresentationCommands();
void testViewportSettingsCommandCompositeWorkflow();
void testViewportFlipPairCommands();
void testViewportPaletteCommandDiscovery();
void testDepthMappedNodeSerializationRoundTrip();
void testDepthOperationHelperContracts();
void testDepthSignColorContracts();
void testDepthCameraProjectionContracts();
void testRenderCameraExportCommands();
void testRenderCameraTextureCompositeWorkflow();
void testDepthMapCommandsUndoRedo();
void testAllPsdDepthDialogCommands();
void testPsdDepthImportRefreshesDepthBoneBindings();
void testDepthCompositeMapOpsWorkflow();
void testDepthBoneCompositeSourcePreviewWorkflow();
void testDepthBoneAncestorPropagationGates();
void testActionStackScopeGuard();
void testUndoDirectMutationAudit();
void testUndoCommandActionsAudit();
void testUndoCompositeCrossFamilyStackWorkflow();
void testShortcutSettingsConflictAndReload();
void testSettingsCompositeShortcutsRecentWorkflow();
void testDefaultShortcutRegistration();
void testTypedSettingsStore();
void testAiMcpSettingsOptInPersistence();
void testAcpProtocolTypesAndErrorJson();
void testAcpClientSourceContract();
void testMcpResourceListingAndContextHelpers();
void testMcpTaskQueueMainThreadDispatch();
void testMcpFailedStartupRetry();
void testApiTransportAndServerContracts();
void testSettingsPathResolution();
void testPlatformVersionMetadata();
void testPlatformTaskQueue();
void testPlatformCrashDumpGeneration();
void testPlatformWindowsConsoleWritesAreGuarded();
void testPlatformStartupShutdownModuleConstructors();
void testI18nVisibleUiStringsAreWrapped();
void testI18nPotTemplateCoversSimpleSourceLiterals();
void testInspectorFormatStringsDoNotEscapeNumericFormats();

void testCommandBrowserResourcePickerSelectionsAreInitialized();
void testCommandBrowserResourceArgumentParsingResolvesLiveResources();
void testCommandBrowserDifferentialReportMatchesBaseline();

bool runAutomatedScenario(nijigenerate::compat::RetainedString id) {
    if (runGeneratedCompositeCaseScenario(id)) return true;
    if (id.view() == "coverage.source-command-inventory") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("coverage-source-command-inventory"), &testCoverageSourceCommandInventory);
        return true;
    }
    if (id.view() == "coverage.command-base") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("command-base-contracts"), &testCommandBaseContracts);
        return true;
    }
    if (id.view() == "coverage.full-feature-scenario-inventory") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("coverage-full-feature-scenario-inventory"), &testCoverageFullFeatureScenarioInventory);
        return true;
    }
    if (id.view() == "coverage.composite-workflow-inventory") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("coverage-composite-workflow-inventory"), &testCoverageCompositeWorkflowInventory);
        return true;
    }
    if (id.view() == "coverage.composite-case-matrix") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("coverage-composite-case-matrix"), &testCompositeCaseMatrixBreadth);
        return true;
    }
    if (id.view() == "mesh.composite-grid-mode-matrix") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mesh-composite-grid-mode-matrix"), &testCompositeMatrixGridModesScenario);
        return true;
    }
    if (id.view() == "node.composite-type-matrix") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-composite-type-matrix"), &testCompositeMatrixNodeTypesScenario);
        return true;
    }
    if (id.view() == "parameter.composite-preset-matrix") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-composite-preset-matrix"), &testCompositeMatrixParameterPresetsScenario);
        return true;
    }
    if (id.view() == "depth.composite-operation-matrix") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depth-composite-operation-matrix"), &testCompositeMatrixDepthOpsScenario);
        return true;
    }
    if (id.view() == "depth.common-target-view") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depth-target-view-contracts"), &testDepthTargetViewContracts);
        return true;
    }
    if (id.view() == "automesh.composite-processor-matrix-scenario") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("automesh-composite-processor-matrix-scenario"), &testCompositeMatrixAutoMeshProcessorsScenario);
        return true;
    }
    if (id.view() == "simplephysics.composite-settings-matrix") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("simplephysics-composite-settings-matrix"), &testCompositeMatrixSimplePhysicsSettingsScenario);
        return true;
    }
    if (id.view() == "coverage.source-module-scenario-inventory") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("coverage-source-module-scenario-inventory"), &testCoverageSourceModuleScenarioInventory);
        return true;
    }
    if (id.view() == "project.new-open-save") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-new-save-open-command-paths"), &testProjectNewSaveOpenCommandPaths);
        return true;
    }
    if (id.view() == "project.memory-restore") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("memory-restore-preserves-dirty-state"), &testPuppetMemoryRestoreDirtyState);
        return true;
    }
    if (id.view() == "project.composite-command-roundtrip") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-composite-command-roundtrip"), &testProjectCompositeCommandRoundTrip);
        return true;
    }
    if (id.view() == "project.composite-image-merge-export" ||
        id.view() == "io.composite-import-export-roundtrip") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-image-merge-export-composite"), &testProjectImageMergeExportCompositeWorkflow);
        return true;
    }
    if (id.view() == "io.serialization-inx" ||
        id.view() == "io.serialization-textures") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-inx-serialization-roundtrip"), &testProjectINXSerializationRoundTrip);
        return true;
    }
    if (id.view() == "io.camera-viewport-roundtrip") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-camera-viewport-roundtrip"), &testProjectCameraViewportRoundTrip);
        return true;
    }
    if (id.view() == "io.save-native") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("native-save-path-overwrite-reload"), &testNativeSavePathOverwriteAndReload);
        return true;
    }
    if (id.view() == "io.image-codecs") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("image-codec-round-trips"), &testImageCodecRoundTrips);
        return true;
    }
    if (id.view() == "project.autosave-recovery") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-autosave-recovery-records"), &testProjectAutosaveRecoveryRecords);
        return true;
    }
    if (id.view() == "project.session-import") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-session-import-command-path"), &testProjectSessionImportCommandPath);
        return true;
    }
    if (id.view() == "project.recent-files") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-recent-files-settings"), &testProjectRecentFilesSettings);
        return true;
    }
    if (id.view() == "project.import-inp" ||
        id.view() == "project.merge-inp" ||
        id.view() == "project.merge-psd-kra-inp" ||
        id.view() == "io.serialization-inp" ||
        id.view() == "io.inimport-model") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-inp-import-merge-roundtrip-command-paths"), &testProjectINPImportMergeRoundTripCommandPaths);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-inp-export-prunes-depth-rig-nodes"), &testProjectINPExportPrunesDepthRigNodes);
        return true;
    }
    if (id.view() == "project.export-inp" ||
        id.view() == "io.inpexport-model" ||
        id.view() == "depthbone.export-pruning") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-inp-export-prunes-depth-rig-nodes"), &testProjectINPExportPrunesDepthRigNodes);
        return true;
    }
    if (id.view() == "project.import-images" ||
        id.view() == "project.merge-images") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-import-images-command-paths"), &testProjectImportImagesCommandPaths);
        return true;
    }
    if (id.view() == "project.import-psd-depth-map") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("psd-depth-map-import-helpers"), &testPsdDepthMapImportHelpers);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("psd-depth-adjusted-values-are-not-renormalized"), &testPsdDepthAdjustedValuesAreNotRenormalized);
        return true;
    }
    if (id.view() == "project.depthdraw-compatibility") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depth-image-facade-matches-psd-depth-sampling"), &testDepthImageFacadeMatchesPsdDepthSampling);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthdraw-calculation-gate-contracts"), &testDepthDrawCalculationGateContracts);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthdraw-fixture-runner"), &testDepthDrawFixtureRunner);
        return true;
    }
    if (id.view() == "project.depthdraw-data-model") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthdraw-data-model-contracts"), &testDepthDrawDataModelContracts);
        return true;
    }
    if (id.view() == "project.depthdraw-composer") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthdraw-composer-contracts"), &testDepthDrawComposerContracts);
        return true;
    }
    if (id.view() == "project.depthdraw-fit-z") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthdraw-fit-z-contracts"), &testDepthDrawFitZContracts);
        return true;
    }
    if (id.view() == "project.import-psd" ||
        id.view() == "project.import-kra" ||
        id.view() == "project.merge-psd" ||
        id.view() == "project.merge-kra" ||
        id.view() == "io.psd-reader" ||
        id.view() == "io.kra-reader") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("psd-kra-reader-import-merge-fixtures"), &testPSDAndKRAReaderImportMergeFixtures);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("rig-clipping-visibility"), &testRigClippingVisibility);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("automesh-parent-cancellation"), &testAutoMeshParentCancellation);
        return true;
    }
    if (id.view() == "project.texture-maintenance") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-texture-maintenance-commands"), &testProjectTextureMaintenanceCommands);
        return true;
    }
    if (id.view() == "project.repair-maintenance") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("project-repair-maintenance-commands"), &testProjectRepairMaintenanceCommands);
        return true;
    }
    if (id.view() == "atlas.pack") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("atlas-packer-rect-lifecycle"), &testAtlasPackerRectLifecycle);
        return true;
    }
    if (id.view() == "render.atlas-packer") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("render-atlas-packer-texture-slots"), &testRenderAtlasPackerTextureSlots);
        return true;
    }
    if (id.view() == "atlas.color-bleed") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("color-bleed-preserves-alpha-and-extends-color"), &testColorBleedPreservesAlphaAndExtendsColor);
        return true;
    }
    if (id.view() == "node.create-delete-undo") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-command-create-delete-toggle-undo-redo"), &testNodeCommandCreateDeleteToggleUndoRedo);
        return true;
    }
    if (id.view() == "node.cut-copy-paste-duplicate") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-clipboard-copy-paste-undo-redo"), &testNodeClipboardCopyPasteUndoRedo);
        return true;
    }
    if (id.view() == "node.add-types") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-dynamic-add-types-undo-redo"), &testNodeDynamicAddTypesUndoRedo);
        return true;
    }
    if (id.view() == "node.insert-types") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-dynamic-insert-types-undo-redo"), &testNodeDynamicInsertTypesUndoRedo);
        return true;
    }
    if (id.view() == "core.node-registry") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-registry-dynamic-command-coverage"), &testNodeRegistryDynamicCommandCoverage);
        return true;
    }
    if (id.view() == "node.resource-selector" ||
        id.view() == "core.selector-parser") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("selector-query-and-tree-store"), &testSelectorQueryAndTreeStore);
        return true;
    }
    if (id.view() == "node.reparent-order") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-command-move-undo-redo"), &testNodeCommandMoveUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-command-move-preserves-world-transform"), &testNodeCommandMovePreservesWorldTransform);
        return true;
    }
    if (id.view() == "node.centralize") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-centralize-command-undo-redo"), &testNodeCentralizeCommandUndoRedo);
        return true;
    }
    if (id.view() == "node.transform-inspector") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-inspector-transform-undo-redo"), &testNodeInspectorTransformUndoRedo);
        return true;
    }
    if (id.view() == "inspectors.node-types" ||
        id.view() == "inspectors.node" ||
        id.view() == "inspectors.drawable" ||
        id.view() == "inspectors.composite" ||
        id.view() == "inspectors.camera") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-type-inspector-commands-undo-redo"), &testNodeTypeInspectorCommandsUndoRedo);
        return true;
    }
    if (id.view() == "inspectors.puppet") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("puppet-inspector-state-roundtrip"), &testPuppetInspectorStateRoundTrip);
        return true;
    }
    if (id.view() == "node.convert-reload") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-convert-command-undo-redo"), &testNodeConvertCommandUndoRedo);
        return true;
    }
    if (id.view() == "node.type-conversion") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-type-conversion-map-undo-redo"), &testNodeTypeConversionMapUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-type-conversion-preserves-child-world-transform"), &testNodeTypeConversionPreservesChildWorldTransform);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-type-conversion-preserves-locked-offset-child-base-transform"), &testNodeTypeConversionPreservesLockedAndOffsetChildBaseTransform);
        return true;
    }
    if (id.view() == "node.composite-hierarchy-binding-roundtrip") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-composite-hierarchy-binding-roundtrip"), &testNodeCompositeHierarchyBindingRoundTrip);
        return true;
    }
    if (id.view() == "mesh.define-grid-command") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("define-grid-command-undo-redo"), &testDefineGridCommandUndoRedo);
        return true;
    }
    if (id.view() == "api.mcp-mode-history" ||
        id.view() == "undo.composite-workflows") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mcp-mode-history-composite-workflows"), &testMcpModeHistoryCompositeWorkflows);
        return true;
    }
    if (id.view() == "api.command-scope") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("current-command-scope-and-lifecycle"), &testPsdDepthDialogCommandsUndoRedo);
        return true;
    }
    if (id.view() == "mesh.define-mesh-command" ||
        id.view() == "part.uv-mesh-coherence") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("define-mesh-and-vertices-commands-undo-redo"), &testDefineMeshAndVerticesCommandsUndoRedo);
        return true;
    }
    if (id.view() == "mesh.composite-topology-scope") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mesh-composite-topology-scope"), &testMeshCompositeTopologyScopeWorkflow);
        return true;
    }
    if (id.view() == "mesh.common-operations") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mesh-common-vertex-connections"), &testMeshCommonVertexConnections);
        return true;
    }
    if (id.view() == "mesh.operations-node" ||
        id.view() == "mesh.operation-node" ||
        id.view() == "mesh.operation-drawable" ||
        id.view() == "mesh.operation-deformable") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mesh-editor-operation-targets"), &testMeshEditorOperationTargets);
        return true;
    }
    if (id.view() == "mesh.multi-object") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mesh-editor-multi-object-apply-undo-redo"), &testMeshEditorMultiObjectApplyUndoRedo);
        return true;
    }
    if (id.view() == "mesh.mirror-symmetry") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mesh-editor-mirror-symmetry-contracts"), &testMeshEditorMirrorSymmetryContracts);
        return true;
    }
    if (id.view() == "render.onion-slice-geometry") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("onion-slice-capture-geometry"), &testOnionSliceCaptureGeometry);
        return true;
    }
    if (id.view() == "deform.grid-tool") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("onetime-deformer-captured-target-task-registration"), &testOneTimeDeformerSchedulesCapturedTargetsOnce);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("onetime-deformer-update-and-release-equivalence"), &testOneTimeDeformerPreservesUpdatePhasesAndDeformation);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("onetime-deformer-selected-vertex-mask"), &testOneTimeDeformerSelectedVertices);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("grid-deformer-tool-virtual-mesh-apply-undo-redo"), &testGridDeformerToolVirtualMeshApplyUndoRedo);
        return true;
    }
    if (id.view() == "deform.path-tool") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("path-deformer-tool-apply-undo-redo"), &testPathDeformerToolApplyUndoRedo);
        return true;
    }
    if (id.view() == "deform.meshgroup-compat") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("meshgroup-griddeformer-compatibility"), &testMeshGroupGridDeformerCompatibility);
        return true;
    }
    if (id.view() == "deform.composite-grid-path-binding") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("deform-composite-grid-path-binding"), &testDeformCompositeGridPathBindingWorkflow);
        return true;
    }
    if (id.view() == "deform.griddeformer-runtime") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("griddeformer-runtime-interpolation-contracts"), &testGridDeformerRuntimeInterpolationContracts);
        return true;
    }
    if (id.view() == "deform.pathdeformer-runtime") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("pathdeformer-runtime-contracts"), &testPathDeformerRuntimeContracts);
        return true;
    }
    if (id.view() == "mesh.spline") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mesh-spline-contracts"), &testMeshSplineContracts);
        return true;
    }
    if (id.view() == "core.math-mesh") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mesh-common-vertex-connections"), &testMeshCommonVertexConnections);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("core-math-triangle-invariants"), &testCoreMathTriangleInvariants);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("core-math-path-extraction"), &testCoreMathPathExtraction);
        return true;
    }
    if (id.view() == "core.math-path") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("core-math-path-extraction"), &testCoreMathPathExtraction);
        return true;
    }
    if (id.view() == "core.math-triangle") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("core-math-triangle-invariants"), &testCoreMathTriangleInvariants);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("rejected-mesh-resampling-group-cleanup"), &testRejectedMeshResamplingGroupCleanup);
        return true;
    }
    if (id.view() == "core.math-skeletonize") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("core-math-skeletonize-invariants"), &testCoreMathSkeletonizeInvariants);
        return true;
    }
    if (id.view() == "core.cv-image-contours") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("core-cv-image-contours-distance-transform"), &testCoreCvImageContoursDistanceTransform);
        return true;
    }
    if (id.view() == "automesh.batch-undo") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("automesh-batch-config-undo-redo"), &testAutoMeshBatchConfigUndoRedo);
        return true;
    }
    if (id.view() == "automesh.composite-processor-matrix") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("automesh-composite-processor-matrix"), &testAutoMeshCompositeProcessorMatrix);
        return true;
    }
    if (id.view() == "automesh.grid-processor") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("automesh-grid-processor-deterministic-output"), &testAutoMeshGridProcessorDeterministicOutput);
        return true;
    }
    if (id.view() == "automesh.contour-processor") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("automesh-contour-processor-deterministic-output"), &testAutoMeshContourProcessorDeterministicOutput);
        return true;
    }
    if (id.view() == "automesh.skeleton-processor") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("automesh-skeleton-processor-deterministic-output"), &testAutoMeshSkeletonProcessorDeterministicOutput);
        return true;
    }
    if (id.view() == "automesh.optimum-processor") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("automesh-optimum-processor-deterministic-output"), &testAutoMeshOptimumProcessorDeterministicOutput);
        return true;
    }
    if (id.view() == "automesh.alpha-provider" ||
        id.view() == "automesh.non-part-targets") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("automesh-alpha-provider-non-part-cached-input"), &testAutoMeshAlphaProviderAndNonPartCachedInput);
        return true;
    }
    if (id.view() == "automesh.schema-values" ||
        id.view() == "automesh.processor-common") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("automesh-schema-values-presets-active"), &testAutoMeshSchemaValuesPresetsAndActiveProcessor);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("automesh-batch-config-undo-redo"), &testAutoMeshBatchConfigUndoRedo);
        return true;
    }
    if (id.view() == "part.texture" ||
        id.view() == "inspectors.part") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("part-inspector-properties-undo-redo"), &testPartInspectorPropertiesUndoRedo);
        return true;
    }
    if (id.view() == "part.composite-inspector-mesh-roundtrip") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("part-composite-inspector-mesh-roundtrip"), &testPartCompositeInspectorMeshRoundTrip);
        return true;
    }
    if (id.view() == "part.texture-reload") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("part-texture-reload-fixture"), &testPartTextureReloadFixture);
        return true;
    }
    if (id.view() == "render.texture-lifecycle") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("part-texture-reload-fixture"), &testPartTextureReloadFixture);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("render-atlas-packer-texture-slots"), &testRenderAtlasPackerTextureSlots);
        return true;
    }
    if (id.view() == "part.clipping-mask") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("part-clipping-mask-properties-undo-redo"), &testPartClippingMaskPropertiesUndoRedo);
        return true;
    }
    if (id.view() == "part.mask-add-remove") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mask-source-add-undo-redo"), &testMaskSourceAddUndoRedo);
        return true;
    }
    if (id.view() == "part.mask-reorder") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mask-source-reorder-undo-redo"), &testMaskSourceReorderUndoRedo);
        return true;
    }
    if (id.view() == "depthbone.sources") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-actions-undo-redo"), &testDepthBoneActionsUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-source-commands-undo-redo"), &testDepthBoneSourceCommandsUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-source-settings-action-merge"), &testDepthBoneSourceSettingsActionMerge);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-source-refresh-ignores-physics"), &testDepthBoneSourceRefreshIgnoresPhysicsParameter);
        return true;
    }
    if (id.view() == "depthbone.fit-z") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthrigroot-fit-z-to-depth"), &testDepthRigRootFitZToDepth);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-planar-rest-axis-ignores-child-z"), &testDepthBonePlanarRestAxisIgnoresChildZ);
        return true;
    }
    if (id.view() == "inspectors.depth-bone") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-inspector-commands-undo-redo"), &testDepthBoneInspectorCommandsUndoRedo);
        return true;
    }
    if (id.view() == "inspectors.mesh-deformers") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mesh-deformer-inspector-commands-undo-redo"), &testMeshDeformerInspectorCommandsUndoRedo);
        return true;
    }
    if (id.view() == "inspectors.composite-node-part-mesh") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("inspector-composite-node-part-mesh"), &testInspectorCompositeNodePartMeshWorkflow);
        return true;
    }
    if (id.view() == "depthbone.binding-create") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-source-commands-undo-redo"), &testDepthBoneSourceCommandsUndoRedo);
        return true;
    }
    if (id.view() == "depthbone.template-bones") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-standard-skeleton-template"), &testDepthBoneStandardSkeletonTemplate);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-creation-notifies-resource-views"), &testDepthBoneCreationNotifiesResourceViews);
        return true;
    }
    if (id.view() == "depthbone.template-parameters" ||
        id.view() == "parameter.template-depth-bone") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-standard-parameter-template"), &testDepthBoneStandardParameterTemplate);
        return true;
    }
    if (id.view() == "depthbone.root-node" ||
        id.view() == "depthbone.bone-node" ||
        id.view() == "depthbone.serialization") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-serialization-roundtrip"), &testDepthBoneSerializationRoundTrip);
        return true;
    }
    if (id.view() == "depthbone.influence-rule") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-influence-rule-command-undo-redo"), &testDepthBoneInfluenceRuleCommandUndoRedo);
        return true;
    }
    if (id.view() == "depthbone.preview-commands") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-preview-apply-commands"), &testDepthBonePreviewApplyCommands);
        return true;
    }
    if (id.view() == "depthbone.gpu-packet") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-gpu-offset-packet-construction"), &testDepthBoneGpuOffsetPacketConstruction);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-source-rotation-deform"), &testDepthBoneSourceRotationDeform);
        return true;
    }
    if (id.view() == "depthbone.gpu-all-keypoints") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-gpu-all-keypoints-dispatch"), &testDepthBoneGpuAllKeypointsDispatch);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-frame-failure-recovery"), &testDepthBoneFrameFailureRecovery);
        return true;
    }
    if (id.view() == "depthbone.overlay-selection") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-overlay-selection-geometry"), &testDepthBoneOverlaySelectionGeometry);
        return true;
    }
    if (id.view() == "depthbone.update-visualization") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-update-visualization-state"), &testDepthBoneUpdateVisualizationState);
        return true;
    }
    if (id.view() == "async.derived-update") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("async-derived-update-registry"), &testAsyncDerivedUpdateRegistry);
        return true;
    }
    if (id.view() == "depthbone.skinning") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-skinning-lock-to-root-terminal"), &testDepthBoneSkinningLockToRootTerminal);
        return true;
    }
    if (id.view() == "depthbone.cleanup") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-delete-cleanup-undo-redo"), &testDepthBoneDeleteCleanupUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-conversion-cleanup-undo-redo"), &testDepthBoneConversionCleanupUndoRedo);
        return true;
    }
    if (id.view() == "part.mask-mode") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mask-source-mode-undo-redo"), &testMaskSourceModeUndoRedo);
        return true;
    }
    if (id.view() == "part.welding") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("coincident-welding-samples"), &testCoincidentWeldingVertexSamples);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("welding-undo-redo"), &testWeldingUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("prepared-welding-mesh"), &testPreparedWeldingMesh);
        return true;
    }
    if (id.view() == "part.welding-runtime") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("welding-runtime-deformation"), &testWeldingRuntimeDeformation);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("welding-index-recovery-and-detachment"), &nijigenerate_tests::welding::ngTestWelding);
        return true;
    }
    if (id.view() == "parameter.lifecycle" ||
        id.view() == "parameter.groups") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-lifecycle-undo-redo"), &testParameterLifecycleUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-command-lifecycle-undo-redo"), &testParameterCommandLifecycleUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-command-rename-group-undo-redo"), &testParameterCommandRenameAndGroupUndoRedo);
        return true;
    }
    if (id.view() == "parameter.create-presets") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-create-presets"), &testParameterCreatePresets);
        return true;
    }
    if (id.view() == "parameter.split-window") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-split-binding-migration-undo-redo"), &testParameterSplitBindingMigrationUndoRedo);
        return true;
    }
    if (id.view() == "parameter.copy-paste" ||
        id.view() == "parameter.link") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-copy-paste-duplicate-link"), &testParameterCopyPasteDuplicateAndLinkCommands);
        return true;
    }
    if (id.view() == "parameter.arm-select") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-arm-select-and-keypoint-commands"), &testParameterArmSelectAndKeypointCommands);
        return true;
    }
    if (id.view() == "parameter.starting-keyframe") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-starting-keyframe-command-undo-redo"), &testParameterStartingKeyframeCommandUndoRedo);
        return true;
    }
    if (id.view() == "parameter.keyframe-basic") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-keyframe-command-undo-redo"), &testParameterKeyframeCommandUndoRedo);
        return true;
    }
    if (id.view() == "parameter.keyframe-mirror-fill") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-keyframe-mirror-fill-commands"), &testParameterKeyframeMirrorFillCommands);
        return true;
    }
    if (id.view() == "parameter.keyframe-copy-paste") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-binding-clipboard-cleanup-commands"), &testParameterBindingClipboardAndCleanupCommands);
        return true;
    }
    if (id.view() == "parameter.keyframe-2d") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-2d-animation-keyframe-group-undo-redo"), &testParameter2DAnimationKeyframeGroupUndoRedo);
        return true;
    }
    if (id.view() == "parameter.binding-interp") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-binding-interpolation"), &testParameterBindingInterpolation);
        return true;
    }
    if (id.view() == "parameter.binding-trs") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-trs-binding-model-command-undo-redo"), &testParameterTRSBindingModelCommandUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-rotation-x-binding-part-undo-redo"), &testParameterRotationXBindingPartUndoRedo);
        return true;
    }
    if (id.view() == "parameter.binding-deform") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-deform-binding-model-command-undo-redo"), &testParameterDeformBindingModelCommandUndoRedo);
        return true;
    }
    if (id.view() == "parameter.binding-model") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-trs-binding-model-command-undo-redo"), &testParameterTRSBindingModelCommandUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-rotation-x-binding-part-undo-redo"), &testParameterRotationXBindingPartUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-deform-binding-model-command-undo-redo"), &testParameterDeformBindingModelCommandUndoRedo);
        return true;
    }
    if (id.view() == "parameter.mesh-binding-composite") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-mesh-binding-composite-workflow"), &testParameterMeshBindingCompositeWorkflow);
        return true;
    }
    if (id.view() == "parameter.composite-lifecycle-binding") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-composite-lifecycle-binding"), &testParameterCompositeLifecycleBindingWorkflow);
        return true;
    }
    if (id.view() == "parameter.axes-props") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-axes-props-command-undo-redo"), &testParameterAxesPropsCommandUndoRedo);
        return true;
    }
    if (id.view() == "parameter.binding-cleanup") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-binding-cleanup-on-delete-undo-redo"), &testParameterBindingCleanupOnDeleteUndoRedo);
        return true;
    }
    if (id.view() == "animation.lifecycle" ||
        id.view() == "animation.properties") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("animation-lifecycle-undo-redo"), &testAnimationLifecycleUndoRedo);
        return true;
    }
    if (id.view() == "animation.track-binding-cleanup") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("animation-track-binding-cleanup"), &testAnimationTrackBindingCleanup);
        return true;
    }
    if (id.view() == "animation.composite-keyframe-history") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("animation-composite-keyframe-history"), &testAnimationCompositeKeyframeHistory);
        return true;
    }
    if (id.view() == "animation.keyframes") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("animation-keyframes-undo-redo"), &testAnimationKeyframesUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("parameter-2d-animation-keyframe-group-undo-redo"), &testParameter2DAnimationKeyframeGroupUndoRedo);
        return true;
    }
    if (id.view() == "node.rename-undo-merge") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("node-name-undo-merge"), &testNodeNameUndoMergesPerNode);
        return true;
    }
    if (id.view() == "simplephysics.parameter") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("simplephysics-parameter-undo-redo"), &testSimplePhysicsParameterUndoRedo);
        return true;
    }
    if (id.view() == "simplephysics.settings" ||
        id.view() == "simplephysics.mapping" ||
        id.view() == "inspectors.simplephysics") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("simplephysics-settings-undo-redo"), &testSimplePhysicsSettingsUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("simplephysics-parameter-undo-redo"), &testSimplePhysicsParameterUndoRedo);
        return true;
    }
    if (id.view() == "simplephysics.serialization") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("simplephysics-serialization-roundtrip"), &testSimplePhysicsSerializationRoundTrip);
        return true;
    }
    if (id.view() == "simplephysics.composite-roundtrip") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("simplephysics-composite-roundtrip"), &testSimplePhysicsCompositeRoundTripWorkflow);
        return true;
    }
    if (id.view() == "undo.grouped-actions") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("action-group-undo-redo"), &testActionGroupUndoRedo);
        return true;
    }
    if (id.view() == "undo.async-grouped-actions") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("async-action-group-undo-redo"), &testAsyncActionGroupUndoRedo);
        return true;
    }
    if (id.view() == "undo.action-merge") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("action-merge-semantics"), &testActionMergeSemantics);
        return true;
    }
    if (id.view() == "viewport.action-history" ||
        id.view() == "panels.action-history") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("action-history-index-modified-state"), &testActionHistoryIndexAndModifiedState);
        return true;
    }
    if (id.view() == "viewport.autorig-presentation") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("autorig-presentation-commands"), &testAutoRigPresentationCommands);
        return true;
    }
    if (id.view() == "viewport.settings-command-composite") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("viewport-settings-command-composite"), &testViewportSettingsCommandCompositeWorkflow);
        return true;
    }
    if (id.view() == "viewport.flip-pairs") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("viewport-flip-pair-commands"), &testViewportFlipPairCommands);
        return true;
    }
    if (id.view() == "viewport.palette-command-list") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("viewport-palette-command-discovery"), &testViewportPaletteCommandDiscovery);
        return true;
    }
    if (id.view() == "depth.persistence") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthmapped-node-serialization-roundtrip"), &testDepthMappedNodeSerializationRoundTrip);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depth-operation-helper-contracts"), &testDepthOperationHelperContracts);
        return true;
    }
    if (id.view() == "depth.exdepthmapped") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthmapped-node-serialization-roundtrip"), &testDepthMappedNodeSerializationRoundTrip);
        return true;
    }
    if (id.view() == "depth.sign-colors") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depth-sign-color-contracts"), &testDepthSignColorContracts);
        return true;
    }
    if (id.view() == "depth.camera") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depth-camera-projection-contracts"), &testDepthCameraProjectionContracts);
        return true;
    }
    if (id.view() == "render.camera") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("render-camera-export-commands"), &testRenderCameraExportCommands);
        return true;
    }
    if (id.view() == "render.composite-camera-texture-export") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("render-camera-texture-composite"), &testRenderCameraTextureCompositeWorkflow);
        return true;
    }
    if (id.view() == "depth.operation-helpers") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depth-operation-helper-contracts"), &testDepthOperationHelperContracts);
        return true;
    }
    if (id.view() == "depth.commands") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depth-map-commands-undo-redo"), &testDepthMapCommandsUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("psd-depth-dialog-commands-undo-redo"), &testPsdDepthDialogCommandsUndoRedo);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("psd-depth-dialog-all-commands"), &testAllPsdDepthDialogCommands);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("psd-depth-import-refreshes-depthbone-bindings"), &testPsdDepthImportRefreshesDepthBoneBindings);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("psd-depth-map-import-helpers"), &testPsdDepthMapImportHelpers);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("psd-depth-adjusted-values-are-not-renormalized"), &testPsdDepthAdjustedValuesAreNotRenormalized);
        return true;
    }
    if (id.view() == "depth.psd-map-import") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("psd-depth-import-refreshes-depthbone-bindings"), &testPsdDepthImportRefreshesDepthBoneBindings);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("psd-depth-map-import-helpers"), &testPsdDepthMapImportHelpers);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("psd-depth-adjusted-values-are-not-renormalized"), &testPsdDepthAdjustedValuesAreNotRenormalized);
        return true;
    }
    if (id.view() == "depth.composite-map-ops") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depth-composite-map-ops"), &testDepthCompositeMapOpsWorkflow);
        return true;
    }
    if (id.view() == "depthbone.composite-source-preview") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-composite-source-preview"), &testDepthBoneCompositeSourcePreviewWorkflow);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("depthbone-ancestor-propagation-gates"), &testDepthBoneAncestorPropagationGates);
        return true;
    }
    if (id.view() == "mesh.vertex-scope" ||
        id.view() == "depth.edit-scope" ||
        id.view() == "deform.onetime-scope" ||
        id.view() == "deform.undo-redo-group" ||
        id.view() == "undo.scope-guards") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("action-stack-scope-guard"), &testActionStackScopeGuard);
        return true;
    }
    if (id.view() == "undo.direct-mutation-audit") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("undo-direct-mutation-audit"), &testUndoDirectMutationAudit);
        return true;
    }
    if (id.view() == "undo.command-actions") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("undo-command-actions-audit"), &testUndoCommandActionsAudit);
        return true;
    }
    if (id.view() == "undo.composite-cross-family-stack") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("undo-composite-cross-family-stack"), &testUndoCompositeCrossFamilyStackWorkflow);
        return true;
    }
    if (id.view() == "settings.shortcuts") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("shortcut-settings-conflict-reload"), &testShortcutSettingsConflictAndReload);
        return true;
    }
    if (id.view() == "settings.composite-shortcuts-recent") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("settings-composite-shortcuts-recent"), &testSettingsCompositeShortcutsRecentWorkflow);
        return true;
    }
    if (id.view() == "settings.default-shortcuts") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("default-shortcut-registration"), &testDefaultShortcutRegistration);
        return true;
    }
    if (id.view() == "settings.ui") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("typed-settings-store"), &testTypedSettingsStore);
        return true;
    }
    if (id.view() == "settings.ai-mcp") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("ai-mcp-settings-opt-in-persistence"), &testAiMcpSettingsOptInPersistence);
        return true;
    }
    if (id.view() == "api.acp-protocol") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("acp-protocol-types-error-json"), &testAcpProtocolTypesAndErrorJson);
        return true;
    }
    if (id.view() == "api.acp-client") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("acp-client-source-contract"), &testAcpClientSourceContract);
        return true;
    }
    if (id.view() == "api.external-control" ||
        id.view() == "api.mcp-resources") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mcp-resource-listing-context-helpers"), &testMcpResourceListingAndContextHelpers);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mcp-task-queue-main-thread-dispatch"), &testMcpTaskQueueMainThreadDispatch);
        return true;
    }
    if (id.view() == "api.mcp-task-queue") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mcp-task-queue-main-thread-dispatch"), &testMcpTaskQueueMainThreadDispatch);
        return true;
    }
    if (id.view() == "api.mcp-server") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mcp-failed-startup-retry"), &testMcpFailedStartupRetry);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("api-transport-server-contracts"), &testApiTransportAndServerContracts);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mcp-resource-listing-context-helpers"), &testMcpResourceListingAndContextHelpers);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("mcp-task-queue-main-thread-dispatch"), &testMcpTaskQueueMainThreadDispatch);
        return true;
    }
    if (id.view() == "api.mcp-auth" ||
        id.view() == "api.mcp-http-transport" ||
        id.view() == "api.acp-stdio" ||
        id.view() == "api.acp-echo-agent") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("api-transport-server-contracts"), &testApiTransportAndServerContracts);
        return true;
    }
    if (id.view() == "settings.paths" ||
        id.view() == "platform.paths-dpi-fonts") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("settings-path-resolution"), &testSettingsPathResolution);
        return true;
    }
    if (id.view() == "platform.version") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("platform-version-metadata"), &testPlatformVersionMetadata);
        return true;
    }
    if (id.view() == "platform.tasks") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("platform-task-queue"), &testPlatformTaskQueue);
        return true;
    }
    if (id.view() == "platform.crashdump") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("platform-crashdump-generation"), &testPlatformCrashDumpGeneration);
        return true;
    }
    if (id.view() == "platform.windows-write" ||
        id.view() == "platform.debug-logging") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("platform-windows-console-writes-guarded"), &testPlatformWindowsConsoleWritesAreGuarded);
        return true;
    }
    if (id.view() == "platform.startup-shutdown") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("platform-startup-shutdown-module-constructors"), &testPlatformStartupShutdownModuleConstructors);
        return true;
    }
    if (id.view() == "i18n.wrapped-strings") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("i18n-visible-ui-strings-wrapped"), &testI18nVisibleUiStringsAreWrapped);
        return true;
    }
    if (id.view() == "i18n.pot") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("i18n-pot-template-covers-simple-source-literals"), &testI18nPotTemplateCoversSimpleSourceLiterals);
        return true;
    }
    if (id.view() == "inspectors.format-strings") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("inspector-format-strings"), &testInspectorFormatStringsDoNotEscapeNumericFormats);
        return true;
    }
    if (id.view() == "tools.command-browser-resource-pickers") {
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("command-browser-resource-picker-initialization"),
            &testCommandBrowserResourcePickerSelectionsAreInitialized);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("command-browser-resource-argument-semantics"),
            &testCommandBrowserResourceArgumentParsingResolvesLiveResources);
        runCase(nijigenerate::compat::RetainedString::fromStaticLiteral("command-browser-differential-baseline"),
            &testCommandBrowserDifferentialReportMatchesBaseline);
        return true;
    }
    return false;
}

std::int32_t runOnly(nijigenerate::compat::RetainedString id) {
    const auto scenarios = allScenarios();
    for (const auto scenario : scenarios) {
        if (scenario.id != id) continue;
        if (scenario.status.view() != automated) {
            detail::writeLine(detail::standardError(), {"scenario is not automated: ", id.view(), " (", scenario.status.view(), ")"});
            printScenario(scenario);
            return 2;
        }
        if (!runAutomatedScenario(id)) {
            detail::writeLine(detail::standardError(), {"automated scenario has no runner: ", id.view()});
            return 1;
        }
        detail::writeLine(detail::standardOutput(), {"regression-tests: OK"});
        return 0;
    }
    detail::writeLine(detail::standardError(), {"unknown scenario: ", id.view()});
    return 2;
}

std::int32_t runAutomatedScenarios() {
    using nijigenerate::compat::RetainedString;
    const auto scenarios = allScenarios();
    for (const auto scenario : scenarios) {
        if (scenario.status.view() == automated) {
            const bool found = runAutomatedScenario(scenario.id);
            const auto message = concatenateSourceStrings({
                RetainedString::fromStaticLiteral("automated scenario has no runner: "), scenario.id});
            require(found, message);
        }
    }
    detail::writeLine(detail::standardOutput(), {"regression-tests: OK"});
    printCoverage(false);
    return 0;
}

// Original regression.d22837-22895. These definitions do not run a driver.
std::int32_t runComputerUseScenarios(nijigenerate::compat::RetainedString driverCommand) {
    using nijigenerate::compat::RetainedString;
    if (driverCommand.empty()) {
        detail::writeLine(detail::standardError(), {"computer-use driver command is empty"});
        return 2;
    }
    std::size_t executed = 0;
    const auto scenarios = allScenarios();
    for (const auto scenario : scenarios) {
        if (scenario.status.view() != computerUse) continue;
        ++executed;
        detail::writeLine(detail::standardOutput(), {"running computer-use\t", scenario.id.view()});
        const auto count = driverCommand.size() + 1 + scenario.id.size();
        nicxlive::core::common::SharedArray<char> bytes(count, std::bit_cast<char>(std::uint8_t{0xff}));
        std::size_t position = 0;
        for (const auto byte : driverCommand.view()) bytes[position++] = byte;
        bytes[position++] = ' ';
        for (const auto byte : scenario.id.view()) bytes[position++] = byte;
        const auto command = RetainedString::retainedView(bytes.retentionOwner(), bytes.data(), count);
        const auto result = nijigenerate::compat::phobos::ngExecuteShell(command);
        if (result.status != 0) {
            detail::writeLine(detail::standardError(), {"computer-use scenario failed: ", scenario.id.view()});
            detail::writeLine(detail::standardError(), {result.output.view()});
            return result.status ? result.status : 1;
        }
    }
    detail::writeLine(detail::standardOutput(), {"computer-use-regression-tests: OK"});
    detail::writeUnsignedLine(detail::standardOutput(), "computer-use executed: ", executed);
    return 0;
}

std::int32_t runComputerUseScenario(nijigenerate::compat::RetainedString id,
    nijigenerate::compat::RetainedString driverCommand) {
    using nijigenerate::compat::RetainedString;
    if (driverCommand.empty()) {
        detail::writeLine(detail::standardError(), {"computer-use driver command is empty"});
        return 2;
    }
    const auto scenarios = allScenarios();
    for (const auto scenario : scenarios) {
        if (scenario.id.view() != id.view()) continue;
        if (scenario.status.view() != computerUse) {
            detail::writeLine(detail::standardError(), {"scenario is not computer-use: ", id.view(), " (", scenario.status.view(), ")"});
            printScenario(scenario);
            return 2;
        }
        detail::writeLine(detail::standardOutput(), {"running computer-use\t", scenario.id.view()});
        const auto count = driverCommand.size() + 1 + scenario.id.size();
        nicxlive::core::common::SharedArray<char> bytes(count, std::bit_cast<char>(std::uint8_t{0xff}));
        std::size_t position = 0;
        for (const auto byte : driverCommand.view()) bytes[position++] = byte;
        bytes[position++] = ' ';
        for (const auto byte : scenario.id.view()) bytes[position++] = byte;
        const auto command = RetainedString::retainedView(bytes.retentionOwner(), bytes.data(), count);
        const auto result = nijigenerate::compat::phobos::ngExecuteShell(command);
        if (result.status != 0) {
            detail::writeLine(detail::standardError(), {"computer-use scenario failed: ", scenario.id.view()});
            detail::writeLine(detail::standardError(), {result.output.view()});
            return result.status ? result.status : 1;
        }
        detail::writeLine(detail::standardOutput(), {"computer-use-regression-tests: OK"});
        detail::writeLine(detail::standardOutput(), {"computer-use executed: 1"});
        return 0;
    }
    detail::writeLine(detail::standardError(), {"unknown scenario: ", id.view()});
    return 2;
}

void testAcpClientSourceContract() {
    using nijigenerate::compat::RetainedString;
    const auto root = regressionSourceRoot(RetainedString::fromStaticLiteral("api"));
    const std::array segments{root, RetainedString::fromStaticLiteral("acp"), RetainedString::fromStaticLiteral("client.d")};
    const auto clientSource = nijigenerate::compat::phobos::ngReadUtf8Text(
        nijigenerate::compat::phobos::ngBuildPath(segments));
    const auto has = [&](std::string_view text) { return clientSource.view().find(text) != std::string_view::npos; };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    check(has("class ACPClient"), "ACP client should expose ACPClient");
    check(has("pipeProcess") && has("Redirect.all") && has("pipes.stdout") && has("pipes.stdin") && has("pipes.stderr"),
        "ACP client should launch child process with stdin/stdout/stderr pipes");
    check(has("startReader()") && has("startStderrReader()"), "ACP client should start stdout and stderr reader threads");
    check(has("setCancelCheck") && has("cancelCheck()"),
        "ACP client should support external cancellation checks while waiting");
    check(has("session/cancel") && has("cancelPrompt"), "ACP client should send session/cancel notifications");
    check(has("sendPermissionResponse") && has("\"outcome\":\"cancelled\"") && has("\"optionId\":\""),
        "ACP client should encode permission grant and cancellation responses");
    check(has("initializeAsync") && has("pollInitialize") && has("PollResult"),
        "ACP client should expose non-blocking initialize polling");
    check(has("void close()") && has("stderrThread.join") && has("reader.join"),
        "ACP client shutdown should stop and join reader threads");
    check(has("drainStderr") && has(" | stderr: "), "ACP client failures should include captured stderr context");
}

void testPlatformVersionMetadata() {
    using nijigenerate::ver::INC_VERSION;
    using nijigenerate::compat::RetainedString;
    require(INC_VERSION.size() > 0, RetainedString::fromStaticLiteral("version string should not be empty"));
    require(INC_VERSION.starts_with("v"), RetainedString::fromStaticLiteral("version string should be release-tag shaped"));
    require(INC_VERSION.find("UNKNOWN") == std::string_view::npos,
        RetainedString::fromStaticLiteral("version string should not be an unresolved placeholder"));
}

#include "detail/actor14_action_history.inc"
#include "detail/actor15_psd_import_helpers.inc"
#include "detail/actor14_psd_import_contracts.inc"
#include "detail/actor15_psd_depth_refresh.inc"
#include "detail/actor15_depth_map_history.inc"
#include "detail/actor15_depth_root_fit.inc"
#include "detail/actor15_depth_gpu_packet.inc"
#include "detail/actor15_depth_gpu_dispatch.inc"
#include "detail/actor15_settings_paths.inc"
#include "detail/actor15_recent_projects.inc"
#include "detail/actor13_depthdraw_dispatch.inc"
#include "detail/actor13_depth_calculation_gate.inc"
#include "detail/actor15_depth_local_merge.inc"
#include "detail/actor14_async_registry.inc"
#include "detail/actor14_selector_tree.inc"
#include "detail/actor14_viewport_flip.inc"
#include "detail/actor14_node_move.inc"
#include "detail/actor14_puppet_restore.inc"
#include "detail/actor14_animation_keyframes.inc"
#include "detail/actor14_deform_binding.inc"
#include "detail/actor14_camera_roundtrip.inc"
#include "detail/actor14_part_clipping.inc"
#include "detail/actor14_parameter_groups.inc"
#include "detail/actor15_standard_skeleton.inc"
#include "detail/actor14_depth_notifications.inc"
#include "detail/actor14_grid_tool.inc"
#include "detail/actor14_runtime_deformers.inc"
#include "detail/actor14_onetime_registration.inc"
#include "detail/actor14_onetime_snapshot.inc"
#include "detail/actor14_onetime_phase_comparison.inc"
#include "detail/actor14_onetime_selected.inc"
#include "detail/actor14_rig_clipping.inc"
#include "detail/actor14_depth_frame_failure.inc"
#include "detail/actor14_depth_skinning.inc"
#include "detail/actor14_depth_child_z.inc"
#include "detail/actor14_depthmapped_roundtrip.inc"
#include "detail/actor14_inp_depth_pruning.inc"
#include "detail/actor14_async_history.inc"
#include "detail/actor14_depth_status.inc"
#include "detail/actor14_depth_rest_constraints.inc"
#include "detail/actor14_autorig_presentation.inc"
#include "detail/actor15_node_type_inspector.inc"
#include "detail/actor14_depth_preview_apply.inc"
#include "detail/actor14_depth_source_commands.inc"
#include "detail/actor14_mesh_inspector.inc"
#include "detail/actor14_automesh_cancellation.inc"
#include "detail/actor15_simplephysics_history.inc"
#include "detail/actor13_automesh_outputs.inc"
#include "detail/actor15_automesh_config.inc"
#include "detail/actor14_automesh_composite.inc"
#include "detail/actor13_automesh_schema.inc"
#include "detail/actor13_psd_dialog_all.inc"
#include "detail/actor15_psd_dialog_history.inc"
#include "detail/actor13_inventory_cases.inc"
#include "detail/actor13_console_original.inc"
#include "detail/actor13_pot_original.inc"
#include "detail/actor13_node_conversion.inc"
#include "detail/actor13_animation_lifecycle.inc"
#include "detail/actor13_parameter_matrix.inc"
#include "detail/actor13_browser_baseline.inc"
#include "detail/actor13_mesh_cleanup.inc"
#include "detail/actor13_depth_target_view.inc"
#include "detail/actor13_math_cv.inc"
#include "detail/actor15_mirror_fill.inc"
#include "detail/actor15_depth_settings_merge.inc"
#include "detail/actor15_depth_roundtrip.inc"
#include "detail/actor15_depth_fit_z.inc"
#include "detail/actor15_existing_triangle.inc"
#include "detail/actor15_depth_composite.inc"
#include "detail/actor15_existing_welding.inc"
#include "detail/actor15_api_transport_contracts.inc"
#include "detail/actor15_default_shortcuts.inc"
#include "detail/actor15_browser_source_contracts.inc"
#include "detail/actor15_mcp_resources.inc"
#include "detail/actor15_existing_text_audits.inc"
#include "detail/actor15_autosave_recovery.inc"
#include "detail/actor15_project_repair.inc"
#include "detail/actor15_onion_geometry.inc"
#include "detail/actor15_existing_undo_audits.inc"

void printFullFeatureInventory() {
    using nijigenerate::compat::RetainedString;
    using Strings = nijigenerate::compat::Slice<RetainedString>;
    Strings failures;
    auto inventory = collectFullFeatureScenarioInventory(failures);
    Strings kinds;
    inventory.forEachRetained([&](RetainedString kind, Strings) { kinds.append(kind); });
    sortRegressionStrings(kinds);
    std::size_t total = 0;
    for (const auto kind : kinds) {
        auto items = inventory.get(kind.view()).duplicate();
        sortRegressionStrings(items);
        standardOutput().withTextWriter([&](FileTextWriter& writer) {
            writer.put("feature-kind\t");
            writer.put(kind.view());
            writer.put("\t");
            putUnsigned(writer, items.size());
            writer.put(static_cast<unsigned char>('\n'));
        });
        total += items.size();
        for (const auto item : items) {
            const auto separator = countUntilSourceString(item, RetainedString::fromStaticLiteral(":"));
            const auto relative = separator >= 0 ? item.subslice(0, static_cast<std::size_t>(separator)) : item;
            const auto owner = featureScenarioOwner(kind.view(), relative.view());
            writeLine(standardOutput(), {"feature-scenario\t", kind.view(), "\t", owner.view(), "\t", item.view()});
        }
    }
    writeUnsignedLine(standardOutput(), "feature-total\t", total);
    if (!failures.empty()) {
        writeLine(standardError(), {"feature inventory failures:"});
        for (const auto failure : failures) writeLine(standardError(), {failure.view()});
    }
}

void printSourceModuleScenarioInventory() {
    using nijigenerate::compat::RetainedString;
    using Strings = nijigenerate::compat::Slice<RetainedString>;
    Strings failures;
    auto inventory = collectSourceModuleScenarioInventory(failures);
    Strings prefixes;
    inventory.forEachRetained([&](RetainedString prefix, Strings) { prefixes.append(prefix); });
    sortRegressionStrings(prefixes);
    std::size_t total = 0;
    for (const auto prefix : prefixes) {
        auto modules = inventory.get(prefix.view()).duplicate();
        sortRegressionStrings(modules);
        standardOutput().withTextWriter([&](FileTextWriter& writer) {
            writer.put("module-scenario-kind\t");
            writer.put(prefix.view());
            writer.put("\t");
            putUnsigned(writer, modules.size());
            writer.put(static_cast<unsigned char>('\n'));
        });
        total += modules.size();
        for (const auto module : modules)
            writeLine(standardOutput(), {"module-scenario\t", prefix.view(), "\t", module.view()});
    }
    writeUnsignedLine(standardOutput(), "module-total\t", total);
    if (!failures.empty()) {
        writeLine(standardError(), {"source module scenario inventory failures:"});
        for (const auto failure : failures) writeLine(standardError(), {failure.view()});
    }
}

std::int32_t sourceMain(nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> args) {
    using namespace nicxlive::core::nodes;
    using nijigenerate::compat::RetainedString;
    configureRegressionConfigDir();
    nicxlive::core::ngSetTimingFuncSource(&regressionNow);
    nijigenerate::core::settings::incSettingsLoad();
    nijigenerate::core::actionstack::incActionInit();
    nijigenerate::ext::incInitExt();
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("Node"),
        [](const std::shared_ptr<Node>& parent) { return Node::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("Part"),
        [](const std::shared_ptr<Node>& parent) { return Part::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("Composite"),
        [](const std::shared_ptr<Node>& parent) { return Composite::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("DynamicComposite"),
        [](const std::shared_ptr<Node>& parent) { return DynamicComposite::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("MeshGroup"),
        [](const std::shared_ptr<Node>& parent) { return MeshGroup::create(parent); });
    Node::inRegisterNodeTypeRetained(RetainedString::fromStaticLiteral("GridDeformer"),
        [](const std::shared_ptr<Node>& parent) { return GridDeformer::create(parent); });
    nijigenerate::commands::ngInitAllCommands();
    if (args.size() == 1) return runAutomatedScenarios();
    if (args.size() == 2 && args[1].view() == "--list") { printList(); return 0; }
    if (args.size() == 2 && args[1].view() == "--feature-inventory") { printFullFeatureInventory(); return 0; }
    if (args.size() == 2 && args[1].view() == "--module-inventory") { printSourceModuleScenarioInventory(); return 0; }
    if (args.size() == 2 && args[1].view() == "--computer-use-manifest") { printComputerUseManifest(); return 0; }
    if (args.size() == 2 && args[1].view() == "--coverage") return printCoverage(false);
    if (args.size() == 2 && args[1].view() == "--require-all") return printCoverage(true);
    if (args.size() == 3 && args[1].view() == "--only") return runOnly(args[2]);
    if (args.size() >= 4 && args[1].view() == "--computer-use-only") {
        return runComputerUseScenario(args[2], joinSourceStrings(
            std::span<const RetainedString>(args.data() + 3, args.size() - 3), RetainedString::fromStaticLiteral(" ")));
    }
    if (args.size() >= 3 && args[1].view() == "--computer-use-driver") {
        return runComputerUseScenarios(joinSourceStrings(
            std::span<const RetainedString>(args.data() + 2, args.size() - 2), RetainedString::fromStaticLiteral(" ")));
    }
    return usage();
}

} // namespace detail

std::int32_t ngRegressionMain(nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> args) {
    // The existing native main-thread identity precedes Source module initialization.
    nijigenerate::core::thread_context::ngRegisterMainThread();
    // The original regression profile selects QueueBackend; instantiate it only on first use.
    nicxlive::core::ngSetRenderBackendFactory(+[]() -> std::shared_ptr<nicxlive::core::RenderBackend> {
        using Backend = nicxlive::core::render::QueueRenderBackend;
        return std::allocate_shared<Backend>(nicxlive::core::d_runtime::SourceAllocator<Backend>{});
    });
    nijigenerate::compat::phobos::socket::ngInitializeSourceSocketModule();
    std::int32_t result = 0;
    nijigenerate::compat::ngRunWithScopeExit([&] { result = detail::sourceMain(args); }, [] {
        nijigenerate::compat::phobos::socket::ngFinalizeSourceSocketModule();
    });
    return result;
}
} // namespace nijigenerate_tests::regression

#include "detail/root_simplephysics_composite.inc"
#include "detail/root_viewport_settings_composite.inc"
#include "detail/root_node_composite.inc"
#include "detail/root_parameter_composite.inc"
#include "detail/root_render_composite.inc"
#include "detail/root_deform_composite.inc"
#include "detail/root_image_merge_composite.inc"
#include "detail/root_part_composite.inc"
#include "detail/root_mesh_composite.inc"
#include "detail/root_undo_composite.inc"
#include "detail/root_atlas_texture_slots.inc"
#include "detail/root_texture_reload.inc"
#include "detail/root_prepared_welding.inc"
#include "detail/root_existing_depth_gpu.inc"
#include "detail/root_existing_depthdraw_gpu.inc"
#include "detail/root_meshgroup_compat.inc"
#include "detail/root_source_command_inventory.inc"
#include "detail/root_depthdraw_fixture.inc"
#include "detail/root_depthdraw_data_model.inc"
#include "detail/root_depthdraw_artifact.inc"
#include "detail/root_coincident_welding.inc"
#include "detail/root_depthbone_composite.inc"
#include "detail/root_depth_image_facade.inc"
#include "detail/root_node_move_world.inc"
