if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_imgui AND TARGET nijigenerate_notification
    AND TARGET nijigenerate_mcp_queue)
    add_library(nijigenerate_actions_asyncprogress STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/asyncprogress.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/asyncprogress.hpp")
    target_include_directories(nijigenerate_actions_asyncprogress PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_actions_asyncprogress PUBLIC
        FILE_SET asyncprogress_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/actions/asyncprogress.hpp")
    target_link_libraries(nijigenerate_actions_asyncprogress PUBLIC nijigenerate_runtime_core nijigenerate_mcp_queue
        PRIVATE nijigenerate_notification nijigenerate_imgui nijigenerate_native_options)
    if(MSVC)
        target_compile_options(nijigenerate_actions_asyncprogress PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_actions_asyncprogress PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
