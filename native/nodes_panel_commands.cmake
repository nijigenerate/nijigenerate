if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET nodes_panel_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/nodes.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/nodes_menu.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/nodes_icon.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET node_command_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/node.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/node_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/node_table.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/node_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/covariant_command.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET node_base_command_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/base.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/clipboard_state.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/conversion_map.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET dynamic_node_command_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/dynamic.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/dynamic_keys.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/dynamic_discovery.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/nodes.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/nodes_menu.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/base.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/node_commands.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/dynamic.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_depthbone_nodes
        nijigenerate_panels nijigenerate_label nijigenerate_button nijigenerate_dummy
        nijigenerate_tooltip nijigenerate_dragdrop nijigenerate_inputtext)
    if(MSVC)
        set_source_files_properties(
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/nodes.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/nodes_menu.cpp"
            PROPERTIES COMPILE_OPTIONS /fp:strict)
    else()
        set_source_files_properties(
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/nodes.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/nodes_menu.cpp"
            PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
    endif()
endif()
if(TARGET nijigenerate_depthbone_nodes)
    target_sources(nijigenerate_depthbone_nodes PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/ext/nodes/exdepthrig_copy.cpp")
endif()
