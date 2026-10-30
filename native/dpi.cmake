# Numeric DPI state; translated text remains a separate porting leaf.
add_library(nijigenerate_dpi STATIC src/nijigenerate/core/dpi.cpp)
target_link_libraries(nijigenerate_dpi PUBLIC nijigenerate_runtime_core nijigenerate_core)
target_sources(nijigenerate_dpi PUBLIC FILE_SET dpi_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/dpi.hpp")
target_compile_features(nijigenerate_dpi PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(nijigenerate_dpi PRIVATE /fp:strict)
else()
    target_compile_options(nijigenerate_dpi PRIVATE -ffp-contract=off -fno-fast-math)
endif()
target_compile_definitions(nijigenerate_dpi PUBLIC NG_USE_UI_SCALING)
if(APPLE)
    target_compile_definitions(nijigenerate_dpi PUBLIC NG_OSX)
endif()

# Build the project graph once instead of invoking MSBuild for each leaf probe.
add_custom_target(nijigenerate_dpi_project)
add_dependencies(nijigenerate_dpi_project nijigenerate_dpi)

if(NIJIGENERATE_RUNTIME_PROBE AND BUILD_TESTING)
    add_dependencies(nijigenerate_dpi_project nijigenerate_translation_probe)
    function(ng_check_dpi_compile_failures)
        # Compile-only checks must not mistake an unresolved external for privacy.
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
        foreach(kind private conflict)
            try_compile(dpi_${kind}_compiled "${CMAKE_CURRENT_BINARY_DIR}/negative-dpi-${kind}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/dpi_${kind}_fail.cpp"
                CXX_STANDARD 20 OUTPUT_VARIABLE dpi_negative_log)
            file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/negative-dpi-${kind}.log" "${dpi_negative_log}")
            if(dpi_${kind}_compiled OR NOT dpi_negative_log MATCHES "uiScale")
                message(FATAL_ERROR "DPI ${kind} compile rejection changed: ${dpi_negative_log}")
            endif()
        endforeach()
    endfunction()
    ng_check_dpi_compile_failures()
    foreach(mode enabled disabled locked osx)
        set(probe "nijigenerate_dpi_${mode}_probe")
        add_executable(${probe} tests/dpi_probe.cpp src/nijigenerate/core/dpi.cpp)
        target_link_libraries(${probe} PRIVATE nijigenerate_runtime_core)
        if(mode STREQUAL "enabled" OR mode STREQUAL "osx")
            target_compile_definitions(${probe} PRIVATE NG_USE_UI_SCALING)
        elseif(mode STREQUAL "locked")
            target_compile_definitions(${probe} PRIVATE NG_NO_UI_SCALING)
        endif()
        if(mode STREQUAL "osx")
            target_compile_definitions(${probe} PRIVATE NG_OSX)
        endif()
        if(WIN32)
            add_custom_command(TARGET ${probe} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${probe}> $<TARGET_FILE_DIR:${probe}>
                COMMAND_EXPAND_LISTS)
        endif()
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
            add_test(NAME native_dpi_${mode}_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/dpi_compare.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --native "$<TARGET_FILE:${probe}>"
                    --mode ${mode} --output "${CMAKE_CURRENT_BINARY_DIR}/dpi-evidence/$<CONFIG>/${mode}")
        endif()
        set(text_probe "nijigenerate_dpi_${mode}_text_probe")
        add_executable(${text_probe} tests/translation_probe.cpp src/nijigenerate/core/dpi.cpp)
        target_link_libraries(${text_probe} PRIVATE nijigenerate_runtime_core)
        add_dependencies(nijigenerate_dpi_project ${probe} ${text_probe})
        target_compile_definitions(${text_probe} PRIVATE NG_DPI_TEXT_PROBE)
        if(mode STREQUAL "enabled" OR mode STREQUAL "osx")
            target_compile_definitions(${text_probe} PRIVATE NG_USE_UI_SCALING)
        elseif(mode STREQUAL "locked")
            target_compile_definitions(${text_probe} PRIVATE NG_NO_UI_SCALING)
        endif()
        if(mode STREQUAL "osx")
            target_compile_definitions(${text_probe} PRIVATE NG_OSX)
        endif()
        if(WIN32)
            add_custom_command(TARGET ${text_probe} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${text_probe}> $<TARGET_FILE_DIR:${text_probe}>
                COMMAND_EXPAND_LISTS)
        endif()
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_I18N_SOURCE_DIR)
            add_test(NAME native_dpi_${mode}_text_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/translation_compare.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}" --i18n-source "${NIJIGENERATE_I18N_SOURCE_DIR}"
                    --native "$<TARGET_FILE:${text_probe}>" --mode ${mode}
                    --output "${CMAKE_CURRENT_BINARY_DIR}/translation-evidence/$<CONFIG>/${mode}")
        endif()
    endforeach()
endif()
