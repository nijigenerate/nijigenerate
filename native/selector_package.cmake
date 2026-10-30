if(TARGET nijigenerate_runtime_core)
    target_sources(nijigenerate_runtime_core PUBLIC
        FILE_SET selector_package_headers TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src"
        FILES "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/query.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/parse_uint.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_trivia.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_symbols.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_comparison_operator.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_identifier_tables.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_identifiers.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_named_entities.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_escape.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_character.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_plain_string.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_delimited_string.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_token_string.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_interpolated_string.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_integer.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_line.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/source_lexical_token.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/compat/translated_source_expression.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/treestore.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/treestore_sort.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/src/nijigenerate/core/selector/phobos_sort.hpp")
    target_sources(nijigenerate_runtime_core PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/port/licenses/source-expression-lexical.txt")
endif()
