include("${CMAKE_CURRENT_SOURCE_DIR}/shortcut_platform.cmake")
nijigenerate_configure_shortcut_policy(nijigenerate_native_options INTERFACE)

# Shared Unicode/Throwable providers are ordinary SDK source dependencies even
# when the optional full renderer probe is disabled. No renderer setup is needed.
set(nijigenerate_common_sdk_root "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}")
if(NOT nijigenerate_common_sdk_root)
    set(nijigenerate_common_sdk_root "${PROJECT_SOURCE_DIR}/../nicxlive")
endif()
include("${nijigenerate_common_sdk_root}/cmake/NicxliveCommon.cmake")
target_link_libraries(nijigenerate_native_options INTERFACE nicxlive::common)
include("${CMAKE_CURRENT_SOURCE_DIR}/phobos_stdio.cmake")


set(nijilive_core_dir "${PROJECT_SOURCE_DIR}/../nijilive/source/nijilive/core")
if(NIJIGENERATE_PUPPET_SOURCE)
    get_filename_component(nijilive_core_dir "${NIJIGENERATE_PUPPET_SOURCE}" DIRECTORY)
endif()

add_library(nijigenerate_core STATIC
    src/nijigenerate/autorig/deterministic/contracts_file.cpp
    src/nijigenerate/core/translation.cpp
    src/nijigenerate/atlas/packer.cpp
    src/nijigenerate/atlas/packer.hpp
    src/nijigenerate/compat/native_arithmetic_fault.cpp
    src/nijigenerate/commands/base.cpp
    src/nijigenerate/commands/command.cpp
    src/nijigenerate/commands/node/dynamic_keys.cpp
    src/nijigenerate/core/shortcut/base.cpp
    src/nijigenerate/core/cv/distance1d.cpp
    src/nijigenerate/core/selector/tokenizer.cpp
    src/nijigenerate/core/selector/parser.cpp
    src/nijigenerate/io/depthsample.cpp
    src/nijigenerate/io/psdbudget.cpp
    src/nijigenerate/io/pngheader.cpp)
target_include_directories(nijigenerate_core PUBLIC src)

target_sources(nijigenerate_core PUBLIC FILE_SET native_arithmetic_fault_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/native_arithmetic_fault.hpp")
target_sources(nijigenerate_core PUBLIC FILE_SET pngheader_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/pngheader.hpp")
target_sources(nijigenerate_core PUBLIC FILE_SET depth_io_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/depthsample.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/psdbudget.hpp")
target_link_libraries(nijigenerate_core PUBLIC nijigenerate_native_options nijigenerate_phobos_stdio)
include("${CMAKE_CURRENT_SOURCE_DIR}/distance_transform.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/command_base_publication.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/typed_format_decimal.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_function_origin.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/vertex_mesheditor_node.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/root_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/editor_app.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/selector_identity.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/selector_token_module.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/translation.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_sanitize.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/tasks.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cv_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/skeleton_path.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/skeletonize.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/notification.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/imageexport.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/imagefmt_reader.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/timeline.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/sdl3_boundary.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/imgui.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/backend_sdl.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depthimage.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depthimage_completion.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/alpha_masks.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_internal_types.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/fixture_tools.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/utils_icon.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/headless.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/osqp.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/quadratic.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_face.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/resources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/version_metadata.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/catalogs.cmake")

if(BUILD_TESTING)
    if(MSVC)
        add_executable(nijigenerate_windows_compile_policy_probe tests/windows_compile_policy_probe.cpp)
        target_link_libraries(nijigenerate_windows_compile_policy_probe PRIVATE nijigenerate_native_options)
        add_test(NAME native_windows_compile_policy COMMAND nijigenerate_windows_compile_policy_probe "$<CONFIG>")
    endif()
    add_executable(nijigenerate_fmt_probe tests/fmt_probe.cpp)
    target_link_libraries(nijigenerate_fmt_probe PRIVATE nijigenerate_core)
    add_test(NAME native_fmt_contract COMMAND nijigenerate_fmt_probe)
    add_executable(nijigenerate_slice_ownership_probe tests/slice_ownership_probe.cpp)
    target_link_libraries(nijigenerate_slice_ownership_probe PRIVATE nijigenerate_core)
    add_test(NAME native_slice_ownership_contract COMMAND nijigenerate_slice_ownership_probe)
    add_executable(nijigenerate_numeric_probe tests/numeric_probe.cpp)
    target_link_libraries(nijigenerate_numeric_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_utf8_probe tests/utf8_probe.cpp)
    target_link_libraries(nijigenerate_utf8_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_translation_format_probe tests/translation_format_probe.cpp)
    target_link_libraries(nijigenerate_translation_format_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_I18N_SOURCE_DIR)
        add_test(NAME native_translation_format_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_translation_format.py"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_translation_format_probe>"
                --i18n-source "${NIJIGENERATE_I18N_SOURCE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/translation-format-evidence/$<CONFIG>"
                --config "$<CONFIG>")
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_utf8_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_utf8.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_utf8_probe>"
                --reference "${CMAKE_CURRENT_SOURCE_DIR}/tests/utf8_reference.d"
                --output "${CMAKE_CURRENT_BINARY_DIR}/utf8-evidence/$<CONFIG>" --config "$<CONFIG>")
    endif()
    add_executable(nijigenerate_template_probe tests/template_probe.cpp)
    target_link_libraries(nijigenerate_template_probe PRIVATE nijigenerate_core)
    try_compile(invalid_template_compiled "${CMAKE_CURRENT_BINARY_DIR}/negative-template"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/template_invalid.cpp"
        CXX_STANDARD 20 OUTPUT_VARIABLE invalid_template_log)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/negative-template.log" "${invalid_template_log}")
    if(invalid_template_compiled OR NOT invalid_template_log MATCHES "constraint|C7602")
        message(FATAL_ERROR "Template constraint compile-fail contract changed: ${invalid_template_log}")
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_template_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_templates.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_template_probe>"
                --reference "${CMAKE_CURRENT_SOURCE_DIR}/tests/template_reference.d"
                --native-negative-log "${CMAKE_CURRENT_BINARY_DIR}/negative-template.log"
                --output "${CMAKE_CURRENT_BINARY_DIR}/template-evidence/$<CONFIG>" --config "$<CONFIG>")
    endif()
    if(NIJIGENERATE_PYTHON)
        add_test(NAME native_persistence_key_contract
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_persistence_keys.py"
                --output "${CMAKE_CURRENT_BINARY_DIR}/persistence-key-evidence/$<CONFIG>/result.json")
        add_test(NAME native_platform_boundary
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_platform_boundary.py"
                --root "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate"
                --output "${CMAKE_CURRENT_BINARY_DIR}/platform-boundary-evidence/$<CONFIG>/result.json")
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_numeric_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_numeric.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_numeric_probe>"
                --reference "${CMAKE_CURRENT_SOURCE_DIR}/tests/numeric_reference.d"
                --output "${CMAKE_CURRENT_BINARY_DIR}/numeric-evidence/$<CONFIG>" --config "$<CONFIG>")
    endif()
    try_compile(multiple_scopes_compiled "${CMAKE_CURRENT_BINARY_DIR}/negative-command-scope"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/command_multiple_scopes_fail.cpp"
        CXX_STANDARD 20 OUTPUT_VARIABLE multiple_scopes_log)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/negative-command-scope.log" "${multiple_scopes_log}")
    if(multiple_scopes_compiled OR NOT multiple_scopes_log MATCHES
            "The D reference cannot instantiate more than one declared scope")
        message(FATAL_ERROR "The multiple-scope compile-fail contract changed: ${multiple_scopes_log}")
    endif()
    add_executable(nijigenerate_command_probe tests/command_probe.cpp)
    target_link_libraries(nijigenerate_command_probe PRIVATE nijigenerate_core)
    add_test(NAME native_command_contract COMMAND nijigenerate_command_probe)
    add_executable(nijigenerate_command_fields_probe tests/command_fields_probe.cpp)
    target_link_libraries(nijigenerate_command_fields_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_selector_probe tests/selector_probe.cpp)
    target_link_libraries(nijigenerate_selector_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_selector_uint_probe tests/selector_uint_probe.cpp)
    target_link_libraries(nijigenerate_selector_uint_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_signal_probe tests/signal_probe.cpp)
    target_link_libraries(nijigenerate_signal_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_selector_tree_probe tests/selector_tree_probe.cpp)
    target_link_libraries(nijigenerate_selector_tree_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depthsample_enum_probe tests/depthsample_enum_probe.cpp)
    target_link_libraries(nijigenerate_depthsample_enum_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depthsample_predicate_probe tests/depthsample_predicate_probe.cpp)
    target_link_libraries(nijigenerate_depthsample_predicate_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depthsample_scalar_probe tests/depthsample_scalar_probe.cpp)
    target_link_libraries(nijigenerate_depthsample_scalar_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_phobos_exp_float_probe tests/phobos_exp_float_probe.cpp)
    target_link_libraries(nijigenerate_phobos_exp_float_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depthsample_pixel_probe tests/depthsample_pixel_probe.cpp)
    target_link_libraries(nijigenerate_depthsample_pixel_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depthsample_convolve_probe tests/depthsample_convolve_probe.cpp)
    target_link_libraries(nijigenerate_depthsample_convolve_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depthsample_median_probe tests/depthsample_median_probe.cpp)
    target_link_libraries(nijigenerate_depthsample_median_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_depthsample_alpha_probe tests/depthsample_alpha_probe.cpp)
    target_link_libraries(nijigenerate_depthsample_alpha_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_io_probe tests/io_probe.cpp)
    add_executable(nijigenerate_psdbudget_probe tests/psdbudget_probe.cpp)
    target_link_libraries(nijigenerate_psdbudget_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_distance1d_probe tests/distance1d_probe.cpp)
    target_link_libraries(nijigenerate_distance1d_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_packer_probe tests/packer_probe.cpp)
    target_link_libraries(nijigenerate_packer_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_INMATH_SOURCE_DIR)
        add_test(NAME native_packer_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_packer.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --native "$<TARGET_FILE:nijigenerate_packer_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/packer-evidence/$<CONFIG>")
    endif()
    target_link_libraries(nijigenerate_io_probe PRIVATE nijigenerate_core)
    add_test(NAME native_io_selftest COMMAND nijigenerate_io_probe selftest)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_distance1d_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_distance1d.py"
                --native "$<TARGET_FILE:nijigenerate_distance1d_probe>"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/core/cv/distancetransform.d"
                --output "${CMAKE_CURRENT_BINARY_DIR}/distance1d-evidence/$<CONFIG>")
        add_test(NAME native_psdbudget_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_psdbudget.py"
                --native "$<TARGET_FILE:nijigenerate_psdbudget_probe>"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/io/psdbudget.d"
                --output "${CMAKE_CURRENT_BINARY_DIR}/psdbudget-evidence/$<CONFIG>")
        add_test(NAME native_signal_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_signal.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_signal_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/signal-evidence/$<CONFIG>")
        add_test(NAME native_selector_uint_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_selector_uint.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_selector_uint_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/selector-uint-evidence/$<CONFIG>")
        add_test(NAME native_selector_tree_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_selector_tree.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_selector_tree_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/selector-tree-evidence/$<CONFIG>")
        add_test(NAME native_command_fields_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/command_fields_compare.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_command_fields_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/command-fields-evidence/$<CONFIG>")
        set(selector_reference "${CMAKE_CURRENT_BINARY_DIR}/d_selector_reference${CMAKE_EXECUTABLE_SUFFIX}")
        add_custom_command(OUTPUT "${selector_reference}"
            COMMAND "${NIJIGENERATE_D_COMPILER}" "-I${PROJECT_SOURCE_DIR}/source"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/selector_reference.d"
                "${PROJECT_SOURCE_DIR}/source/nijigenerate/core/selector/tokenizer.d"
                "${PROJECT_SOURCE_DIR}/source/nijigenerate/core/selector/parser.d"
                "-of=${selector_reference}" -g
            DEPENDS tests/selector_reference.d
                "${PROJECT_SOURCE_DIR}/source/nijigenerate/core/selector/tokenizer.d"
                "${PROJECT_SOURCE_DIR}/source/nijigenerate/core/selector/parser.d"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" VERBATIM)
        add_custom_target(nijigenerate_d_selector_reference ALL DEPENDS "${selector_reference}")
        add_dependencies(nijigenerate_selector_probe nijigenerate_d_selector_reference)
        add_test(NAME native_selector_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_selector.py"
                --native "$<TARGET_FILE:nijigenerate_selector_probe>" --reference "${selector_reference}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/selector-evidence/$<CONFIG>")
        add_test(NAME native_command_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_commands.py"
                --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/commands/base.d"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_command_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/command-evidence/$<CONFIG>")
        set(reference "${CMAKE_CURRENT_BINARY_DIR}/d_io_reference${CMAKE_EXECUTABLE_SUFFIX}")
        add_custom_command(OUTPUT "${reference}"
            COMMAND "${NIJIGENERATE_D_COMPILER}"
                "-I${PROJECT_SOURCE_DIR}/source"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/d_io_reference.d"
                "${PROJECT_SOURCE_DIR}/source/nijigenerate/io/depthsample.d"
                "${PROJECT_SOURCE_DIR}/source/nijigenerate/io/pngheader.d"
                "-of=${reference}" -g
            DEPENDS tests/d_io_reference.d
                "${PROJECT_SOURCE_DIR}/source/nijigenerate/io/depthsample.d"
                "${PROJECT_SOURCE_DIR}/source/nijigenerate/io/pngheader.d"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" VERBATIM)
        add_custom_target(nijigenerate_d_io_reference ALL DEPENDS "${reference}")
        add_dependencies(nijigenerate_io_probe nijigenerate_d_io_reference)
        add_test(NAME native_depthsample_enum_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthsample_enums.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthsample_enum_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthsample-enum-evidence/$<CONFIG>")
        add_test(NAME native_depthsample_scalar_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthsample_scalars.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthsample_scalar_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthsample-scalar-evidence/$<CONFIG>")
        add_test(NAME native_phobos_exp_float_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_phobos_exp_float.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_phobos_exp_float_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/phobos-exp-float-evidence/$<CONFIG>")
        add_test(NAME native_depthsample_pixel_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthsample_pixel.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthsample_pixel_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthsample-pixel-evidence/$<CONFIG>")
        add_test(NAME native_depthsample_convolve_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthsample_convolve.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthsample_convolve_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthsample-convolve-evidence/$<CONFIG>")
        add_test(NAME native_depthsample_median_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthsample_median.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthsample_median_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthsample-median-evidence/$<CONFIG>")
        # These groups launch hundreds of original and native processes. Run76900
        # needed 116s for convolve's original phase alone (no per-case timeout).
        # Budget both phases plus compile/boundary replay and scheduling margin;
        # retain each fixture's 30s case limit and every strict comparison.
        set_tests_properties(native_phobos_exp_float_differential
            native_depthsample_pixel_differential native_depthsample_convolve_differential
            native_depthsample_median_differential PROPERTIES TIMEOUT 600)
        add_test(NAME native_depthsample_alpha_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthsample_alpha.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthsample_alpha_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthsample-alpha-evidence/$<CONFIG>")
        add_test(NAME native_depthsample_predicate_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthsample_predicates.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthsample_predicate_probe>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthsample-predicate-evidence/$<CONFIG>")
        add_test(NAME native_io_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_io.py"
                --native "$<TARGET_FILE:nijigenerate_io_probe>" --reference "${reference}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/evidence/$<CONFIG>")
        add_test(NAME native_depthsample_defaults_replay
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_depthsample_defaults.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_io_probe>"
                --reference "${CMAKE_CURRENT_BINARY_DIR}/d_io_reference${CMAKE_EXECUTABLE_SUFFIX}"
                --reference-output "${CMAKE_CURRENT_BINARY_DIR}/evidence/$<CONFIG>"
                --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthsample-defaults-replay/$<CONFIG>")
        set_tests_properties(native_depthsample_defaults_replay PROPERTIES DEPENDS native_io_differential)
    endif()
endif()

# The existing real runtime sources are production registrations, not probe dependencies.
# Explicit local SDK path and installed SDL3/Boost requirements remain mandatory.
include("${PROJECT_SOURCE_DIR}/cmake/NativeDependencies.cmake")
# Revisit the guarded real publisher after SDL3 was supplied; preserve ImGui selection.
include("${CMAKE_CURRENT_SOURCE_DIR}/backend_sdl.cmake")
option(NIJIGENERATE_DEBUG_PATH_DEFORM "Enable independent path deformation diagnostics" OFF)
if(NIJIGENERATE_DEBUG_PATH_DEFORM)
    # The public definition keeps PathDeformer layout identical in runtime and consumers.
    target_compile_definitions(nicxlive PUBLIC NIJIGENERATE_DEBUG_PATH_DEFORM=1)
    if(TARGET nicxlive_shared)
        target_compile_definitions(nicxlive_shared PUBLIC NIJIGENERATE_DEBUG_PATH_DEFORM=1)
    endif()
endif()
add_library(nijigenerate_runtime_core STATIC
    src/nijigenerate/autorig/deterministic/templates.cpp
    src/nijigenerate/core/selector/query.cpp
    src/nijigenerate/panels/agent_lifecycle.cpp
    src/nijigenerate/project.cpp
    src/nijigenerate/ext/param.cpp
    src/nijigenerate/ext/package.cpp
    src/nijigenerate/core/actionstack.cpp
    src/nijigenerate/core/settings.cpp
    src/nijigenerate/actions/package.cpp
    src/nijigenerate/ext/nodes/node_name.hpp
    src/nijigenerate/core/asyncderivedupdate.cpp
    src/nijigenerate/core/asyncderivedupdate.hpp
    src/nijigenerate/compat/phobos_monotime.cpp
    src/nijigenerate/compat/phobos_monotime.hpp
    src/nijigenerate/core/colorbleed.cpp
    src/nijigenerate/core/math/triangle_resampling.cpp
    src/nijigenerate/core/math/triangulate.cpp
    src/nijigenerate/core/math/welding.cpp)
target_sources(nijigenerate_runtime_core PUBLIC
    FILE_SET math_welding_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/core/math/welding.hpp")
if(MSVC)
    set_property(SOURCE src/nijigenerate/core/math/triangle_resampling.cpp
        src/nijigenerate/core/math/triangulate.cpp
        APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
else()
    set_property(SOURCE src/nijigenerate/core/math/triangle_resampling.cpp
        src/nijigenerate/core/math/triangulate.cpp
        APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/core_path.cmake")
target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_core nijigenerate_core_path
    nicxlive::nicxlive Boost::json SDL3::SDL3)
target_sources(nijigenerate_runtime_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/viewport/depth/common/targetview.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/viewport/depth/draw/coordinate.cpp")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_fitgap.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_layer_presence.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_session.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_source.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_manifest.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_persistence.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_autobind.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_diagnostics.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_readback.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_camera.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/actions_depthbone.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExtDepthOps.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/dpi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/input.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/debug.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/expart.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/ext_parameter_module.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/exgrid.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/expath.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depthbone_nodes.cmake")

if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/ownership_graph.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/depth_grid.cmake")
    if(BUILD_TESTING)
        add_executable(nijigenerate_selector_runtime_probe tests/selector_runtime_probe.cpp)
        target_link_libraries(nijigenerate_selector_runtime_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
        add_test(NAME native_selector_runtime_contract COMMAND nijigenerate_selector_runtime_probe)
        add_executable(nijigenerate_selector_filter_probe tests/selector_filter_probe.cpp)
        target_link_libraries(nijigenerate_selector_filter_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
        add_executable(nijigenerate_parameter_order_probe tests/parameter_order_probe.cpp)
        target_link_libraries(nijigenerate_parameter_order_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
        add_executable(nijigenerate_path_constants_probe tests/path_constants_probe.cpp)
        target_link_libraries(nijigenerate_path_constants_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_path_probe tests/path_probe.cpp)
        target_link_libraries(nijigenerate_path_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_settings_files_probe tests/settings_files_probe.cpp)
        target_link_libraries(nijigenerate_settings_files_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_ext_param_probe tests/ext_param_probe.cpp)
        target_link_libraries(nijigenerate_ext_param_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_ext_param_dup_probe tests/ext_param_dup_probe.cpp)
        target_link_libraries(nijigenerate_ext_param_dup_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_animation_initialization_probe tests/animation_initialization_probe.cpp)
        target_link_libraries(nijigenerate_animation_initialization_probe PRIVATE nijigenerate_runtime_core)
        add_test(NAME native_animation_initialization_contract COMMAND nijigenerate_animation_initialization_probe)
        add_executable(nijigenerate_project_state_probe tests/project_state_probe.cpp)
        target_link_libraries(nijigenerate_project_state_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_project_emit_lifetime_probe tests/project_emit_lifetime_probe.cpp)
        target_link_libraries(nijigenerate_project_emit_lifetime_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_project_arm_probe tests/project_arm_probe.cpp)
        target_link_libraries(nijigenerate_project_arm_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_serde_event_probe tests/serde_event_probe.cpp)
        target_link_libraries(nijigenerate_serde_event_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_puppet_metadata_probe tests/puppet_metadata_probe.cpp)
        add_executable(nijigenerate_typed_reader_probe tests/typed_reader_probe.cpp)
        target_link_libraries(nijigenerate_typed_reader_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_puppet_metadata_reader_probe tests/puppet_metadata_reader_probe.cpp)
        target_link_libraries(nijigenerate_puppet_metadata_reader_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_animation_data_probe tests/animation_data_probe.cpp)
        add_executable(nijigenerate_fmt_boundary_probe tests/fmt_boundary_probe.cpp)
        target_include_directories(nijigenerate_fmt_boundary_probe PRIVATE "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/fmt")
        target_link_libraries(nijigenerate_fmt_boundary_probe PRIVATE nijigenerate_runtime_core)
        add_test(NAME native_fmt_boundary_contract COMMAND nijigenerate_fmt_boundary_probe)
        foreach(packet texture_slots texture_slots3 texture_session transform_typed)
            add_executable(nijigenerate_${packet}_probe tests/${packet}_probe.cpp)
            target_link_libraries(nijigenerate_${packet}_probe PRIVATE nijigenerate_runtime_core)
        endforeach()
        foreach(packet transform_math transform2d transform_string node_typed node_typed_differential
                binding_shared_array binding_typed simplephysics_typed_differential deform_value_typed
                deformation_recovery load_diagnostics meshdata_typed_differential
                deformation_binding_typed weldinglink_typed_differential binding_identity drawable_typed_differential
                parameter_typed parameter_factory_typed puppet_array_ownership mask_typed_differential
                part_schema_typed_differential ext_parameter_typed_writer ext_parameter_typed_reader
                ext_group_color_typed node_parent_factory meshgroup_typed_differential verlet_node_typed
                grid_typed_writer grid_normalize part_typed_differential sine_automation_math sine_automation_typed
                physics_automation_math physics_automation_typed automation_builtin_factory floating_diagnostics
                node_registry_lifecycle triangle_bounds inputtext_buffers vertex_proxy automesh_id automesh_config_enum category_flags)
            add_executable(nijigenerate_${packet}_probe tests/${packet}_probe.cpp)
            target_link_libraries(nijigenerate_${packet}_probe PRIVATE nijigenerate_runtime_core)
        endforeach()
        add_test(NAME native_node_typed_contract COMMAND nijigenerate_node_typed_probe)
        add_test(NAME native_node_parent_factory_contract COMMAND nijigenerate_node_parent_factory_probe)
        add_test(NAME native_automation_builtin_factory_contract COMMAND nijigenerate_automation_builtin_factory_probe)
        add_test(NAME native_puppet_array_ownership COMMAND nijigenerate_puppet_array_ownership_probe)
        add_test(NAME native_binding_shared_array_contract COMMAND nijigenerate_binding_shared_array_probe)
        add_executable(nijigenerate_animation_reader_probe tests/animation_reader_probe.cpp)
        target_link_libraries(nijigenerate_animation_reader_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_automation_reader_probe tests/automation_reader_probe.cpp)
        target_link_libraries(nijigenerate_automation_reader_probe PRIVATE nijigenerate_runtime_core)
        target_link_libraries(nijigenerate_animation_data_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_automation_base_probe tests/automation_base_probe.cpp)
        target_link_libraries(nijigenerate_automation_base_probe PRIVATE nijigenerate_runtime_core)
        target_link_libraries(nijigenerate_puppet_metadata_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_puppet_metadata_ownership_probe tests/puppet_metadata_ownership_probe.cpp)
        target_link_libraries(nijigenerate_puppet_metadata_ownership_probe PRIVATE nijigenerate_runtime_core)
        add_test(NAME native_puppet_metadata_ownership COMMAND nijigenerate_puppet_metadata_ownership_probe)
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_SERIALIZER_SOURCE
                AND NIJIGENERATE_FGHJ_SOURCE_DIR AND NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR
                AND NIJIGENERATE_MIR_CORE_SOURCE_DIR)
            add_test(NAME native_serde_event_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_serde_event.py"
                    --native "$<TARGET_FILE:nijigenerate_serde_event_probe>" --compiler "${NIJIGENERATE_D_COMPILER}"
                    --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}" --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                    --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                    --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/serde-event-evidence/$<CONFIG>")
            if(NIJIGENERATE_PUPPET_SOURCE)
                add_test(NAME native_load_diagnostics_differential
                    COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_load_diagnostics.py"
                        --native "$<TARGET_FILE:nijigenerate_load_diagnostics_probe>"
                        --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}"
                        --compiler "${NIJIGENERATE_D_COMPILER}"
                        --output "${CMAKE_CURRENT_BINARY_DIR}/load-diagnostics-evidence/$<CONFIG>")
                get_filename_component(binding_core_dir "${NIJIGENERATE_PUPPET_SOURCE}" DIRECTORY)
                get_filename_component(binding_d_root "${binding_core_dir}" DIRECTORY)
                add_test(NAME native_node_registry_lifecycle_differential
                    COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_node_registry_lifecycle.py"
                        --native "$<TARGET_FILE:nijigenerate_node_registry_lifecycle_probe>"
                        --d-root "${binding_d_root}" --runtime-root "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}"
                        --compiler "${NIJIGENERATE_D_COMPILER}"
                        --output "${CMAKE_CURRENT_BINARY_DIR}/node-registry-lifecycle-evidence/$<CONFIG>")
                foreach(packet binding_typed binding_identity floating_diagnostics)
                add_test(NAME native_${packet}_differential
                    COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_${packet}.py"
                        --native "$<TARGET_FILE:nijigenerate_${packet}_probe>"
                        --d-root "${binding_d_root}" --compiler "${NIJIGENERATE_D_COMPILER}"
                        --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                        --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                        --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                        --output "${CMAKE_CURRENT_BINARY_DIR}/${packet}-evidence/$<CONFIG>")
                endforeach()
                if(NIJIGENERATE_INMATH_SOURCE_DIR)
                    get_filename_component(nijilive_core_dir "${NIJIGENERATE_PUPPET_SOURCE}" DIRECTORY)
                    foreach(packet transform_math transform_string node_typed simplephysics_typed meshdata_typed
                            weldinglink_typed drawable_typed part_schema_typed grid_typed_writer part_typed)
                        if(packet STREQUAL "node_typed")
                            set(packet_probe nijigenerate_node_typed_differential_probe)
                        elseif(packet STREQUAL "simplephysics_typed")
                            set(packet_probe nijigenerate_simplephysics_typed_differential_probe)
                        elseif(packet STREQUAL "meshdata_typed")
                            set(packet_probe nijigenerate_meshdata_typed_differential_probe)
                        elseif(packet STREQUAL "weldinglink_typed")
                            set(packet_probe nijigenerate_weldinglink_typed_differential_probe)
                        elseif(packet STREQUAL "drawable_typed")
                            set(packet_probe nijigenerate_drawable_typed_differential_probe)
                        elseif(packet STREQUAL "part_schema_typed")
                            set(packet_probe nijigenerate_part_schema_typed_differential_probe)
                        elseif(packet STREQUAL "part_typed")
                            set(packet_probe nijigenerate_part_typed_differential_probe)
                        else()
                            set(packet_probe nijigenerate_${packet}_probe)
                        endif()
                        add_test(NAME native_${packet}_differential
                            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_${packet}.py"
                                --native "$<TARGET_FILE:${packet_probe}>"
                                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                                --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                                --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                                --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                                --output "${CMAKE_CURRENT_BINARY_DIR}/${packet}-evidence/$<CONFIG>")
                    endforeach()
                    add_test(NAME native_inputtext_buffers_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_inputtext_buffers.py"
                            --native "$<TARGET_FILE:nijigenerate_inputtext_buffers_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/inputtext-buffers-evidence/$<CONFIG>")
                    add_test(NAME native_inputtext_buffers_replay
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_inputtext_buffers.py"
                            --native "$<TARGET_FILE:nijigenerate_inputtext_buffers_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --reference-output "${CMAKE_CURRENT_BINARY_DIR}/inputtext-buffers-evidence/$<CONFIG>"
                            --configuration "$<CONFIG>"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/inputtext-buffers-replay/$<CONFIG>")
                    set_tests_properties(native_inputtext_buffers_replay PROPERTIES
                        DEPENDS native_inputtext_buffers_differential)
                    add_test(NAME native_triangle_bounds_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_triangle_bounds.py"
                            --native "$<TARGET_FILE:nijigenerate_triangle_bounds_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/triangle-bounds-evidence/$<CONFIG>")
                    add_test(NAME native_vertex_proxy_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_vertex_proxy.py"
                            --native "$<TARGET_FILE:nijigenerate_vertex_proxy_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/vertex-proxy-evidence/$<CONFIG>")
                    add_test(NAME native_vertex_proxy_replay
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_vertex_proxy.py"
                            --native "$<TARGET_FILE:nijigenerate_vertex_proxy_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                            --reference-output "${CMAKE_CURRENT_BINARY_DIR}/vertex-proxy-evidence/$<CONFIG>"
                            --configuration "$<CONFIG>"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/vertex-proxy-replay/$<CONFIG>")
                    set_tests_properties(native_vertex_proxy_replay PROPERTIES
                        DEPENDS native_vertex_proxy_differential)
                    add_test(NAME native_automesh_config_enum_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_automesh_config_enum.py"
                            --native "$<TARGET_FILE:nijigenerate_automesh_config_enum_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/automesh-config-enum-evidence/$<CONFIG>")
                    add_test(NAME native_automesh_config_enum_replay
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_automesh_config_enum.py"
                            --native "$<TARGET_FILE:nijigenerate_automesh_config_enum_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --reference-output "${CMAKE_CURRENT_BINARY_DIR}/automesh-config-enum-evidence/$<CONFIG>"
                            --configuration "$<CONFIG>"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/automesh-config-enum-replay/$<CONFIG>")
                    set_tests_properties(native_automesh_config_enum_replay PROPERTIES
                        DEPENDS native_automesh_config_enum_differential)
                    add_test(NAME native_automesh_id_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_automesh_id.py"
                            --native "$<TARGET_FILE:nijigenerate_automesh_id_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/automesh-id-evidence/$<CONFIG>")
                    add_test(NAME native_automesh_id_replay
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_automesh_id.py"
                            --native "$<TARGET_FILE:nijigenerate_automesh_id_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --reference-output "${CMAKE_CURRENT_BINARY_DIR}/automesh-id-evidence/$<CONFIG>"
                            --configuration "$<CONFIG>"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/automesh-id-replay/$<CONFIG>")
                    set_tests_properties(native_automesh_id_replay PROPERTIES
                        DEPENDS native_automesh_id_differential)
                    add_test(NAME native_category_flags_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_category_flags.py"
                            --native "$<TARGET_FILE:nijigenerate_category_flags_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/category-flags-evidence/$<CONFIG>")
                    add_test(NAME native_triangle_bounds_replay
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_triangle_bounds.py"
                            --native "$<TARGET_FILE:nijigenerate_triangle_bounds_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                            --reference-output "${CMAKE_CURRENT_BINARY_DIR}/triangle-bounds-evidence/$<CONFIG>"
                            --configuration "$<CONFIG>"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/triangle-bounds-replay/$<CONFIG>")
                    set_tests_properties(native_triangle_bounds_replay PROPERTIES DEPENDS native_triangle_bounds_differential)
                    add_test(NAME native_grid_normalize_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_grid_normalize.py"
                            --native "$<TARGET_FILE:nijigenerate_grid_normalize_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/grid-normalize-evidence/$<CONFIG>")
                    add_test(NAME native_mask_typed_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_drawable_typed.py"
                            --mask --native "$<TARGET_FILE:nijigenerate_mask_typed_differential_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                            --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                            --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/mask-typed-evidence/$<CONFIG>")
                    add_test(NAME native_meshgroup_typed_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_drawable_typed.py"
                            --meshgroup --native "$<TARGET_FILE:nijigenerate_meshgroup_typed_differential_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                            --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                            --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/meshgroup-typed-evidence/$<CONFIG>")
                    foreach(packet deform_value_typed deformation_binding_typed parameter_typed parameter_factory_typed
                            verlet_node_typed sine_automation_typed physics_automation_typed)
                    add_test(NAME native_${packet}_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_${packet}.py"
                            --native "$<TARGET_FILE:nijigenerate_${packet}_probe>"
                            --d-root "${binding_d_root}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                            --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                            --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/${packet}-evidence/$<CONFIG>")
                    endforeach()
                    get_filename_component(d_compiler_bin "${NIJIGENERATE_D_COMPILER}" DIRECTORY)
                    set(automation_phobos_math "${d_compiler_bin}/../import/std/math")
                    if(EXISTS "${automation_phobos_math}/trigonometry.d")
                        foreach(packet sine_automation_math physics_automation_math)
                            add_test(NAME native_${packet}_differential
                                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_${packet}.py"
                                    --native "$<TARGET_FILE:nijigenerate_${packet}_probe>"
                                    --d-root "${binding_d_root}" --compiler "${NIJIGENERATE_D_COMPILER}"
                                    --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                                    --phobos-source "${automation_phobos_math}"
                                    --output "${CMAKE_CURRENT_BINARY_DIR}/${packet}-evidence/$<CONFIG>")
                        endforeach()
                    else()
                        message(STATUS "Automation numeric comparisons unavailable: compiler Phobos math sources not found")
                    endif()
                    foreach(packet ext_parameter_typed_writer ext_parameter_typed_reader ext_group_color_typed)
                        if(packet STREQUAL "ext_group_color_typed")
                            set(ext_reference_args --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}")
                        else()
                            set(ext_reference_args --d-root "${binding_d_root}")
                        endif()
                        add_test(NAME native_${packet}_differential
                            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_${packet}.py"
                                --native "$<TARGET_FILE:nijigenerate_${packet}_probe>"
                                ${ext_reference_args}
                                --editor-source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/param.d"
                                --compiler "${NIJIGENERATE_D_COMPILER}"
                                --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                                --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                                --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                                --output "${CMAKE_CURRENT_BINARY_DIR}/${packet}-evidence/$<CONFIG>")
                    endforeach()
                    add_test(NAME native_deformation_recovery_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_deformation_recovery.py"
                            --native "$<TARGET_FILE:nijigenerate_deformation_recovery_probe>"
                            --d-root "${binding_d_root}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/deformation-recovery-evidence/$<CONFIG>")
                    add_test(NAME native_transform2d_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_transform2d.py"
                            --native "$<TARGET_FILE:nijigenerate_transform2d_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/transform2d-evidence/$<CONFIG>")
                    add_test(NAME native_transform_typed_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_transform_typed.py"
                            --native "$<TARGET_FILE:nijigenerate_transform_typed_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                            --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                            --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/transform-typed-evidence/$<CONFIG>")
                    foreach(packet texture_slots texture_slots3)
                        add_test(NAME native_${packet}_differential
                            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_${packet}.py"
                                --native "$<TARGET_FILE:nijigenerate_${packet}_probe>"
                                --compiler "${NIJIGENERATE_D_COMPILER}" --source "${NIJIGENERATE_PUPPET_SOURCE}"
                                --output "${CMAKE_CURRENT_BINARY_DIR}/${packet}-evidence/$<CONFIG>")
                    endforeach()
                    add_test(NAME native_texture_session_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_texture_session.py"
                            --native "$<TARGET_FILE:nijigenerate_texture_session_probe>"
                            --compiler "${NIJIGENERATE_D_COMPILER}" --texture-source "${nijilive_core_dir}/texture.d"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/texture-session-evidence/$<CONFIG>")
                    add_test(NAME native_typed_reader_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_typed_reader.py"
                            --native "$<TARGET_FILE:nijigenerate_typed_reader_probe>" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}"
                            --math-source "${nijilive_core_dir}/../math/serialization.d"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                            --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                            --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/typed-reader-evidence/$<CONFIG>")
                    add_test(NAME native_automation_base_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_automation_base.py"
                            --native "$<TARGET_FILE:nijigenerate_automation_base_probe>"
                            --compiler "${NIJIGENERATE_D_COMPILER}"
                            --source "${nijilive_core_dir}/automation/package.d"
                            --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}"
                            --math-source "${nijilive_core_dir}/../math/serialization.d"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                            --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                            --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                            --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/automation-base-evidence/$<CONFIG>")
                    add_test(NAME native_automation_reader_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_automation_reader.py"
                            --native "$<TARGET_FILE:nijigenerate_automation_reader_probe>"
                            --compiler "${NIJIGENERATE_D_COMPILER}"
                            --source "${nijilive_core_dir}/automation/package.d"
                            --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}"
                            --math-source "${nijilive_core_dir}/../math/serialization.d"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                            --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                            --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                            --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/automation-reader-evidence/$<CONFIG>")
                    add_test(NAME native_animation_data_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_animation_data.py"
                            --native "$<TARGET_FILE:nijigenerate_animation_data_probe>"
                            --native-reader "$<TARGET_FILE:nijigenerate_animation_reader_probe>"
                            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                            --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                            --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                            --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/animation-data-evidence/$<CONFIG>")
                endif()
                add_test(NAME native_puppet_metadata_differential
                    COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_puppet_metadata.py"
                        --native "$<TARGET_FILE:nijigenerate_puppet_metadata_probe>"
                        --compiler "${NIJIGENERATE_D_COMPILER}" --puppet-source "${NIJIGENERATE_PUPPET_SOURCE}"
                        --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}" --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                        --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                        --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                        --output "${CMAKE_CURRENT_BINARY_DIR}/puppet-metadata-evidence/$<CONFIG>")
                add_test(NAME native_puppet_metadata_reader_differential
                    COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_puppet_metadata_reader.py"
                        --native "$<TARGET_FILE:nijigenerate_puppet_metadata_reader_probe>"
                        --compiler "${NIJIGENERATE_D_COMPILER}" --puppet-source "${NIJIGENERATE_PUPPET_SOURCE}"
                        --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}" --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}"
                        --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                        --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                        --output "${CMAKE_CURRENT_BINARY_DIR}/puppet-metadata-reader-evidence/$<CONFIG>")
            endif()
        endif()
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
            add_test(NAME native_project_arm_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/project_arm_compare.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_project_arm_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/project-arm-evidence/$<CONFIG>")
        endif()
        add_executable(nijigenerate_selector_query_probe tests/selector_query_probe.cpp)
        target_link_libraries(nijigenerate_selector_query_probe PRIVATE nijigenerate_runtime_core)
        add_test(NAME native_selector_query_contract COMMAND nijigenerate_selector_query_probe)
        add_executable(nijigenerate_actionstack_probe tests/actionstack_probe.cpp)
        target_link_libraries(nijigenerate_actionstack_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_actionstack_settings_probe tests/actionstack_settings_probe.cpp)
        target_link_libraries(nijigenerate_actionstack_settings_probe PRIVATE nijigenerate_runtime_core)
        if(WIN32)
            foreach(probe nijigenerate_actionstack_probe nijigenerate_actionstack_settings_probe
                    nijigenerate_project_emit_lifetime_probe
                    nijigenerate_path_probe nijigenerate_path_constants_probe
                    nijigenerate_settings_files_probe nijigenerate_ext_param_probe)
                add_custom_command(TARGET ${probe} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        $<TARGET_RUNTIME_DLLS:${probe}> $<TARGET_FILE_DIR:${probe}>
                    COMMAND_EXPAND_LISTS)
            endforeach()
        endif()
        add_executable(nijigenerate_welding_probe tests/welding_probe.cpp)
        target_link_libraries(nijigenerate_welding_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_asyncderived_probe tests/asyncderived_probe.cpp)
        target_link_libraries(nijigenerate_asyncderived_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_actions_probe tests/actions_probe.cpp)
        target_link_libraries(nijigenerate_actions_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_colorbleed_probe tests/colorbleed_probe.cpp)
        target_link_libraries(nijigenerate_colorbleed_probe PRIVATE nijigenerate_runtime_core)
        add_executable(nijigenerate_texture_probe tests/colorbleed_texture_probe.cpp)
        target_link_libraries(nijigenerate_texture_probe PRIVATE nijigenerate_runtime_core)
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
            add_test(NAME native_ext_param_dup_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_ext_param_dup.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_ext_param_dup_probe>"
                    --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/param.d"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/ext-param-dup-evidence/$<CONFIG>")
            add_test(NAME native_settings_files_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/settings_files_compare.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_settings_files_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/settings-files-evidence/$<CONFIG>")
            add_test(NAME native_ext_param_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_ext_param.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_ext_param_probe>"
                    --param-source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/param.d"
                    --puppet-source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/package.d"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/ext-param-evidence/$<CONFIG>")
            add_test(NAME native_path_constants_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_path_constants.py"
                    --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_path_constants_probe>"
                    --configuration "$<CONFIG>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/path-constants-evidence/$<CONFIG>")
            add_test(NAME native_path_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/path_compare.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_path_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/path-evidence/$<CONFIG>")
            add_test(NAME native_parameter_order_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_parameter_order.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_parameter_order_probe>"
                    --source "${nijilive_core_dir}/param/package.d"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/parameter-order-evidence/$<CONFIG>")
            add_test(NAME native_selector_filter_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_selector_filters.py"
                    --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_selector_filter_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/selector-filter-evidence/$<CONFIG>")
            add_test(NAME native_actionstack_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/actionstack_compare.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_actionstack_probe>"
                    --settings-native "$<TARGET_FILE:nijigenerate_actionstack_settings_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/actionstack-evidence/$<CONFIG>")
            add_test(NAME native_texture_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_colorbleed_texture.py"
                    --native "$<TARGET_FILE:nijigenerate_texture_probe>" --compiler "${NIJIGENERATE_D_COMPILER}"
                    --texture-source "${nijilive_core_dir}/texture.d"
                    --queue-source "${nijilive_core_dir}/render/backends/queue/package.d"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/texture-evidence/$<CONFIG>")
            add_test(NAME native_colorbleed_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_colorbleed.py"
                    --native "$<TARGET_FILE:nijigenerate_colorbleed_probe>" --compiler "${NIJIGENERATE_D_COMPILER}"
                    --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/core/colorbleed.d"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/colorbleed-evidence/$<CONFIG>")
        endif()
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_INMATH_SOURCE_DIR)
            add_test(NAME native_project_emit_lifetime_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_project_emit_lifetime.py"
                    --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                    --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                    --native "$<TARGET_FILE:nijigenerate_project_emit_lifetime_probe>"
                    --configuration "$<CONFIG>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/project-emit-lifetime-evidence/$<CONFIG>")
            add_test(NAME native_project_state_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/project_state_compare.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_project_state_probe>"
                    --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/project-state-evidence/$<CONFIG>")
            add_test(NAME native_asyncderived_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_asyncderived.py"
                    --native "$<TARGET_FILE:nijigenerate_asyncderived_probe>"
                    --actions-native "$<TARGET_FILE:nijigenerate_actions_probe>"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/asyncderived-evidence/$<CONFIG>")
            set_tests_properties(native_asyncderived_differential PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
            add_test(NAME native_welding_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_welding.py"
                    --native "$<TARGET_FILE:nijigenerate_welding_probe>"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                    --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/core/math/welding.d"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/welding-evidence/$<CONFIG>")
        endif()
    endif()
    add_executable(nijigenerate_runtime_probe tests/runtime_probe.cpp)
    target_include_directories(nijigenerate_runtime_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(nijigenerate_runtime_probe PRIVATE
        nijigenerate_native_options nicxlive::nicxlive SDL3::SDL3)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_runtime_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_runtime_probe>
                $<TARGET_FILE_DIR:nijigenerate_runtime_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    if(BUILD_TESTING)
        add_test(NAME native_runtime_link COMMAND nijigenerate_runtime_probe)
    endif()
endif()
if(NIJIGENERATE_RUNTIME_PROBE)
    include(project_lifetime.cmake)
endif()
if(NIJIGENERATE_RUNTIME_PROBE)
    include(project_editmode.cmake)
endif()

if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/vertex_fixtures.cmake")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/psd_retained_budget.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/action_scope_fixtures.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/image_enums.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_vertex_contract.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_topology_contract.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/parameter_api_fixtures.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/image_channels.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mainfont.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/binding_points.cmake")
if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/clipboard_parameter_state.cmake")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/logo.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/texture_widget.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_alpha_provider.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/dialog.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/settings_persistence.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/settings_load.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/settings.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_review.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_neck.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_hierarchy.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_presentation.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/dragdrop_nodes.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/tooltip.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/asyncderivedupdatedetails.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/category.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/window_theme.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/brushstate.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/brushstate_product.cmake")
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_brushstate)
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_brushstate)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/excamera.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/transformhandle.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/imgexport_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/asyncderivedupdateoverlay.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/window_platform.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/category_begin.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/statusbar.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/widget_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/controller.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/toolbar.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/markdown.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/nagscreen.cmake")
if(NIJIGENERATE_IMGUI)
    include("${CMAKE_CURRENT_SOURCE_DIR}/modal.cmake")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/nagscreen_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/palette_enum.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/palette_state.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/binding_ready_fixtures.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depthsample_verification.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_bone_status_types.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_vertex_declaration.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_disabled_log.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/depthsample_private_verification.cmake")

if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/project_verification.cmake")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/psd_mask_bpp.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/psd_budget_functions_replay.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/psd_provider_foundation.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/psd_provider.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/psdlayers.cmake")

if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/ext_parameter_parent_dispatch.cmake")
endif()
if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/expuppet.cmake")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/palette_open.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/mainfont_getter.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/depthsample_reductions_verification.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/psd_retained_budget_replay.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/binding_points_replay.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/palette_state_fields_replay.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/binding_drag_data.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/psd_settings_scalar_fields.cmake")

if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/asyncderivedupdate_primitives_verification.cmake")
endif()





if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/ext_parameter_default_constructors.cmake")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/psd_settings_reference_fields.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/root-automesh-snapshot-registration.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/root-mutation-hook-registration.cmake")

if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/path_state_fields_replay.cmake")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/palette_open_replay.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/depth_gpu_constants.cmake")
if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/dpi_getter_bits.cmake")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/mainfont_state_replay.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/depth_bone_status_types_replay.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/depth_bone_work_key_declaration.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_dynamic_disabled_log_replay.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/autosave.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_pngexport.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_dynamic_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_dynamic_logging.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_dynamic_types.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_dynamic_key.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_dynamic_progress.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_dynamic_apply_environment.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_dynamic_registry.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_dynamic.cmake")

if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/sol-project-display-flags-registration.cmake")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/node_bounds_dispatch.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/set_deform_binding_module_replay.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/triangulation_predicates_replay.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/port_enum_values.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/actions_node_module.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/root-mutation-record-runtime-registration.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/root-mutation-publishers-registration.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/root-mesh-public-declarations-registration.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/root-binding-value-storage-registration.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/actions_drawable.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/actions_parameter.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/root-colorbleed-texdata-registration.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/editor_type_contracts.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/root-puppet-debug-flags-registration.cmake")
if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/locale_module_declarations.cmake")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/locale_catalog_original.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/dpi_actual_setter.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/editor_predicate_pair.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/model_mesheditor_node.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_common.cmake")
if(NIJIGENERATE_IMGUI AND TARGET nijigenerate_runtime_core)
    if(NOT TARGET nijigenerate_inspector_common)
        message(FATAL_ERROR "Inspector source group requires actual runtime, ImGui, drag and dummy targets")
    endif()
    if(NIJIGENERATE_RUNTIME_PROBE AND BUILD_TESTING AND (NOT TARGET nijigenerate_inspector_common_header_probe
            OR NOT TARGET nijigenerate_inspector_common_ownership))
        message(FATAL_ERROR "Inspector source group validation targets were not registered")
    endif()
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/command_scope_record.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/command_scope_json.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/screenshot_json.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/font_init.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/dynamic_factory.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mcp_queue.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/window_base.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/rename_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/tinyfiledialogs_windows.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/io_package_windows.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/texviewer.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/command_execution.cmake")
if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/node_base_module.cmake")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_config_key.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/panel_key_module.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_config_disabled_log_replay.cmake")
if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_framework_fixtures.cmake")
endif()
if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_framework_ids_fixtures.cmake")
endif()
if(NIJIGENERATE_RUNTIME_PROBE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_framework_counters_fixtures.cmake")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_json_crt_fixtures.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_framework.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_templates.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_checkpoint.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_observation.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_registered.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_anatomy.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_evidence.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_program.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_controls.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_storage.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_workflow.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_depth.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_surface.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_pipeline.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_processor.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_editor.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/linear_module_replay.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/repair.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/repair_completion.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/transform_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/flipconfig_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/backend_gl_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_gpu.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_composer.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/debug_gl.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/runtime_gl_core.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/runtime_gl_geometry.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/runtime_gl_texture_consumers.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/runtime_gl_lifecycle.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/atlas_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_processor.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/runtime_snapshot.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/onionslice.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/videoexport.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/animation_player.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/videoexport_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/acp_protocol.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/acp_types.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/acp_stdio.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/acp_echo_agent.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mcp_protocol.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/default_random.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/acp_agent.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/vertex_runtime.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_vertices.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/math_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_editor_operations_base.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_editor_operations_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/viewport_mesh.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/transform_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/crashdump_thread.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/crashdump.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/acp_client.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/acp_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/actions_asyncprogress.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/node_hierarchy_actions.cmake")

# Reference producers use the configured actual runtime, including relocated source cuts.
if(BUILD_TESTING)
    get_property(reference_tests DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY TESTS)
    if(reference_tests)
        if(NIJIGENERATE_PUPPET_SOURCE)
            get_filename_component(reference_core_dir "${NIJIGENERATE_PUPPET_SOURCE}" DIRECTORY)
            get_filename_component(reference_module_root "${reference_core_dir}" DIRECTORY)
            set_property(TEST ${reference_tests} APPEND PROPERTY ENVIRONMENT
                "NIJIGENERATE_NIJILIVE_MODULE_ROOT=${reference_module_root}")
        endif()
        if(NIJIGENERATE_NICXLIVE_SOURCE_DIR)
            set_property(TEST ${reference_tests} APPEND PROPERTY ENVIRONMENT
                "NIJIGENERATE_NICXLIVE_ROOT=${NIJIGENERATE_NICXLIVE_SOURCE_DIR}")
        endif()
    endif()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/simplephysics_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/viewport_animation.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/project_imgui.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/viewport_common.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_brushes.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/circle_brush.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/doublethresh_brush.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/rectangle_brush.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/spline.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/file_command_table.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/physics_geometry.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_physics.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/link.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/about.cmake")
target_link_libraries(nijigenerate_core PUBLIC nijigenerate_link)
include("${CMAKE_CURRENT_SOURCE_DIR}/mcp_server_actions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/actions_camera.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_action_descriptions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/set_deform_binding_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/node_command_tables.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depthmap_psd_path.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/puppet_tool_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/njc_config.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/njc_jsonrpc.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/njc_http_transport.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/njc_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/njc_app.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/psddepthmap_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depthmap_psd_names.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depthmap_psd_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_map_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/command_map_storage.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_editor_traversal.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/psddepthmap_state.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/action_history_panel.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/armed_parameter_panel.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/viewport_depth_common.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_registry_state.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_operation_labels.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/version_literal.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/binding_command_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/psd_dialog_scope.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/flipconfig_state.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/flipconfig_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/parameter_base.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/parameter_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/animedit_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_editor_aggregate.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/model_drawable_editor.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_editor_operations_impl.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/vertex_drawable_editor.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/vertex_deformable_editor.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/vertex_editor.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/vertex_mesheditor_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/model_editor_completion.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_editor_tools.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/lasso_publication.cmake")
if(TARGET nijigenerate_mesh_editor_tools)
    ng_publish_lasso_tool()
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/node_editor.cmake")
if(TARGET nijigenerate_node_editor)
    target_link_libraries(nijigenerate_model_editor_completion PUBLIC nijigenerate_node_editor)
    target_link_libraries(nijigenerate_vertex_editor PUBLIC nijigenerate_node_editor)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_editor_grid_bezier.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/edgecutter_publication.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/connect_tool_publication.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/point_tool_publication.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/pathdeform_tool_publication.cmake")
if(TARGET nijigenerate_mesh_editor_tools)
    ng_publish_edgecutter_tool()
    ng_publish_connect_tool()
    ng_publish_point_tool()
    ng_publish_pathdeform_tool()
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/main_viewport.cmake")
if(TARGET nijigenerate_model_drawable_editor AND TARGET nijigenerate_mesh_editor_operations_impl)
    target_link_libraries(nijigenerate_model_drawable_editor PUBLIC nijigenerate_mesh_editor_operations_impl)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/core_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/parameter_group_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/puppet_edit_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mask_command_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_tool_command_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_editor_command_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_editor_completion.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_dirty_queue.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/viewport_control_command_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/model_binding_command_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/psddepthmap_value_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_common.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_skeleton.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_grid.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_contours.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_optimum.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/vertex_viewport.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/vertex_define_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesh_tool_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/vertex_actions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mesheditor_actions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_viewport_completion.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_tool_catalog.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_directdepth.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_attachedpoint.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_packages.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_apply.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_draw_viewport.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/window_public_state.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/window_loop.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/node_class_name.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/paramsplit_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/binding_base.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/deformable_actions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/parameter_prop.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/parameditor_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/psdmerge_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/psddepthmap_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/psd_dialog_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/scene_tool_panels.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/node_mask_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/edit_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/settings_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/palette_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_camera.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_drawable.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_meshgroup.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_composite.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_apply_completion.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_part.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_grid.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_puppet.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_base.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_depthbone.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automeshbatch_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/viewport_control_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/shortcut_defaults.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/shortcut_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/selector_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/actionstack_publication.cmake")
if(TARGET nijigenerate_runtime_core AND TARGET nicxlive::common)
    ng_publish_actionstack()
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/selector_resource_module.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/kra_zip.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/kra_xml.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/kra_provider.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/kramerge_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/viewport_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/welcome_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/command_browser_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/io_save.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/importer_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/psd_adapter_publication.cmake")
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_io)
    ng_publish_psd_adapter()
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/kra_import.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/project_open.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/editanim_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/animation_list_panel.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/parameters_panel.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inpexport_window.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inpexport_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/project_completion.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/nodes_panel_commands.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/output_widget_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/resource_panel.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/timeline_panel.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/viewport_panel.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mainmenu_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/puppet_file_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/panel_commands_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/shortcut_execution.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/shell_panel.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/logger_panel.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mcp_resource_listing.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mcp_helpers.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mcp_auth.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_config_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/automesh_dynamic_complete.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/thread_context.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/command_package_initialization.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mcp_sdk_server.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mcp_http_transport.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mcp_server_complete.cmake")
# The main/model/vertex viewports and editor commands call each other's real providers.
if(TARGET nijigenerate_main_viewport AND TARGET nijigenerate_model_editor_completion)
    target_link_libraries(nijigenerate_main_viewport PUBLIC nijigenerate_model_editor_completion)
    target_link_libraries(nijigenerate_model_editor_completion PUBLIC nijigenerate_main_viewport)
endif()
if(TARGET nijigenerate_vertex_viewport)
    target_link_libraries(nijigenerate_vertex_viewport PUBLIC
        nijigenerate_main_viewport nijigenerate_model_editor_completion nijigenerate_mesh_editor_tools)
    target_link_libraries(nijigenerate_main_viewport PUBLIC nijigenerate_vertex_viewport)
    target_link_libraries(nijigenerate_model_editor_completion PUBLIC nijigenerate_vertex_viewport)
    target_link_libraries(nijigenerate_mesh_editor_operations_impl PUBLIC nijigenerate_vertex_viewport)
    target_link_libraries(nijigenerate_mesh_editor_tools PUBLIC nijigenerate_vertex_viewport)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/actions_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/mcp_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/api_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/atlas_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_contracts.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/depth_bone_key_projection.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/core_path.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/window_context_product.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/window_open_linux.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/ext_initialization.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_panel_declarations.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_panel_registration.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/autorig_native.cmake")
if(TARGET nijigenerate_autorig_panel_registration AND TARGET nijigenerate_autorig_native)
    target_link_libraries(nijigenerate_autorig_panel_registration PRIVATE nijigenerate_autorig_native)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/project_public_headers.cmake")
# Revisit the genuine frame after its providers, then attach the existing pump.
include("${CMAKE_CURRENT_SOURCE_DIR}/window_loop.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/window_event_pump.cmake")
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_window_state
        AND TARGET nijigenerate_tasks AND TARGET nijigenerate_io_save
        AND TARGET nijigenerate_gl_backend_source AND TARGET nijigenerate_window_simple
        AND TARGET nijigenerate_modal AND TARGET nijigenerate_dialogs
        AND TARGET nijigenerate_label AND TARGET nijigenerate_dpi AND TARGET OpenGL::GL)
    ng_attach_window_event_pump()
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/windows_package.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/inspector_apply_commands.cmake")
# Keep product composition after the real module/provider publications.
include("${CMAKE_CURRENT_SOURCE_DIR}/editor_product.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m347_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m339_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m349_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m351_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m350_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m107_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m341_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m125_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m354_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m355_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m348_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m352_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m353_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m105_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m106_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m194_port.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/phobos_socket.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/source_m340_port.cmake")
