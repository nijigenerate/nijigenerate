if(TARGET nijigenerate_runtime_core AND NOT TARGET nijigenerate_acp_protocol)
    add_library(nijigenerate_acp_protocol STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/protocol.cpp")
    target_sources(nijigenerate_acp_protocol PUBLIC FILE_SET acp_protocol_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/protocol.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/api/acp/protocol_values.hpp")
    target_include_directories(nijigenerate_acp_protocol PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_acp_protocol PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_acp_protocol PUBLIC nijigenerate_runtime_core)
    if(TARGET nijigenerate_native_options)
        target_link_libraries(nijigenerate_acp_protocol PUBLIC nijigenerate_native_options)
    endif()
endif()
