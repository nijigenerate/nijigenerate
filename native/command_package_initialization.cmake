if(TARGET nijigenerate_runtime_core)
    option(NG_AUTOMESH_INIT_DIAG_FORCE "Instantiate all AutoMesh command types at compile time" OFF)
    if(NG_AUTOMESH_INIT_DIAG_FORCE)
        target_compile_definitions(nijigenerate_runtime_core PUBLIC NG_AUTOMESH_INIT_DIAG_FORCE)
    endif()
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/package_initialization.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET command_package_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/package.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/source_diagnostic_text.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/cmd.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/all_command_maps.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/command_table.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/map_initialization.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/initializer_body.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/initializer_scan.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/initializer_diagnostics.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/automesh_forced_aliases.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/map_selection.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/cmd_map_adapters.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/map_validation.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/base_ex_args.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/command_by_id.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/apply_args.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/registered_dispatch.hpp")
endif()
