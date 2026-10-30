# Actual Linux provider publication; no fallback target or test is fabricated.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT TARGET nijigenerate_tinyfiledialogs)
    add_library(nijigenerate_tinyfiledialogs STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/tinyfiledialogs_linux.cpp")
    target_include_directories(nijigenerate_tinyfiledialogs PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_tinyfiledialogs PUBLIC cxx_std_20)
    target_compile_definitions(nijigenerate_tinyfiledialogs PRIVATE _GNU_SOURCE)
    target_sources(nijigenerate_tinyfiledialogs PUBLIC FILE_SET tinyfiledialogs_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/tinyfiledialogs.hpp")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET nijigenerate_runtime_core
        AND TARGET nijigenerate_core AND TARGET nijigenerate_dialog_queue AND NOT TARGET nijigenerate_io)
    if(NOT TARGET nijigenerate_tinyfiledialogs)
        message(FATAL_ERROR "Linux io package requires the actual Linux tinyfiledialogs provider")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NG_PORTAL_DBUS REQUIRED IMPORTED_TARGET dbus-1)
    add_library(nijigenerate_portal_filechooser STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/portal_filechooser.cpp")
    target_include_directories(nijigenerate_portal_filechooser PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(nijigenerate_portal_filechooser PUBLIC
        PkgConfig::NG_PORTAL_DBUS SDL3::SDL3 nijigenerate_tinyfiledialogs nicxlive::common)
    target_compile_features(nijigenerate_portal_filechooser PUBLIC cxx_std_20)
    target_sources(nijigenerate_portal_filechooser PUBLIC FILE_SET portal_filechooser_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/portal_filechooser.hpp")
    add_library(nijigenerate_io STATIC "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/package_linux.cpp")
    target_link_libraries(nijigenerate_io PUBLIC nijigenerate_runtime_core nijigenerate_core
        nijigenerate_dialog_queue nijigenerate_tinyfiledialogs nijigenerate_portal_filechooser)
    target_compile_features(nijigenerate_io PUBLIC cxx_std_20)
    target_sources(nijigenerate_io PUBLIC FILE_SET io_package_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/package.hpp")
endif()
