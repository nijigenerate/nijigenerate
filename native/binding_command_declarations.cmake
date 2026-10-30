if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/binding_declarations.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/binding.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET binding_command_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/binding.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/binding_commands.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/binding_types.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/binding_table.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/binding_value.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/detail/initialization.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/detail/source_alias.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/detail/constructor_body.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/detail/canonical_constructor_programs.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/binding/detail/registration_diagnostic.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC
        nijigenerate_flipconfig nijigenerate_model_editor_completion nijigenerate_transform_complete)
    if(NOT TARGET nijigenerate_binding_commands)
        add_library(nijigenerate_binding_commands INTERFACE)
        target_include_directories(nijigenerate_binding_commands INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_binding_commands INTERFACE cxx_std_20)
        target_link_libraries(nijigenerate_binding_commands INTERFACE nijigenerate_runtime_core)
    endif()
endif()
