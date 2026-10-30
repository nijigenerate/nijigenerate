# Production queue provider and one grouped source/native module comparison.
find_package(Threads REQUIRED)
add_library(nijigenerate_mcp_queue STATIC
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/task/task.cpp")
target_compile_features(nijigenerate_mcp_queue PUBLIC cxx_std_20)
target_sources(nijigenerate_mcp_queue PUBLIC FILE_SET mcp_queue_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/task/task.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/task.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/auth_wait.hpp")
target_include_directories(nijigenerate_mcp_queue PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/src")
target_link_libraries(nijigenerate_mcp_queue PUBLIC Threads::Threads nicxlive::common)
if(TARGET nijigenerate_tasks)
    # The normal master registers this real producer later in mcp_http_transport.cmake.
    target_link_libraries(nijigenerate_mcp_queue PUBLIC
        nijigenerate_tasks nijigenerate_vibe_task_runtime)
endif()

if(BUILD_TESTING)
    # This probe includes the actual provider TU to inspect private queue state.
    # Consumers link the production library above instead of this executable.
    add_executable(nijigenerate_mcp_queue_probe tests/mcp_queue/queue_probe.cpp)
    target_compile_features(nijigenerate_mcp_queue_probe PRIVATE cxx_std_20)
    target_include_directories(nijigenerate_mcp_queue_probe PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/task")
    target_link_libraries(nijigenerate_mcp_queue_probe PRIVATE Threads::Threads)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_mcp_queue_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/mcp_queue/compare_queue.py"
                --source-root "${PROJECT_SOURCE_DIR}"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --native "$<TARGET_FILE:nijigenerate_mcp_queue_probe>"
                --provider-root "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/mcp/task"
                --output "${CMAKE_CURRENT_BINARY_DIR}/mcp-queue/$<CONFIG>")
        set_tests_properties(native_mcp_queue_differential PROPERTIES TIMEOUT 420)
    endif()
endif()
