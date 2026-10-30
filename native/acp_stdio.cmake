if(TARGET nijigenerate_runtime_core AND NOT TARGET nijigenerate_acp_stdio)
    add_library(nijigenerate_acp_stdio STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/transport/stdio.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/transport/mcp_stdio.cpp")
    target_sources(nijigenerate_acp_stdio PUBLIC FILE_SET acp_stdio_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/transport/stdio.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/transport/mcp_stdio.hpp")
    target_include_directories(nijigenerate_acp_stdio PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_acp_stdio PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_acp_stdio PUBLIC nijigenerate_runtime_core nijigenerate_phobos_stdio)
    if(TARGET nijigenerate_native_options)
        target_link_libraries(nijigenerate_acp_stdio PUBLIC nijigenerate_native_options)
    endif()
    if(MSVC)
        target_compile_options(nijigenerate_acp_stdio PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_acp_stdio PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
