if(TARGET nijigenerate_imgui)
    add_library(nijigenerate_controller STATIC src/nijigenerate/widgets/controller_dashed.cpp)
    target_include_directories(nijigenerate_controller PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(nijigenerate_controller PUBLIC cxx_std_20)
    target_sources(nijigenerate_controller PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/controller.hpp")
    target_link_libraries(nijigenerate_controller PUBLIC nijigenerate_imgui nijigenerate_native_options)
    target_link_libraries(nijigenerate_controller PRIVATE Boost::headers)
    if(TARGET nicxlive::nicxlive)
        target_sources(nijigenerate_controller PRIVATE src/nijigenerate/widgets/controller_body.cpp)
        target_link_libraries(nijigenerate_controller PUBLIC nicxlive::nicxlive)
    endif()
    if(BUILD_TESTING)
        add_library(nijigenerate_controller_header_probe OBJECT tests/controller_header_probe.cpp)
        target_link_libraries(nijigenerate_controller_header_probe PRIVATE nijigenerate_controller)
        add_executable(nijigenerate_controller_imgui_probe tests/controller_imgui_probe.cpp)
        target_link_libraries(nijigenerate_controller_imgui_probe PRIVATE nijigenerate_controller)
        add_executable(nijigenerate_controller_recording_probe tests/controller_recording_probe.cpp
            src/nijigenerate/widgets/controller_dashed.cpp)
        target_include_directories(nijigenerate_controller_recording_probe PRIVATE tests/controller_recording src)
        target_link_libraries(nijigenerate_controller_recording_probe PRIVATE nijigenerate_native_options)
        add_custom_target(nijigenerate_controller_project DEPENDS nijigenerate_controller
            nijigenerate_controller_header_probe nijigenerate_controller_imgui_probe
            nijigenerate_controller_recording_probe)
        add_test(NAME native_controller_imgui_contract COMMAND nijigenerate_controller_imgui_probe)
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
            add_test(NAME native_controller_original_contract
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/controller_contract.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/controller-original/$<CONFIG>")
            add_test(NAME native_controller_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_controller.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_controller_recording_probe>"
                    --original "${CMAKE_CURRENT_BINARY_DIR}/controller-original/$<CONFIG>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/controller-differential/$<CONFIG>")
            set_tests_properties(native_controller_differential PROPERTIES DEPENDS native_controller_original_contract)
            if(NIJIGENERATE_INMATH_SOURCE_DIR)
                add_test(NAME native_controller_behavior_original_contract
                    COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/controller_behavior_contract.py"
                        --compiler "${NIJIGENERATE_D_COMPILER}" --inmath "${NIJIGENERATE_INMATH_SOURCE_DIR}"
                        --output "${CMAKE_CURRENT_BINARY_DIR}/controller-behavior-original/$<CONFIG>")
                if(TARGET nicxlive::nicxlive)
                    add_test(NAME native_controller_snap_mapping_observation
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/controller_snap_mapping.py"
                            --native "$<TARGET_FILE:nijigenerate_controller_snap_mapping_probe>"
                            --original "${CMAKE_CURRENT_BINARY_DIR}/controller-behavior-original/$<CONFIG>"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/controller-snap-mapping/$<CONFIG>")
                    set_tests_properties(native_controller_snap_mapping_observation PROPERTIES
                        DEPENDS native_controller_behavior_original_contract)
                    add_test(NAME native_controller_behavior_differential
                        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_controller_behavior.py"
                            --native "$<TARGET_FILE:nijigenerate_controller_behavior_recording_probe>"
                            --original "${CMAKE_CURRENT_BINARY_DIR}/controller-behavior-original/$<CONFIG>"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/controller-behavior-differential/$<CONFIG>")
                    set_tests_properties(native_controller_behavior_differential PROPERTIES
                        DEPENDS native_controller_behavior_original_contract)
                endif()
            endif()
        endif()
        if(TARGET nicxlive::nicxlive)
            target_compile_definitions(nijigenerate_controller_imgui_probe PRIVATE NIJIGENERATE_CONTROLLER_RUNTIME)
            add_executable(nijigenerate_controller_snap_mapping_probe tests/controller_snap_mapping_probe.cpp)
            target_link_libraries(nijigenerate_controller_snap_mapping_probe PRIVATE
                nicxlive::nicxlive nijigenerate_native_options)
            add_dependencies(nijigenerate_controller_project nijigenerate_controller_snap_mapping_probe)
            add_executable(nijigenerate_controller_behavior_recording_probe tests/controller_behavior_recording_probe.cpp
                src/nijigenerate/widgets/controller_body.cpp src/nijigenerate/widgets/controller_dashed.cpp)
            target_link_libraries(nijigenerate_controller_behavior_recording_probe PRIVATE Boost::headers)
            target_include_directories(nijigenerate_controller_behavior_recording_probe PRIVATE
                tests/controller_behavior_recording src)
            target_link_libraries(nijigenerate_controller_behavior_recording_probe PRIVATE
                nicxlive::nicxlive nijigenerate_native_options)
            add_dependencies(nijigenerate_controller_project nijigenerate_controller_behavior_recording_probe)
            if(WIN32 AND TARGET Boost::json)
                get_target_property(controller_json_type Boost::json TYPE)
                if(controller_json_type STREQUAL "SHARED_LIBRARY")
                    foreach(controller_probe IN ITEMS nijigenerate_controller_imgui_probe
                            nijigenerate_controller_snap_mapping_probe nijigenerate_controller_behavior_recording_probe)
                        add_custom_command(TARGET ${controller_probe} POST_BUILD
                            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Boost::json>"
                                "$<TARGET_FILE_DIR:${controller_probe}>")
                    endforeach()
                endif()
            endif()
        endif()
    endif()
endif()
