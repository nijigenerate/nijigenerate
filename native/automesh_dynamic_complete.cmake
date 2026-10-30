if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_vertex_viewport AND TARGET nijigenerate_tasks)
    set(_automesh_dynamic_sources
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/dynamic_commands.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/dynamic_result.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/dynamic_worker.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE ${_automesh_dynamic_sources})
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET automesh_dynamic_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/dynamic.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/dynamic_key.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/dynamic_registry.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/dynamic_types_fwd.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/detail/dynamic_progress.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_tasks
        nijigenerate_vertex_viewport nijigenerate_automesh_dynamic_registry nijigenerate_notification
        nijigenerate_mcp_queue nijigenerate_automesh_dynamic_progress
        nijigenerate_automesh_dynamic_apply_environment)
    target_link_libraries(nijigenerate_runtime_core PRIVATE nijigenerate_automesh_dynamic_logging)
    if(TARGET nijigenerate_window_finalize)
        target_link_libraries(nijigenerate_window_finalize PRIVATE nijigenerate_runtime_core)
    endif()
    if(MSVC)
        set_property(SOURCE ${_automesh_dynamic_sources} APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE ${_automesh_dynamic_sources} APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
    unset(_automesh_dynamic_sources)
endif()
