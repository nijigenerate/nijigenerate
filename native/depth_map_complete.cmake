option(NIJIGENERATE_COMMAND_BROWSER_DIFFERENTIAL "Map the D CommandBrowserDifferential source version" OFF)

if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_depth_draw_gpu AND
        TARGET nijigenerate_depth_draw_pngexport AND TARGET nijigenerate_actions_asyncprogress AND
        TARGET nijigenerate_actions_depthbone)
    if(NIJIGENERATE_COMMAND_BROWSER_DIFFERENTIAL)
        target_compile_definitions(nijigenerate_runtime_core PUBLIC NG_COMMAND_BROWSER_DIFFERENTIAL=1)
    endif()
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_map_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/covariant_command.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map_internal.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map_json.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map_composition.hpp")
    set(_depth_map_sources
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map_json.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map_summary.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map_mutation.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map_commands.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map_composition.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/depth/map_apply.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE ${_depth_map_sources})
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_depth_draw_gpu
        nijigenerate_depth_draw_pngexport nijigenerate_actions_asyncprogress nijigenerate_actions_depthbone)
    if(MSVC)
        set_property(SOURCE ${_depth_map_sources} APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE ${_depth_map_sources} APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
    unset(_depth_map_sources)
endif()
