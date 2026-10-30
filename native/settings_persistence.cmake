if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_file_rename.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_file_write.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/core_time_exception.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_local_timestamp.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/settings_json_string.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        src/nijigenerate/core/settings_save.cpp src/nijigenerate/core/settings_value.cpp
        src/nijigenerate/compat/json_stringify.hpp
        src/nijigenerate/platform/file_move.cpp
        src/nijigenerate/compat/phobos_local_timestamp.cpp)
    if(NIJIGENERATE_RUNTIME_PROBE AND BUILD_TESTING)
        add_executable(nijigenerate_settings_save_probe tests/settings_save_probe.cpp)
        target_link_libraries(nijigenerate_settings_save_probe PRIVATE nijigenerate_runtime_core)
        if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
            add_test(NAME native_settings_save_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/settings_save_compare.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_settings_save_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/settings-save-evidence/$<CONFIG>")
            add_test(NAME native_settings_project_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/settings_project_contract.py"
                    --compiler "${NIJIGENERATE_D_COMPILER}"
                    --native "$<TARGET_FILE:nijigenerate_actionstack_settings_probe>"
                    --output "${CMAKE_CURRENT_BINARY_DIR}/settings-project-evidence/$<CONFIG>")
        endif()
        if(WIN32)
            add_custom_command(TARGET nijigenerate_settings_save_probe POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:nijigenerate_settings_save_probe>
                    $<TARGET_FILE_DIR:nijigenerate_settings_save_probe>
                COMMAND_EXPAND_LISTS)
        endif()
    endif()
endif()

if(TARGET nijigenerate_dialog_queue)
    add_library(nijigenerate_settings_dialog STATIC src/nijigenerate/core/settings_dialog.cpp)
    target_link_libraries(nijigenerate_settings_dialog PUBLIC nijigenerate_core nijigenerate_dialog_queue)
    if(MSVC)
        target_compile_options(nijigenerate_settings_dialog PRIVATE /W4 /WX)
    endif()
    if(BUILD_TESTING)
        add_executable(nijigenerate_settings_dialog_probe tests/settings_dialog_probe.cpp)
        target_link_libraries(nijigenerate_settings_dialog_probe PRIVATE nijigenerate_settings_dialog)
    endif()
endif()
