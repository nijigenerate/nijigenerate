if(TARGET nijigenerate_core)
    target_sources(nijigenerate_core PUBLIC
        FILE_SET translation_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/translation.hpp"
            "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/i18n_mo.hpp")
endif()

set(NIJIGENERATE_I18N_SOURCE_DIR "" CACHE PATH "Existing local i18n-d source directory for D comparisons")
if(BUILD_TESTING)
    add_executable(nijigenerate_translation_probe tests/translation_probe.cpp)
    target_link_libraries(nijigenerate_translation_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON AND NIJIGENERATE_I18N_SOURCE_DIR)
        add_test(NAME native_translation_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/translation_compare.py"
                --compiler "${NIJIGENERATE_D_COMPILER}" --i18n-source "${NIJIGENERATE_I18N_SOURCE_DIR}"
                --native "$<TARGET_FILE:nijigenerate_translation_probe>" --mode translation
                --output "${CMAKE_CURRENT_BINARY_DIR}/translation-evidence/$<CONFIG>/translation")
    endif()
endif()
