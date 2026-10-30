if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET viewport_depth_common_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/common/package.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/common/session.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/common/targetview.hpp")
endif()
# The real action helper is registered by depth_map_complete.cmake immediately
# before this fragment. The action bodies are registered by
# depth_editor_completion.cmake under the additional three capabilities below.
if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_depth_draw_gpu AND
        TARGET nijigenerate_depth_draw_pngexport AND TARGET nijigenerate_actions_asyncprogress AND
        TARGET nijigenerate_actions_depthbone AND TARGET nijigenerate_gl_backend_source AND
        TARGET nijigenerate_debug AND TARGET nijigenerate_viewport_base_primitives)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/common/session.cpp")
endif()
