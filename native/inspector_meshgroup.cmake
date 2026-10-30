if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/meshgroup_commands.cpp")
    if(TARGET nijigenerate_inspector_common AND TARGET nijigenerate_imgui
            AND TARGET nijigenerate_category AND TARGET nijigenerate_button
            AND TARGET nijigenerate_tooltip
            AND NOT TARGET nijigenerate_inspector_meshgroup)
        add_library(nijigenerate_inspector_meshgroup INTERFACE)
        target_include_directories(nijigenerate_inspector_meshgroup INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_inspector_meshgroup INTERFACE cxx_std_20)
        target_sources(nijigenerate_inspector_meshgroup INTERFACE FILE_SET HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/meshgroup.hpp"
                "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/meshgroup_run.hpp")
        target_link_libraries(nijigenerate_inspector_meshgroup INTERFACE nijigenerate_runtime_core
            nijigenerate_inspector_common nijigenerate_imgui nijigenerate_category
            nijigenerate_button nijigenerate_tooltip)
    endif()
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/meshgroup_commands.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/meshgroup_commands.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
