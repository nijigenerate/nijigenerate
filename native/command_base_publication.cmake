if(TARGET nijigenerate_core)
    target_sources(nijigenerate_core PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/base.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/command.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/context.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_call_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/command_scope_transition.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/fields.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/metadata.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/source_reflection.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/source_enum_metadata.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/source_diagnostic_text.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/register_command.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/menu_item.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/concrete_catalog.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/all_maps.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/additional_tables.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/all_map_key_metadata.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/dynamic_map_key_metadata.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/base_ex_args.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/apply_args.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/registered_dispatch.hpp")
    target_sources(nijigenerate_core PUBLIC FILE_SET retained_exception_message_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/retained_exception_message.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/exception_message.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/retained_string.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/throwable.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/out_of_memory_error.hpp")
endif()
