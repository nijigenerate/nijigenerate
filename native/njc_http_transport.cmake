if(TARGET nijigenerate_runtime_core AND NOT TARGET nijigenerate_njc_http_transport)
    add_library(nijigenerate_njc_http_transport STATIC "${CMAKE_CURRENT_LIST_DIR}/src/njc/http_transport.cpp")
    target_include_directories(nijigenerate_njc_http_transport PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_sources(nijigenerate_njc_http_transport PUBLIC
        FILE_SET njc_http_transport_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/njc/http_transport.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/njc/http_transport_errors.hpp")
    target_compile_features(nijigenerate_njc_http_transport PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_njc_http_transport PUBLIC nijigenerate_runtime_core nijigenerate_njc_config)
    if(WIN32)
        target_link_libraries(nijigenerate_njc_http_transport PRIVATE ws2_32)
    endif()
endif()
