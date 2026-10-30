# Actual non-Linux Unix branch of the original io package.
if(APPLE AND NOT TARGET nijigenerate_tinyfiledialogs)
    add_library(nijigenerate_tinyfiledialogs STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/tinyfiledialogs_linux.cpp")
    target_include_directories(nijigenerate_tinyfiledialogs PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_tinyfiledialogs PUBLIC cxx_std_20)
    target_sources(nijigenerate_tinyfiledialogs PUBLIC FILE_SET tinyfiledialogs_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/tinyfiledialogs.hpp")
endif()
if(APPLE AND TARGET nijigenerate_runtime_core AND TARGET nijigenerate_core
        AND TARGET nijigenerate_dialog_queue AND NOT TARGET nijigenerate_io)
    add_library(nijigenerate_io STATIC "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/package_macos.cpp")
    target_link_libraries(nijigenerate_io PUBLIC nijigenerate_runtime_core nijigenerate_core
        nijigenerate_dialog_queue nijigenerate_tinyfiledialogs)
    target_compile_features(nijigenerate_io PUBLIC cxx_std_20)
    target_sources(nijigenerate_io PUBLIC FILE_SET io_package_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/package.hpp")
endif()
