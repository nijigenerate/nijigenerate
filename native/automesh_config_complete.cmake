if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_vertex_viewport)
    set(_automesh_config_sources
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/config_common.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/config_commands.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/config_registry.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE ${_automesh_config_sources})
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET automesh_config_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/config.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/detail/declared_constructor_arguments.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/config_tables.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/config_keys.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/detail/config_description.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/detail/config_fields.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/detail/config_enum_names.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/detail/config_initialization.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh_config_command.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/automesh/detail/config_snapshot.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_vertex_viewport nijigenerate_dialog_queue)
    if(MSVC)
        set_property(SOURCE ${_automesh_config_sources} APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE ${_automesh_config_sources} APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
    unset(_automesh_config_sources)
endif()

if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_automesh_alpha_provider
        AND TARGET nijigenerate_category AND TARGET nijigenerate_drag
        AND TARGET nijigenerate_label AND TARGET nijigenerate_dummy
        AND TARGET nijigenerate_button AND NOT TARGET nijigenerate_automesh_meta)
    add_library(nijigenerate_automesh_meta INTERFACE)
    target_include_directories(nijigenerate_automesh_meta INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_automesh_meta INTERFACE cxx_std_20)
    target_sources(nijigenerate_automesh_meta INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/meta.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/meta_reflect.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/meta_class_info.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/meta_reflection.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/remove_float.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/meta_source_binding.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/meta_source_grid.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/meta_source_contours.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/meta_source_optimum.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/viewport/vertex/automesh/meta_source_skeletonize.hpp")
    target_link_libraries(nijigenerate_automesh_meta INTERFACE nijigenerate_runtime_core
        nijigenerate_automesh_alpha_provider nijigenerate_category nijigenerate_drag
        nijigenerate_label nijigenerate_dummy nijigenerate_button)
    if(MSVC)
        target_compile_options(nijigenerate_automesh_meta INTERFACE /fp:strict)
    else()
        target_compile_options(nijigenerate_automesh_meta INTERFACE -ffp-contract=off -fno-fast-math)
    endif()
endif()

if(TARGET nijigenerate_automesh_meta)
    foreach(_automesh_host IN ITEMS nijigenerate_automesh_grid nijigenerate_automesh_contours
            nijigenerate_automesh_optimum nijigenerate_automesh_skeleton)
        if(TARGET ${_automesh_host})
            target_link_libraries(${_automesh_host} PRIVATE nijigenerate_automesh_meta)
        endif()
    endforeach()
    unset(_automesh_host)
endif()
