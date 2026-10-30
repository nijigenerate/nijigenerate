if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_window_base)
    add_library(nijigenerate_mesh_editor_aggregate STATIC
        src/nijigenerate/viewport/common/mesheditor/package.cpp
        src/nijigenerate/viewport/common/mesheditor/package_object.cpp
        src/nijigenerate/commands/mesheditor/tool.cpp)
    target_compile_features(nijigenerate_mesh_editor_aggregate PUBLIC cxx_std_20)
    target_sources(nijigenerate_mesh_editor_aggregate PUBLIC
        FILE_SET mesh_editor_aggregate_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/common/mesheditor/package.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/mesheditor/tool.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/mesheditor/tool_commands.hpp")
    target_link_libraries(nijigenerate_mesh_editor_aggregate PUBLIC
        nijigenerate_runtime_core nijigenerate_parameter_actions
        PRIVATE nijigenerate_button nijigenerate_flipconfig)
    if(MSVC)
        target_compile_options(nijigenerate_mesh_editor_aggregate PRIVATE /fp:strict)
    else()
        target_compile_options(nijigenerate_mesh_editor_aggregate PRIVATE -ffp-contract=off)
    endif()
endif()
