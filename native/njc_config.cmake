add_library(nijigenerate_njc_config STATIC
    "${CMAKE_CURRENT_LIST_DIR}/src/njc/config.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/platform/njc_environment.cpp")
target_include_directories(nijigenerate_njc_config PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
target_sources(nijigenerate_njc_config PUBLIC
    FILE_SET njc_config_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/njc/config.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/njc/config_errors.hpp")
target_link_libraries(nijigenerate_njc_config PUBLIC nijigenerate_native_options)
