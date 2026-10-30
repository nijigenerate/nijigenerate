if(TARGET nicxlive::nicxlive)
    add_library(nijigenerate_flipconfig_declarations INTERFACE)
    target_compile_features(nijigenerate_flipconfig_declarations INTERFACE cxx_std_20)
    target_include_directories(nijigenerate_flipconfig_declarations INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core"
        "${NIJIGENERATE_NICXLIVE_SOURCE_DIR}/core/third_party")
    target_link_libraries(nijigenerate_flipconfig_declarations INTERFACE nijigenerate_native_options Boost::json)
    target_sources(nijigenerate_flipconfig_declarations INTERFACE FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/windows/flipconfig.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/windows/flipconfig_model.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/nijigenerate/windows/flipconfig_window.hpp")
    if(TARGET nijigenerate_window_base)
        target_link_libraries(nijigenerate_flipconfig_declarations INTERFACE nijigenerate_window_base)
    endif()
    if(TARGET nijigenerate_inputtext_declarations)
        target_link_libraries(nijigenerate_flipconfig_declarations INTERFACE nijigenerate_inputtext_declarations)
    endif()
    add_library(nijigenerate_flipconfig_state STATIC src/nijigenerate/windows/flipconfig_state.cpp)
    target_link_libraries(nijigenerate_flipconfig_state PUBLIC nijigenerate_flipconfig_declarations)
    if(BUILD_TESTING)
        add_library(nijigenerate_flipconfig_header_probe OBJECT
            tests/flipconfig_header_a.cpp tests/flipconfig_header_b.cpp)
        target_link_libraries(nijigenerate_flipconfig_header_probe PRIVATE nijigenerate_flipconfig_declarations)
        add_custom_target(nijigenerate_flipconfig_header_project DEPENDS nijigenerate_flipconfig_header_probe)
        add_executable(nijigenerate_flipconfig_result_fields_probe tests/flipconfig_result_fields_probe.cpp)
        target_link_libraries(nijigenerate_flipconfig_result_fields_probe PRIVATE nijigenerate_flipconfig_declarations)
        add_custom_target(nijigenerate_flipconfig_result_fields_project
            DEPENDS nijigenerate_flipconfig_header_probe nijigenerate_flipconfig_result_fields_probe)
        add_executable(nijigenerate_flipconfig_key_probe tests/flipconfig_key_probe.cpp)
        target_link_libraries(nijigenerate_flipconfig_key_probe PRIVATE nijigenerate_flipconfig_state)
        add_custom_target(nijigenerate_flipconfig_value_state_project
            DEPENDS nijigenerate_flipconfig_header_probe nijigenerate_flipconfig_result_fields_probe
                nijigenerate_flipconfig_key_probe)
        if(NIJIGENERATE_PYTHON AND NIJIGENERATE_D_COMPILER)
            add_test(NAME native_flipconfig_key
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_flipconfig_key.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_flipconfig_key_probe>"
                    --configuration "$<CONFIG>"
                    --output "${CMAKE_BINARY_DIR}/native/flipconfig-key/$<CONFIG>")
            add_test(NAME native_flipconfig_result_fields
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_flipconfig_result_fields.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_flipconfig_result_fields_probe>"
                    --configuration "$<CONFIG>"
                    --output "${CMAKE_BINARY_DIR}/native/flipconfig-result-fields/$<CONFIG>")
        endif()
        if(TARGET nijigenerate_transform_header_probe)
            add_custom_target(nijigenerate_flip_transform_headers_project
                DEPENDS nijigenerate_flipconfig_header_probe nijigenerate_transform_header_probe)
        endif()
    endif()
endif()
