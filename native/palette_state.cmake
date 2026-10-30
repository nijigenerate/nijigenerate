add_library(nijigenerate_palette_state STATIC src/nijigenerate/commands/viewport/palette.cpp)
target_link_libraries(nijigenerate_palette_state PUBLIC nijigenerate_core)
target_sources(nijigenerate_palette_state PUBLIC FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/palette.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/palette_types.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/palette_table.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/palette_command.hpp")
