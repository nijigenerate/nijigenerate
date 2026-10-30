if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET simplephysics_inspector_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/simplephysics.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/simplephysics_run.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET path_inspector_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/pathdeform.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/pathdeform_run.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET node_inspector_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/node.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/node_run.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/path_commands.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/simplephysics_commands.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/node_commands.cpp")
endif()
