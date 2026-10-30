if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_flipconfig
        AND TARGET nijigenerate_main_viewport AND TARGET nijigenerate_automeshbatch_window
        AND NOT TARGET nijigenerate_viewport_control_commands)
    add_library(nijigenerate_viewport_control_commands STATIC
        src/nijigenerate/commands/viewport/control.cpp)
    target_sources(nijigenerate_viewport_control_commands PUBLIC
        FILE_SET viewport_control_command_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/control.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/control_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/control_pair_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/control_constructor_programs.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/control_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/viewport/control_table.hpp")
    target_include_directories(nijigenerate_viewport_control_commands PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_viewport_control_commands PUBLIC cxx_std_20)
    target_link_libraries(nijigenerate_viewport_control_commands PUBLIC
        nijigenerate_runtime_core nijigenerate_flipconfig nijigenerate_main_viewport
        nijigenerate_automeshbatch_window
        PRIVATE nijigenerate_native_options)
    if(MSVC)
        target_compile_options(nijigenerate_viewport_control_commands PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_viewport_control_commands PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
endif()
