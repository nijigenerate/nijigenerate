add_library(nijigenerate_depthbone_nodes STATIC src/nijigenerate/ext/nodes/exdepthbone_node.cpp
    src/nijigenerate/ext/nodes/exdepthrig_root.cpp src/nijigenerate/ext/nodes/exdepthrig_registry.cpp)
target_link_libraries(nijigenerate_depthbone_nodes PUBLIC nijigenerate_core nicxlive::nicxlive)
# Keep the original finite guards and scalar operations in every real body,
# including public inline helpers and serializer templates in consumers.
# The skeleton is registered separately on the runtime target.
if(MSVC)
    target_compile_options(nijigenerate_depthbone_nodes PUBLIC /fp:strict)
    set_property(SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthrig_skeleton.cpp"
        APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
else()
    target_compile_options(nijigenerate_depthbone_nodes PUBLIC -ffp-contract=off -fno-fast-math)
    set_property(SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthrig_skeleton.cpp"
        APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
endif()

target_sources(nijigenerate_depthbone_nodes PUBLIC
    FILE_SET depthbone_module_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthbone_module.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthbone.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthrig_data.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthbone_node.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthrig_create.hpp")
if(NIJIGENERATE_RUNTIME_PROBE AND BUILD_TESTING)
    add_library(nijigenerate_depthbone_node_header_probe OBJECT tests/depthbone_node_header_probe.cpp)
    target_link_libraries(nijigenerate_depthbone_node_header_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_library(nijigenerate_depthrig_data_header_probe OBJECT tests/depthrig_data_header_probe.cpp)
    target_link_libraries(nijigenerate_depthrig_data_header_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_library(nijigenerate_depthrig_root_header_probe OBJECT tests/depthrig_root_header_probe.cpp)
    target_link_libraries(nijigenerate_depthrig_root_header_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_executable(nijigenerate_depthrig_helpers_stream tests/depthrig_helpers_stream.cpp)
    target_link_libraries(nijigenerate_depthrig_helpers_stream PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_executable(nijigenerate_depthrig_data_fields_probe tests/depthrig_data_fields_probe.cpp)
    target_link_libraries(nijigenerate_depthrig_data_fields_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_test(NAME native_depthrig_data_fields COMMAND nijigenerate_depthrig_data_fields_probe)
    add_executable(nijigenerate_depthrig_data_methods_probe tests/depthrig_data_methods_probe.cpp)
    target_link_libraries(nijigenerate_depthrig_data_methods_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_test(NAME native_depthrig_data_methods COMMAND nijigenerate_depthrig_data_methods_probe)
    add_executable(nijigenerate_depthrig_data_methods_stream tests/depthrig_data_methods_stream.cpp)
    target_link_libraries(nijigenerate_depthrig_data_methods_stream PRIVATE nijigenerate_core nicxlive::nicxlive)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_depthrig_data_fields_differential
            COMMAND "${NIJIGENERATE_PYTHON}" -X utf8 "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthrig_data_fields.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthrig_data_fields_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthrig-data-fields-evidence/$<CONFIG>")
        set_tests_properties(native_depthrig_data_fields_differential PROPERTIES TIMEOUT 30)
    endif()
    add_executable(nijigenerate_depthbone_node_probe tests/depthbone_node_probe.cpp)
    target_link_libraries(nijigenerate_depthbone_node_probe PRIVATE nijigenerate_depthbone_nodes)
    add_executable(nijigenerate_depthbone_stream_probe tests/depthbone_stream_probe.cpp)
    target_link_libraries(nijigenerate_depthbone_stream_probe PRIVATE nijigenerate_depthbone_nodes)
    add_test(NAME native_depthbone_node_contract COMMAND nijigenerate_depthbone_node_probe)
    set_tests_properties(native_depthbone_node_contract PROPERTIES TIMEOUT 10)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_SERIALIZER_SOURCE
            AND NIJIGENERATE_FGHJ_SOURCE_DIR AND NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR
            AND NIJIGENERATE_MIR_CORE_SOURCE_DIR AND NIJIGENERATE_INMATH_SOURCE_DIR)
        get_filename_component(depthbone_serde_directory "${NIJIGENERATE_SERIALIZER_SOURCE}" DIRECTORY)
        add_test(NAME native_depthbone_node_differential
            COMMAND "${NIJIGENERATE_PYTHON}" -X utf8 "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthbone_node.py"
                --source-root "${PROJECT_SOURCE_DIR}"
                --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}"
                --math-source "${depthbone_serde_directory}/../math/serialization.d"
                --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthbone_stream_probe>"
                --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}" --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthbone-node-evidence/$<CONFIG>")
        set_tests_properties(native_depthbone_node_differential PROPERTIES TIMEOUT 30)
        add_test(NAME native_depthrig_data_methods_differential
            COMMAND "${NIJIGENERATE_PYTHON}" -X utf8 "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthrig_data_methods.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}"
                --native "$<TARGET_FILE:nijigenerate_depthrig_data_methods_stream>"
                --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}" --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthrig-data-methods-evidence/$<CONFIG>")
        set_tests_properties(native_depthrig_data_methods_differential PROPERTIES TIMEOUT 30)
        add_test(NAME native_depthrig_helpers_differential
            COMMAND "${NIJIGENERATE_PYTHON}" -X utf8 "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depthrig_helpers.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depthrig_helpers_stream>"
                --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}" --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}" --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depthrig-helpers-evidence/$<CONFIG>")
        set_tests_properties(native_depthrig_helpers_differential PROPERTIES TIMEOUT 30)
    endif()
    add_custom_target(nijigenerate_depthbone_nodes_project)
    add_dependencies(nijigenerate_depthbone_nodes_project nijigenerate_depthbone_node_header_probe
        nijigenerate_depthbone_nodes nijigenerate_depthbone_node_probe nijigenerate_depthbone_stream_probe
        nijigenerate_depthrig_data_header_probe nijigenerate_depthrig_data_fields_probe
        nijigenerate_depthrig_data_methods_probe nijigenerate_depthrig_data_methods_stream
        nijigenerate_depthrig_root_header_probe nijigenerate_depthrig_helpers_stream)
endif()
