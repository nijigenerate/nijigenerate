if(TARGET nijigenerate_runtime_core AND TARGET nicxlive::nicxlive)
    target_link_libraries(nijigenerate_runtime_core PUBLIC nicxlive::nicxlive)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET animation_player_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/animation/player.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/animation/player_runtime.hpp")
endif()
