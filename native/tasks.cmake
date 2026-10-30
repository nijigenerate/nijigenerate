option(NIJIGENERATE_STACKFUL_TASKS "Build cooperative tasks using installed Boost.Context" OFF)
if(NIJIGENERATE_STACKFUL_TASKS)
    find_package(Boost CONFIG REQUIRED COMPONENTS context)
    add_library(nijigenerate_tasks STATIC
        src/nijigenerate/core/tasks.cpp
        src/nijigenerate/platform/automesh_batch_stack.cpp)
    target_link_libraries(nijigenerate_tasks PUBLIC nijigenerate_core Boost::context)
    target_sources(nijigenerate_tasks PUBLIC
        FILE_SET task_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/tasks.hpp")
    if(BUILD_TESTING)
        add_executable(nijigenerate_task_fiber_probe tests/task_fiber_probe.cpp)
        target_link_libraries(nijigenerate_task_fiber_probe PRIVATE nijigenerate_tasks)
        get_target_property(task_context_type Boost::context TYPE)
        if(WIN32 AND task_context_type STREQUAL "SHARED_LIBRARY")
            add_custom_command(TARGET nijigenerate_task_fiber_probe POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "$<TARGET_FILE:Boost::context>" "$<TARGET_FILE_DIR:nijigenerate_task_fiber_probe>")
        endif()
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
            add_test(NAME native_task_fiber_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/task_fiber_reference.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --source "${PROJECT_SOURCE_DIR}/source/nijigenerate/core/tasks.d"
                    --native "$<TARGET_FILE:nijigenerate_task_fiber_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/task-fiber-evidence/$<CONFIG>")
        endif()
    endif()
endif()
