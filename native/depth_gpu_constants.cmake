include("${CMAKE_CURRENT_LIST_DIR}/depth_gpu_nonrender.cmake")

set(NIJIGENERATE_BINDBC_OPENGL_SOURCE_DIR "" CACHE PATH "BindBC OpenGL source for original D binding observations")
if(BUILD_TESTING)
    add_executable(nijigenerate_depth_gpu_constants_probe tests/depth_gpu_constants_probe.cpp)
    target_include_directories(nijigenerate_depth_gpu_constants_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(nijigenerate_depth_gpu_constants_probe PRIVATE nijigenerate_depth_gpu_nonrender)
    find_package(Threads REQUIRED)
    target_link_libraries(nijigenerate_depth_gpu_constants_probe PRIVATE Threads::Threads)
    if(TARGET SDL3::SDL3)
        target_sources(nijigenerate_depth_gpu_constants_probe PRIVATE tests/depth_gpu_render_state_other_tu.cpp)
        target_link_libraries(nijigenerate_depth_gpu_constants_probe PRIVATE SDL3::SDL3)
        target_compile_definitions(nijigenerate_depth_gpu_constants_probe PRIVATE NIJIGENERATE_GPU_GL_DECLARATIONS=1)
    endif()
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_depth_gpu_constants_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_depth_gpu_constants.py"
                --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_depth_gpu_constants_probe>" --configuration "$<CONFIG>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/depth-gpu-constants-evidence/$<CONFIG>"
                --gl-bindings "$<BOOL:$<TARGET_EXISTS:SDL3::SDL3>>"
                --bindbc-source "${NIJIGENERATE_BINDBC_OPENGL_SOURCE_DIR}")
    endif()
endif()
