if(TARGET nijigenerate_parameter_base)
    target_sources(nijigenerate_parameter_base PRIVATE
        src/nijigenerate/commands/parameter/param.cpp
        src/nijigenerate/commands/parameter/group.cpp
        src/nijigenerate/commands/parameter/paramedit.cpp)
    target_link_libraries(nijigenerate_parameter_base PUBLIC nijigenerate_model_editor_completion)
    target_sources(nijigenerate_parameter_base PUBLIC FILE_SET parameter_group_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/group.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/covariant_command.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/group_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/group_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/group_table.hpp")
    target_sources(nijigenerate_parameter_base PUBLIC FILE_SET parameter_param_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/param.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/param_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/param_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/param_table.hpp")
    target_sources(nijigenerate_parameter_base PUBLIC FILE_SET parameter_paramedit_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/paramedit.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/paramedit_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/paramedit_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/parameter/paramedit_table.hpp")
endif()
