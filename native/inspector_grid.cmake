if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET grid_inspector_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/griddeform.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/griddeform_run.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/griddeform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/griddeform_commands.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/apply_node_init.cpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        FILE_SET grid_inspector_private_headers TYPE HEADERS BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/griddeform_depth_dispatch.hpp")
    if(TARGET nijigenerate_inspector_common AND TARGET nijigenerate_imgui
            AND TARGET nijigenerate_category AND TARGET nijigenerate_button
            AND TARGET nijigenerate_tooltip AND TARGET nijigenerate_label AND TARGET nijigenerate_dummy
            AND NOT TARGET nijigenerate_inspector_grid)
        add_library(nijigenerate_inspector_grid INTERFACE)
        target_include_directories(nijigenerate_inspector_grid INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_inspector_grid INTERFACE cxx_std_20)
        target_sources(nijigenerate_inspector_grid INTERFACE FILE_SET HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/griddeform.hpp"
                "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/griddeform_run.hpp")
        target_link_libraries(nijigenerate_inspector_grid INTERFACE nijigenerate_runtime_core
            nijigenerate_inspector_common nijigenerate_imgui nijigenerate_category
            nijigenerate_button nijigenerate_tooltip nijigenerate_label nijigenerate_dummy)
    endif()
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/griddeform.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/griddeform.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
