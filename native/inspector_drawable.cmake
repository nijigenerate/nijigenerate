if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/drawable_commands.cpp")
    if(TARGET nijigenerate_inspector_common AND TARGET nijigenerate_imgui
            AND TARGET nijigenerate_category AND TARGET nijigenerate_label
            AND TARGET nijigenerate_drag AND TARGET nijigenerate_dummy
            AND NOT TARGET nijigenerate_inspector_drawable)
        add_library(nijigenerate_inspector_drawable INTERFACE)
        target_include_directories(nijigenerate_inspector_drawable INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_inspector_drawable INTERFACE cxx_std_20)
        target_sources(nijigenerate_inspector_drawable INTERFACE FILE_SET HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/drawable.hpp"
                "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/drawable_run.hpp")
        target_link_libraries(nijigenerate_inspector_drawable INTERFACE nijigenerate_runtime_core
            nijigenerate_inspector_common nijigenerate_imgui nijigenerate_category
            nijigenerate_label nijigenerate_drag nijigenerate_dummy)
    endif()
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/drawable_commands.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/drawable_commands.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
