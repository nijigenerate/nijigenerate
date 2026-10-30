if(TARGET nijigenerate_mesh_editor_tools)
    # Keep the editor/tool/action strongly connected component in its existing target.
    target_sources(nijigenerate_mesh_editor_tools PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/grid.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/bezierdeform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/pathdeform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/connect.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/mesheditor/tools/edgecutter.cpp")
    target_sources(nijigenerate_mesh_editor_tools PUBLIC
        FILE_SET mesh_editor_grid_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/grid.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/tools/bezierdeform.hpp")
endif()
