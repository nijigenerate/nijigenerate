if(TARGET nijigenerate_njc_http_transport AND NOT TARGET nijigenerate_njc_commands)
    add_library(nijigenerate_njc_commands STATIC "${CMAKE_CURRENT_LIST_DIR}/src/njc/commands.cpp")
    target_include_directories(nijigenerate_njc_commands PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_njc_commands PUBLIC
        FILE_SET njc_commands_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/njc/commands.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/njc/commands_errors.hpp")
    target_compile_features(nijigenerate_njc_commands PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_njc_commands PUBLIC nijigenerate_njc_http_transport nijigenerate_njc_jsonrpc)
endif()
