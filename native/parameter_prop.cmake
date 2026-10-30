if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/prop.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/axis_provider.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET parameter_prop_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/prop.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/prop_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/prop_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/prop_table.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/prop_axis_normalization.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/prop_axis_snapshots.hpp")
endif()
