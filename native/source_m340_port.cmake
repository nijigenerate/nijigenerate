# Faithful original regression.d module; no generated cases or execution target.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_core
        AND TARGET nijigenerate_source_m341 AND TARGET nijigenerate_phobos_socket
        AND TARGET nijigenerate_phobos_stdio
        AND TARGET nijigenerate_acp_client AND TARGET nijigenerate_mcp_queue
        AND TARGET nijigenerate_depthbone_nodes AND TARGET nijigenerate_excamera
        AND TARGET nijigenerate_transformhandle AND TARGET nijigenerate_model_editor_completion
        AND TARGET nijigenerate_vertex_editor AND TARGET nijigenerate_vertex_deformable_editor
        AND TARGET nijigenerate_depth_draw_fitgap
        AND TARGET nijigenerate_node_editor AND TARGET nijigenerate_vertex_drawable_editor
        AND TARGET nijigenerate_expart AND TARGET nijigenerate_depth_mesh_editor
        AND TARGET nijigenerate_depth_renderer
        AND TARGET nijigenerate_depth_draw_viewport
        AND TARGET nijigenerate_depth_viewport
        AND TARGET nijigenerate_dpi
        AND TARGET nijigenerate_cv AND TARGET nijigenerate_skeleton_path
        AND TARGET nijigenerate_skeletonize AND TARGET nijigenerate_automesh_processor
        AND TARGET nijigenerate_window_base AND TARGET nijigenerate_gl_backend_source
        AND TARGET nijigenerate_depth_draw_gpu AND TARGET nijigenerate_dialogs
        AND TARGET nijigenerate_io AND TARGET nijigenerate_tooltip AND TARGET nijigenerate_texture_widget
        AND TARGET nijigenerate_autorig_evidence
        AND TARGET nijigenerate_inspector_meshgroup AND TARGET nijigenerate_inspector_grid
        AND TARGET nijigenerate_inspector_drawable AND TARGET nijigenerate_inspector_camera
        AND TARGET nijigenerate_autorig_native AND TARGET nijigenerate_autosave
        AND TARGET nijigenerate_mesh_editor_tools AND TARGET nijigenerate_crashdump
        AND TARGET nijigenerate_crashdump_common
        AND TARGET nijigenerate_tasks
        AND TARGET nicxlive::nicxlive AND NOT TARGET nijigenerate_source_m340)
    add_library(nijigenerate_source_m340 STATIC EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/regression.cpp")
    target_compile_features(nijigenerate_source_m340 PUBLIC cxx_std_20)
    # Both the actual browser provider and its original regression consumer share the D version.
    target_compile_definitions(nijigenerate_runtime_core PUBLIC NG_COMMAND_BROWSER_DIFFERENTIAL=1)
    target_compile_definitions(nijigenerate_source_m340 PUBLIC NG_COMMAND_BROWSER_DIFFERENTIAL=1)
    target_compile_definitions(nijigenerate_runtime_core PUBLIC NIJIGENERATE_HAVE_MCP=1)
    target_compile_definitions(nijigenerate_source_m340 PUBLIC NIJIGENERATE_HAVE_MCP=1)
    target_include_directories(nijigenerate_source_m340 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_source_m340 PUBLIC FILE_SET HEADERS TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate_tests/regression.hpp")
    target_sources(nijigenerate_source_m340 PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_existing_depth_gpu.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_existing_depthdraw_gpu.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_existing_depthdraw_gpu.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_meshgroup_compat.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_source_command_inventory.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_depthdraw_fixture.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_depthdraw_data_model.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_depthdraw_data_model.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_depthdraw_artifact.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_depthdraw_artifact.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_coincident_welding.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_coincident_welding.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_depthbone_composite.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_depthbone_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_depth_image_facade.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_depth_image_facade.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_node_move_world.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_node_move_world.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_depthdraw_fixture.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_source_command_inventory.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_meshgroup_compat.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_existing_depth_gpu.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/regression_internal.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/regression_commands.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/source_strings.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/fixture_base64.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/report_regex_unicode.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/stdio_text_writer.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/uint_associative_array.hpp")
    target_sources(nijigenerate_source_m340 PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_action_history.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_psd_import_helpers.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_psd_import_contracts.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_depthdraw_dispatch.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_depth_calculation_gate.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_depth_local_merge.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_async_registry.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_selector_tree.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_viewport_flip.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_node_move.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_puppet_restore.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_animation_keyframes.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_deform_binding.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_camera_roundtrip.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_part_clipping.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_parameter_groups.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_depth_notifications.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_simplephysics_history.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_automesh_config.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_automesh_outputs.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_automesh_schema.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_inventory_cases.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_console_original.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_pot_original.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_node_conversion.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_animation_lifecycle.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_parameter_matrix.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_browser_baseline.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_mesh_cleanup.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_depth_target_view.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_math_cv.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor13_psd_dialog_all.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_psd_dialog_history.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_grid_tool.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_runtime_deformers.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_onetime_registration.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_onetime_snapshot.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_onetime_phase_comparison.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_onetime_selected.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_rig_clipping.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_depth_frame_failure.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_depth_skinning.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_depth_child_z.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_depthmapped_roundtrip.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_inp_depth_pruning.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_async_history.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_depth_status.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_depth_rest_constraints.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_autorig_presentation.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_node_type_inspector.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_psd_depth_refresh.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_depth_map_history.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_depth_root_fit.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_depth_gpu_packet.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_depth_gpu_dispatch.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_settings_paths.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_recent_projects.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_depth_preview_apply.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_depth_source_commands.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_mesh_inspector.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_automesh_cancellation.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor14_automesh_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_mirror_fill.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_depth_settings_merge.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_depth_roundtrip.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_depth_fit_z.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_existing_triangle.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_depth_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_existing_welding.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_api_transport_contracts.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_default_shortcuts.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_browser_source_contracts.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_mcp_resources.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_existing_text_audits.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_autosave_recovery.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_project_repair.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_onion_geometry.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_standard_skeleton.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/actor15_existing_undo_audits.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_simplephysics_composite.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_simplephysics_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_viewport_settings_composite.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_viewport_settings_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_node_composite.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_node_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_parameter_composite.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_parameter_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_render_composite.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_render_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_deform_composite.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_deform_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_image_merge_composite.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_image_merge_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_part_composite.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_part_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_mesh_composite.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_mesh_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_undo_composite.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_undo_composite.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_atlas_texture_slots.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_atlas_texture_slots.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_texture_reload.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_texture_reload.inc"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_prepared_welding.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/source/nijigenerate_tests/detail/root_prepared_welding.inc")
    target_link_libraries(nijigenerate_source_m340 PUBLIC nijigenerate_runtime_core
        nijigenerate_core nicxlive::nicxlive nijigenerate_source_m341
        nijigenerate_phobos_socket nijigenerate_phobos_stdio nijigenerate_autorig_native nijigenerate_autosave
        nijigenerate_mesh_editor_tools nijigenerate_crashdump nijigenerate_crashdump_common nijigenerate_tasks
        nijigenerate_acp_client nijigenerate_mcp_queue nijigenerate_depthbone_nodes nijigenerate_excamera
        nijigenerate_transformhandle nijigenerate_model_editor_completion
        nijigenerate_vertex_editor nijigenerate_vertex_deformable_editor nijigenerate_depth_draw_fitgap
        nijigenerate_node_editor nijigenerate_vertex_drawable_editor
        nijigenerate_expart nijigenerate_depth_mesh_editor nijigenerate_depth_renderer
        nijigenerate_cv nijigenerate_skeleton_path nijigenerate_skeletonize nijigenerate_automesh_processor
        nijigenerate_window_base nijigenerate_autorig_evidence
        nijigenerate_inspector_meshgroup nijigenerate_inspector_grid
        nijigenerate_inspector_drawable nijigenerate_inspector_camera
        nijigenerate_depth_draw_viewport nijigenerate_depth_viewport nijigenerate_dpi)
    if(MSVC)
        target_compile_options(nijigenerate_source_m340 PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_source_m340 PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
