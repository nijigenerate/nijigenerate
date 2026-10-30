if(TARGET nijigenerate_runtime_core)
    add_library(nijigenerate_debug STATIC "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/dbg.cpp")
    target_link_libraries(nijigenerate_debug PUBLIC nijigenerate_runtime_core)
    target_sources(nijigenerate_debug PUBLIC
        FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/dbg.hpp")
endif()
