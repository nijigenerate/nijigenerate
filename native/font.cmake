# Actual font helper uses the project-pinned ImGui implementation and atlas state.
add_library(nijigenerate_font STATIC
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/font_add_data.cpp")
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
