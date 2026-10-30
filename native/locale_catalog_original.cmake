if(TARGET nijigenerate_runtime_core)
    # The actual file adapter is available to production catalog consumers too.
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_initialization.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/platform/locale_catalog_file.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/platform/locale_catalog_entry_name.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/platform/locale_scan.cpp")
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET locale_module_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_entry.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_entries.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_entry_for.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_current_name.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_set.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_compare.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_duplicates.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_scan.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_state.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_case.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_case_data.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_culture.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_culture_data.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_catalog_bytes.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_catalog_native_data.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_catalog_file_bytes.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_catalog_entry_name.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/locale_catalog_file.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/locale_catalog_entry_name.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/platform/locale_scan.hpp")
endif()

if(NIJIGENERATE_RUNTIME_PROBE AND BUILD_TESTING AND TARGET nijigenerate_runtime_core AND
    NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_I18N_SOURCE_DIR)
    add_executable(nijigenerate_locale_catalog_module_probe tests/locale_catalog_module_probe.cpp)
    target_link_libraries(nijigenerate_locale_catalog_module_probe PRIVATE nijigenerate_runtime_core)
    if(WIN32)
        add_custom_command(TARGET nijigenerate_locale_catalog_module_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:nijigenerate_locale_catalog_module_probe>
                $<TARGET_FILE_DIR:nijigenerate_locale_catalog_module_probe>
            COMMAND_EXPAND_LISTS)
    endif()
    add_test(NAME native_locale_catalog_original_observations
        COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/observe_locale_catalog_original.py"
            --native "$<TARGET_FILE:nijigenerate_locale_catalog_module_probe>"
            --source-root "${PROJECT_SOURCE_DIR}" --compiler "${NIJIGENERATE_D_COMPILER}"
            --i18n-source "${NIJIGENERATE_I18N_SOURCE_DIR}" --configuration "$<CONFIG>"
            --output "${CMAKE_CURRENT_BINARY_DIR}/locale-catalog-original/$<CONFIG>")
endif()
