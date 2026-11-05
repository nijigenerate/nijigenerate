# Actual font helper uses the project-pinned ImGui implementation and atlas state.
find_package(Freetype QUIET)
if(NOT TARGET Freetype::Freetype)
    set(NIJIGENERATE_FREETYPE_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../i2d-imgui/deps/freetype"
        CACHE PATH "Local FreeType sources used by the D font renderer")
    if(NOT EXISTS "${NIJIGENERATE_FREETYPE_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "FreeType is required for D-compatible fonts; install FreeType or set NIJIGENERATE_FREETYPE_SOURCE_DIR")
    endif()
    function(ng_add_font_freetype)
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        set(BUILD_SHARED_LIBS OFF)
        set(FT_DISABLE_ZLIB ON)
        set(FT_DISABLE_BZIP2 ON)
        set(FT_DISABLE_PNG ON)
        set(FT_DISABLE_HARFBUZZ ON)
        set(FT_DISABLE_BROTLI ON)
        add_subdirectory("${NIJIGENERATE_FREETYPE_SOURCE_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}/font-freetype" EXCLUDE_FROM_ALL)
    endfunction()
    ng_add_font_freetype()
    add_library(Freetype::Freetype ALIAS freetype)
endif()
add_library(nijigenerate_font STATIC
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/font_add_data.cpp"
    "${NIJIGENERATE_IMGUI_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp")
target_compile_features(nijigenerate_font PUBLIC cxx_std_20)
target_include_directories(nijigenerate_font PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
target_sources(nijigenerate_font PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/font.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/font_entry.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/font_entries.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/font_state.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/font_add_data.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/font_init_state.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/font_list_init.hpp")
target_link_libraries(nijigenerate_font PUBLIC nijigenerate_imgui_sdl3)
target_link_libraries(nijigenerate_font PRIVATE Freetype::Freetype)
