if(TARGET nijigenerate_runtime_core AND TARGET nicxlive::nicxlive AND NOT TARGET nijigenerate_exgrid)
    add_library(nijigenerate_exgrid STATIC src/nijigenerate/ext/nodes/exgriddeformer.cpp)
    target_sources(nijigenerate_exgrid PUBLIC
        FILE_SET exgrid_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exgriddeformer.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exgriddeformer_node.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthmapped.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthops.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthgrid.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthnumeric.hpp")
    target_include_directories(nijigenerate_exgrid PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_exgrid PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_exgrid PUBLIC nijigenerate_runtime_core)
    # The public remap helper also executes finite guards and ordered distances.
    if(MSVC)
        target_compile_options(nijigenerate_exgrid PUBLIC /fp:strict)
    else()
        target_compile_options(nijigenerate_exgrid PUBLIC -ffp-contract=off -fno-fast-math)
    endif()
endif()
if(NIJIGENERATE_RUNTIME_PROBE AND BUILD_TESTING)
    add_library(nijigenerate_exgrid_header_probe OBJECT tests/exgrid_header_probe.cpp)
    target_link_libraries(nijigenerate_exgrid_header_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_executable(nijigenerate_exgrid_remap_probe tests/ext_depthops/grid_remap_snapshot.cpp)
    target_link_libraries(nijigenerate_exgrid_remap_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_executable(nijigenerate_exgrid_node_probe tests/exgrid_node_probe.cpp)
    target_link_libraries(nijigenerate_exgrid_node_probe PRIVATE nijigenerate_exgrid)
    add_test(NAME native_exgrid_node_contract COMMAND nijigenerate_exgrid_node_probe)
    add_executable(nijigenerate_exgrid_copy_probe tests/exgrid_copy_probe.cpp)
    target_link_libraries(nijigenerate_exgrid_copy_probe PRIVATE nijigenerate_exgrid)
    add_executable(nijigenerate_exgrid_orchestration_probe tests/exgrid_orchestration_probe.cpp)
    target_link_libraries(nijigenerate_exgrid_orchestration_probe PRIVATE nijigenerate_exgrid)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_SERIALIZER_SOURCE
            AND NIJIGENERATE_INMATH_SOURCE_DIR)
        add_test(NAME native_exgrid_mixin_inventory
            COMMAND "${NIJIGENERATE_PYTHON}" -X utf8
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_exgrid_mixin_inventory.py"
                --source-root "${PROJECT_SOURCE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/exgrid-mixin-evidence/$<CONFIG>/report.json")
        get_filename_component(exgrid_serde_directory "${NIJIGENERATE_SERIALIZER_SOURCE}" DIRECTORY)
        add_test(NAME native_exgrid_remap_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_exgrid_project_remap.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --source-root "${PROJECT_SOURCE_DIR}"
                --nijilive-source "${exgrid_serde_directory}/../.." --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                --native "$<TARGET_FILE:nijigenerate_exgrid_remap_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/exgrid-remap-evidence/$<CONFIG>")
        add_test(NAME native_exgrid_rebuffer_differential
            COMMAND "${NIJIGENERATE_PYTHON}" -X utf8
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_exgrid_rebuffer.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_exgrid_node_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/exgrid-rebuffer-evidence/$<CONFIG>")
        add_test(NAME native_exgrid_copy_differential
            COMMAND "${NIJIGENERATE_PYTHON}" -X utf8
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_exgrid_copy.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_exgrid_copy_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/exgrid-copy-evidence/$<CONFIG>")
        add_test(NAME native_exgrid_orchestration_differential
            COMMAND "${NIJIGENERATE_PYTHON}" -X utf8
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_exgrid_orchestration.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_exgrid_orchestration_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/exgrid-orchestration-evidence/$<CONFIG>")
    endif()
    add_custom_target(nijigenerate_exgrid_project)
    add_dependencies(nijigenerate_exgrid_project nijigenerate_exgrid
        nijigenerate_exgrid_header_probe nijigenerate_exgrid_remap_probe nijigenerate_exgrid_node_probe
        nijigenerate_exgrid_copy_probe nijigenerate_exgrid_orchestration_probe)
endif()
