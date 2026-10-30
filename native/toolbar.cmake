add_library(nijigenerate_toolbar_declarations INTERFACE)
target_include_directories(nijigenerate_toolbar_declarations INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(nijigenerate_toolbar_declarations INTERFACE cxx_std_20)
target_sources(nijigenerate_toolbar_declarations INTERFACE FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/widgets/toolbar.hpp")
target_link_libraries(nijigenerate_toolbar_declarations INTERFACE nijigenerate_native_options)
if(TARGET nijigenerate_imgui AND TARGET nijigenerate_dummy AND TARGET nijigenerate_label)
    add_library(nijigenerate_toolbar STATIC src/nijigenerate/widgets/toolbar.cpp)
    target_link_libraries(nijigenerate_toolbar PUBLIC nijigenerate_toolbar_declarations
        PRIVATE nijigenerate_imgui nijigenerate_dummy nijigenerate_label)
endif()
if(BUILD_TESTING)
    add_library(nijigenerate_toolbar_header_probe OBJECT tests/toolbar_header_probe.cpp)
    target_link_libraries(nijigenerate_toolbar_header_probe PRIVATE nijigenerate_toolbar_declarations)
    add_custom_target(nijigenerate_toolbar_project DEPENDS nijigenerate_toolbar_header_probe)
    if(TARGET nijigenerate_toolbar)
        add_dependencies(nijigenerate_toolbar_project nijigenerate_toolbar)
        add_executable(nijigenerate_toolbar_recording_probe tests/toolbar_recording_probe.cpp
            src/nijigenerate/widgets/toolbar.cpp)
        target_include_directories(nijigenerate_toolbar_recording_probe PRIVATE tests/toolbar_recording src)
        target_link_libraries(nijigenerate_toolbar_recording_probe PRIVATE nijigenerate_native_options)
        add_dependencies(nijigenerate_toolbar_project nijigenerate_toolbar_recording_probe)
        if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_D_IMGUI_SOURCE_DIR)
            add_test(NAME native_toolbar_production_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_toolbar.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --binding-root "${NIJIGENERATE_D_IMGUI_SOURCE_DIR}"
                    --native "$<TARGET_FILE:nijigenerate_toolbar_recording_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/toolbar-differential/$<CONFIG>")
        endif()
    endif()
    if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER AND NIJIGENERATE_D_IMGUI_SOURCE_DIR)
        add_test(NAME native_toolbar_original_contract
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/toolbar_contract.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --binding-root "${NIJIGENERATE_D_IMGUI_SOURCE_DIR}"
                --output "${CMAKE_CURRENT_BINARY_DIR}/toolbar-original/$<CONFIG>")
    endif()
    if(NIJIGENERATE_PYTHON AND TARGET nijigenerate_imgui)
        set(toolbar_observation "${CMAKE_CURRENT_BINARY_DIR}/toolbar_imgui_observation.cpp")
        add_custom_command(OUTPUT "${toolbar_observation}"
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/toolbar_contract.py"
                --output "${CMAKE_CURRENT_BINARY_DIR}/toolbar-generated" --generate-gui "${toolbar_observation}"
            DEPENDS tests/toolbar_contract.py tests/toolbar_imgui_observation.cpp.inc
                "${CMAKE_CURRENT_SOURCE_DIR}/../source/nijigenerate/widgets/toolbar.d")
        add_executable(nijigenerate_toolbar_imgui_observation "${toolbar_observation}")
        target_link_libraries(nijigenerate_toolbar_imgui_observation PRIVATE nijigenerate_imgui nijigenerate_native_options)
        add_dependencies(nijigenerate_toolbar_project nijigenerate_toolbar_imgui_observation)
        add_test(NAME native_toolbar_source_imgui_observation COMMAND nijigenerate_toolbar_imgui_observation)
        if(TARGET nijigenerate_toolbar)
            add_executable(nijigenerate_toolbar_production_imgui_probe "${toolbar_observation}")
            target_compile_definitions(nijigenerate_toolbar_production_imgui_probe PRIVATE PRODUCTION_TOOLBAR)
            target_link_libraries(nijigenerate_toolbar_production_imgui_probe PRIVATE nijigenerate_toolbar nijigenerate_imgui)
            add_dependencies(nijigenerate_toolbar_project nijigenerate_toolbar_production_imgui_probe)
            add_test(NAME native_toolbar_production_imgui_contract COMMAND nijigenerate_toolbar_production_imgui_probe)
        endif()
    endif()
endif()
