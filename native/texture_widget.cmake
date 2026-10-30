if(NIJIGENERATE_LOGO_RESOURCES AND NIJIGENERATE_IMGUI AND TARGET nijigenerate_logo
        AND TARGET nijigenerate_label AND TARGET nijigenerate_dummy)
    add_library(nijigenerate_texture_widget STATIC src/nijigenerate/widgets/texture.cpp)
    target_compile_features(nijigenerate_texture_widget PUBLIC cxx_std_20)
    target_sources(nijigenerate_texture_widget PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/texture.hpp")
    target_link_libraries(nijigenerate_texture_widget PUBLIC nijigenerate_logo nijigenerate_label nijigenerate_dummy)
    add_custom_target(nijigenerate_texture_widget_project DEPENDS nijigenerate_texture_widget)
    if(BUILD_TESTING)
        add_executable(nijigenerate_texture_widget_probe tests/texture_widget_probe.cpp)
        target_link_libraries(nijigenerate_texture_widget_probe PRIVATE nijigenerate_texture_widget)
        add_dependencies(nijigenerate_texture_widget_project nijigenerate_texture_widget_probe)
        if(WIN32)
            get_target_property(texture_json_type Boost::json TYPE)
            if(texture_json_type STREQUAL "SHARED_LIBRARY")
                add_custom_command(TARGET nijigenerate_texture_widget_probe POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "$<TARGET_FILE:Boost::json>" "$<TARGET_FILE_DIR:nijigenerate_texture_widget_probe>")
            endif()
        endif()
        add_test(NAME native_texture_widget_contract COMMAND nijigenerate_texture_widget_probe)
        add_executable(nijigenerate_texture_recording_probe tests/texture_recording_probe.cpp
            src/nijigenerate/widgets/texture.cpp)
        target_include_directories(nijigenerate_texture_recording_probe PRIVATE tests/texture_recording src)
        target_link_libraries(nijigenerate_texture_recording_probe PRIVATE nijigenerate_native_options)
        add_dependencies(nijigenerate_texture_widget_project nijigenerate_texture_recording_probe)
        if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_INMATH_SOURCE_DIR)
            add_test(NAME native_texture_widget_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_texture_widget.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                    --native "$<TARGET_FILE:nijigenerate_texture_recording_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/texture-differential/$<CONFIG>")
        endif()
    endif()
endif()
