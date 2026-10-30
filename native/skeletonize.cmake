if(TARGET nijigenerate_tasks)
    add_library(nijigenerate_skeletonize INTERFACE)
    target_sources(nijigenerate_skeletonize INTERFACE
        FILE_SET skeletonize_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/math/skeletonize.hpp")
    target_compile_features(nijigenerate_skeletonize INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_skeletonize INTERFACE nijigenerate_tasks)
endif()

if(TARGET nijigenerate_tasks AND BUILD_TESTING)
    add_executable(nijigenerate_skeletonize_probe "${CMAKE_CURRENT_LIST_DIR}/tests/skeletonize_probe.cpp")
    target_link_libraries(nijigenerate_skeletonize_probe PRIVATE nijigenerate_tasks)
    get_target_property(thinning_context_type Boost::context TYPE)
    if(WIN32 AND thinning_context_type STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET nijigenerate_skeletonize_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Boost::context>" "$<TARGET_FILE_DIR:nijigenerate_skeletonize_probe>")
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_INMATH_SOURCE_DIR
            AND NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR AND NIJIGENERATE_MIR_CORE_SOURCE_DIR)
        add_test(NAME native_skeletonize_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_skeletonize.py"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_skeletonize_probe>"
                --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                --mir-algorithm "${NIJIGENERATE_MIR_ALGORITHM_SOURCE_DIR}"
                --mir-core "${NIJIGENERATE_MIR_CORE_SOURCE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/skeletonize-evidence/$<CONFIG>")
    endif()
endif()
