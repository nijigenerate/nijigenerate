if(BUILD_TESTING)
    add_executable(nijigenerate_depth_deserialize_probe tests/depth_deserialize_probe.cpp)
    target_link_libraries(nijigenerate_depth_deserialize_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_test(NAME native_depth_deserialize_contract COMMAND nijigenerate_depth_deserialize_probe)
    add_executable(nijigenerate_depth_serialize_probe tests/depth_serialize_probe.cpp)
    target_link_libraries(nijigenerate_depth_serialize_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_test(NAME native_depth_serialize_contract COMMAND nijigenerate_depth_serialize_probe)
    add_executable(nijigenerate_depth_grid_probe tests/depth_grid_probe.cpp)
    target_link_libraries(nijigenerate_depth_grid_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_executable(nijigenerate_depth_interface_probe tests/depth_interface_probe.cpp)
    target_link_libraries(nijigenerate_depth_interface_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    add_test(NAME native_depth_interface_contract COMMAND nijigenerate_depth_interface_probe)
    add_executable(nijigenerate_depth_state_probe tests/depth_state_probe.cpp)
    target_link_libraries(nijigenerate_depth_state_probe PRIVATE nijigenerate_core nicxlive::nicxlive)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_depth_interface_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depth_interface.py"
                --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/nodes/exdepthmapped.d"
                --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_depth_interface_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depth-interface-evidence/$<CONFIG>")
        if(NIJIGENERATE_SERIALIZER_SOURCE AND NIJIGENERATE_FGHJ_SOURCE_DIR
                AND NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR AND NIJIGENERATE_MIR_CORE_SOURCE_DIR)
            get_filename_component(depth_serde_directory "${NIJIGENERATE_SERIALIZER_SOURCE}" DIRECTORY)
            add_test(NAME native_depth_deserialize_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depth_deserialize.py"
                    --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/nodes/exdepthmapped.d"
                    --math-source "${depth_serde_directory}/../math/serialization.d"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_depth_deserialize_probe>"
                    --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}" --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                    --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/depth-deserialize-evidence/$<CONFIG>")
            add_test(NAME native_depth_serialize_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depth_serialize.py"
                    --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/nodes/exdepthmapped.d"
                    --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_depth_serialize_probe>"
                    --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}" --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                    --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/depth-serialize-evidence/$<CONFIG>")
        endif()
        add_test(NAME native_depth_state_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depth_state.py"
                --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/nodes/exdepthmapped.d"
                --node-header "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core/nodes/node.hpp"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depth_state_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depth-state-evidence/$<CONFIG>" --config "$<CONFIG>")
        add_test(NAME native_depth_grid_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_exdepth_grid.py"
                --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/nodes/exdepthmapped.d"
                --veca-source "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core/math/veca.hpp"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depth_grid_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depth-grid-evidence/$<CONFIG>")
    endif()
endif()
