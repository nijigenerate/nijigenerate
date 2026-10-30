if(TARGET nijigenerate_acp_agent AND TARGET nijigenerate_runtime_core
    AND NOT TARGET nijigenerate_acp_client)
    include("${CMAKE_CURRENT_LIST_DIR}/crashdump.cmake")
    add_library(nijigenerate_acp_client STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/client.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/client_bytes.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/detail/client_storage.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/client_process.cpp")
    if(WIN32)
        target_sources(nijigenerate_acp_client PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/acp_client_process_windows.cpp")
    elseif(UNIX)
        target_sources(nijigenerate_acp_client PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/acp_client_process_posix.cpp")
    endif()
    target_include_directories(nijigenerate_acp_client PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_acp_client PUBLIC cxx_std_20)
    find_package(Threads REQUIRED)
    target_link_libraries(nijigenerate_acp_client PUBLIC nijigenerate_acp_agent
        nijigenerate_runtime_core nijigenerate_crashdump Threads::Threads nicxlive::common)
    target_sources(nijigenerate_acp_client PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src" FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/client.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/client_bytes.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/client_process.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_process_exception.hpp")
    target_compile_definitions(nijigenerate_acp_client PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    option(NIJIGENERATE_HAVE_MCP "Enable the original HaveMCP client settings branch" ON)
    if(NIJIGENERATE_HAVE_MCP)
        target_compile_definitions(nijigenerate_acp_client PRIVATE NIJIGENERATE_HAVE_MCP=1)
    endif()
    if(TARGET nijigenerate_native_options)
        target_link_libraries(nijigenerate_acp_client PUBLIC nijigenerate_native_options)
    endif()
endif()
