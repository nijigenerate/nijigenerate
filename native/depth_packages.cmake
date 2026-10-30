if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET depth_package_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/draw/package.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/mesheditor/package.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/depth/package.hpp")
endif()
