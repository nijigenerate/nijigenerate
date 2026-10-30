option(NIJIGENERATE_DIALOGS "Build dialog queue, real icon initialization and ImGui rendering" OFF)
if(NIJIGENERATE_DIALOGS)
    if(NOT TARGET nijigenerate_runtime_core OR NOT TARGET nijigenerate_resources OR NOT NIJIGENERATE_IMGUI)
        message(FATAL_ERROR "Dialogs require runtime, embedded resources and pinned ImGui")
    endif()
    # Production targets must not depend on captured test reference packets.
    add_library(nijigenerate_dialog_enums INTERFACE)
    target_include_directories(nijigenerate_dialog_enums INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(nijigenerate_dialog_enums INTERFACE cxx_std_20)
    target_link_libraries(nijigenerate_dialog_enums INTERFACE nijigenerate_native_options nicxlive::nicxlive)
    target_sources(nijigenerate_dialog_enums INTERFACE
        FILE_SET dialog_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/dialog.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/widgets/detail/dialog_values.hpp")
    add_library(nijigenerate_dialog_queue STATIC src/nijigenerate/widgets/dialog.cpp)
    target_link_libraries(nijigenerate_dialog_queue PUBLIC nijigenerate_dialog_enums)
    if(MSVC)
        target_compile_options(nijigenerate_dialog_queue PRIVATE /W4 /WX)
    endif()
    add_library(nijigenerate_dialogs STATIC
        src/nijigenerate/widgets/dialog_init.cpp src/nijigenerate/widgets/dialog_render.cpp)
    target_link_libraries(nijigenerate_dialogs PUBLIC nijigenerate_dialog_queue
        PRIVATE nijigenerate_core nijigenerate_resources nicxlive::nicxlive
            nijigenerate_imgui nijigenerate_dummy nijigenerate_label nijigenerate_button)
    if(MSVC)
        target_compile_options(nijigenerate_dialogs PRIVATE /W4 /WX)
    endif()
    add_custom_target(nijigenerate_dialog_project
        DEPENDS nijigenerate_dialogs)
    if(BUILD_TESTING)
        add_subdirectory(tests/dialog-types dialog-contracts)
        add_dependencies(nijigenerate_dialog_project nijigenerate_dialog_types_project)
        add_executable(nijigenerate_dialog_runtime_probe tests/dialog_runtime_probe.cpp)
        target_link_libraries(nijigenerate_dialog_runtime_probe PRIVATE nijigenerate_dialogs
            nicxlive::nicxlive nijigenerate_imgui)
        add_dependencies(nijigenerate_dialog_project nijigenerate_dialog_runtime_probe)
        if(WIN32)
            get_target_property(dialog_json_type Boost::json TYPE)
            if(dialog_json_type STREQUAL "SHARED_LIBRARY")
                add_custom_command(TARGET nijigenerate_dialog_runtime_probe POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "$<TARGET_FILE:Boost::json>" "$<TARGET_FILE_DIR:nijigenerate_dialog_runtime_probe>")
            endif()
        endif()
        add_test(NAME native_dialog_runtime_contract COMMAND nijigenerate_dialog_runtime_probe)
        add_executable(nijigenerate_dialog_render_recording_probe
            tests/dialog_render_recording_probe.cpp src/nijigenerate/widgets/dialog_render.cpp)
        target_include_directories(nijigenerate_dialog_render_recording_probe PRIVATE
            "${NIJIGENERATE_IMGUI_SOURCE_DIR}")
        target_link_libraries(nijigenerate_dialog_render_recording_probe PRIVATE
            nijigenerate_dialog_queue nijigenerate_core nicxlive::nicxlive)
        if(MSVC)
            target_compile_options(nijigenerate_dialog_render_recording_probe PRIVATE /W4 /WX)
        endif()
        add_dependencies(nijigenerate_dialog_project nijigenerate_dialog_render_recording_probe)
        add_executable(nijigenerate_dialog_texture_array_probe tests/dialog_texture_array_probe.cpp)
        target_link_libraries(nijigenerate_dialog_texture_array_probe PRIVATE
            nijigenerate_dialog_queue nicxlive::nicxlive)
        add_dependencies(nijigenerate_dialog_project nijigenerate_dialog_texture_array_probe)
        add_test(NAME native_dialog_texture_array_contract COMMAND nijigenerate_dialog_texture_array_probe)
    endif()
endif()
