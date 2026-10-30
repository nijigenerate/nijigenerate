if(TARGET nijigenerate_mcp AND TARGET nijigenerate_acp AND NOT TARGET nijigenerate_api)
    add_library(nijigenerate_api INTERFACE)
    target_sources(nijigenerate_api INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/package.hpp")
    target_include_directories(nijigenerate_api INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_api INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_api INTERFACE nijigenerate_mcp nijigenerate_acp)
endif()
