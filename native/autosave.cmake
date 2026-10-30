add_library(nijigenerate_autosave STATIC
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/autosave.cpp")
target_include_directories(nijigenerate_autosave PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
target_compile_features(nijigenerate_autosave PUBLIC cxx_std_20)
target_sources(nijigenerate_autosave PUBLIC
    FILE_SET autosave_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/autosave.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/autosave_directory_entry.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/autosave_files.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_file_rmdir.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_file_directory_range.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_set_extension.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/autosave_path_policy.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/detail/path_allocation.hpp")

if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_autosave PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/autosave_settings.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/autosave_directory.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/autosave_lockfiles.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/autosave_release.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/autosave_history.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/io/autosave_backups.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/autosave_files.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/phobos_file_rmdir.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/autosave_path_policy.cpp")
    target_link_libraries(nijigenerate_autosave PUBLIC nijigenerate_runtime_core)
endif()

if(BUILD_TESTING)
    set(NIJIGENERATE_AUTOSAVE_REFERENCE_ROOT "${PROJECT_SOURCE_DIR}" CACHE PATH
        "Checkout containing verified original autosave observations")
    set(autosave_reference_arguments
        --reference-root "${NIJIGENERATE_AUTOSAVE_REFERENCE_ROOT}"
        --output-root "${CMAKE_CURRENT_BINARY_DIR}/autosave-comparisons/$<CONFIG>"
        --compiler "${NIJIGENERATE_D_COMPILER}")
    add_executable(nijigenerate_autosave_header_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/autosave_declarations_probe.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/autosave_header_consumer_probe.cpp")
    target_include_directories(nijigenerate_autosave_header_probe PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src")
    target_compile_features(nijigenerate_autosave_header_probe PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(nijigenerate_autosave_header_probe PRIVATE /Z7)
    endif()
    add_custom_target(nijigenerate_autosave_project)
    add_executable(nijigenerate_autosave_private_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/autosave_private_declarations_probe.cpp")
    target_link_libraries(nijigenerate_autosave_private_probe PRIVATE nijigenerate_autosave)
    add_executable(nijigenerate_autosave_record_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/autosave_record_probe.cpp")
    target_link_libraries(nijigenerate_autosave_record_probe PRIVATE nijigenerate_autosave)
    add_executable(nijigenerate_autosave_helpers_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/autosave_helpers_probe.cpp")
    target_link_libraries(nijigenerate_autosave_helpers_probe PRIVATE nijigenerate_autosave)
    add_executable(nijigenerate_autosave_public_helpers_probe
        "${CMAKE_CURRENT_LIST_DIR}/tests/autosave_public_helpers_probe.cpp")
    target_link_libraries(nijigenerate_autosave_public_helpers_probe PRIVATE nijigenerate_autosave)
    add_dependencies(nijigenerate_autosave_project
        nijigenerate_autosave_header_probe nijigenerate_autosave_private_probe
        nijigenerate_autosave_record_probe nijigenerate_autosave_helpers_probe
        nijigenerate_autosave_public_helpers_probe)
    if(NIJIGENERATE_RUNTIME_PROBE AND TARGET nijigenerate_runtime_core)
        add_executable(nijigenerate_autosave_settings_probe
            "${CMAKE_CURRENT_LIST_DIR}/tests/autosave_settings_probe.cpp")
        target_link_libraries(nijigenerate_autosave_settings_probe PRIVATE nijigenerate_autosave)
        add_executable(nijigenerate_autosave_history_get_probe
            "${CMAKE_CURRENT_LIST_DIR}/tests/autosave_history_get_probe.cpp")
        target_link_libraries(nijigenerate_autosave_history_get_probe PRIVATE nijigenerate_autosave)
        add_dependencies(nijigenerate_autosave_project
            nijigenerate_autosave_settings_probe nijigenerate_autosave_history_get_probe)
        add_executable(nijigenerate_autosave_directory_probe
            "${CMAKE_CURRENT_LIST_DIR}/tests/autosave_directory_probe.cpp")
        target_link_libraries(nijigenerate_autosave_directory_probe PRIVATE nijigenerate_autosave)
        add_dependencies(nijigenerate_autosave_project nijigenerate_autosave_directory_probe)
        add_executable(nijigenerate_autosave_lockfiles_probe
            "${CMAKE_CURRENT_LIST_DIR}/tests/autosave_lockfiles_probe.cpp")
        target_link_libraries(nijigenerate_autosave_lockfiles_probe PRIVATE nijigenerate_autosave)
        add_dependencies(nijigenerate_autosave_project nijigenerate_autosave_lockfiles_probe)
        add_executable(nijigenerate_autosave_backups_probe
            "${CMAKE_CURRENT_LIST_DIR}/tests/autosave_backups_probe.cpp")
        target_link_libraries(nijigenerate_autosave_backups_probe PRIVATE nijigenerate_autosave)
        add_dependencies(nijigenerate_autosave_project nijigenerate_autosave_backups_probe)
        if(WIN32)
            foreach(probe nijigenerate_autosave_private_probe nijigenerate_autosave_record_probe
                    nijigenerate_autosave_helpers_probe nijigenerate_autosave_public_helpers_probe
                    nijigenerate_autosave_settings_probe nijigenerate_autosave_history_get_probe
                    nijigenerate_autosave_directory_probe nijigenerate_autosave_lockfiles_probe
                    nijigenerate_autosave_backups_probe)
                add_custom_command(TARGET ${probe} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        $<TARGET_RUNTIME_DLLS:${probe}> $<TARGET_FILE_DIR:${probe}>
                    COMMAND_EXPAND_LISTS)
            endforeach()
        endif()
        if(NIJIGENERATE_PYTHON)
            add_test(NAME native_autosave_backups_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_autosave_backups.py"
                ${autosave_reference_arguments}
                    --native "$<TARGET_FILE:nijigenerate_autosave_backups_probe>"
                    --library "$<TARGET_FILE:nijigenerate_autosave>")
            add_test(NAME native_autosave_lockfiles_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_autosave_lockfiles.py"
                ${autosave_reference_arguments}
                    --native "$<TARGET_FILE:nijigenerate_autosave_lockfiles_probe>"
                    --library "$<TARGET_FILE:nijigenerate_autosave>")
            add_test(NAME native_autosave_directory_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_autosave_directory.py"
                ${autosave_reference_arguments}
                    --native "$<TARGET_FILE:nijigenerate_autosave_directory_probe>"
                    --library "$<TARGET_FILE:nijigenerate_autosave>")
            add_test(NAME native_autosave_settings_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_autosave_settings.py"
                ${autosave_reference_arguments}
                    --native "$<TARGET_FILE:nijigenerate_autosave_settings_probe>"
                    --library "$<TARGET_FILE:nijigenerate_autosave>")
            add_test(NAME native_autosave_history_get_differential
                COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tests/compare_autosave_history_get.py"
                ${autosave_reference_arguments}
                    --native "$<TARGET_FILE:nijigenerate_autosave_history_get_probe>"
                    --library "$<TARGET_FILE:nijigenerate_autosave>")
        endif()
    endif()
    add_test(NAME native_autosave_header_connection
        COMMAND nijigenerate_autosave_header_probe)
    add_test(NAME native_autosave_private_declarations
        COMMAND nijigenerate_autosave_private_probe)
    if(NIJIGENERATE_PYTHON)
        add_test(NAME native_autosave_declarations_differential
            COMMAND "${NIJIGENERATE_PYTHON}"
                "${CMAKE_CURRENT_LIST_DIR}/tests/compare_autosave_declarations.py"
                ${autosave_reference_arguments}
                --native "$<TARGET_FILE:nijigenerate_autosave_private_probe>")
        add_test(NAME native_autosave_record_differential
            COMMAND "${NIJIGENERATE_PYTHON}"
                "${CMAKE_CURRENT_LIST_DIR}/tests/compare_autosave_record.py"
                ${autosave_reference_arguments}
                --native "$<TARGET_FILE:nijigenerate_autosave_record_probe>")
        add_test(NAME native_autosave_helpers_differential
            COMMAND "${NIJIGENERATE_PYTHON}"
                "${CMAKE_CURRENT_LIST_DIR}/tests/compare_autosave_helpers.py"
                ${autosave_reference_arguments}
                --native "$<TARGET_FILE:nijigenerate_autosave_helpers_probe>"
                --public-native "$<TARGET_FILE:nijigenerate_autosave_public_helpers_probe>"
                --library "$<TARGET_FILE:nijigenerate_autosave>" --config "$<CONFIG>")
    endif()
endif()
