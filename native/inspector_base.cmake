if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_panels
        AND TARGET nijigenerate_inspector_common AND TARGET nijigenerate_inspector_puppet)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/base.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_panels
        nijigenerate_inspector_common nijigenerate_inspector_puppet)
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET inspector_base_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/base.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/package.hpp")
endif()
