if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET inspector_depthbone_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/depthbone.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/depthbone_run.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/depthbone_dispatch.hpp")
endif()
