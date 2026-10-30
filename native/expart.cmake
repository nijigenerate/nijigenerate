add_library(nijigenerate_expart STATIC src/nijigenerate/ext/nodes/expart.cpp)
target_sources(nijigenerate_expart PUBLIC
    FILE_SET expart_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/expart.hpp")
target_link_libraries(nijigenerate_expart PUBLIC nijigenerate_core nicxlive::nicxlive)

if(NIJIGENERATE_RUNTIME_PROBE AND BUILD_TESTING)
    add_executable(nijigenerate_expart_probe tests/expart_probe.cpp)
    target_link_libraries(nijigenerate_expart_probe PRIVATE nijigenerate_expart)
    add_test(NAME native_expart_contract COMMAND nijigenerate_expart_probe)
    add_executable(nijigenerate_expart_serde_probe tests/expart_serde_probe.cpp)
    target_link_libraries(nijigenerate_expart_serde_probe PRIVATE nijigenerate_expart)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_expart_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_expart.py"
                --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/nodes/expart.d"
                --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_expart_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/expart-evidence/$<CONFIG>")
        if(NIJIGENERATE_SERIALIZER_SOURCE AND NIJIGENERATE_FGHJ_SOURCE_DIR
                AND NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR AND NIJIGENERATE_MIR_CORE_SOURCE_DIR)
            get_filename_component(expart_serde_directory "${NIJIGENERATE_SERIALIZER_SOURCE}" DIRECTORY)
            add_test(NAME native_expart_serde_differential
                COMMAND "${NIJIGENERATE_PYTHON}" -X utf8 "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_expart_serde.py"
                    --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/ext/nodes/expart.d"
                    --serializer-source "${NIJIGENERATE_SERIALIZER_SOURCE}"
                    --math-source "${expart_serde_directory}/../math/serialization.d"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:nijigenerate_expart_serde_probe>"
                    --fghj "${NIJIGENERATE_FGHJ_SOURCE_DIR}" --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                    --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/expart-serde-evidence/$<CONFIG>")
        endif()
    endif()
    add_library(nijigenerate_expart_header_probe OBJECT tests/expart_header_probe.cpp)
    target_link_libraries(nijigenerate_expart_header_probe PRIVATE
        nijigenerate_core nicxlive::nicxlive)
    add_custom_target(nijigenerate_expart_project)
    add_dependencies(nijigenerate_expart_project nijigenerate_expart_header_probe
        nijigenerate_expart_probe nijigenerate_expart_serde_probe)
endif()
