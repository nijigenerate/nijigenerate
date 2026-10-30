if(NOT TARGET nijigenerate_njc_jsonrpc)
    add_library(nijigenerate_njc_jsonrpc STATIC "${CMAKE_CURRENT_LIST_DIR}/src/njc/jsonrpc.cpp")
    add_library(njc::jsonrpc ALIAS nijigenerate_njc_jsonrpc)
    target_include_directories(nijigenerate_njc_jsonrpc PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_njc_jsonrpc PUBLIC cxx_std_20)
    if(NOT TARGET nijigenerate_core)
        message(FATAL_ERROR "NJC JSON-RPC Source formatting requires the actual nijigenerate_core provider")
    endif()
    target_link_libraries(nijigenerate_njc_jsonrpc PUBLIC nijigenerate_core)
    target_sources(nijigenerate_njc_jsonrpc PUBLIC
        FILE_SET jsonrpc_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/njc/jsonrpc.hpp")
    if(TARGET nijigenerate_native_options)
        target_link_libraries(nijigenerate_njc_jsonrpc PUBLIC nijigenerate_native_options)
    elseif(MSVC)
        target_compile_options(nijigenerate_njc_jsonrpc PRIVATE /W4 /utf-8)
    endif()
endif()
