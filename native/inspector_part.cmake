if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET part_inspector_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/part.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/part_run.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/part_commands.hpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET node_welding_command_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/welding.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/welding_commands.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/part.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/part_run.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/part.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/part_commands.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/node/welding_complete.cpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_imagefmt_writer nijigenerate_imagefmt_reader)
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/part.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/base.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/part_commands.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/part.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/base.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/part_commands.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
