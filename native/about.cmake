# Include after window_base.cmake, logo.cmake, markdown.cmake and link.cmake.
if(TARGET nijigenerate_window_base AND TARGET nijigenerate_logo AND TARGET nijigenerate_markdown
        AND TARGET nijigenerate_link AND TARGET nicxlive::nicxlive)
    set(NIJIGENERATE_ABOUT_RUNTIME_VERSION_SOURCE
        "${PROJECT_SOURCE_DIR}/../nijilive/source/nijilive/ver.d" CACHE FILEPATH
        "Current independent nijilive version source for the About dialog")
    set(about_contributors "${PROJECT_SOURCE_DIR}/CONTRIBUTORS.md")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${about_contributors}" "${NIJIGENERATE_ABOUT_RUNTIME_VERSION_SOURCE}")
    file(READ "${about_contributors}" about_bytes HEX)
    string(LENGTH "${about_bytes}" about_hex_length)
    math(EXPR about_size "${about_hex_length} / 2")
    string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," about_bytes "${about_bytes}")
    file(READ "${NIJIGENERATE_ABOUT_RUNTIME_VERSION_SOURCE}" about_version_source)
    string(REGEX MATCH "enum IN_VERSION = (\"[^\"]*\");" about_version_match "${about_version_source}")
    if(NOT about_version_match)
        message(FATAL_ERROR "About requires the current independent IN_VERSION literal")
    endif()
    set(about_version "${CMAKE_MATCH_1}")
    set(about_generated "${CMAKE_CURRENT_BINARY_DIR}/about-generated")
    file(GENERATE OUTPUT "${about_generated}/nijigenerate/windows/about_resources.hpp" CONTENT
"#pragma once
#include <cstddef>
namespace nijigenerate::windows::about::detail {
inline constexpr unsigned char contributors[] = {${about_bytes}0};
inline constexpr std::size_t contributorsSize = ${about_size};
inline constexpr char runtimeVersion[] = ${about_version};
}
")
    add_library(nijigenerate_about STATIC "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/about.cpp")
    target_sources(nijigenerate_about PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/windows/about.hpp")
    target_include_directories(nijigenerate_about PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src"
        PRIVATE "${about_generated}")
    target_link_libraries(nijigenerate_about PUBLIC nijigenerate_window_base nijigenerate_markdown
        PRIVATE nijigenerate_logo nicxlive::nicxlive nijigenerate_core nijigenerate_dummy
        nijigenerate_label nijigenerate_button nijigenerate_link)
endif()
