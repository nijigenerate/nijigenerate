if(TARGET nijigenerate_tasks)
    add_library(nijigenerate_skeleton_path INTERFACE)
    target_sources(nijigenerate_skeleton_path INTERFACE
        FILE_SET skeleton_path_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/path.hpp")
    target_compile_features(nijigenerate_skeleton_path INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_skeleton_path INTERFACE nijigenerate_tasks)
endif()

if(TARGET nijigenerate_tasks AND BUILD_TESTING)
    add_executable(nijigenerate_skeleton_path_probe "${CMAKE_CURRENT_LIST_DIR}/tests/skeleton_path_probe.cpp")
    target_link_libraries(nijigenerate_skeleton_path_probe PRIVATE nijigenerate_tasks)
    get_target_property(skeleton_context_type Boost::context TYPE)
    if(WIN32 AND skeleton_context_type STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET nijigenerate_skeleton_path_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Boost::context>" "$<TARGET_FILE_DIR:nijigenerate_skeleton_path_probe>")
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_INMATH_SOURCE_DIR
            AND NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR AND NIJIGENERATE_MIR_CORE_SOURCE_DIR)
        add_test(NAME native_skeleton_path_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_skeleton_path.py"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_skeleton_path_probe>"
                --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/skeleton-path-evidence/$<CONFIG>")
    endif()
endif()
