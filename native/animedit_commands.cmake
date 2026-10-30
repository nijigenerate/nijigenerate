if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET animedit_command_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/animedit.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/animedit_commands.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/animedit.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_animation_keyframe.cpp")
    if(MSVC)
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/animedit.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_animation_keyframe.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/animedit.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/project_animation_keyframe.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
