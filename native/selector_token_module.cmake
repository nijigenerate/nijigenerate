target_sources(nijigenerate_core PUBLIC
    FILE_SET selector_tokenizer_headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
    FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/tokenizer.hpp"
          "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_readonly_value.hpp"
          "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_json_arrays.hpp"
          "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_json_enums.hpp"
          "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/parser.hpp")
target_sources(nijigenerate_core PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/phobos_sort.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/phobos_sort.LICENSE"
    "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/unicode_ranges.inc")

if(BUILD_TESTING)
    add_executable(nijigenerate_selector_token_record_probe tests/selector_token_record_probe.cpp)
    target_link_libraries(nijigenerate_selector_token_record_probe PRIVATE nijigenerate_core)
    add_executable(nijigenerate_tokenizer_contract_probe tests/tokenizer_contract_probe.cpp)
    target_link_libraries(nijigenerate_tokenizer_contract_probe PRIVATE nijigenerate_core)
    if(NIJIGENERATE_D_COMPILER AND NIJIGENERATE_PYTHON)
        add_test(NAME native_selector_token_module_differential
            COMMAND "${NIJIGENERATE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_selector_token_module.py"
                --compiler "${NIJIGENERATE_D_COMPILER}"
                --record "$<TARGET_FILE:nijigenerate_selector_token_record_probe>"
                --contract "$<TARGET_FILE:nijigenerate_tokenizer_contract_probe>"
                --output "${CMAKE_CURRENT_BINARY_DIR}/selector-token-module/$<CONFIG>")
        set_tests_properties(native_selector_token_module_differential PROPERTIES TIMEOUT 300)
    endif()
endif()
