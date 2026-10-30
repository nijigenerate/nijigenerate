if(TARGET nijigenerate_runtime_core AND TARGET nijigenerate_imgui)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/color_edit_fields.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET color_field_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/drag_float_fields.hpp")
    target_link_libraries(nijigenerate_runtime_core PUBLIC nijigenerate_imgui)
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/color_edit_fields.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/color_edit_fields.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()

if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/camera_command.cpp")
    if(TARGET nijigenerate_inspector_common AND TARGET nijigenerate_imgui
            AND TARGET nijigenerate_category AND TARGET nijigenerate_label AND TARGET nijigenerate_dummy
            AND TARGET nijigenerate_excamera AND NOT TARGET nijigenerate_inspector_camera)
        add_library(nijigenerate_inspector_camera INTERFACE)
        target_include_directories(nijigenerate_inspector_camera INTERFACE "${CMAKE_CURRENT_LIST_DIR}/src")
        target_compile_features(nijigenerate_inspector_camera INTERFACE cxx_std_20)
        target_sources(nijigenerate_inspector_camera INTERFACE FILE_SET HEADERS
            BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
            FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/camera.hpp"
                "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/panels/inspector/camera_run.hpp"
                "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/drag_float_fields.hpp")
        target_link_libraries(nijigenerate_inspector_camera INTERFACE nijigenerate_runtime_core
            nijigenerate_inspector_common nijigenerate_imgui nijigenerate_excamera
            nijigenerate_category nijigenerate_label nijigenerate_dummy)
    endif()
    if(MSVC)
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/camera_command.cpp"
            APPEND PROPERTY COMPILE_OPTIONS /fp:strict)
    else()
        set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/commands/inspector/camera_command.cpp"
            APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off -fno-fast-math)
    endif()
endif()
